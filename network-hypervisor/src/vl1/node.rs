// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::collections::HashMap;
use std::convert::Infallible;
use std::hash::Hash;
use std::sync::atomic::Ordering;
use std::sync::{Arc, Mutex, RwLock, Weak};
use std::time::Duration;

use crate::protocol::*;
use crate::vl1::address::{Address, LegacyAddress};
use crate::vl1::debug_event;
use crate::vl1::endpoint::Endpoint;
use crate::vl1::event::Event;
use crate::vl1::identity::{Identity, IdentitySecret};
use crate::vl1::path::{Path, PathServiceResult};
use crate::vl1::peer::Peer;
use crate::vl1::peermap::PeerMap;
use crate::vl1::rootset::RootSet;

use zerotier_crypto::typestate::{Valid, Verified};
use zerotier_utils::gate::IntervalGate;
use zerotier_utils::hex;
use zerotier_utils::marshalable::Marshalable;
use zerotier_utils::ringbuffer::RingBuffer;
use zerotier_utils::tokio::io::AsyncWriteExt;

/// Interface trait to be implemented by code that's using the ZeroTier network hypervisor.
///
/// This is analogous to a C struct full of function pointers to callbacks along with some
/// associated type definitions.
pub trait ApplicationLayer: Sync + Send + 'static {
    /// Type for local system sockets.
    type LocalSocket: Sync + Send + Hash + PartialEq + Eq + Clone + ToString + Sized + 'static;

    /// Type for local system interfaces.
    type LocalInterface: Sync + Send + Hash + PartialEq + Eq + Clone + ToString + Sized + 'static;

    /// A VL1 level event occurred.
    fn event(&self, event: Event);

    /// Get a pooled packet buffer for internal use.
    fn get_buffer(&self) -> PooledPacketBuffer;

    /// Check a local socket for validity.
    ///
    /// This could return false if the socket's interface no longer exists, its port has been
    /// unbound, etc.
    fn local_socket_is_valid(&self, socket: &Self::LocalSocket) -> bool;

    /// Check if this node should respond to messages from a given peer at all.
    fn should_respond_to(&self, id: &Valid<Identity>) -> bool;

    /// Called to send a packet over the physical network (virtual -> physical).
    ///
    /// This sends with UDP-like semantics. It should do whatever best effort it can and return.
    ///
    /// If a local socket is specified the implementation should send from that socket or not
    /// at all (returning false). If a local interface is specified the implementation should
    /// send from all sockets on that interface. If neither is specified the packet may be
    /// sent on all sockets or a random subset.
    ///
    /// For endpoint types that support a packet TTL, the implementation may set the TTL
    /// if the 'ttl' parameter is not zero. If the parameter is zero or TTL setting is not
    /// supported, the default TTL should be used. This parameter is ignored for types that
    /// don't support it.
    fn wire_send(
        &self,
        endpoint: &Endpoint,
        local_socket: Option<&Self::LocalSocket>,
        local_interface: Option<&Self::LocalInterface>,
        data: &[u8],
        packet_ttl: u8,
    );

    /// Called to check and see if a physical address should be used for ZeroTier traffic to a node.
    ///
    /// The default implementation always returns true.
    #[allow(unused_variables)]
    fn should_use_physical_path<Application: ApplicationLayer + ?Sized>(
        &self,
        id: &Valid<Identity>,
        endpoint: &Endpoint,
        local_socket: Option<&Application::LocalSocket>,
        local_interface: Option<&Application::LocalInterface>,
    ) -> bool {
        true
    }

    /// Called to look up any statically defined or memorized paths to known nodes.
    ///
    /// The default implementation always returns None.
    #[allow(unused_variables)]
    fn get_path_hints<Application: ApplicationLayer + ?Sized>(
        &self,
        id: &Valid<Identity>,
    ) -> Option<Vec<(Endpoint, Option<Application::LocalSocket>, Option<Application::LocalInterface>)>> {
        None
    }

    /// Called to get the current time in milliseconds from the system monotonically increasing clock.
    /// This needs to be accurate to about 250 milliseconds resolution or better.
    fn time_ticks(&self) -> i64;

    /// Called to get the current time in milliseconds since epoch from the real-time clock.
    /// This needs to be accurate to about one second resolution or better.
    fn time_clock(&self) -> i64;
}

/// Result of a packet handler.
pub enum PacketHandlerResult {
    /// Packet was handled successfully.
    Ok,

