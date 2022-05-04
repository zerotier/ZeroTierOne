/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::num::NonZeroI64;
use std::str::FromStr;
use std::sync::{Arc, Weak};
use std::time::Duration;

use dashmap::DashMap;
use parking_lot::Mutex;

use crate::error::InvalidParameterError;
use crate::util::buffer::Buffer;
use crate::util::gate::IntervalGate;
use crate::vl1::path::Path;
use crate::vl1::peer::Peer;
use crate::vl1::protocol::*;
use crate::vl1::whoisqueue::{QueuedPacket, WhoisQueue};
use crate::vl1::{Address, Endpoint, Identity};
use crate::{PacketBuffer, PacketBufferFactory, PacketBufferPool};

/// Trait implemented by external code to handle events and provide an interface to the system or application.
///
/// These methods are basically callbacks that the core calls to request or transmit things. They are called
/// during calls to things like wire_recieve() and do_background_tasks().
pub trait SystemInterface: Sync + Send {
    /// Node is up and ready for operation.
    fn event_node_is_up(&self);

    /// Node is shutting down.
    fn event_node_is_down(&self);

    /// Node has gone online or offline.
    fn event_online_status_change(&self, online: bool);

    /// A USER_MESSAGE packet was received.
    fn event_user_message(&self, source: &Identity, message_type: u64, message: &[u8]);

    /// Load this node's identity from the data store.
    fn load_node_identity(&self) -> Option<Vec<u8>>;

    /// Save this node's identity.
    fn save_node_identity(&self, id: &Identity);

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
    fn wire_send(&self, endpoint: &Endpoint, local_socket: Option<NonZeroI64>, local_interface: Option<NonZeroI64>, data: &[&[u8]], packet_ttl: u8) -> bool;

    /// Called to check and see if a physical address should be used for ZeroTier traffic to a node.
    fn check_path(&self, id: &Identity, endpoint: &Endpoint, local_socket: Option<NonZeroI64>, local_interface: Option<NonZeroI64>) -> bool;

    /// Called to look up a path to a known node.
    ///
    /// If a path is found, this returns a tuple of an endpoint and optional local socket and local
    /// interface IDs. If these are None they will be None when this is sent with wire_send.
    fn get_path_hints(&self, id: &Identity) -> Option<&[(&Endpoint, Option<NonZeroI64>, Option<NonZeroI64>)]>;

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
pub trait InnerProtocolInterface: Sync + Send {
    /// Handle a packet, returning true if it was handled by the next layer.
    ///
    /// Do not attempt to handle OK or ERROR. Instead implement handle_ok() and handle_error().
    /// The return values of these must follow the same semantic of returning true if the message
    /// was handled.
    fn handle_packet(&self, peer: &Peer, source_path: &Arc<Path>, forward_secrecy: bool, extended_authentication: bool, verb: u8, payload: &Buffer<{ PACKET_SIZE_MAX }>) -> bool;

    /// Handle errors, returning true if the error was recognized.
    fn handle_error(&self, peer: &Peer, source_path: &Arc<Path>, forward_secrecy: bool, extended_authentication: bool, in_re_verb: u8, in_re_message_id: u64, error_code: u8, payload: &Buffer<{ PACKET_SIZE_MAX }>, cursor: &mut usize) -> bool;

    /// Handle an OK, returing true if the OK was recognized.
    fn handle_ok(&self, peer: &Peer, source_path: &Arc<Path>, forward_secrecy: bool, extended_authentication: bool, in_re_verb: u8, in_re_message_id: u64, payload: &Buffer<{ PACKET_SIZE_MAX }>, cursor: &mut usize) -> bool;

    /// Check if this remote peer has a trust relationship with this node.
    ///
    /// This is checked to determine if we should do things like make direct links ore respond to
    /// various other VL1 messages.
    fn has_trust_relationship(&self, id: &Identity) -> bool;
}

/// Trait for objects that are serviced in the background loop (the actual loop is external).
pub(crate) trait BackgroundServicable {
    /// How often in milliseconds to call service().
    const SERVICE_INTERVAL_MS: i64;

    /// Service object and return true if the object should be retained (if applicable).
    fn service<SI: SystemInterface>(&self, si: &SI, node: &Node, time_ticks: i64) -> bool;
}

#[derive(Default)]
struct BackgroundTaskIntervals {
    whois: IntervalGate<{ WhoisQueue::SERVICE_INTERVAL_MS }>,
    paths: IntervalGate<{ Path::SERVICE_INTERVAL_MS }>,
    peers: IntervalGate<{ Peer::SERVICE_INTERVAL_MS }>,
}

pub struct Node {
    /// A random ID generated to identify this particular running instance.
    pub instance_id: u64,

    /// This node's identity and permanent keys.
    pub identity: Identity,

    /// Interval latches for periodic background tasks.
    intervals: Mutex<BackgroundTaskIntervals>,

    /// Canonicalized network paths, held as Weak<> to be automatically cleaned when no longer in use.
    paths: DashMap<u128, Weak<Path>>,

    /// Peers with which we are currently communicating.
    peers: DashMap<Address, Arc<Peer>>,

