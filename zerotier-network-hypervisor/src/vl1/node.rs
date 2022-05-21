// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::collections::HashMap;
use std::hash::Hash;
use std::num::NonZeroI64;
use std::sync::atomic::Ordering;
use std::sync::Arc;
use std::time::Duration;

use dashmap::DashMap;
use parking_lot::{Mutex, RwLock};

use crate::error::InvalidParameterError;
use crate::util::gate::IntervalGate;
use crate::vl1::path::Path;
use crate::vl1::peer::Peer;
use crate::vl1::protocol::*;
use crate::vl1::whoisqueue::{QueuedPacket, WhoisQueue};
use crate::vl1::{Address, Endpoint, Identity, RootSet};

/// Trait implemented by external code to handle events and provide an interface to the system or application.
///
/// These methods are basically callbacks that the core calls to request or transmit things. They are called
/// during calls to things like wire_recieve() and do_background_tasks().
pub trait SystemInterface: Sync + Send + 'static {
    /// Type for local system sockets.
    type LocalSocket: Sync + Send + Sized + Hash + PartialEq + Eq + Clone;

    /// Type for local system interfaces.    
    type LocalInterface: Sync + Send + Sized + Hash + PartialEq + Eq + Clone;

    /// Node is up and ready for operation.
    fn event_node_is_up(&self);

    /// Node is shutting down.
    fn event_node_is_down(&self);

    /// Node has gone online or offline.
    fn event_online_status_change(&self, online: bool);

    /// A USER_MESSAGE packet was received.
    fn event_user_message(&self, source: &Identity, message_type: u64, message: &[u8]);

    /// VL1 core generated a security warning.
    fn event_security_warning(&self, warning: &str);

    /// Check a local socket for validity.
    ///
    /// This could return false if the socket's interface no longer exists, its port has been
    /// unbound, etc.
    fn local_socket_is_valid(&self, socket: &Self::LocalSocket) -> bool;

    /// Load this node's identity from the data store.
    fn load_node_identity(&self) -> Option<Identity>;

    /// Save this node's identity.
    fn save_node_identity(&self, id: &Identity);

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
    fn wire_send(&self, endpoint: &Endpoint, local_socket: Option<&Self::LocalSocket>, local_interface: Option<&Self::LocalInterface>, data: &[&[u8]], packet_ttl: u8) -> bool;

    /// Called to check and see if a physical address should be used for ZeroTier traffic to a node.
    fn check_path(&self, id: &Identity, endpoint: &Endpoint, local_socket: Option<&Self::LocalSocket>, local_interface: Option<&Self::LocalInterface>) -> bool;

    /// Called to look up any statically defined or memorized paths to known nodes.
    fn get_path_hints(&self, id: &Identity) -> Option<Vec<(Endpoint, Option<NonZeroI64>, Option<NonZeroI64>)>>;

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
pub trait InnerProtocolInterface: Sync + Send + 'static {
    /// Handle a packet, returning true if it was handled by the next layer.
    ///
    /// Do not attempt to handle OK or ERROR. Instead implement handle_ok() and handle_error().
    /// The return values of these must follow the same semantic of returning true if the message
    /// was handled.
    fn handle_packet<SI: SystemInterface>(&self, peer: &Peer<SI>, source_path: &Path<SI>, forward_secrecy: bool, extended_authentication: bool, verb: u8, payload: &PacketBuffer) -> bool;

    /// Handle errors, returning true if the error was recognized.
    fn handle_error<SI: SystemInterface>(&self, peer: &Peer<SI>, source_path: &Path<SI>, forward_secrecy: bool, extended_authentication: bool, in_re_verb: u8, in_re_message_id: u64, error_code: u8, payload: &PacketBuffer, cursor: &mut usize) -> bool;

    /// Handle an OK, returing true if the OK was recognized.
    fn handle_ok<SI: SystemInterface>(&self, peer: &Peer<SI>, source_path: &Path<SI>, forward_secrecy: bool, extended_authentication: bool, in_re_verb: u8, in_re_message_id: u64, payload: &PacketBuffer, cursor: &mut usize) -> bool;

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
    whois: IntervalGate<{ crate::vl1::whoisqueue::SERVICE_INTERVAL_MS }>,
    paths: IntervalGate<{ crate::vl1::path::SERVICE_INTERVAL_MS }>,
    peers: IntervalGate<{ crate::vl1::peer::SERVICE_INTERVAL_MS }>,
    root_sync: IntervalGate<ROOT_SYNC_INTERVAL_MS>,
    root_hello: IntervalGate<ROOT_HELLO_INTERVAL>,
}