    /// Packet was handled and an error occurred (malformed, authentication failure, etc.)
    Error,

    /// Packet was not handled by this handler.
    NotHandled,
}

/// Interface between VL1 and higher/inner protocol layers.
///
/// This is implemented by Switch in VL2. It's usually not used outside of VL2 in the core but
/// it could also be implemented for testing or "off label" use of VL1 to carry different protocols.
#[allow(unused)]
pub trait InnerProtocolLayer: Sync + Send {
    /// Handle a packet, returning true if it was handled by the next layer.
    ///
    /// Do not attempt to handle OK or ERROR. Instead implement handle_ok() and handle_error().
    /// The default version returns NotHandled.
    fn handle_packet<Application: ApplicationLayer + ?Sized>(
        &self,
        app: &Application,
        node: &Node<Application>,
        source: &Arc<Peer<Application>>,
        source_path: &Arc<Path<Application>>,
        source_hops: u8,
        message_id: u64,
        verb: u8,
        payload: &PacketBuffer,
        cursor: usize,
    ) -> PacketHandlerResult {
        PacketHandlerResult::NotHandled
    }

    /// Handle errors, returning true if the error was recognized.
    /// The default version returns NotHandled.
    fn handle_error<Application: ApplicationLayer + ?Sized>(
        &self,
        app: &Application,
        node: &Node<Application>,
        source: &Arc<Peer<Application>>,
        source_path: &Arc<Path<Application>>,
        source_hops: u8,
        message_id: u64,
        in_re_verb: u8,
        in_re_message_id: u64,
        error_code: u8,
        payload: &PacketBuffer,
        cursor: usize,
    ) -> PacketHandlerResult {
        PacketHandlerResult::NotHandled
    }

    /// Handle an OK, returning true if the OK was recognized.
    /// The default version returns NotHandled.
    fn handle_ok<Application: ApplicationLayer + ?Sized>(
        &self,
        app: &Application,
        node: &Node<Application>,
        source: &Arc<Peer<Application>>,
        source_path: &Arc<Path<Application>>,
        source_hops: u8,
        message_id: u64,
        in_re_verb: u8,
        in_re_message_id: u64,
        payload: &PacketBuffer,
        cursor: usize,
    ) -> PacketHandlerResult {
        PacketHandlerResult::NotHandled
    }
}

struct RootInfo<Application: ApplicationLayer + ?Sized> {
    /// Root sets to which we are a member.
    sets: HashMap<String, Verified<RootSet>>,

    /// Root peers and their statically defined endpoints (from root sets).
    roots: HashMap<Arc<Peer<Application>>, Vec<Endpoint>>,

    /// If this node is a root, these are the root sets to which it's a member in binary serialized form.
    /// Set to None if this node is not a root, meaning it doesn't appear in any of its root sets.
    this_root_sets: Option<Vec<u8>>,

    /// True if sets have been modified and things like 'roots' need to be rebuilt.
    sets_modified: bool,

    /// True if this node is online, which means it can talk to at least one of its roots.
    online: bool,
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
    whois_queue_retry: IntervalGate<{ WHOIS_RETRY_INTERVAL }>,
}

struct WhoisQueueItem<Application: ApplicationLayer + ?Sized> {
    v1_proto_waiting_packets: RingBuffer<(Weak<Path<Application>>, PooledPacketBuffer), WHOIS_MAX_WAITING_PACKETS>,
    last_retry_time: i64,
    retry_count: u16,
}

pub struct Node<Application: ApplicationLayer + ?Sized> {
    identity_secret: IdentitySecret,
    intervals: Mutex<BackgroundTaskIntervals>,
    paths: RwLock<HashMap<PathKey<'static, 'static, Application::LocalSocket>, Arc<Path<Application>>>>,
    peers: PeerMap<Application>,
    roots: RwLock<RootInfo<Application>>,
    best_root: RwLock<Option<Arc<Peer<Application>>>>,
    whois_queue: Mutex<HashMap<LegacyAddress, WhoisQueueItem<Application>>>,
}

