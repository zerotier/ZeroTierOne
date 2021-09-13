use std::str::FromStr;
use std::sync::Arc;
use std::time::Duration;

use dashmap::DashMap;
use parking_lot::Mutex;

use crate::crypto::random::{SecureRandom, next_u64_secure};
use crate::error::InvalidParameterError;
use crate::util::gate::IntervalGate;
use crate::util::pool::{Pool, Pooled};
use crate::vl1::{Address, Endpoint, Identity, Locator};
use crate::vl1::buffer::{Buffer, PooledBufferFactory};
use crate::vl1::path::Path;
use crate::vl1::peer::Peer;
use crate::vl1::protocol::*;
use crate::vl1::whois::{WhoisQueue, QueuedPacket};
use crate::vl1::rootset::RootSet;

/// Standard packet buffer type including pool container.
pub type PacketBuffer = Pooled<Buffer<{ PACKET_SIZE_MAX }>, PooledBufferFactory<{ PACKET_SIZE_MAX }>>;

/// Callback interface and call context for calls to the node (for VL1).
///
/// Every non-trivial call takes a reference to this, which it passes all the way through
/// the call stack. This can be used to call back into the caller to send packets, get or
/// store data, report events, etc.
pub trait VL1CallerInterface {
    /// Node is up and ready for operation.
    fn event_node_is_up(&self);

    /// Node is shutting down.
    fn event_node_is_down(&self);

    /// A root signaled an identity collision.
    /// This should cause the external code to shut down this node, delete its identity, and recreate.
    fn event_identity_collision(&self);

    /// Node has gone online or offline.
    fn event_online_status_change(&self, online: bool);

    /// A USER_MESSAGE packet was received.
    fn event_user_message(&self, source: &Identity, message_type: u64, message: &[u8]);

    /// Load this node's identity from the data store.
    fn load_node_identity(&self) -> Option<&[u8]>;

    /// Save this node's identity.
    /// Note that this is only called on first startup (after up) and after identity_changed.
    fn save_node_identity(&self, id: &Identity, public: &[u8], secret: &[u8]);

    /// Load this node's latest locator.
    fn load_locator(&self) -> Option<&[u8]>;

    /// Save this node's latest locator.
    fn save_locator(&self, locator: &[u8]);

    /// Called to send a packet over the physical network (virtual -> physical).
    ///
    /// This may return false if the send definitely failed, and may return true if the send
    /// succeeded or may have succeeded (in the case of UDP and similar).
    ///
    /// If local socket and/or local interface are None, the sending code should make its
    /// own decision about what local socket or interface to use. It may send on a random
    /// one, the best fit, or all at once.
    ///
    /// If packet TTL is non-zero it should be used to set the packet TTL for outgoing packets
    /// for supported protocols such as UDP, but otherwise it can be ignored. It can also be
    /// ignored if the platform does not support setting the TTL.
    fn wire_send(&self, endpoint: &Endpoint, local_socket: Option<i64>, local_interface: Option<i64>, data: &[&[u8]], packet_ttl: u8) -> bool;

    /// Called to check and see if a physical address should be used for ZeroTier traffic to a node.
    fn check_path(&self, id: &Identity, endpoint: &Endpoint, local_socket: Option<i64>, local_interface: Option<i64>) -> bool;

    /// Called to look up a path to a known node.
    ///
    /// If a path is found, this returns a tuple of an endpoint and optional local socket and local
    /// interface IDs. If these are None they will be None when this is sent with wire_send.
    fn get_path_hints(&self, id: &Identity) -> Option<&[(&Endpoint, Option<i64>, Option<i64>)]>;

    /// Called to get the current time in milliseconds from the system monotonically increasing clock.
    fn time_ticks(&self) -> i64;

    /// Called to get the current time in milliseconds since epoch from the real-time clock.
    fn time_clock(&self) -> i64;
}

/// Trait implemented by VL2 to handle messages after they are unwrapped by VL1.
///
/// This normally isn't used from outside this crate except for testing or if you want to harness VL1
/// for some entirely unrelated purpose.
pub trait VL1PacketHandler {
    /// Handle a packet, returning true if the verb was recognized.
    ///
    /// True should be returned even if the packet is not valid, since the return value is used
    /// to determine if this is a VL2 or VL1 packet. ERROR and OK should not be handled here but
    /// in handle_error() and handle_ok() instead.
    fn handle_packet(&self, peer: &Peer, source_path: &Arc<Path>, forward_secrecy: bool, extended_authentication: bool, verb: u8, payload: &Buffer<{ PACKET_SIZE_MAX }>) -> bool;