struct RootInfo<SI: SystemInterface> {
    roots: HashMap<Arc<Peer<SI>>, Vec<Endpoint>>,
    sets: HashMap<String, RootSet>,
    sets_modified: bool,
}

/// A VL1 global P2P network node.
pub struct Node<SI: SystemInterface> {
    /// A random ID generated to identify this particular running instance.
    pub instance_id: u64,

    /// This node's identity and permanent keys.
    pub identity: Identity,

    /// Interval latches for periodic background tasks.
    intervals: Mutex<BackgroundTaskIntervals>,

    /// Canonicalized network paths, held as Weak<> to be automatically cleaned when no longer in use.
    paths: DashMap<Endpoint, parking_lot::RwLock<HashMap<SI::LocalSocket, Arc<Path<SI>>>>>,

    /// Peers with which we are currently communicating.
    peers: DashMap<Address, Arc<Peer<SI>>>,

    /// This node's trusted roots, sorted in ascending order of quality/preference, and cluster definitions.
    roots: Mutex<RootInfo<SI>>,

    /// Current best root.
    best_root: RwLock<Option<Arc<Peer<SI>>>>,

    /// Identity lookup queue, also holds packets waiting on a lookup.
    whois: WhoisQueue,

    /// Reusable network buffer pool.
    buffer_pool: PacketBufferPool,
}

impl<SI: SystemInterface> Node<SI> {
    /// Create a new Node.
    pub fn new(si: &SI, auto_generate_identity: bool) -> Result<Self, InvalidParameterError> {
        let mut id = {
            let id = si.load_node_identity();
            if id.is_none() {
                if !auto_generate_identity {
                    return Err(InvalidParameterError("no identity found and auto-generate not enabled"));
                } else {
                    let id = Identity::generate();
                    si.save_node_identity(&id);
                    id
                }
            } else {
                id.unwrap()
            }
        };

        // Automatically upgrade old type identities to add P-384 keys.
        if id.upgrade()? {
            si.save_node_identity(&id);
        }

        Ok(Self {
            instance_id: zerotier_core_crypto::random::next_u64_secure(),
            identity: id,
            intervals: Mutex::new(BackgroundTaskIntervals::default()),
            paths: DashMap::new(),
            peers: DashMap::new(),
            roots: Mutex::new(RootInfo {
                roots: HashMap::new(),
                sets: HashMap::new(),
                sets_modified: false,
            }),
            best_root: RwLock::new(None),
            whois: WhoisQueue::new(),
            buffer_pool: PacketBufferPool::new(64, PacketBufferFactory::new()),
        })
    }

    /// Get a packet buffer that will automatically check itself back into the pool on drop.
    #[inline(always)]
    pub fn get_packet_buffer(&self) -> PooledPacketBuffer {
        self.buffer_pool.get()
    }

    /// Get a peer by address.
    #[inline(always)]
    pub fn peer(&self, a: Address) -> Option<Arc<Peer<SI>>> {
        self.peers.get(&a).map(|peer| peer.value().clone())
    }

