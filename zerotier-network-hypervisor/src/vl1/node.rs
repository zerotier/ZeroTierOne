// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::collections::HashMap;
use std::hash::Hash;
use std::io::Write;
use std::sync::atomic::Ordering;
use std::sync::Arc;
use std::time::Duration;

use async_trait::async_trait;
use parking_lot::{Mutex, RwLock};

use crate::error::InvalidParameterError;
use crate::util::debug_event;
use crate::util::gate::IntervalGate;
use crate::util::marshalable::Marshalable;
use crate::vl1::careof::CareOf;
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
    ///
    /// This isn't async to avoid all kinds of issues in code that deals with locks. If you need
    /// it to be async use a channel or something.
    fn event(&self, event: Event);

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
    /// This is checked to determine if we should do things like make direct links or respond to
    /// various other VL1 messages.
    fn has_trust_relationship(&self, id: &Identity) -> bool;
}

/// How often to check the root cluster definitions against the root list and update.
const ROOT_SYNC_INTERVAL_MS: i64 = 1000;

#[derive(Default)]
struct BackgroundTaskIntervals {
    root_sync: IntervalGate<{ ROOT_SYNC_INTERVAL_MS }>,
    root_hello: IntervalGate<{ ROOT_HELLO_INTERVAL }>,
    root_spam_hello: IntervalGate<{ ROOT_HELLO_SPAM_INTERVAL }>,
    peer_service: IntervalGate<{ crate::vl1::peer::SERVICE_INTERVAL_MS }>,
    path_service: IntervalGate<{ crate::vl1::path::SERVICE_INTERVAL_MS }>,
    whois_service: IntervalGate<{ crate::vl1::whoisqueue::SERVICE_INTERVAL_MS }>,
}

