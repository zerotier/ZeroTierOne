// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::collections::{HashMap, HashSet};
use std::hash::Hash;
use std::sync::atomic::Ordering;
use std::sync::Arc;
use std::time::Duration;

use async_trait::async_trait;
use parking_lot::{Mutex, RwLock};

use crate::error::InvalidParameterError;
use crate::util::debug_event;
use crate::util::gate::IntervalGate;
use crate::vl1::path::{Path, PathServiceResult};
use crate::vl1::peer::Peer;
use crate::vl1::protocol::*;
use crate::vl1::whoisqueue::{QueuedPacket, WhoisQueue};
use crate::vl1::{Address, Endpoint, Identity, RootSet};
use crate::Event;

/// Trait implemented by external code to handle events and provide an interface to the system or application.
///
/// These methods are basically callbacks that the core calls to request or transmit things. They are called
/// during calls to things like wire_recieve() and do_background_tasks().
#[async_trait]
pub trait SystemInterface: Sync + Send + 'static {
    /// Type for local system sockets.
    type LocalSocket: Sync + Send + Sized + Hash + PartialEq + Eq + Clone + ToString;

    /// Type for local system interfaces.    
    type LocalInterface: Sync + Send + Sized + Hash + PartialEq + Eq + Clone + ToString;

    /// An event occurred.
    async fn event(&self, event: Event);

    /// A USER_MESSAGE packet was received.
    async fn user_message(&self, source: &Identity, message_type: u64, message: &[u8]);

    /// Check a local socket for validity.
    ///
    /// This could return false if the socket's interface no longer exists, its port has been
    /// unbound, etc.
    fn local_socket_is_valid(&self, socket: &Self::LocalSocket) -> bool;

    /// Load this node's identity from the data store.
    async fn load_node_identity(&self) -> Option<Identity>;

    /// Save this node's identity.
    async fn save_node_identity(&self, id: &Identity);

    /// Called to send a packet over the physical network (virtual -> physical).
    ///
    /// This may return false if the send definitely failed. Otherwise it should return true
    /// which indicates possible success but with no guarantee (UDP semantics).
    ///
    /// If a local socket is specified the implementation should send from that socket or not
    /// at all (returning false). If a local interface is specified the implementation should
    /// send from all sockets on that interface. If neither is specified the packet may be
    /// sent on all sockets or a random subset.
    ///
    /// For endpoint types that support a packet TTL, the implementation may set the TTL
    /// if the 'ttl' parameter is not zero. If the parameter is zero or TTL setting is not
    /// supported, the default TTL should be used.
    async fn wire_send(&self, endpoint: &Endpoint, local_socket: Option<&Self::LocalSocket>, local_interface: Option<&Self::LocalInterface>, data: &[&[u8]], packet_ttl: u8) -> bool;

    /// Called to check and see if a physical address should be used for ZeroTier traffic to a node.
    async fn check_path(&self, id: &Identity, endpoint: &Endpoint, local_socket: Option<&Self::LocalSocket>, local_interface: Option<&Self::LocalInterface>) -> bool;

    /// Called to look up any statically defined or memorized paths to known nodes.
    async fn get_path_hints(&self, id: &Identity) -> Option<Vec<(Endpoint, Option<Self::LocalSocket>, Option<Self::LocalInterface>)>>;

    /// Called to get the current time in milliseconds from the system monotonically increasing clock.
    /// This needs to be accurate to about 250 milliseconds resolution or better.
    fn time_ticks(&self) -> i64;

    /// Called to get the current time in milliseconds since epoch from the real-time clock.
    /// This needs to be accurate to about one second resolution or better.
    fn time_clock(&self) -> i64;
}