impl<Application: ApplicationLayer + ?Sized> Node<Application> {
    pub fn new(identity_secret: IdentitySecret) -> Self {
        Self {
            identity_secret,
            intervals: Mutex::new(BackgroundTaskIntervals::default()),
            paths: RwLock::new(HashMap::new()),
            peers: PeerMap::new(),
            roots: RwLock::new(RootInfo {
                sets: HashMap::new(),
                roots: HashMap::new(),
                this_root_sets: None,
                sets_modified: false,
                online: false,
            }),
            best_root: RwLock::new(None),
            whois_queue: Mutex::new(HashMap::new()),
        }
    }

    #[inline(always)]
    pub fn identity(&self) -> &Valid<Identity> {
        &self.identity_secret.public
    }

    #[inline(always)]
    pub fn peer(&self, a: &Address) -> Option<Arc<Peer<Application>>> {
        self.peers.get(a)
    }

    #[inline(always)]
    pub(crate) fn peer_legacy(&self, a: &LegacyAddress) -> Option<Arc<Peer<Application>>> {
        self.peers.get_legacy(a)
    }

    #[inline]
    pub fn is_online(&self) -> bool {
        self.roots.read().unwrap().online
    }

    /// Get the current "best" root from among this node's trusted roots.
    #[inline]
    pub fn best_root(&self) -> Option<Arc<Peer<Application>>> {
        self.best_root.read().unwrap().clone()
    }

    /// Check whether a peer is a root according to any root set trusted by this node.
    #[inline]
    pub fn is_peer_root(&self, peer: &Peer<Application>) -> bool {
        self.roots.read().unwrap().roots.keys().any(|p| p.identity.eq(&peer.identity))
    }

    /// Returns true if this node is a member of a root set (that it knows about).
    #[inline]
    pub fn this_node_is_root(&self) -> bool {
        self.roots.read().unwrap().this_root_sets.is_some()
    }

    /// Add a new root set or update the existing root set if the new root set is newer and otherwise matches.
    pub fn add_update_root_set(&self, rs: Verified<RootSet>) -> bool {
        let mut roots = self.roots.write().unwrap();
        if let Some(entry) = roots.sets.get_mut(&rs.name) {
            if rs.should_replace(entry) {
                *entry = rs;
                roots.sets_modified = true;
                true
            } else {
                false
            }
        } else {
            let _ = roots.sets.insert(rs.name.clone(), rs);
            roots.sets_modified = true;
            true
        }
    }

    /// Returns whether or not this node has any root sets defined.
    pub fn has_roots_defined(&self) -> bool {
        self.roots.read().unwrap().sets.iter().any(|rs| !rs.1.members.is_empty())
    }

    /// Initialize with default roots if there are no roots defined, otherwise do nothing.
    pub fn init_default_roots(&self) -> bool {
        if !self.has_roots_defined() {
            self.add_update_root_set(RootSet::zerotier_default())
        } else {
            false
        }
    }

    /// Get the root sets that this node trusts.
    pub fn root_sets(&self) -> Vec<RootSet> {
        self.roots.read().unwrap().sets.values().cloned().map(|s| s.remove_typestate()).collect()
    }