    /// Handle errors, returning true if the error was recognized.
    fn handle_error(&self, peer: &Peer, source_path: &Arc<Path>, forward_secrecy: bool, extended_authentication: bool, in_re_verb: u8, in_re_packet_id: PacketID, error_code: u8, payload: &Buffer<{ PACKET_SIZE_MAX }>, cursor: &mut usize) -> bool;

    /// Handle an OK, returing true if the OK was recognized.
    fn handle_ok(&self, peer: &Peer, source_path: &Arc<Path>, forward_secrecy: bool, extended_authentication: bool, in_re_verb: u8, in_re_packet_id: PacketID, payload: &Buffer<{ PACKET_SIZE_MAX }>, cursor: &mut usize) -> bool;
}

#[derive(Default)]
struct BackgroundTaskIntervals {
    whois: IntervalGate<{ WhoisQueue::INTERVAL }>,
    paths: IntervalGate<{ Path::INTERVAL }>,
    peers: IntervalGate<{ Peer::INTERVAL }>,
}

pub struct Node {
    pub(crate) instance_id: u64,
    identity: Identity,
    intervals: Mutex<BackgroundTaskIntervals>,
    locator: Mutex<Option<Arc<Locator>>>,
    paths: DashMap<Endpoint, Arc<Path>>,
    peers: DashMap<Address, Arc<Peer>>,
    roots: Mutex<Vec<Arc<Peer>>>,
    root_sets: Mutex<Vec<RootSet>>,
    whois: WhoisQueue,
    buffer_pool: Pool<Buffer<{ PACKET_SIZE_MAX }>, PooledBufferFactory<{ PACKET_SIZE_MAX }>>,
    secure_prng: SecureRandom,
    fips_mode: bool,
}

impl Node {
    /// Create a new Node.
    ///
    /// If the auto-generate identity type is not None, a new identity will be generated if
    /// no identity is currently stored in the data store.
    pub fn new<CI: VL1CallerInterface>(ci: &CI, auto_generate_identity_type: Option<crate::vl1::identity::Type>) -> Result<Self, InvalidParameterError> {
        let id = {
            let id_str = ci.load_node_identity();
            if id_str.is_none() {
                if auto_generate_identity_type.is_none() {
                    return Err(InvalidParameterError("no identity found and auto-generate not enabled"));
                } else {
                    let id = Identity::generate(auto_generate_identity_type.unwrap());
                    ci.save_node_identity(&id, id.to_string().as_bytes(), id.to_secret_string().as_bytes());
                    id
                }
            } else {
                let id_str = String::from_utf8_lossy(id_str.unwrap());
                let id = Identity::from_str(id_str.as_ref());
                if id.is_err() {
                    return Err(InvalidParameterError("invalid identity"));
                } else {
                    id.unwrap()
                }
            }
        };

        Ok(Self {
            instance_id: next_u64_secure(),
            identity: id,
            intervals: Mutex::new(BackgroundTaskIntervals::default()),
            locator: Mutex::new(None),
            paths: DashMap::new(),
            peers: DashMap::new(),
            roots: Mutex::new(Vec::new()),
            root_sets: Mutex::new(Vec::new()),
            whois: WhoisQueue::new(),
            buffer_pool: Pool::new(64, PooledBufferFactory),
            secure_prng: SecureRandom::get(),
            fips_mode: false,
        })
    }

    #[inline(always)]
    pub fn address(&self) -> Address { self.identity.address() }

    #[inline(always)]
    pub fn identity(&self) -> &Identity { &self.identity }

    #[inline(always)]
    pub fn locator(&self) -> Option<Arc<Locator>> { self.locator.lock().clone() }

    /// Get a reusable packet buffer.
    /// The buffer will automatically be returned to the pool if it is dropped.
    pub fn get_packet_buffer(&self) -> PacketBuffer { self.buffer_pool.get() }

    /// Get a peer by address.
    pub fn peer(&self, a: Address) -> Option<Arc<Peer>> { self.peers.get(&a).map(|peer| peer.value().clone()) }

    #[inline(always)]
    pub fn fips_mode(&self) -> bool { self.fips_mode }

    /// Get all peers currently in the peer cache.
    pub fn peers(&self) -> Vec<Arc<Peer>> {
        let mut v: Vec<Arc<Peer>> = Vec::new();
        v.reserve(self.peers.len());
        for p in self.peers.iter() {
            v.push(p.value().clone());
        }
        v
    }