/// Interface between VL1 and higher/inner protocol layers.
///
/// This is implemented by Switch in VL2. It's usually not used outside of VL2 in the core but
/// it could also be implemented for testing or "off label" use of VL1 to carry different protocols.
#[async_trait]
pub trait InnerProtocolInterface: Sync + Send + 'static {
    /// Handle a packet, returning true if it was handled by the next layer.
    ///
    /// Do not attempt to handle OK or ERROR. Instead implement handle_ok() and handle_error().
    /// The return values of these must follow the same semantic of returning true if the message
    /// was handled.
    async fn handle_packet<SI: SystemInterface>(&self, source: &Peer<SI>, source_path: &Path<SI>, forward_secrecy: bool, extended_authentication: bool, verb: u8, payload: &PacketBuffer) -> bool;

    /// Handle errors, returning true if the error was recognized.
    async fn handle_error<SI: SystemInterface>(
        &self,
        source: &Peer<SI>,
        source_path: &Path<SI>,
        forward_secrecy: bool,
        extended_authentication: bool,
        in_re_verb: u8,
        in_re_message_id: u64,
        error_code: u8,
        payload: &PacketBuffer,
        cursor: &mut usize,
    ) -> bool;

    /// Handle an OK, returing true if the OK was recognized.
    async fn handle_ok<SI: SystemInterface>(&self, source: &Peer<SI>, source_path: &Path<SI>, forward_secrecy: bool, extended_authentication: bool, in_re_verb: u8, in_re_message_id: u64, payload: &PacketBuffer, cursor: &mut usize) -> bool;

    /// Check if this remote peer has a trust relationship with this node.
    ///
    /// This is checked to determine if we should do things like make direct links ore respond to
    /// various other VL1 messages.
    fn has_trust_relationship(&self, id: &Identity) -> bool;
}

/// How often to check the root cluster definitions against the root list and update.
const ROOT_SYNC_INTERVAL_MS: i64 = 1000;

#[derive(Default)]
struct BackgroundTaskIntervals {
    root_sync: IntervalGate<{ ROOT_SYNC_INTERVAL_MS }>,
    root_hello: IntervalGate<{ ROOT_HELLO_INTERVAL }>,
    peers: IntervalGate<{ crate::vl1::peer::SERVICE_INTERVAL_MS }>,
    paths: IntervalGate<{ crate::vl1::path::SERVICE_INTERVAL_MS }>,
    whois: IntervalGate<{ crate::vl1::whoisqueue::SERVICE_INTERVAL_MS }>,
}

struct RootInfo<SI: SystemInterface> {
    roots: HashMap<Arc<Peer<SI>>, Vec<Endpoint>>,
    sets: HashMap<String, RootSet>,
    sets_modified: bool,
}

enum PathKey<'a, SI: SystemInterface> {
    Copied(Endpoint, SI::LocalSocket),
    Ref(&'a Endpoint, &'a SI::LocalSocket),
}

impl<'a, SI: SystemInterface> Hash for PathKey<'a, SI> {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        match self {
            Self::Copied(ep, ls) => {
                ep.hash(state);
                ls.hash(state);
            }
            Self::Ref(ep, ls) => {
                (*ep).hash(state);
                (*ls).hash(state);
            }
        }
    }
}

impl<'a, SI: SystemInterface> PartialEq for PathKey<'_, SI> {
    fn eq(&self, other: &Self) -> bool {
        match (self, other) {
            (Self::Copied(ep1, ls1), Self::Copied(ep2, ls2)) => ep1.eq(ep2) && ls1.eq(ls2),
            (Self::Copied(ep1, ls1), Self::Ref(ep2, ls2)) => ep1.eq(*ep2) && ls1.eq(*ls2),
            (Self::Ref(ep1, ls1), Self::Copied(ep2, ls2)) => (*ep1).eq(ep2) && (*ls1).eq(ls2),
            (Self::Ref(ep1, ls1), Self::Ref(ep2, ls2)) => (*ep1).eq(*ep2) && (*ls1).eq(*ls2),
        }
    }
}

impl<'a, SI: SystemInterface> Eq for PathKey<'_, SI> {}

impl<'a, SI: SystemInterface> PathKey<'a, SI> {
    #[inline(always)]
    fn local_socket(&self) -> &SI::LocalSocket {
        match self {
            Self::Copied(_, ls) => ls,
            Self::Ref(_, ls) => *ls,
        }
    }