    pub fn do_background_tasks(&self, app: &Application) -> Duration {
        const INTERVAL_MS: i64 = 1000;
        const INTERVAL: Duration = Duration::from_millis(INTERVAL_MS as u64);
        let time_ticks = app.time_ticks();

        let (root_sync, root_hello, root_spam_hello, peer_service, path_service, whois_queue_retry) = {
            let mut intervals = self.intervals.lock().unwrap();
            (
                intervals.root_sync.gate(time_ticks),
                intervals.root_hello.gate(time_ticks),
                intervals.root_spam_hello.gate(time_ticks) && !self.is_online(),
                intervals.peer_service.gate(time_ticks),
                intervals.path_service.gate(time_ticks),
                intervals.whois_queue_retry.gate(time_ticks),
            )
        };

        if root_sync {
            if {
                let mut roots = self.roots.write().unwrap();
                if roots.sets_modified {
                    roots.sets_modified = false;
                    true
                } else {
                    false
                }
            } {
                debug_event!(app, "[vl1] root sets modified, synchronizing internal data structures");

                let (mut old_root_identities, new_roots, bad_identities, my_root_sets) = {
                    let roots = self.roots.read().unwrap();

                    let old_root_identities: Vec<Identity> = roots.roots.iter().map(|(p, _)| p.identity.as_ref().clone()).collect();
                    let mut new_roots = HashMap::new();
                    let mut bad_identities = Vec::new();
                    let mut my_root_sets: Option<Vec<u8>> = None;

                    for (_, rs) in roots.sets.iter() {
                        for m in rs.members.iter() {
                            if m.identity.eq(&self.identity_secret.public) {
                                let _ = my_root_sets
                                    .get_or_insert_with(|| Vec::new())
                                    .write_all(rs.to_buffer::<{ RootSet::MAX_MARSHAL_SIZE }>().unwrap().as_bytes());
                            } else if m.endpoints.is_some() {
                                debug_event!(
                                    app,
                                    "[vl1] examining root {} with {} endpoints",
                                    m.identity.address.to_string(),
                                    m.endpoints.as_ref().map_or(0, |e| e.len())
                                );
                                if let Some(peer) = self.peers.get(&m.identity.address) {
                                    new_roots.insert(peer.clone(), m.endpoints.as_ref().unwrap().iter().cloned().collect());
                                } else {
                                    if let Some(peer) = Peer::new(&self.identity_secret, Valid::mark_valid(m.identity.clone()), time_ticks) {
                                        new_roots.insert(
                                            self.peers.insert_if_unique(Arc::new(peer)).0,
                                            m.endpoints.as_ref().unwrap().iter().cloned().collect(),
                                        );
                                    } else {
                                        bad_identities.push(m.identity.clone());
                                    }
                                }
                            }
                        }
                    }

                    (old_root_identities, new_roots, bad_identities, my_root_sets)
                };

                for i in bad_identities.iter() {
                    app.event(Event::SecurityWarning(format!(
                        "bad identity detected for address {} in at least one root set, ignoring (error creating peer object)",
                        i.address.to_string()
                    )));
                }

                let mut new_root_identities: Vec<Identity> = new_roots.iter().map(|(p, _)| p.identity.as_ref().clone()).collect();
                old_root_identities.sort_unstable();
                new_root_identities.sort_unstable();

                if !old_root_identities.eq(&new_root_identities) {
                    let mut roots = self.roots.write().unwrap();
                    roots.roots = new_roots;
                    roots.this_root_sets = my_root_sets;
                    app.event(Event::UpdatedRoots(old_root_identities, new_root_identities));
                }
            }

            {
                let roots = self.roots.read().unwrap();

                // The best root is the one that has replied to a HELLO most recently. Since we send HELLOs in unison
                // this is a proxy for latency and also causes roots that fail to reply to drop out quickly.
                let mut best = None;
                let mut latest_hello_reply = 0;
                for (r, _) in roots.roots.iter() {
                    let t = r.last_hello_reply_time_ticks.load(Ordering::Relaxed);
                    if t > latest_hello_reply {
                        latest_hello_reply = t;
                        let _ = best.insert(r);
                    }
                }

                if let Some(best) = best {
                    let best_root = self.best_root.read().unwrap();
                    if best_root.as_ref().map_or(true, |br| !Arc::ptr_eq(&br, best)) {
                        drop(best_root);
                        let mut best_root = self.best_root.write().unwrap();
                        if let Some(best_root) = best_root.as_mut() {
                            debug_event!(
                                app,
                                "[vl1] selected new best root: {} (replaced {})",
                                best.identity.address.to_string(),
                                best_root.identity.address.to_string()
                            );
                            *best_root = best.clone();
                        } else {
                            debug_event!(app, "[vl1] selected new best root: {} (was empty)", best.identity.address.to_string());
                            let _ = best_root.insert(best.clone());
                        }
                    }
                } else {
                    if let Some(old_best) = self.best_root.write().unwrap().take() {
                        debug_event!(
                            app,
                            "[vl1] selected new best root: NONE (replaced {})",
                            old_best.identity.address.to_string()
                        );
                    }
                }

                // Determine if the node is online by whether there is a currently reachable root.
                if (time_ticks - latest_hello_reply) < (ROOT_HELLO_INTERVAL * 2) && best.is_some() {
                    if !roots.online {
                        drop(roots);
                        self.roots.write().unwrap().online = true;
                        app.event(Event::Online(true));
                    }
                } else if roots.online {
                    drop(roots);
                    self.roots.write().unwrap().online = false;
                    app.event(Event::Online(false));
                }
            }
        }

        // Say HELLO to all roots periodically. For roots we send HELLO to every single endpoint
        // they have, which is a behavior that differs from normal peers. This allows roots to
        // e.g. see our IPv4 and our IPv6 address which can be important for us to learn our
        // external addresses from them.
        if root_hello || root_spam_hello {
            let roots = {
                let roots = self.roots.read().unwrap();
                let mut roots_copy = Vec::with_capacity(roots.roots.len());
                for (root, endpoints) in roots.roots.iter() {
                    roots_copy.push((root.clone(), endpoints.clone()));
                }
                roots_copy
            };
            for (root, endpoints) in roots.iter() {
                for ep in endpoints.iter() {
                    debug_event!(
                        app,
                        "sending HELLO to root {} (root interval: {})",
                        root.identity.address.to_string(),
                        ROOT_HELLO_INTERVAL
                    );
                    let root = root.clone();
                    let ep = ep.clone();
                    root.send_hello(app, self, Some(&ep));
                }
            }
        }

        if peer_service {
            // Service all peers, removing any whose service() method returns false AND that are not
            // roots. Roots on the other hand remain in the peer list as long as they are roots.
            let mut dead_peers = Vec::new();
            {
                let roots = self.roots.read().unwrap();
                self.peers.each(|peer| {
                    if !peer.service(app, self, time_ticks) && !roots.roots.contains_key(peer) {
                        dead_peers.push(peer.identity.address.clone());
                    }
                });
            }
            for dp in dead_peers.iter() {
                self.peers.remove(dp);
            }
        }

        if path_service {
            let mut dead_paths = Vec::new();
            let mut need_keepalive = Vec::new();

            // First check all paths in read mode to avoid blocking the entire node.
            for (k, path) in self.paths.read().unwrap().iter() {
                if app.local_socket_is_valid(k.local_socket()) {
                    match path.service(time_ticks) {
                        PathServiceResult::Ok => {}
                        PathServiceResult::Dead => dead_paths.push(k.to_copied()),
                        PathServiceResult::NeedsKeepalive => need_keepalive.push(path.clone()),
                    }
                } else {
                    dead_paths.push(k.to_copied());
                }
            }

            // Lock in write mode and remove dead paths, doing so piecemeal to again avoid blocking.
            for dp in dead_paths.iter() {
                self.paths.write().unwrap().remove(dp);
            }

            // Finally run keepalive sends as a batch.
            let keepalive_buf = [time_ticks as u8]; // just an arbitrary byte, no significance
            for p in need_keepalive.iter() {
                app.wire_send(&p.endpoint, Some(&p.local_socket), Some(&p.local_interface), &keepalive_buf, 0);
            }
        }

        if whois_queue_retry {
            let need_whois = {
                let mut need_whois = Vec::new();
                let mut whois_queue = self.whois_queue.lock().unwrap();
                whois_queue.retain(|_, qi| qi.retry_count <= WHOIS_RETRY_COUNT_MAX);
                for (address, qi) in whois_queue.iter_mut() {
                    if (time_ticks - qi.last_retry_time) >= WHOIS_RETRY_INTERVAL {
                        qi.retry_count += 1;
                        qi.last_retry_time = time_ticks;
                        need_whois.push(address.clone());
                    }
                }
                need_whois
            };
            if !need_whois.is_empty() {
                self.send_whois(app, need_whois.as_slice(), time_ticks);
            }
        }

        INTERVAL
    }

