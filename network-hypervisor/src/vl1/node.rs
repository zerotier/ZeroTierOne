// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::collections::HashMap;
use std::hash::Hash;
use std::io::Write;
use std::sync::atomic::Ordering;
use std::sync::{Arc, Mutex, RwLock};
use std::time::Duration;

use super::address::{Address, PartialAddress};
use super::api::{ApplicationLayer, InnerProtocolLayer};
use super::debug_event;
use super::endpoint::Endpoint;
use super::event::Event;
use super::identity::{Identity, IdentitySecret};
use super::path::{Path, PathServiceResult};
use super::peer::Peer;
use super::peermap::PeerMap;
use super::rootset::RootSet;
use crate::protocol::*;

use zerotier_crypto::typestate::{Valid, Verified};
use zerotier_utils::gate::IntervalGate;
use zerotier_utils::hex;
use zerotier_utils::marshalable::Marshalable;

/// A VL1 node on the ZeroTier global peer to peer network.
///
/// VL1 nodes communicate to/from both the outside world and the inner protocol layer via the two
/// supplied API traits that must be implemented by the application. ApplicationLayer provides a
/// means of interacting with the application/OS and InnerProtocolLayer provides the interface for
/// implementing the protocol (e.g. ZeroTier VL2) that will be carried by VL1.
pub struct Node<Application: ApplicationLayer> {
    pub identity: IdentitySecret,
    intervals: Mutex<BackgroundTaskIntervals>,
    paths: RwLock<HashMap<PathKey<'static, 'static, Application::LocalSocket>, Arc<Path<Application>>>>,
    pub(super) peers: PeerMap<Application>,
    roots: RwLock<RootInfo<Application>>,
    best_root: RwLock<Option<Arc<Peer<Application>>>>,
}

struct RootInfo<Application: ApplicationLayer> {
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
const ROOT_SYNC_INTERVAL_MS: i64 = 2000;

#[derive(Default)]
struct BackgroundTaskIntervals {
    root_sync: IntervalGate<{ ROOT_SYNC_INTERVAL_MS }>,
    root_hello: IntervalGate<{ ROOT_HELLO_INTERVAL }>,
    root_spam_hello: IntervalGate<{ ROOT_HELLO_SPAM_INTERVAL }>,
    peer_service: IntervalGate<{ crate::vl1::peer::SERVICE_INTERVAL_MS }>,
    path_service: IntervalGate<{ crate::vl1::path::SERVICE_INTERVAL_MS }>,
    whois_queue_retry: IntervalGate<{ WHOIS_RETRY_INTERVAL }>,
}

impl<Application: ApplicationLayer> Node<Application> {
    pub fn new(identity_secret: IdentitySecret) -> Self {
        Self {
            identity: identity_secret,
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
        }
    }

    #[inline(always)]
    pub fn peer(&self, a: &Address) -> Option<Arc<Peer<Application>>> {
        self.peers.get_exact(a)
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
                            if m.identity.eq(&self.identity.public) {
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
                                if let Some(peer) = self.peers.get_exact(&m.identity.address) {
                                    new_roots.insert(peer.clone(), m.endpoints.as_ref().unwrap().iter().cloned().collect());
                                } else {
                                    if let Some(peer) = Peer::new(&self.identity, Valid::mark_valid(m.identity.clone()), time_ticks) {
                                        new_roots.insert(self.peers.add(Arc::new(peer)).0, m.endpoints.as_ref().unwrap().iter().cloned().collect());
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
            /*
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
            */
        }

        INTERVAL
    }

    pub fn handle_incoming_physical_packet<Inner: InnerProtocolLayer>(
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
            if let Ok(dest) = PartialAddress::from_legacy_address_bytes(&fragment_header.dest) {
                // Packet is addressed to this node.

                if dest.matches(&self.identity.public.address) {
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
                                    if let Ok(source) = PartialAddress::from_legacy_address_bytes(&packet_header.src) {
                                        if let Some(peer) = self.peers.get_unambiguous(&source) {
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
                                                // TODO
                                                //self.whois(app, source.clone(), Some((Arc::downgrade(&path), combined_packet)), time_ticks);
                                            }
                                        }
                                    } // else source address invalid
                                } // else header incomplete
                            } // else reassembly failed (in a way that shouldn't be possible)
                        } // else packet not fully assembled yet
                    } else if let Ok(packet_header) = packet.struct_at::<v1::PacketHeader>(0) {
                        debug_event!(app, "[vl1] [v1] #{:0>16x} is unfragmented", u64::from_be_bytes(packet_header.id));

                        if let Ok(source) = PartialAddress::from_legacy_address_bytes(&packet_header.src) {
                            if let Some(peer) = self.peers.get_unambiguous(&source) {
                                peer.v1_proto_receive(self, app, inner, time_ticks, &path, packet_header, packet.as_ref(), &[]);
                            } else {
                                // TODO
                                //self.whois(app, source, Some((Arc::downgrade(&path), packet)), time_ticks);
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

                    if let Some(peer) = self.peers.get_unambiguous(&dest) {
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

    /// Called when a remote node sends us a root set update, applying the update if it is valid and applicable.
    ///
    /// This will only replace an existing root set with a newer one. It won't add a new root set, which must be
    /// done by an authorized user or administrator not just by a root.
    #[allow(unused)]
    pub(super) fn on_remote_update_root_set(&self, received_from: &Identity, rs: Verified<RootSet>) {
        let mut roots = self.roots.write().unwrap();
        if let Some(entry) = roots.sets.get_mut(&rs.name) {
            if entry.members.iter().any(|m| m.identity.eq(received_from)) && rs.should_replace(entry) {
                *entry = rs;
                roots.sets_modified = true;
            }
        }
    }

    /// Get the canonical Path object corresponding to an endpoint.
    pub(super) fn canonical_path(
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

/// Key used to look up paths in a hash map efficiently. It can be constructed for lookup without full copy.
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