    #[inline(always)]
    fn to_copied(&self) -> PathKey<'static, SI> {
        match self {
            Self::Copied(ep, ls) => PathKey::<'static, SI>::Copied(ep.clone(), ls.clone()),
            Self::Ref(ep, ls) => PathKey::<'static, SI>::Copied((*ep).clone(), (*ls).clone()),
        }
    }
}

/// A VL1 global P2P network node.
pub struct Node<SI: SystemInterface> {
    /// A random ID generated to identify this particular running instance.
    pub instance_id: [u8; 16],

    /// This node's identity and permanent keys.
    pub identity: Identity,

    /// Interval latches for periodic background tasks.
    intervals: Mutex<BackgroundTaskIntervals>,

    /// Canonicalized network paths, held as Weak<> to be automatically cleaned when no longer in use.
    paths: parking_lot::RwLock<HashMap<PathKey<'static, SI>, Arc<Path<SI>>>>,

    /// Peers with which we are currently communicating.
    peers: parking_lot::RwLock<HashMap<Address, Arc<Peer<SI>>>>,

    /// This node's trusted roots, sorted in ascending order of quality/preference, and cluster definitions.
    roots: RwLock<RootInfo<SI>>,

    /// Current best root.
    best_root: RwLock<Option<Arc<Peer<SI>>>>,

    /// Identity lookup queue, also holds packets waiting on a lookup.
    whois: WhoisQueue,

    /// Reusable network buffer pool.
    buffer_pool: PacketBufferPool,
}

impl<SI: SystemInterface> Node<SI> {
    pub async fn new(si: &SI, auto_generate_identity: bool, auto_upgrade_identity: bool) -> Result<Self, InvalidParameterError> {
        let mut id = {
            let id = si.load_node_identity().await;
            if id.is_none() {
                if !auto_generate_identity {
                    return Err(InvalidParameterError("no identity found and auto-generate not enabled"));
                } else {
                    let id = Identity::generate();
                    si.event(Event::IdentityAutoGenerated(id.clone())).await;
                    si.save_node_identity(&id).await;
                    id
                }
            } else {
                id.unwrap()
            }
        };

        if auto_upgrade_identity {
            let old = id.clone();
            if id.upgrade()? {
                si.save_node_identity(&id).await;
                si.event(Event::IdentityAutoUpgraded(old, id.clone())).await;
            }
        }

        Ok(Self {
            instance_id: zerotier_core_crypto::random::get_bytes_secure(),
            identity: id,
            intervals: Mutex::new(BackgroundTaskIntervals::default()),
            paths: parking_lot::RwLock::new(HashMap::new()),
            peers: parking_lot::RwLock::new(HashMap::new()),
            roots: RwLock::new(RootInfo {
                roots: HashMap::new(),
                sets: HashMap::new(),
                sets_modified: false,
            }),
            best_root: RwLock::new(None),
            whois: WhoisQueue::new(),
            buffer_pool: PacketBufferPool::new(64, PacketBufferFactory::new()),
        })
    }

    #[inline(always)]
    pub fn get_packet_buffer(&self) -> PooledPacketBuffer {
        self.buffer_pool.get()
    }

    pub fn peer(&self, a: Address) -> Option<Arc<Peer<SI>>> {
        self.peers.read().get(&a).cloned()
    }