    /// This node's trusted roots, sorted in descending order of preference.
    roots: Mutex<Vec<Arc<Peer>>>,

    /// Identity lookup queue, also holds packets waiting on a lookup.
    whois: WhoisQueue,

    /// Reusable network buffer pool.
    buffer_pool: Arc<PacketBufferPool>,
}

impl Node {
    /// Create a new Node.
    pub fn new<SI: SystemInterface>(si: &SI, auto_generate_identity: bool) -> Result<Self, InvalidParameterError> {
        let mut id = {
            let id_str = si.load_node_identity();
            if id_str.is_none() {
                if !auto_generate_identity {
                    return Err(InvalidParameterError("no identity found and auto-generate not enabled"));
                } else {
                    let id = Identity::generate();
                    si.save_node_identity(&id);
                    id
                }
            } else {
                let id_str = String::from_utf8_lossy(id_str.as_ref().unwrap().as_slice());
                let id = Identity::from_str(id_str.as_ref().trim());
                if id.is_err() {
                    return Err(InvalidParameterError("invalid identity"));
                } else {
                    id.unwrap()
                }
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
            paths: DashMap::with_capacity(256),
            peers: DashMap::with_capacity(128),
            roots: Mutex::new(Vec::new()),
            whois: WhoisQueue::new(),
            buffer_pool: Arc::new(PacketBufferPool::new(64, PacketBufferFactory::new())),
        })
    }

    /// Get a packet buffer that will automatically check itself back into the pool on drop.
    #[inline(always)]
    pub fn get_packet_buffer(&self) -> PacketBuffer {
        self.buffer_pool.get()
    }

    /// Get a peer by address.
    pub fn peer(&self, a: Address) -> Option<Arc<Peer>> {
        self.peers.get(&a).map(|peer| peer.value().clone())
    }

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
    /// This should only be called periodically from a single thread, but that thread can be
    /// different each time. Calling it concurrently won't crash but won't accomplish anything.
    pub fn do_background_tasks<SI: SystemInterface>(&self, si: &SI) -> Duration {
        let mut intervals = self.intervals.lock();
        let tt = si.time_ticks();

        if intervals.peers.gate(tt) {
            self.peers.retain(|_, peer| peer.service(si, self, tt));
        }

        if intervals.paths.gate(tt) {
            self.paths.retain(|_, path| path.upgrade().map_or(false, |p| p.service(si, self, tt)));
        }

        if intervals.whois.gate(tt) {
            let _ = self.whois.service(si, self, tt);
        }

        Duration::from_millis((WhoisQueue::SERVICE_INTERVAL_MS.min(Path::SERVICE_INTERVAL_MS).min(Peer::SERVICE_INTERVAL_MS) as u64) / 2)
    }

    /// Called when a packet is received on the physical wire.
    pub fn wire_receive<SI: SystemInterface, PH: InnerProtocolInterface>(&self, si: &SI, ph: &PH, source_endpoint: &Endpoint, source_local_socket: Option<NonZeroI64>, source_local_interface: Option<NonZeroI64>, mut data: PacketBuffer) {
        if let Ok(fragment_header) = data.struct_mut_at::<FragmentHeader>(0) {
            if let Some(dest) = Address::from_bytes(&fragment_header.dest) {
                let time_ticks = si.time_ticks();
                if dest == self.identity.address {
                    // Handle packets (seemingly) addressed to this node.

                    let path = self.path_to_endpoint(source_endpoint, source_local_socket, source_local_interface);
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
                                            peer.receive(self, si, ph, time_ticks, source_endpoint, &path, &packet_header, frag0, &assembled_packet.frags[1..(assembled_packet.have as usize)]);
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
                                    peer.receive(self, si, ph, time_ticks, source_endpoint, &path, &packet_header, data.as_ref(), &[]);
                                } else {
                                    self.whois.query(self, si, source, Some(QueuedPacket::Unfragmented(data)));
                                }
                            }
                        }
                    }
                } else {
                    // Forward packets not destined for this node.

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
    pub fn root(&self) -> Option<Arc<Peer>> {
        self.roots.lock().first().cloned()
    }

    /// Return true if a peer is a root.
    pub fn is_peer_root(&self, peer: &Peer) -> bool {
        self.roots.lock().iter().any(|p| Arc::as_ptr(p) == (peer as *const Peer))
    }

    /// Get the canonical Path object for a given endpoint and local socket information.
    ///
    /// This is a canonicalizing function that returns a unique path object for every tuple
    /// of endpoint, local socket, and local interface.
    pub fn path_to_endpoint(&self, ep: &Endpoint, local_socket: Option<NonZeroI64>, local_interface: Option<NonZeroI64>) -> Arc<Path> {
        let key = Path::local_lookup_key(ep, local_socket, local_interface);
        let mut path_entry = self.paths.entry(key).or_insert_with(|| Weak::new());
        if let Some(path) = path_entry.value().upgrade() {
            path
        } else {
            let p = Arc::new(Path::new(ep.clone(), local_socket, local_interface));
            *path_entry.value_mut() = Arc::downgrade(&p);
            p
        }
    }
}

unsafe impl Send for Node {}

unsafe impl Sync for Node {}