    /// Run background tasks and return desired delay until next call in milliseconds.
    ///
    /// This should only be called periodically from a single thread, but that thread can be
    /// different each time. Calling it concurrently won't crash but won't accomplish anything.
    pub fn do_background_tasks(&self, si: &SI) -> Duration {
        let mut intervals = self.intervals.lock();
        let tt = si.time_ticks();

        if intervals.root_sync.gate(tt) {
            match &mut (*self.roots.lock()) {
                RootInfo { roots, sets, sets_modified } => {
                    // Sychronize root info with root sets info if the latter has changed.
                    if *sets_modified {
                        *sets_modified = false;
                        roots.clear();
                        let mut colliding_root_addresses = Vec::new(); // see security note below
                        for (_, rc) in sets.iter() {
                            for m in rc.members.iter() {
                                if m.endpoints.is_some() && !colliding_root_addresses.contains(&m.identity.address) {
                                    /*
                                     * SECURITY NOTE: it should be impossible to get an address/identity collision here unless
                                     * the user adds a maliciously crafted root set with an identity that collides another. Under
                                     * normal circumstances the root backplane combined with the address PoW should rule this
                                     * out. However since we trust roots as identity lookup authorities it's important to take
                                     * extra care to check for this case. If it's detected, all roots with the offending
                                     * address are ignored/disabled.
                                     *
                                     * The apparently over-thought functional chain here on peers.entry() is to make access to
                                     * the peer map atomic since we use a "lock-free" data structure here (DashMap).
                                     */

                                    let _ = self
                                        .peers
                                        .entry(m.identity.address)
                                        .or_try_insert_with(|| Peer::<SI>::new(&self.identity, m.identity.clone(), si.time_clock()).map_or(Err(crate::error::UnexpectedError), |new_root| Ok(Arc::new(new_root))))
                                        .and_then(|root_peer_entry| {
                                            let rp = root_peer_entry.value();
                                            if rp.identity.eq(&m.identity) {
                                                Ok(root_peer_entry)
                                            } else {
                                                colliding_root_addresses.push(m.identity.address);
                                                si.event_security_warning(
                                                    format!("address/identity collision between root {} (from root cluster definition '{}') and known peer {}", m.identity.address.to_string(), rc.name, rp.identity.to_string()).as_str(),
                                                );
                                                Err(crate::error::UnexpectedError)
                                            }
                                        })
                                        .map(|r| roots.insert(r.value().clone(), m.endpoints.as_ref().unwrap().iter().map(|e| e.clone()).collect()));
                                }
                            }
                        }
                    }

                    // Say HELLO to all roots periodically. For roots we send HELLO to every single endpoint
                    // they have, which is a behavior that differs from normal peers. This allows roots to
                    // e.g. see our IPv4 and our IPv6 address which can be important for us to learn our
                    // external addresses from them.
                    assert!(ROOT_SYNC_INTERVAL_MS <= (ROOT_HELLO_INTERVAL / 2));
                    if intervals.root_hello.gate(tt) {
                        for (root, endpoints) in roots.iter() {
                            for ep in endpoints.iter() {
                                root.send_hello(si, self, Some(ep));
                            }
                        }
                    }

                    // The best root is the one that has replied to a HELLO most recently. Since we send HELLOs in unison
                    // this is a proxy for latency and also causes roots that fail to reply to drop out quickly.
                    let mut latest_hello_reply = 0;
                    let mut best: Option<&Arc<Peer<SI>>> = None;
                    for (r, _) in roots.iter() {
                        let t = r.last_hello_reply_time_ticks.load(Ordering::Relaxed);
                        if t >= latest_hello_reply {
                            latest_hello_reply = t;
                            let _ = best.insert(r);
                        }
                    }
                    *(self.best_root.write()) = best.cloned();
                }
            }
        }

        if intervals.peers.gate(tt) {
            // Service all peers, removing any whose service() method returns false AND that are not
            // roots. Roots on the other hand remain in the peer list as long as they are roots.
            self.peers.retain(|_, peer| if peer.service(si, self, tt) { true } else { !self.roots.lock().roots.contains_key(peer) });
        }

        if intervals.paths.gate(tt) {
            // Service all paths, removing expired or invalid ones.
            self.paths.retain(|_, pbs| {
                let mut expired_paths = Vec::new();
                for (ls, path) in pbs.read().iter() {
                    if !si.local_socket_is_valid(ls) || !path.service(si, self, tt) {
                        expired_paths.push(Arc::as_ptr(path));
                    }
                }
                if expired_paths.is_empty() {
                    true
                } else {
                    let mut pbs_w = pbs.write();
                    pbs_w.retain(|_, path| !expired_paths.contains(&Arc::as_ptr(path)));
                    !pbs_w.is_empty()
                }
            })
        }

        if intervals.whois.gate(tt) {
            let _ = self.whois.service(si, self, tt);
        }

        Duration::from_millis((ROOT_SYNC_INTERVAL_MS.min(crate::vl1::whoisqueue::SERVICE_INTERVAL_MS).min(crate::vl1::path::SERVICE_INTERVAL_MS).min(crate::vl1::peer::SERVICE_INTERVAL_MS) as u64) / 2)
    }