struct RootInfo<SI: SystemInterface> {
    sets: HashMap<String, RootSet>,
    roots: HashMap<Arc<Peer<SI>>, Vec<Endpoint>>,
    care_of: Vec<u8>,
    my_root_sets: Option<Vec<u8>>,
    sets_modified: bool,
    online: bool,
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
                    si.event(Event::IdentityAutoGenerated(id.clone()));
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
                si.event(Event::IdentityAutoUpgraded(old, id.clone()));
            }
        }

        debug_event!(si, "[vl1] loaded identity {}", id.to_string());

        Ok(Self {
            instance_id: zerotier_core_crypto::random::get_bytes_secure(),
            identity: id,
            intervals: Mutex::new(BackgroundTaskIntervals::default()),
            paths: parking_lot::RwLock::new(HashMap::new()),
            peers: parking_lot::RwLock::new(HashMap::new()),
            roots: RwLock::new(RootInfo {
                sets: HashMap::new(),
                roots: HashMap::new(),
                care_of: Vec::new(),
                my_root_sets: None,
                sets_modified: false,
                online: false,
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

    pub fn is_online(&self) -> bool {
        self.roots.read().online
    }

    fn update_best_root(&self, si: &SI, time_ticks: i64) {
        let roots = self.roots.read();

        // The best root is the one that has replied to a HELLO most recently. Since we send HELLOs in unison
        // this is a proxy for latency and also causes roots that fail to reply to drop out quickly.
        let mut best: Option<&Arc<Peer<SI>>> = None;
        let mut latest_hello_reply = 0;
        for (r, _) in roots.roots.iter() {
            let t = r.last_hello_reply_time_ticks.load(Ordering::Relaxed);
            if t > latest_hello_reply {
                latest_hello_reply = t;
                let _ = best.insert(r);
            }
        }

        if let Some(best) = best {
            let mut best_root = self.best_root.write();
            if let Some(best_root) = best_root.as_mut() {
                if !Arc::ptr_eq(best_root, best) {
                    debug_event!(si, "[vl1] new best root: {} (replaced {})", best.identity.address.to_string(), best_root.identity.address.to_string());
                    *best_root = best.clone();
                }
            } else {
                debug_event!(si, "[vl1] new best root: {} (was empty)", best.identity.address.to_string());
                let _ = best_root.insert(best.clone());
            }
        } else {
            if let Some(old_best) = self.best_root.write().take() {
                debug_event!(si, "[vl1] new best root: NONE (replaced {})", old_best.identity.address.to_string());
            }
        }

        // Determine if the node is online by whether there is a currently reachable root.
        if (time_ticks - latest_hello_reply) < (ROOT_HELLO_INTERVAL * 2) && best.is_some() {
            if !roots.online {
                drop(roots);
                self.roots.write().online = true;
                si.event(Event::Online(true));
            }
        } else if roots.online {
            drop(roots);
            self.roots.write().online = false;
            si.event(Event::Online(false));
        }
    }

    pub async fn do_background_tasks(&self, si: &SI) -> Duration {
        let tt = si.time_ticks();
        let (root_sync, root_hello, mut root_spam_hello, peer_service, path_service, whois_service) = {
            let mut intervals = self.intervals.lock();
            (intervals.root_sync.gate(tt), intervals.root_hello.gate(tt), intervals.root_spam_hello.gate(tt), intervals.peer_service.gate(tt), intervals.path_service.gate(tt), intervals.whois_service.gate(tt))
        };

        // We only "spam" if we are offline.
        if root_spam_hello {
            root_spam_hello = !self.is_online();
        }

        debug_event!(
            si,
            "[vl1] do_background_tasks:{}{}{}{}{}{} ----",
            if root_sync { " root_sync" } else { "" },
            if root_hello { " root_hello" } else { "" },
            if root_spam_hello { " root_spam_hello" } else { "" },
            if peer_service { " peer_service" } else { "" },
            if path_service { " path_service" } else { "" },
            if whois_service { " whois_service" } else { "" },
        );

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
                debug_event!(si, "[vl1] root sets modified, synchronizing internal data structures");

                let (mut old_root_identities, address_collisions, new_roots, bad_identities, my_root_sets) = {
                    let roots = self.roots.read();

                    let old_root_identities: Vec<Identity> = roots.roots.iter().map(|(p, _)| p.identity.clone()).collect();
                    let mut new_roots = HashMap::new();
                    let mut bad_identities = Vec::new();
                    let mut my_root_sets: Option<Vec<u8>> = None;

                    // This is a sanity check to make sure we don't have root sets that contain roots with the same address
                    // but a different identity. If we do, the offending address is blacklisted. This would indicate something
                    // weird and possibly nasty happening with whomever is making your root set definitions.
                    let mut address_collisions = Vec::new();
                    {
                        let mut address_collision_check = HashMap::with_capacity(roots.sets.len() * 8);
                        for (_, rs) in roots.sets.iter() {
                            for m in rs.members.iter() {
                                if m.identity.eq(&self.identity) {
                                    let _ = my_root_sets.get_or_insert_with(|| Vec::new()).write_all(rs.to_bytes().as_slice());
                                } else if self.peers.read().get(&m.identity.address).map_or(false, |p| !p.identity.eq(&m.identity)) || address_collision_check.insert(m.identity.address, &m.identity).map_or(false, |old_id| !old_id.eq(&m.identity)) {
                                    address_collisions.push(m.identity.address);
                                }
                            }
                        }
                    }

                    for (_, rs) in roots.sets.iter() {
                        for m in rs.members.iter() {
                            if m.endpoints.is_some() && !address_collisions.contains(&m.identity.address) && !m.identity.eq(&self.identity) {
                                debug_event!(si, "[vl1] examining root {} with {} endpoints", m.identity.address.to_string(), m.endpoints.as_ref().map_or(0, |e| e.len()));
                                let peers = self.peers.upgradable_read();
                                if let Some(peer) = peers.get(&m.identity.address) {
                                    new_roots.insert(peer.clone(), m.endpoints.as_ref().unwrap().iter().cloned().collect());
                                } else {
                                    if let Some(peer) = Peer::<SI>::new(&self.identity, m.identity.clone(), si.time_clock(), tt) {
                                        new_roots.insert(parking_lot::RwLockUpgradableReadGuard::upgrade(peers).entry(m.identity.address).or_insert_with(|| Arc::new(peer)).clone(), m.endpoints.as_ref().unwrap().iter().cloned().collect());
                                    } else {
                                        bad_identities.push(m.identity.clone());
                                    }
                                }
                            }
                        }
                    }

                    (old_root_identities, address_collisions, new_roots, bad_identities, my_root_sets)
                };

                for c in address_collisions.iter() {
                    si.event(Event::SecurityWarning(format!("address/identity collision in root sets! address {} collides across root sets or with an existing peer and is being ignored as a root!", c.to_string())));
                }
                for i in bad_identities.iter() {
                    si.event(Event::SecurityWarning(format!("bad identity detected for address {} in at least one root set, ignoring (error creating peer object)", i.address.to_string())));
                }

                let mut new_root_identities: Vec<Identity> = new_roots.iter().map(|(p, _)| p.identity.clone()).collect();
                old_root_identities.sort_unstable();
                new_root_identities.sort_unstable();

                if !old_root_identities.eq(&new_root_identities) {
                    let mut care_of = CareOf::new(si.time_clock());
                    for id in new_root_identities.iter() {
                        care_of.add_care_of(id);
                    }
                    assert!(care_of.sign(&self.identity));
                    let care_of = care_of.to_bytes();

                    {
                        let mut roots = self.roots.write();
                        roots.roots = new_roots;
                        roots.care_of = care_of;
                        roots.my_root_sets = my_root_sets;
                    }

                    si.event(Event::UpdatedRoots(old_root_identities, new_root_identities));
                }
            }

            self.update_best_root(si, tt);
        }

        // Say HELLO to all roots periodically. For roots we send HELLO to every single endpoint
        // they have, which is a behavior that differs from normal peers. This allows roots to
        // e.g. see our IPv4 and our IPv6 address which can be important for us to learn our
        // external addresses from them.
        if root_hello || root_spam_hello {
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

        if peer_service {
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

        if path_service {
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
            let ka = [tt as u8]; // send different bytes every time for keepalive in case some things filter zero packets
            let ka2 = [&ka[..1]];
            for ka in need_keepalive.iter() {
                si.wire_send(&ka.endpoint, Some(&ka.local_socket), Some(&ka.local_interface), &ka2, 0).await;
            }
        }

        if whois_service {
            self.whois.service(si, self, tt);
        }

        debug_event!(si, "[vl1] do_background_tasks DONE ----");
        Duration::from_millis(1000)
    }

    pub async fn handle_incoming_physical_packet<PH: InnerProtocolInterface>(&self, si: &SI, ph: &PH, source_endpoint: &Endpoint, source_local_socket: &SI::LocalSocket, source_local_interface: &SI::LocalInterface, mut data: PooledPacketBuffer) {
        debug_event!(
            si,
            "[vl1] {} -> #{} {}->{} length {} (on socket {}@{})",
            source_endpoint.to_string(),
            data.bytes_fixed_at::<8>(0).map_or("????????????????".into(), |pid| zerotier_core_crypto::hex::to_string(pid)),
            data.bytes_fixed_at::<5>(13).map_or("??????????".into(), |src| zerotier_core_crypto::hex::to_string(src)),
            data.bytes_fixed_at::<5>(8).map_or("??????????".into(), |dest| zerotier_core_crypto::hex::to_string(dest)),
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
                        #[cfg(debug_assertions)]
                        let fragment_header_id = u64::from_be_bytes(fragment_header.id);
                        debug_event!(si, "[vl1] #{:0>16x} fragment {} of {} received", u64::from_be_bytes(fragment_header.id), fragment_header.fragment_no(), fragment_header.total_fragments());

                        if let Some(assembled_packet) = path.receive_fragment(fragment_header.packet_id(), fragment_header.fragment_no(), fragment_header.total_fragments(), data, time_ticks) {
                            if let Some(frag0) = assembled_packet.frags[0].as_ref() {
                                #[cfg(debug_assertions)]
                                debug_event!(si, "[vl1] #{:0>16x} packet fully assembled!", fragment_header_id);

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
                        #[cfg(debug_assertions)]
                        if let Ok(packet_header) = data.struct_at::<PacketHeader>(0) {
                            debug_event!(si, "[vl1] #{:0>16x} is unfragmented", u64::from_be_bytes(packet_header.id));

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
                    #[cfg(debug_assertions)]
                    let debug_packet_id;

                    if fragment_header.is_fragment() {
                        #[cfg(debug_assertions)]
                        {
                            debug_packet_id = u64::from_be_bytes(fragment_header.id);
                            debug_event!(si, "[vl1] #{:0>16x} forwarding packet fragment to {}", debug_packet_id, dest.to_string());
                        }
                        if fragment_header.increment_hops() > FORWARD_MAX_HOPS {
                            #[cfg(debug_assertions)]
                            debug_event!(si, "[vl1] #{:0>16x} discarded: max hops exceeded!", debug_packet_id);
                            return;
                        }
                    } else {
                        if let Ok(packet_header) = data.struct_mut_at::<PacketHeader>(0) {
                            #[cfg(debug_assertions)]
                            {
                                debug_packet_id = u64::from_be_bytes(packet_header.id);
                                debug_event!(si, "[vl1] #{:0>16x} forwarding packet to {}", debug_packet_id, dest.to_string());
                            }
                            if packet_header.increment_hops() > FORWARD_MAX_HOPS {
                                #[cfg(debug_assertions)]
                                debug_event!(si, "[vl1] #{:0>16x} discarded: max hops exceeded!", u64::from_be_bytes(packet_header.id));
                                return;
                            }
                        } else {
                            return;
                        }
                    }

                    if let Some(peer) = self.peer(dest) {
                        // TODO: SHOULD we forward? Need a way to check.
                        peer.forward(si, time_ticks, data.as_ref()).await;
                        #[cfg(debug_assertions)]
                        debug_event!(si, "[vl1] #{:0>16x} forwarded successfully", debug_packet_id);
                    }
                }
            }
        }
    }

    pub fn best_root(&self) -> Option<Arc<Peer<SI>>> {
        self.best_root.read().clone()
    }

    pub fn is_peer_root(&self, peer: &Peer<SI>) -> bool {
        self.roots.read().roots.keys().any(|p| (**p).eq(peer))
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

    pub(crate) fn my_root_sets(&self) -> Option<Vec<u8>> {
        self.roots.read().my_root_sets.clone()
    }

    #[allow(unused)]
    pub(crate) fn this_node_is_root(&self) -> bool {
        self.roots.read().my_root_sets.is_some()
    }

    pub(crate) fn care_of_bytes(&self) -> Vec<u8> {
        self.roots.read().care_of.clone()
    }

    pub(crate) fn canonical_path(&self, ep: &Endpoint, local_socket: &SI::LocalSocket, local_interface: &SI::LocalInterface, time_ticks: i64) -> Arc<Path<SI>> {
        if let Some(path) = self.paths.read().get(&PathKey::Ref(ep, local_socket)) {
            return path.clone();
        }
        return self.paths.write().entry(PathKey::Copied(ep.clone(), local_socket.clone())).or_insert_with(|| Arc::new(Path::new(ep.clone(), local_socket.clone(), local_interface.clone(), time_ticks))).clone();
    }
}