    pub fn handle_incoming_physical_packet<Inner: InnerProtocolLayer + ?Sized>(
        &self,
        app: &Application,
        inner: &Inner,
        source_endpoint: &Endpoint,
        source_local_socket: &Application::LocalSocket,
        source_local_interface: &Application::LocalInterface,
        time_ticks: i64,
        mut packet: PooledPacketBuffer,
    ) {
        debug_event!(
            app,
            "[vl1] {} -> #{} {}->{} length {} (on socket {}@{})",
            source_endpoint.to_string(),
            packet.bytes_fixed_at::<8>(0).map_or("????????????????".into(), |pid| hex::to_string(pid)),
            packet.bytes_fixed_at::<5>(13).map_or("??????????".into(), |src| hex::to_string(src)),
            packet.bytes_fixed_at::<5>(8).map_or("??????????".into(), |dest| hex::to_string(dest)),
            packet.len(),
            source_local_socket.to_string(),
            source_local_interface.to_string()
        );

        // TODO: detect inbound ZSSP sessions, handle ZSSP mode.

        // Legacy ZeroTier V1 packet handling
        if let Ok(fragment_header) = packet.struct_mut_at::<v1::FragmentHeader>(0) {
            if let Some(dest) = LegacyAddress::from_bytes_exact(&fragment_header.dest) {
                // Packet is addressed to this node.

                if dest == self.identity_secret.public.address.legacy_address() {
                    let fragment_header = &*fragment_header; // discard mut
                    let path = self.canonical_path(source_endpoint, source_local_socket, source_local_interface, time_ticks);
                    path.log_receive_anything(time_ticks);

                    if fragment_header.is_fragment() {
                        #[cfg(debug_assertions)]
                        let fragment_header_id = u64::from_be_bytes(fragment_header.id);
                        debug_event!(
                            app,
                            "[vl1] [v1] #{:0>16x} fragment {} of {} received",
                            u64::from_be_bytes(fragment_header.id),
                            fragment_header.fragment_no(),
                            fragment_header.total_fragments()
                        );

                        if let Some(assembled_packet) = path.v1_proto_receive_fragment(
                            fragment_header.packet_id(),
                            fragment_header.fragment_no(),
                            fragment_header.total_fragments(),
                            packet,
                            time_ticks,
                        ) {
                            if let Some(frag0) = assembled_packet.frags[0].as_ref() {
                                #[cfg(debug_assertions)]
                                debug_event!(app, "[vl1] [v1] #{:0>16x} packet fully assembled!", fragment_header_id);

                                if let Ok(packet_header) = frag0.struct_at::<v1::PacketHeader>(0) {
                                    if let Some(source) = LegacyAddress::from_bytes_exact(&packet_header.src) {
                                        if let Some(peer) = self.peers.get_legacy(&source) {
                                            peer.v1_proto_receive(
                                                self,
                                                app,
                                                inner,
                                                time_ticks,
                                                &path,
                                                packet_header,
                                                frag0,
                                                &assembled_packet.frags[1..(assembled_packet.have as usize)],
                                            );
                                        } else {
                                            // If WHOIS is needed we need to go ahead and combine the packet so it can be cached
                                            // for later processing when a WHOIS reply comes back.
                                            let mut combined_packet = app.get_buffer();
                                            let mut ok = combined_packet.append_bytes(frag0.as_bytes()).is_ok();
                                            for i in 1..assembled_packet.have {
                                                if let Some(f) = assembled_packet.frags[i as usize].as_ref() {
                                                    if f.len() > v1::FRAGMENT_HEADER_SIZE {
                                                        ok |= combined_packet.append_bytes(&f.as_bytes()[v1::FRAGMENT_HEADER_SIZE..]).is_ok();
                                                    }
                                                }
                                            }
                                            if ok {
                                                self.whois(app, source.clone(), Some((Arc::downgrade(&path), combined_packet)), time_ticks);
                                            }
                                        }
                                    } // else source address invalid
                                } // else header incomplete
                            } // else reassembly failed (in a way that shouldn't be possible)
                        } // else packet not fully assembled yet
                    } else if let Ok(packet_header) = packet.struct_at::<v1::PacketHeader>(0) {
                        debug_event!(app, "[vl1] [v1] #{:0>16x} is unfragmented", u64::from_be_bytes(packet_header.id));

                        if let Some(source) = LegacyAddress::from_bytes_exact(&packet_header.src) {
                            if let Some(peer) = self.peers.get_legacy(&source) {
                                peer.v1_proto_receive(self, app, inner, time_ticks, &path, packet_header, packet.as_ref(), &[]);
                            } else {
                                self.whois(app, source, Some((Arc::downgrade(&path), packet)), time_ticks);
                            }
                        }
                    } // else not fragment and header incomplete
                } else if self.this_node_is_root() {
                    // Packet is addressed somewhere else, forward if this node is a root.

                    #[cfg(debug_assertions)]
                    let debug_packet_id;

                    // Increment and check hop count in packet header, return if max hops exceeded or error.
                    if fragment_header.is_fragment() {
                        #[cfg(debug_assertions)]
                        {
                            debug_packet_id = u64::from_be_bytes(fragment_header.id);
                            debug_event!(
                                app,
                                "[vl1] [v1] #{:0>16x} forwarding packet fragment to {}",
                                debug_packet_id,
                                dest.to_string()
                            );
                        }
                        if fragment_header.increment_hops() > v1::FORWARD_MAX_HOPS {
                            #[cfg(debug_assertions)]
                            debug_event!(app, "[vl1] [v1] #{:0>16x} discarded: max hops exceeded!", debug_packet_id);
                            return;
                        }
                    } else if let Ok(packet_header) = packet.struct_mut_at::<v1::PacketHeader>(0) {
                        #[cfg(debug_assertions)]
                        {
                            debug_packet_id = u64::from_be_bytes(packet_header.id);
                            debug_event!(app, "[vl1] [v1] #{:0>16x} forwarding packet to {}", debug_packet_id, dest.to_string());
                        }
                        if packet_header.increment_hops() > v1::FORWARD_MAX_HOPS {
                            #[cfg(debug_assertions)]
                            debug_event!(
                                app,
                                "[vl1] [v1] #{:0>16x} discarded: max hops exceeded!",
                                u64::from_be_bytes(packet_header.id)
                            );
                            return;
                        }
                    } else {
                        return;
                    }

                    if let Some(peer) = self.peers.get_legacy(&dest) {
                        if let Some(forward_path) = peer.direct_path() {
                            app.wire_send(
                                &forward_path.endpoint,
                                Some(&forward_path.local_socket),
                                Some(&forward_path.local_interface),
                                packet.as_bytes(),
                                0,
                            );

                            peer.last_forward_time_ticks.store(time_ticks, Ordering::Relaxed);

                            #[cfg(debug_assertions)]
                            debug_event!(app, "[vl1] [v1] #{:0>16x} forwarded successfully", debug_packet_id);
                        }
                    }
                } // else not for this node and shouldn't be forwarded
            }
        }
    }