    pub async fn do_background_tasks(&self, si: &SI) -> Duration {
        let tt = si.time_ticks();
        let (root_sync, root_hello, peer_check, path_check, whois_check) = {
            let mut intervals = self.intervals.lock();
            (intervals.root_sync.gate(tt), intervals.root_hello.gate(tt), intervals.peers.gate(tt), intervals.paths.gate(tt), intervals.whois.gate(tt))
        };

        if root_sync {
            if {
                let mut roots = self.roots.write();
                if roots.sets_modified {
                    roots.sets_modified = false;
                    true
                } else {
                    false
                }
            } {
                let (mut old_root_identities, address_collisions, new_roots, bad_identities) = {
                    let roots = self.roots.read();

                    let old_root_identities: Vec<Identity> = roots.roots.iter().map(|(p, _)| p.identity.clone()).collect();
                    let mut new_roots = HashMap::new();
                    let mut bad_identities = Vec::new();

                    // This is a sanity check to make sure we don't have root sets that contain roots with the same address
                    // but a different identity. If we do, the offending address is blacklisted. This would indicate something
                    // weird and possibly nasty happening with whomever is making your root set definitions.
                    let mut address_collisions = Vec::new();
                    {
                        let mut address_collision_check = HashMap::with_capacity(roots.sets.len() * 8);
                        for (_, rc) in roots.sets.iter() {
                            for m in rc.members.iter() {
                                if self.peers.read().get(&m.identity.address).map_or(false, |p| !p.identity.eq(&m.identity)) || address_collision_check.insert(m.identity.address, &m.identity).map_or(false, |old_id| !old_id.eq(&m.identity)) {
                                    address_collisions.push(m.identity.address);
                                }
                            }
                        }
                    }

                    for (_, rc) in roots.sets.iter() {
                        for m in rc.members.iter() {
                            if m.endpoints.is_some() && !address_collisions.contains(&m.identity.address) {
                                let peers = self.peers.upgradable_read();
                                if let Some(peer) = peers.get(&m.identity.address) {
                                    new_roots.insert(peer.clone(), m.endpoints.as_ref().unwrap().iter().cloned().collect());
                                } else {
                                    if let Some(peer) = Peer::<SI>::new(&self.identity, m.identity.clone(), si.time_clock()) {
                                        new_roots.insert(parking_lot::RwLockUpgradableReadGuard::upgrade(peers).entry(m.identity.address).or_insert_with(|| Arc::new(peer)).clone(), m.endpoints.as_ref().unwrap().iter().cloned().collect());
                                    } else {
                                        bad_identities.push(m.identity.clone());
                                    }
                                }
                            }
                        }
                    }

                    (old_root_identities, address_collisions, new_roots, bad_identities)
                };

                for c in address_collisions.iter() {
                    si.event(Event::SecurityWarning(format!("address/identity collision in root sets! address {} collides across root sets or with an existing peer and is being ignored as a root!", c.to_string()))).await;
                }
                for i in bad_identities.iter() {
                    si.event(Event::SecurityWarning(format!("bad identity detected for address {} in at least one root set, ignoring (error creating peer object)", i.address.to_string()))).await;
                }

                let mut new_root_identities: Vec<Identity> = new_roots.iter().map(|(p, _)| p.identity.clone()).collect();

                old_root_identities.sort_unstable();
                new_root_identities.sort_unstable();
                if !old_root_identities.eq(&new_root_identities) {
                    let mut best: Option<Arc<Peer<SI>>> = None;

                    {
                        let mut roots = self.roots.write();
                        roots.roots = new_roots;

                        // The best root is the one that has replied to a HELLO most recently. Since we send HELLOs in unison
                        // this is a proxy for latency and also causes roots that fail to reply to drop out quickly.
                        let mut latest_hello_reply = 0;
                        for (r, _) in roots.roots.iter() {
                            let t = r.last_hello_reply_time_ticks.load(Ordering::Relaxed);
                            if t >= latest_hello_reply {
                                latest_hello_reply = t;
                                let _ = best.insert(r.clone());
                            }
                        }
                    }

                    *(self.best_root.write()) = best;

                    //debug_event!(si, "new best root: {}", best.as_ref().map_or("none".into(), |p| p.identity.address.to_string()));
                    //si.event(Event::UpdatedRoots(old_root_identities, new_root_identities)).await;
                }
            }
        }

        // Say HELLO to all roots periodically. For roots we send HELLO to every single endpoint
        // they have, which is a behavior that differs from normal peers. This allows roots to
        // e.g. see our IPv4 and our IPv6 address which can be important for us to learn our
        // external addresses from them.
        if root_hello {
            let roots = {
                let roots = self.roots.read();
                let mut roots_copy = Vec::with_capacity(roots.roots.len());
                for (root, endpoints) in roots.roots.iter() {
                    roots_copy.push((root.clone(), endpoints.clone()));
                }
                roots_copy
            };
            for (root, endpoints) in roots.iter() {
                for ep in endpoints.iter() {
                    debug_event!(si, "sending HELLO to root {} (root interval: {})", root.identity.address.to_string(), ROOT_HELLO_INTERVAL);
                    root.send_hello(si, self, Some(ep)).await;
                }
            }
        }

        if peer_check {
            // Service all peers, removing any whose service() method returns false AND that are not
            // roots. Roots on the other hand remain in the peer list as long as they are roots.
            let mut dead_peers = Vec::new();
            {
                let roots = self.roots.read();
                for (a, peer) in self.peers.read().iter() {
                    if !peer.service(si, self, tt) && !roots.roots.contains_key(peer) {
                        dead_peers.push(*a);
                    }
                }
            }
            for dp in dead_peers.iter() {
                self.peers.write().remove(dp);
            }
        }

        if path_check {
            // Service all paths, removing expired or invalid ones. This is done in two passes to
            // avoid introducing latency into a flow.
            let mut dead_paths = Vec::new();
            let mut need_keepalive = Vec::new();
            for (k, path) in self.paths.read().iter() {
                if si.local_socket_is_valid(k.local_socket()) {
                    match path.service(tt) {
                        PathServiceResult::Ok => {}
                        PathServiceResult::Dead => dead_paths.push(k.to_copied()),
                        PathServiceResult::NeedsKeepalive => need_keepalive.push(path.clone()),
                    }
                } else {
                    dead_paths.push(k.to_copied());
                }
            }
            for dp in dead_paths.iter() {
                self.paths.write().remove(dp);
            }
            let z = [&crate::util::ZEROES[..1]];
            for ka in need_keepalive.iter() {
                si.wire_send(&ka.endpoint, Some(&ka.local_socket), Some(&ka.local_interface), &z, 0).await;
            }
        }

        if whois_check {
            self.whois.service(si, self, tt);
        }

        Duration::from_millis((ROOT_SYNC_INTERVAL_MS.min(crate::vl1::whoisqueue::SERVICE_INTERVAL_MS).min(crate::vl1::path::SERVICE_INTERVAL_MS).min(crate::vl1::peer::SERVICE_INTERVAL_MS) as u64) / 2)
    }