    /// Called when a packet is received on the physical wire.
    pub fn wire_receive<PH: InnerProtocolInterface>(&self, si: &SI, ph: &PH, source_endpoint: &Endpoint, source_local_socket: &SI::LocalSocket, source_local_interface: &SI::LocalInterface, mut data: PooledPacketBuffer) {
        if let Ok(fragment_header) = data.struct_mut_at::<FragmentHeader>(0) {
            if let Some(dest) = Address::from_bytes_fixed(&fragment_header.dest) {
                let time_ticks = si.time_ticks();
                if dest == self.identity.address {
                    // Handle packets addressed to this node.

                    let path = self.canonical_path(source_endpoint, source_local_socket, source_local_interface, time_ticks);
                    path.log_receive_anything(time_ticks);

                    if fragment_header.is_fragment() {
                        if let Some(assembled_packet) = path.receive_fragment(u64::from_ne_bytes(fragment_header.id), fragment_header.fragment_no(), fragment_header.total_fragments(), data, time_ticks) {
                            if let Some(frag0) = assembled_packet.frags[0].as_ref() {
                                // Fragmented packet is fully assembled.

                                let packet_header = frag0.struct_at::<PacketHeader>(0);
                                if packet_header.is_ok() {
                                    let packet_header = packet_header.unwrap();
                                    if let Some(source) = Address::from_bytes(&packet_header.src) {
                                        if let Some(peer) = self.peer(source) {
                                            peer.receive(self, si, ph, time_ticks, &path, &packet_header, frag0, &assembled_packet.frags[1..(assembled_packet.have as usize)]);
                                        } else {
                                            self.whois.query(self, si, source, Some(QueuedPacket::Fragmented(assembled_packet)));
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        if let Ok(packet_header) = data.struct_at::<PacketHeader>(0) {
                            // Packet is not fragmented.

                            if let Some(source) = Address::from_bytes(&packet_header.src) {
                                if let Some(peer) = self.peer(source) {
                                    peer.receive(self, si, ph, time_ticks, &path, &packet_header, data.as_ref(), &[]);
                                } else {
                                    self.whois.query(self, si, source, Some(QueuedPacket::Unfragmented(data)));
                                }
                            }
                        }
                    }
                } else {
                    // Forward packets not destined for this node.
                    // TODO: SHOULD we forward? Need a way to check.

                    if fragment_header.is_fragment() {
                        if fragment_header.increment_hops() > FORWARD_MAX_HOPS {
                            return;
                        }
                    } else {
                        if let Ok(packet_header) = data.struct_mut_at::<PacketHeader>(0) {
                            if packet_header.increment_hops() > FORWARD_MAX_HOPS {
                                return;
                            }
                        } else {
                            return;
                        }
                    }

                    if let Some(peer) = self.peer(dest) {
                        peer.forward(si, time_ticks, data.as_ref());
                    }
                }
            };
        }
    }

    /// Get the current best root peer that we should use for WHOIS, relaying, etc.
    #[inline(always)]
    pub fn root(&self) -> Option<Arc<Peer<SI>>> {
        self.best_root.read().clone()
    }

    /// Return true if a peer is a root.
    pub fn is_peer_root(&self, peer: &Peer<SI>) -> bool {
        self.roots.lock().roots.contains_key(peer)
    }

    /// Add or update a root set.
    ///
    /// If no root set exists by this name, a new root set is added. If one already
    /// exists it's checked against the new one and updated if the new set is valid
    /// and should supersede it.
    ///
    /// Changes will take effect within a few seconds when root sets are next
    /// examined and synchronized with peer and root list state.
    ///
    /// This returns true if the new root set was accepted and false otherwise.
    pub fn add_update_root_set(&self, rs: RootSet) -> bool {
        let mut roots = self.roots.lock();
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

    /// Get the canonical Path object for a given endpoint and local socket information.
    ///
    /// This is a canonicalizing function that returns a unique path object for every tuple
    /// of endpoint, local socket, and local interface.
    pub fn canonical_path(&self, ep: &Endpoint, local_socket: &SI::LocalSocket, local_interface: &SI::LocalInterface, time_ticks: i64) -> Arc<Path<SI>> {
        // It's faster to do a read only lookup first since most of the time this will succeed. The second
        // version below this only gets invoked if it's a new path.
        if let Some(path) = self.paths.get(ep) {
            if let Some(path) = path.value().read().get(local_socket) {
                return path.clone();
            }
        }

        return self
            .paths
            .entry(ep.clone())
            .or_insert_with(|| parking_lot::RwLock::new(HashMap::with_capacity(2)))
            .value_mut()
            .write()
            .entry(local_socket.clone())
            .or_insert_with(|| Arc::new(Path::new(ep.clone(), local_socket.clone(), local_interface.clone(), time_ticks)))
            .clone();
    }
}