    /// Enqueue and send a WHOIS query for a given address, adding the supplied packet (if any) to the list to be processed on reply.
    fn whois(
        &self,
        app: &Application,
        address: LegacyAddress,
        waiting_packet: Option<(Weak<Path<Application>>, PooledPacketBuffer)>,
        time_ticks: i64,
    ) {
        {
            let mut whois_queue = self.whois_queue.lock().unwrap();
            let qi = whois_queue.entry(address).or_insert_with(|| WhoisQueueItem {
                v1_proto_waiting_packets: RingBuffer::new(),
                last_retry_time: 0,
                retry_count: 0,
            });
            if let Some(p) = waiting_packet {
                qi.v1_proto_waiting_packets.add(p);
            }
            if qi.retry_count > 0 {
                return;
            } else {
                qi.last_retry_time = time_ticks;
                qi.retry_count += 1;
            }
        }
        self.send_whois(app, &[address], time_ticks);
    }

    /// Send a WHOIS query to the current best root.
    fn send_whois(&self, app: &Application, mut addresses: &[LegacyAddress], time_ticks: i64) {
        debug_assert!(!addresses.is_empty());
        debug_event!(app, "[vl1] [v1] sending WHOIS for {}", {
            let mut tmp = String::new();
            for a in addresses.iter() {
                if !tmp.is_empty() {
                    tmp.push(',');
                }
                tmp.push_str(a.to_string().as_str());
            }
            tmp
        });
        if let Some(root) = self.best_root() {
            while !addresses.is_empty() {
                if !root
                    .send(app, self, None, time_ticks, |packet| -> Result<(), Infallible> {
                        assert!(packet.append_u8(message_type::VL1_WHOIS).is_ok());
                        while !addresses.is_empty() && (packet.len() + ADDRESS_SIZE) <= UDP_DEFAULT_MTU {
                            assert!(packet.append_bytes_fixed(addresses[0].as_bytes()).is_ok());
                            addresses = &addresses[1..];
                        }
                        Ok(())
                    })
                    .is_some()
                {
                    break;
                }
            }
        }
    }