    pub async fn handle_incoming_physical_packet<PH: InnerProtocolInterface>(&self, si: &SI, ph: &PH, source_endpoint: &Endpoint, source_local_socket: &SI::LocalSocket, source_local_interface: &SI::LocalInterface, mut data: PooledPacketBuffer) {
        debug_event!(
            si,
            "<< #{} ->{} from {} length {} via socket {}@{}",
            data.bytes_fixed_at::<8>(0).map_or("????????????????".into(), |pid| zerotier_core_crypto::hex::to_string(pid)),
            data.bytes_fixed_at::<5>(8).map_or("??????????".into(), |dest| zerotier_core_crypto::hex::to_string(dest)),
            source_endpoint.to_string(),
            data.len(),
            source_local_socket.to_string(),
            source_local_interface.to_string()
        );

        if let Ok(fragment_header) = data.struct_mut_at::<FragmentHeader>(0) {
            if let Some(dest) = Address::from_bytes_fixed(&fragment_header.dest) {
                let time_ticks = si.time_ticks();
                if dest == self.identity.address {
                    let path = self.canonical_path(source_endpoint, source_local_socket, source_local_interface, time_ticks);
                    path.log_receive_anything(time_ticks);

                    if fragment_header.is_fragment() {
                        debug_event!(si, "-- #{:0>16x} fragment {} of {} received", u64::from_be_bytes(fragment_header.id), fragment_header.fragment_no(), fragment_header.total_fragments());

                        if let Some(assembled_packet) = path.receive_fragment(fragment_header.packet_id(), fragment_header.fragment_no(), fragment_header.total_fragments(), data, time_ticks) {
                            if let Some(frag0) = assembled_packet.frags[0].as_ref() {
                                debug_event!(si, "-- #{:0>16x} packet fully assembled!", u64::from_be_bytes(fragment_header.id));

                                if let Ok(packet_header) = frag0.struct_at::<PacketHeader>(0) {
                                    if let Some(source) = Address::from_bytes(&packet_header.src) {
                                        if let Some(peer) = self.peer(source) {
                                            peer.receive(self, si, ph, time_ticks, &path, &packet_header, frag0, &assembled_packet.frags[1..(assembled_packet.have as usize)]).await;
                                        } else {
                                            self.whois.query(self, si, source, Some(QueuedPacket::Fragmented(assembled_packet)));
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        if let Ok(packet_header) = data.struct_at::<PacketHeader>(0) {
                            debug_event!(si, "-- #{:0>16x} is unfragmented", u64::from_be_bytes(fragment_header.id));

                            if let Some(source) = Address::from_bytes(&packet_header.src) {
                                if let Some(peer) = self.peer(source) {
                                    peer.receive(self, si, ph, time_ticks, &path, &packet_header, data.as_ref(), &[]).await;
                                } else {
                                    self.whois.query(self, si, source, Some(QueuedPacket::Unfragmented(data)));
                                }
                            }
                        }
                    }
                } else {
                    if fragment_header.is_fragment() {
                        debug_event!(si, "-- #{:0>16x} forwarding packet fragment to {}", u64::from_be_bytes(fragment_header.id), dest.to_string());
                        if fragment_header.increment_hops() > FORWARD_MAX_HOPS {
                            debug_event!(si, "-- #{:0>16x} discarded: max hops exceeded!", u64::from_be_bytes(fragment_header.id));
                            return;
                        }
                    } else {
                        if let Ok(packet_header) = data.struct_mut_at::<PacketHeader>(0) {
                            debug_event!(si, "-- #{:0>16x} forwarding packet to {}", u64::from_be_bytes(fragment_header.id), dest.to_string());
                            if packet_header.increment_hops() > FORWARD_MAX_HOPS {
                                debug_event!(si, "-- #{:0>16x} discarded: max hops exceeded!", u64::from_be_bytes(fragment_header.id));
                                return;
                            }
                        } else {
                            return;
                        }
                    }

                    if let Some(peer) = self.peer(dest) {
                        // TODO: SHOULD we forward? Need a way to check.
                        peer.forward(si, time_ticks, data.as_ref()).await;
                        debug_event!(si, "-- #{:0>16x} forwarded successfully", u64::from_be_bytes(fragment_header.id));
                    }
                }
            }
        }
    }

    pub fn root(&self) -> Option<Arc<Peer<SI>>> {
        self.best_root.read().clone()
    }

    pub fn is_peer_root(&self, peer: &Peer<SI>) -> bool {
        self.roots.read().roots.contains_key(peer)
    }

    pub fn add_update_root_set(&self, rs: RootSet) -> bool {
        let mut roots = self.roots.write();
        if let Some(entry) = roots.sets.get_mut(&rs.name) {
            if rs.should_replace(entry) {
                *entry = rs;
                roots.sets_modified = true;
                return true;
            }
        } else if rs.verify() {
            roots.sets.insert(rs.name.clone(), rs);
            roots.sets_modified = true;
            return true;
        }
        return false;
    }

    pub fn has_roots_defined(&self) -> bool {
        self.roots.read().sets.iter().any(|rs| !rs.1.members.is_empty())
    }

    pub fn root_sets(&self) -> Vec<RootSet> {
        self.roots.read().sets.values().cloned().collect()
    }

    pub fn canonical_path(&self, ep: &Endpoint, local_socket: &SI::LocalSocket, local_interface: &SI::LocalInterface, time_ticks: i64) -> Arc<Path<SI>> {
        if let Some(path) = self.paths.read().get(&PathKey::Ref(ep, local_socket)) {
            path.clone()
        } else {
            self.paths.write().entry(PathKey::Copied(ep.clone(), local_socket.clone())).or_insert_with(|| Arc::new(Path::new(ep.clone(), local_socket.clone(), local_interface.clone(), time_ticks))).clone()
        }
    }
}