    /// Run background tasks and return desired delay until next call in milliseconds.
    ///
    /// This should only be called once at a time. It technically won't hurt anything to
    /// call concurrently but it will waste CPU cycles.
    pub fn do_background_tasks<CI: VL1CallerInterface>(&self, ci: &CI) -> Duration {
        let mut intervals = self.intervals.lock();
        let tt = ci.time_ticks();

        if intervals.whois.gate(tt) {
            self.whois.on_interval(self, ci, tt);
        }

        if intervals.paths.gate(tt) {
            self.paths.retain(|_, path| {
                path.on_interval(ci, tt);
                todo!();
                true
            });
        }

        if intervals.peers.gate(tt) {
            self.peers.retain(|_, peer| {
                peer.on_interval(ci, tt);
                todo!();
                true
            });
        }

        Duration::from_millis(1000)
    }

    /// Called when a packet is received on the physical wire.
    pub fn wire_receive<CI: VL1CallerInterface, PH: VL1PacketHandler>(&self, ci: &CI, ph: &PH, source_endpoint: &Endpoint, source_local_socket: i64, source_local_interface: i64, mut data: PacketBuffer) {
        let fragment_header = data.struct_mut_at::<FragmentHeader>(0);
        if fragment_header.is_ok() {
            let fragment_header = fragment_header.unwrap();
            let time_ticks = ci.time_ticks();
            let dest = Address::from_bytes(&fragment_header.dest);
            if dest.is_some() {
                let dest = dest.unwrap();
                if dest == self.identity.address() {

                    let path = self.path(source_endpoint, source_local_socket, source_local_interface);
                    path.log_receive(time_ticks);
                    if fragment_header.is_fragment() {

                        let _ = path.receive_fragment(fragment_header.id, fragment_header.fragment_no(), fragment_header.total_fragments(), data, time_ticks).map(|assembled_packet| {
                            if assembled_packet.frags[0].is_some() {
                                let frag0 = assembled_packet.frags[0].as_ref().unwrap();
                                let packet_header = frag0.struct_at::<PacketHeader>(0);
                                if packet_header.is_ok() {
                                    let packet_header = packet_header.unwrap();
                                    let source = Address::from_bytes(&packet_header.src);
                                    if source.is_some() {
                                        let source = source.unwrap();
                                        let peer = self.peer(source);
                                        if peer.is_some() {
                                            peer.unwrap().receive(self, ci, ph, time_ticks, &path, &packet_header, frag0, &assembled_packet.frags[1..(assembled_packet.have as usize)]);
                                        } else {
                                            self.whois.query(self, ci, source, Some(QueuedPacket::Fragmented(assembled_packet)));
                                        }
                                    }
                                }
                            }
                        });

                    } else {

                        let packet_header = data.struct_at::<PacketHeader>(0);
                        if packet_header.is_ok() {
                            let packet_header = packet_header.unwrap();
                            let source = Address::from_bytes(&packet_header.src);
                            if source.is_some() {
                                let source = source.unwrap();
                                let peer = self.peer(source);
                                if peer.is_some() {
                                    peer.unwrap().receive(self, ci, ph, time_ticks, &path, &packet_header, data.as_ref(), &[]);
                                } else {
                                    self.whois.query(self, ci, source, Some(QueuedPacket::Unfragmented(data)));
                                }
                            }
                        }

                    }

                } else {

                    if fragment_header.is_fragment() {
                        if fragment_header.increment_hops() > FORWARD_MAX_HOPS {
                            return;
                        }
                    } else {
                        let packet_header = data.struct_mut_at::<PacketHeader>(0);
                        if packet_header.is_ok() {
                            if packet_header.unwrap().increment_hops() > FORWARD_MAX_HOPS {
                                return;
                            }
                        } else {
                            return;
                        }
                    }
                    let _ = self.peer(dest).map(|peer| peer.forward(ci, time_ticks, data.as_ref()));

                }
            };
        }
    }

    /// Get the current best root peer that we should use for WHOIS, relaying, etc.
    pub(crate) fn root(&self) -> Option<Arc<Peer>> { self.roots.lock().first().map(|p| p.clone()) }

    /// Return true if a peer is a root.
    pub(crate) fn is_peer_root(&self, peer: &Peer) -> bool { self.roots.lock().iter().any(|p| Arc::as_ptr(p) == (peer as *const Peer)) }

    /// Get the canonical Path object for a given endpoint and local socket information.
    ///
    /// This is a canonicalizing function that returns a unique path object for every tuple
    /// of endpoint, local socket, and local interface.
    pub(crate) fn path(&self, ep: &Endpoint, local_socket: i64, local_interface: i64) -> Arc<Path> {
        self.paths.get(ep).map_or_else(|| {
            let p = Arc::new(Path::new(ep.clone(), local_socket, local_interface));
            self.paths.insert(ep.clone(), p.clone()).unwrap_or(p) // if another thread added one, return that instead
        }, |path| path.value().clone())
    }
}

unsafe impl Send for Node {}

unsafe impl Sync for Node {}