    /// Called by Peer when an identity is received from another node, e.g. via OK(WHOIS).
    pub(crate) fn handle_incoming_identity<Inner: InnerProtocolLayer + ?Sized>(
        &self,
        app: &Application,
        inner: &Inner,
        received_identity: Identity,
        time_ticks: i64,
        authoritative: bool,
    ) {
        if authoritative {
            if let Some(received_identity) = received_identity.validate() {
                let mut whois_queue = self.whois_queue.lock().unwrap();
                if let Some(qi) = whois_queue.get_mut(&received_identity.address.legacy_address()) {
                    let address = received_identity.address.legacy_address();
                    /*
                    if app.should_respond_to(&received_identity) {
                        if let Some(peer) = peers.get(&address).cloned().or_else(|| {
                            Peer::new(&self.identity_secret, received_identity, time_ticks)
                                .map(|p| Arc::new(p))
                                .and_then(|peer| Some(peers.entry(address.clone()).or_insert(peer).clone()))
                        }) {
                            drop(peers);
                            for p in qi.v1_proto_waiting_packets.iter() {
                                if let Some(path) = p.0.upgrade() {
                                    if let Ok(packet_header) = p.1.struct_at::<v1::PacketHeader>(0) {
                                        peer.v1_proto_receive(self, app, inner, time_ticks, &path, packet_header, &p.1, &[]);
                                    }
                                }
                            }
                        }
                    }
                    */
                    whois_queue.remove(&address);
                }
            }
        }
    }

    /// Called when a remote node sends us a root set update, applying the update if it is valid and applicable.
    ///
    /// This will only replace an existing root set with a newer one. It won't add a new root set, which must be
    /// done by an authorized user or administrator not just by a root.
    #[allow(unused)]
    pub(crate) fn on_remote_update_root_set(&self, received_from: &Identity, rs: Verified<RootSet>) {
        let mut roots = self.roots.write().unwrap();
        if let Some(entry) = roots.sets.get_mut(&rs.name) {
            if entry.members.iter().any(|m| m.identity.eq(received_from)) && rs.should_replace(entry) {
                *entry = rs;
                roots.sets_modified = true;
            }
        }
    }

    /// Get the canonical Path object corresponding to an endpoint.
    pub(crate) fn canonical_path(
        &self,
        ep: &Endpoint,
        local_socket: &Application::LocalSocket,
        local_interface: &Application::LocalInterface,
        time_ticks: i64,
    ) -> Arc<Path<Application>> {
        let paths = self.paths.read().unwrap();
        if let Some(path) = paths.get(&PathKey::Ref(ep, local_socket)) {
            path.clone()
        } else {
            drop(paths);
            self.paths
                .write()
                .unwrap()
                .entry(PathKey::Copied(ep.clone(), local_socket.clone()))
                .or_insert_with(|| Arc::new(Path::new(ep.clone(), local_socket.clone(), local_interface.clone(), time_ticks)))
                .clone()
        }
    }
}

/// Key used to look up paths in a hash map efficiently.
enum PathKey<'a, 'b, LocalSocket: Hash + PartialEq + Eq + Clone> {
    Copied(Endpoint, LocalSocket),
    Ref(&'a Endpoint, &'b LocalSocket),
}

impl<LocalSocket: Hash + PartialEq + Eq + Clone> Hash for PathKey<'_, '_, LocalSocket> {
    #[inline(always)]
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

impl<LocalSocket: Hash + PartialEq + Eq + Clone> PartialEq for PathKey<'_, '_, LocalSocket> {
    #[inline(always)]
    fn eq(&self, other: &Self) -> bool {
        match (self, other) {
            (Self::Copied(ep1, ls1), Self::Copied(ep2, ls2)) => ep1.eq(ep2) && ls1.eq(ls2),
            (Self::Copied(ep1, ls1), Self::Ref(ep2, ls2)) => ep1.eq(*ep2) && ls1.eq(*ls2),
            (Self::Ref(ep1, ls1), Self::Copied(ep2, ls2)) => (*ep1).eq(ep2) && (*ls1).eq(ls2),
            (Self::Ref(ep1, ls1), Self::Ref(ep2, ls2)) => (*ep1).eq(*ep2) && (*ls1).eq(*ls2),
        }
    }
}

impl<LocalSocket: Hash + PartialEq + Eq + Clone> Eq for PathKey<'_, '_, LocalSocket> {}

impl<LocalSocket: Hash + PartialEq + Eq + Clone> PathKey<'_, '_, LocalSocket> {
    #[inline(always)]
    fn local_socket(&self) -> &LocalSocket {
        match self {
            Self::Copied(_, ls) => ls,
            Self::Ref(_, ls) => *ls,
        }
    }

    #[inline(always)]
    fn to_copied(&self) -> PathKey<'static, 'static, LocalSocket> {
        match self {
            Self::Copied(ep, ls) => PathKey::<'static, 'static, LocalSocket>::Copied(ep.clone(), ls.clone()),
            Self::Ref(ep, ls) => PathKey::<'static, 'static, LocalSocket>::Copied((*ep).clone(), (*ls).clone()),
        }
    }
}
