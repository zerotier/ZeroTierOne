// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::collections::HashMap;
use std::hash::Hash;
use std::sync::atomic::{AtomicI64, AtomicU64, Ordering};
use std::sync::{Arc, Mutex, RwLock, Weak};

use zerotier_crypto::poly1305;
use zerotier_crypto::random;
use zerotier_crypto::salsa::Salsa;
use zerotier_crypto::secret::Secret;
use zerotier_utils::marshalable::Marshalable;
use zerotier_utils::memory::array_range;
use zerotier_utils::NEVER_HAPPENED_TICKS;

use crate::protocol::*;
use crate::vl1::address::Address;
use crate::vl1::debug_event;
use crate::vl1::node::*;
use crate::vl1::{Endpoint, Identity, Path};
use crate::{VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION};

pub(crate) const SERVICE_INTERVAL_MS: i64 = 10000;

pub struct Peer<HostSystemImpl: HostSystem + ?Sized> {
    pub identity: Identity,

    v1_proto_static_secret: v1::SymmetricSecret,
    paths: Mutex<Vec<PeerPath<HostSystemImpl>>>,

    pub(crate) last_send_time_ticks: AtomicI64,
    pub(crate) last_receive_time_ticks: AtomicI64,
    pub(crate) last_hello_reply_time_ticks: AtomicI64,
    pub(crate) last_forward_time_ticks: AtomicI64,
    pub(crate) create_time_ticks: i64,

    random_ticks_offset: u32,
    message_id_counter: AtomicU64,
    remote_node_info: RwLock<RemoteNodeInfo>,
}

struct PeerPath<HostSystemImpl: HostSystem + ?Sized> {
    path: Weak<Path<HostSystemImpl>>,
    last_receive_time_ticks: i64,
}

struct RemoteNodeInfo {
    reported_local_endpoints: HashMap<Endpoint, i64>,
    remote_protocol_version: u8,
    remote_version: (u8, u8, u16),
}

/// Sort a list of paths by quality or priority, with best paths first.
fn prioritize_paths<HostSystemImpl: HostSystem + ?Sized>(paths: &mut Vec<PeerPath<HostSystemImpl>>) {
    paths.sort_unstable_by(|a, b| a.last_receive_time_ticks.cmp(&b.last_receive_time_ticks).reverse());
}

impl<HostSystemImpl: HostSystem + ?Sized> Peer<HostSystemImpl> {
    /// Create a new peer.
    ///
    /// This only returns None if this_node_identity does not have its secrets or if some
    /// fatal error occurs performing key agreement between the two identities.
    pub(crate) fn new(this_node_identity: &Identity, id: Identity, time_ticks: i64) -> Option<Self> {
        this_node_identity.agree(&id).map(|static_secret| -> Self {
            Self {
                identity: id,
                v1_proto_static_secret: v1::SymmetricSecret::new(static_secret),
                paths: Mutex::new(Vec::with_capacity(4)),
                last_send_time_ticks: AtomicI64::new(NEVER_HAPPENED_TICKS),
                last_receive_time_ticks: AtomicI64::new(NEVER_HAPPENED_TICKS),
                last_forward_time_ticks: AtomicI64::new(NEVER_HAPPENED_TICKS),
                last_hello_reply_time_ticks: AtomicI64::new(NEVER_HAPPENED_TICKS),
                create_time_ticks: time_ticks,
                random_ticks_offset: random::xorshift64_random() as u32,
                message_id_counter: AtomicU64::new(random::xorshift64_random()),
                remote_node_info: RwLock::new(RemoteNodeInfo {
                    reported_local_endpoints: HashMap::new(),
                    remote_protocol_version: 0,
                    remote_version: (0, 0, 0),
                }),
            }
        })
    }

    /// Get the remote version of this peer: major, minor, revision.
    /// Returns None if it's not yet known.
    pub fn version(&self) -> Option<(u8, u8, u16)> {
        let rv = self.remote_node_info.read().unwrap().remote_version;
        if rv.0 != 0 || rv.1 != 0 || rv.2 != 0 {
            Some(rv)
        } else {
            None
        }
    }

    /// Get the remote protocol version of this peer or None if not yet known.
    pub fn protocol_version(&self) -> Option<u8> {
        let pv = self.remote_node_info.read().unwrap().remote_protocol_version;
        if pv != 0 {
            Some(pv)
        } else {
            None
        }
    }

    /// Get current best path or None if there are no direct paths to this peer.
    pub fn direct_path(&self) -> Option<Arc<Path<HostSystemImpl>>> {
        for p in self.paths.lock().unwrap().iter() {
            let pp = p.path.upgrade();
            if pp.is_some() {
                return pp;
            }
        }
        return None;
    }

    /// Get either the current best direct path or an indirect path via e.g. a root.
    pub fn path(&self, node: &Node<HostSystemImpl>) -> Option<Arc<Path<HostSystemImpl>>> {
        let direct_path = self.direct_path();
        if direct_path.is_some() {
            return direct_path;
        }
        if let Some(root) = node.best_root() {
            return root.direct_path();
        }
        return None;
    }

    fn learn_path(&self, host_system: &HostSystemImpl, new_path: &Arc<Path<HostSystemImpl>>, time_ticks: i64) {
        let mut paths = self.paths.lock().unwrap();

        match &new_path.endpoint {
            Endpoint::IpUdp(new_ip) => {
                // If this is an IpUdp endpoint, scan the existing paths and replace any that come from
                // the same IP address but a different port. This prevents the accumulation of duplicate
                // paths to the same peer over different ports.
                for pi in paths.iter_mut() {
                    if std::ptr::eq(pi.path.as_ptr(), new_path.as_ref()) {
                        return;
                    }
                    if let Some(p) = pi.path.upgrade() {
                        match &p.endpoint {
                            Endpoint::IpUdp(existing_ip) => {
                                if existing_ip.ip_bytes().eq(new_ip.ip_bytes()) {
                                    debug_event!(
                                        host_system,
                                        "[vl1] {} replacing path {} with {} (same IP, different port)",
                                        self.identity.address.to_string(),
                                        p.endpoint.to_string(),
                                        new_path.endpoint.to_string()
                                    );
                                    pi.path = Arc::downgrade(new_path);
                                    pi.last_receive_time_ticks = time_ticks;
                                    prioritize_paths(&mut paths);
                                    return;
                                }
                            }
                            _ => {}
                        }
                    }
                }
            }
            _ => {
                for pi in paths.iter() {
                    if std::ptr::eq(pi.path.as_ptr(), new_path.as_ref()) {
                        return;
                    }
                }
            }
        }

        // Learn new path if it's not a duplicate or should not replace an existing path.
        debug_event!(
            host_system,
            "[vl1] {} learned new path: {}",
            self.identity.address.to_string(),
            new_path.endpoint.to_string()
        );
        paths.push(PeerPath::<HostSystemImpl> {
            path: Arc::downgrade(new_path),
            last_receive_time_ticks: time_ticks,
        });
        prioritize_paths(&mut paths);
    }

    #[inline(always)]
    pub(crate) fn v1_proto_next_message_id(&self) -> MessageId {
        self.message_id_counter.fetch_add(1, Ordering::SeqCst)
    }

    /// Called every SERVICE_INTERVAL_MS by the background service loop in Node.
    pub(crate) fn service(&self, _: &HostSystemImpl, _: &Node<HostSystemImpl>, time_ticks: i64) -> bool {
        // Prune dead paths and sort in descending order of quality.
        {
            let mut paths = self.paths.lock().unwrap();
            paths.retain(|p| ((time_ticks - p.last_receive_time_ticks) < PEER_EXPIRATION_TIME) && (p.path.strong_count() > 0));
            if paths.capacity() > 16 {
                paths.shrink_to_fit();
            }
            prioritize_paths(&mut paths);
        }

        // Prune dead entries from the map of reported local endpoints (e.g. externally visible IPs).
        self.remote_node_info
            .write()
            .unwrap()
            .reported_local_endpoints
            .retain(|_, ts| (time_ticks - *ts) < PEER_EXPIRATION_TIME);
        (time_ticks - self.last_receive_time_ticks.load(Ordering::Relaxed).max(self.create_time_ticks)) < PEER_EXPIRATION_TIME
    }

    /// Send a prepared and encrypted packet using the V1 protocol with fragmentation if needed.
    fn v1_proto_internal_send(
        &self,
        host_system: &HostSystemImpl,
        endpoint: &Endpoint,
        local_socket: Option<&HostSystemImpl::LocalSocket>,
        local_interface: Option<&HostSystemImpl::LocalInterface>,
        max_fragment_size: usize,
        packet: PooledPacketBuffer,
    ) {
        let packet_size = packet.len();
        if packet_size > max_fragment_size {
            let bytes = packet.as_bytes();
            host_system.wire_send(endpoint, local_socket, local_interface, &bytes[0..UDP_DEFAULT_MTU], 0);
            let mut pos = UDP_DEFAULT_MTU;

            let overrun_size = (packet_size - UDP_DEFAULT_MTU) as u32;
            let fragment_count = (overrun_size / (UDP_DEFAULT_MTU - v1::FRAGMENT_HEADER_SIZE) as u32)
                + (((overrun_size % (UDP_DEFAULT_MTU - v1::FRAGMENT_HEADER_SIZE) as u32) != 0) as u32);
            debug_assert!(fragment_count <= v1::FRAGMENT_COUNT_MAX as u32);

            let mut header = v1::FragmentHeader {
                id: *packet.bytes_fixed_at(0).unwrap(),
                dest: *packet.bytes_fixed_at(v1::DESTINATION_INDEX).unwrap(),
                fragment_indicator: v1::FRAGMENT_INDICATOR,
                total_and_fragment_no: ((fragment_count + 1) << 4) as u8,
                reserved_hops: 0,
            };

            let mut chunk_size = (packet_size - pos).min(UDP_DEFAULT_MTU - v1::HEADER_SIZE);
            let mut tmp_buf: [u8; v1::SIZE_MAX] = unsafe { std::mem::MaybeUninit::uninit().assume_init() };
            loop {
                header.total_and_fragment_no += 1;
                let next_pos = pos + chunk_size;
                let fragment_size = v1::FRAGMENT_HEADER_SIZE + chunk_size;
                tmp_buf[..v1::FRAGMENT_HEADER_SIZE].copy_from_slice(header.as_bytes());
                tmp_buf[v1::FRAGMENT_HEADER_SIZE..fragment_size].copy_from_slice(&bytes[pos..next_pos]);
                host_system.wire_send(endpoint, local_socket, local_interface, &tmp_buf[..fragment_size], 0);
                pos = next_pos;
                if pos < packet_size {
                    chunk_size = (packet_size - pos).min(UDP_DEFAULT_MTU - v1::HEADER_SIZE);
                } else {
                    break;
                }
            }
        } else {
            host_system.wire_send(endpoint, local_socket, local_interface, packet.as_bytes(), 0);
        }
    }

    /// Send a packet to this peer, returning true on (potential) success.
    ///
    /// This will go directly if there is an active path, or otherwise indirectly
    /// via a root or some other route.
    pub(crate) fn send(
        &self,
        host_system: &HostSystemImpl,
        path: Option<&Arc<Path<HostSystemImpl>>>,
        node: &Node<HostSystemImpl>,
        time_ticks: i64,
        mut packet: PooledPacketBuffer,
    ) -> bool {
        let mut _path_arc = None;
        let path = if let Some(path) = path {
            path
        } else {
            _path_arc = self.path(node);
            if let Some(path) = _path_arc.as_ref() {
                path
            } else {
                return false;
            }
        };

        let max_fragment_size = path.endpoint.max_fragment_size();

        if self.remote_node_info.read().unwrap().remote_protocol_version >= 11 {
            let flags_cipher_hops = if packet.len() > max_fragment_size {
                v1::HEADER_FLAG_FRAGMENTED | v1::CIPHER_AES_GMAC_SIV
            } else {
                v1::CIPHER_AES_GMAC_SIV
            };

            let mut aes_gmac_siv = self.v1_proto_static_secret.aes_gmac_siv.get();
            aes_gmac_siv.encrypt_init(&self.v1_proto_next_message_id().to_be_bytes());
            aes_gmac_siv.encrypt_set_aad(&v1::get_packet_aad_bytes(
                self.identity.address,
                node.identity.address,
                flags_cipher_hops,
            ));
            let tag = if let Ok(payload) = packet.as_bytes_starting_at_mut(v1::HEADER_SIZE) {
                aes_gmac_siv.encrypt_first_pass(payload);
                aes_gmac_siv.encrypt_first_pass_finish();
                aes_gmac_siv.encrypt_second_pass_in_place(payload);
                aes_gmac_siv.encrypt_second_pass_finish()
            } else {
                return false;
            };

            let header = packet.struct_mut_at::<v1::PacketHeader>(0).unwrap();
            header.id.copy_from_slice(&tag[0..8]);
            header.dest = self.identity.address.to_bytes();
            header.src = node.identity.address.to_bytes();
            header.flags_cipher_hops = flags_cipher_hops;
            header.mac.copy_from_slice(&tag[8..16]);
        } else {
            let packet_len = packet.len();
            let flags_cipher_hops = if packet.len() > max_fragment_size {
                v1::HEADER_FLAG_FRAGMENTED | v1::CIPHER_SALSA2012_POLY1305
            } else {
                v1::CIPHER_SALSA2012_POLY1305
            };

            let (mut salsa, poly1305_otk) = v1_proto_salsa_poly_create(
                &self.v1_proto_static_secret,
                {
                    let header = packet.struct_mut_at::<v1::PacketHeader>(0).unwrap();
                    header.id = self.v1_proto_next_message_id().to_be_bytes();
                    header.dest = self.identity.address.to_bytes();
                    header.src = node.identity.address.to_bytes();
                    header.flags_cipher_hops = flags_cipher_hops;
                    header
                },
                packet_len,
            );

            let tag = if let Ok(payload) = packet.as_bytes_starting_at_mut(v1::HEADER_SIZE) {
                salsa.crypt_in_place(payload);
                poly1305::compute(&poly1305_otk, payload)
            } else {
                return false;
            };
            packet.as_bytes_mut()[v1::MAC_FIELD_INDEX..(v1::MAC_FIELD_INDEX + 8)].copy_from_slice(&tag[0..8]);
        }

        self.v1_proto_internal_send(
            host_system,
            &path.endpoint,
            Some(&path.local_socket),
            Some(&path.local_interface),
            max_fragment_size,
            packet,
        );

        self.last_send_time_ticks.store(time_ticks, Ordering::Relaxed);

        return true;
    }

    /// Send a HELLO to this peer.
    ///
    /// If explicit_endpoint is not None the packet will be sent directly to this endpoint.
    /// Otherwise it will be sent via the best direct or indirect path known.
    ///
    /// Unlike other messages HELLO is sent partially in the clear and always with the long-lived
    /// static identity key. Authentication in old versions is via Poly1305 and in new versions
    /// via HMAC-SHA512.
    pub(crate) fn send_hello(
        &self,
        host_system: &HostSystemImpl,
        node: &Node<HostSystemImpl>,
        explicit_endpoint: Option<&Endpoint>,
    ) -> bool {
        let mut path = None;
        let destination = if let Some(explicit_endpoint) = explicit_endpoint {
            explicit_endpoint
        } else {
            if let Some(p) = self.path(node) {
                let _ = path.insert(p);
                &path.as_ref().unwrap().endpoint
            } else {
                return false;
            }
        };

        let max_fragment_size = destination.max_fragment_size();
        let time_ticks = host_system.time_ticks();

        let mut packet = host_system.get_buffer();
        {
            let message_id = self.v1_proto_next_message_id();

            {
                let f: &mut (v1::PacketHeader, v1::message_component_structs::HelloFixedHeaderFields) =
                    packet.append_struct_get_mut().unwrap();
                f.0.id = message_id.to_ne_bytes();
                f.0.dest = self.identity.address.to_bytes();
                f.0.src = node.identity.address.to_bytes();
                f.0.flags_cipher_hops = v1::CIPHER_NOCRYPT_POLY1305;
                f.1.verb = verbs::VL1_HELLO | v1::VERB_FLAG_EXTENDED_AUTHENTICATION;
                f.1.version_proto = PROTOCOL_VERSION;
                f.1.version_major = VERSION_MAJOR;
                f.1.version_minor = VERSION_MINOR;
                f.1.version_revision = VERSION_REVISION.to_be_bytes();
                f.1.timestamp = (time_ticks as u64).wrapping_add(self.random_ticks_offset as u64).to_be_bytes();
            }

            debug_assert_eq!(packet.len(), 41);
            assert!(node.identity.write_public(packet.as_mut(), self.identity.p384.is_none()).is_ok());

            let (_, poly1305_key) = v1_proto_salsa_poly_create(
                &self.v1_proto_static_secret,
                packet.struct_at::<v1::PacketHeader>(0).unwrap(),
                packet.len(),
            );
            let mac = poly1305::compute(&poly1305_key, packet.as_bytes_starting_at(v1::HEADER_SIZE).unwrap());
            packet.as_bytes_mut()[v1::MAC_FIELD_INDEX..v1::MAC_FIELD_INDEX + 8].copy_from_slice(&mac[0..8]);

            self.last_send_time_ticks.store(time_ticks, Ordering::Relaxed);

            debug_event!(
                host_system,
                "HELLO -> {} @ {} ({} bytes)",
                self.identity.address.to_string(),
                destination.to_string(),
                packet.len()
            );
        }

        if let Some(p) = path.as_ref() {
            self.v1_proto_internal_send(
                host_system,
                destination,
                Some(&p.local_socket),
                Some(&p.local_interface),
                max_fragment_size,
                packet,
            );
            p.log_send_anything(time_ticks);
        } else {
            self.v1_proto_internal_send(host_system, destination, None, None, max_fragment_size, packet);
        }

        return true;
    }

    /// Receive, decrypt, authenticate, and process an incoming packet from this peer.
    ///
    /// If the packet comes in multiple fragments, the fragments slice should contain all
    /// those fragments after the main packet header and first chunk.
    ///
    /// This returns true if the packet decrypted and passed authentication.
    pub(crate) fn v1_proto_receive<InnerProtocolImpl: InnerProtocol + ?Sized>(
        self: &Arc<Self>,
        node: &Node<HostSystemImpl>,
        host_system: &HostSystemImpl,
        inner: &InnerProtocolImpl,
        time_ticks: i64,
        source_path: &Arc<Path<HostSystemImpl>>,
        packet_header: &v1::PacketHeader,
        frag0: &PacketBuffer,
        fragments: &[Option<PooledPacketBuffer>],
    ) -> PacketHandlerResult {
        if let Ok(packet_frag0_payload_bytes) = frag0.as_bytes_starting_at(v1::VERB_INDEX) {
            let mut payload = PacketBuffer::new();

            let message_id = if let Some(message_id2) = v1_proto_try_aead_decrypt(
                &self.v1_proto_static_secret,
                packet_frag0_payload_bytes,
                packet_header,
                fragments,
                &mut payload,
            ) {
                // Decryption successful with static secret.
                message_id2
            } else {
                // Packet failed to decrypt using either ephemeral or permament key, reject.
                debug_event!(
                    host_system,
                    "[vl1] #{:0>16x} failed authentication",
                    u64::from_be_bytes(packet_header.id)
                );
                return PacketHandlerResult::Error;
            };

            if let Ok(mut verb) = payload.u8_at(0) {
                if (verb & v1::VERB_FLAG_COMPRESSED) != 0 {
                    let mut decompressed_payload = [0u8; v1::SIZE_MAX];
                    decompressed_payload[0] = verb;
                    if let Ok(dlen) = lz4_flex::block::decompress_into(&payload.as_bytes()[1..], &mut decompressed_payload[1..]) {
                        payload.set_to(&decompressed_payload[..(dlen + 1)]);
                    } else {
                        return PacketHandlerResult::Error;
                    }
                }

                // ---------------------------------------------------------------
                // If we made it here it decrypted and passed authentication.
                // ---------------------------------------------------------------

                self.last_receive_time_ticks.store(time_ticks, Ordering::Relaxed);

                let mut path_is_known = false;
                for p in self.paths.lock().unwrap().iter_mut() {
                    if std::ptr::eq(p.path.as_ptr(), source_path.as_ref()) {
                        p.last_receive_time_ticks = time_ticks;
                        path_is_known = true;
                        break;
                    }
                }

                verb &= v1::VERB_MASK; // mask off flags
                debug_event!(
                    host_system,
                    "[vl1] #{:0>16x} decrypted and authenticated, verb: {} ({:0>2x})",
                    u64::from_be_bytes(packet_header.id),
                    verbs::name(verb),
                    verb as u32
                );

                return match verb {
                    verbs::VL1_NOP => PacketHandlerResult::Ok,
                    verbs::VL1_HELLO => self.handle_incoming_hello(
                        host_system,
                        inner,
                        node,
                        time_ticks,
                        message_id,
                        source_path,
                        packet_header.hops(),
                        &payload,
                    ),
                    verbs::VL1_ERROR => self.handle_incoming_error(host_system, inner, node, time_ticks, source_path, message_id, &payload),
                    verbs::VL1_OK => self.handle_incoming_ok(
                        host_system,
                        inner,
                        node,
                        time_ticks,
                        source_path,
                        message_id,
                        packet_header.hops(),
                        path_is_known,
                        &payload,
                    ),
                    verbs::VL1_WHOIS => self.handle_incoming_whois(host_system, inner, node, time_ticks, message_id, &payload),
                    verbs::VL1_RENDEZVOUS => {
                        self.handle_incoming_rendezvous(host_system, node, time_ticks, message_id, source_path, &payload)
                    }
                    verbs::VL1_ECHO => self.handle_incoming_echo(host_system, inner, node, time_ticks, message_id, &payload),
                    verbs::VL1_PUSH_DIRECT_PATHS => {
                        self.handle_incoming_push_direct_paths(host_system, node, time_ticks, source_path, &payload)
                    }
                    verbs::VL1_USER_MESSAGE => self.handle_incoming_user_message(host_system, node, time_ticks, source_path, &payload),
                    _ => inner.handle_packet(node, self, &source_path, message_id, verb, &payload),
                };
            }
        }

        return PacketHandlerResult::Error;
    }

    fn handle_incoming_hello<InnerProtocolImpl: InnerProtocol + ?Sized>(
        &self,
        host_system: &HostSystemImpl,
        inner: &InnerProtocolImpl,
        node: &Node<HostSystemImpl>,
        time_ticks: i64,
        message_id: MessageId,
        source_path: &Arc<Path<HostSystemImpl>>,
        _hops: u8,
        payload: &PacketBuffer,
    ) -> PacketHandlerResult {
        if !(inner.should_communicate_with(&self.identity) || node.this_node_is_root() || node.is_peer_root(self)) {
            debug_event!(
                host_system,
                "[vl1] dropping HELLO from {} due to lack of trust relationship",
                self.identity.address.to_string()
            );
            return PacketHandlerResult::Ok; // packet wasn't invalid, just ignored
        }

        let mut cursor = 0;
        if let Ok(hello_fixed_headers) = payload.read_struct::<v1::message_component_structs::HelloFixedHeaderFields>(&mut cursor) {
            if let Ok(identity) = Identity::unmarshal(payload, &mut cursor) {
                if identity.eq(&self.identity) {
                    {
                        let mut remote_node_info = self.remote_node_info.write().unwrap();
                        remote_node_info.remote_protocol_version = hello_fixed_headers.version_proto;
                        remote_node_info.remote_version = (
                            hello_fixed_headers.version_major,
                            hello_fixed_headers.version_minor,
                            u16::from_be_bytes(hello_fixed_headers.version_revision),
                        );
                    }

                    let mut packet = host_system.get_buffer();
                    packet.set_size(v1::HEADER_SIZE);
                    {
                        let f: &mut (
                            v1::message_component_structs::OkHeader,
                            v1::message_component_structs::OkHelloFixedHeaderFields,
                        ) = packet.append_struct_get_mut().unwrap();
                        f.0.verb = verbs::VL1_OK;
                        f.0.in_re_verb = verbs::VL1_HELLO;
                        f.0.in_re_message_id = message_id.to_ne_bytes();
                        f.1.timestamp_echo = hello_fixed_headers.timestamp;
                        f.1.version_proto = PROTOCOL_VERSION;
                        f.1.version_major = VERSION_MAJOR;
                        f.1.version_minor = VERSION_MINOR;
                        f.1.version_revision = VERSION_REVISION.to_be_bytes();
                    }

                    self.send(host_system, Some(source_path), node, time_ticks, packet);
                    return PacketHandlerResult::Ok;
                }
            }
        }

        return PacketHandlerResult::Error;
    }

    fn handle_incoming_error<InnerProtocolImpl: InnerProtocol + ?Sized>(
        self: &Arc<Self>,
        _: &HostSystemImpl,
        inner: &InnerProtocolImpl,
        node: &Node<HostSystemImpl>,
        _: i64,
        source_path: &Arc<Path<HostSystemImpl>>,
        message_id: u64,
        payload: &PacketBuffer,
    ) -> PacketHandlerResult {
        let mut cursor = 0;
        if let Ok(error_header) = payload.read_struct::<v1::message_component_structs::ErrorHeader>(&mut cursor) {
            let in_re_message_id: MessageId = u64::from_be_bytes(error_header.in_re_message_id);
            if self.message_id_counter.load(Ordering::Relaxed).wrapping_sub(in_re_message_id) <= PACKET_RESPONSE_COUNTER_DELTA_MAX {
                match error_header.in_re_verb {
                    _ => {
                        return inner.handle_error(
                            node,
                            self,
                            &source_path,
                            message_id,
                            error_header.in_re_verb,
                            in_re_message_id,
                            error_header.error_code,
                            payload,
                            &mut cursor,
                        );
                    }
                }
            }
        }
        return PacketHandlerResult::Error;
    }

    fn handle_incoming_ok<InnerProtocolImpl: InnerProtocol + ?Sized>(
        self: &Arc<Self>,
        host_system: &HostSystemImpl,
        inner: &InnerProtocolImpl,
        node: &Node<HostSystemImpl>,
        time_ticks: i64,
        source_path: &Arc<Path<HostSystemImpl>>,
        message_id: u64,
        hops: u8,
        path_is_known: bool,
        payload: &PacketBuffer,
    ) -> PacketHandlerResult {
        let mut cursor = 0;
        if let Ok(ok_header) = payload.read_struct::<v1::message_component_structs::OkHeader>(&mut cursor) {
            let in_re_message_id: MessageId = u64::from_ne_bytes(ok_header.in_re_message_id);
            if self.message_id_counter.load(Ordering::Relaxed).wrapping_sub(in_re_message_id) <= PACKET_RESPONSE_COUNTER_DELTA_MAX {
                match ok_header.in_re_verb {
                    verbs::VL1_HELLO => {
                        if let Ok(_ok_hello_fixed_header_fields) =
                            payload.read_struct::<v1::message_component_structs::OkHelloFixedHeaderFields>(&mut cursor)
                        {
                            if hops == 0 {
                                debug_event!(host_system, "[vl1] {} OK(HELLO)", self.identity.address.to_string(),);
                                if let Ok(reported_endpoint) = Endpoint::unmarshal(&payload, &mut cursor) {
                                    #[cfg(debug_assertions)]
                                    let reported_endpoint2 = reported_endpoint.clone();
                                    if self
                                        .remote_node_info
                                        .write()
                                        .unwrap()
                                        .reported_local_endpoints
                                        .insert(reported_endpoint, time_ticks)
                                        .is_none()
                                    {
                                        #[cfg(debug_assertions)]
                                        debug_event!(
                                            host_system,
                                            "[vl1] {} reported new remote perspective, local endpoint: {}",
                                            self.identity.address.to_string(),
                                            reported_endpoint2.to_string()
                                        );
                                    }
                                }
                            }

                            if hops == 0 && !path_is_known {
                                self.learn_path(host_system, source_path, time_ticks);
                            }

                            self.last_hello_reply_time_ticks.store(time_ticks, Ordering::Relaxed);
                        }
                    }

                    verbs::VL1_WHOIS => {
                        if node.is_peer_root(self) {
                            while cursor < payload.len() {
                                let r = Identity::unmarshal(payload, &mut cursor);
                                if let Ok(received_identity) = r {
                                    debug_event!(
                                        host_system,
                                        "[vl1] {} OK(WHOIS): new identity: {}",
                                        self.identity.address.to_string(),
                                        received_identity.to_string()
                                    );
                                    node.handle_incoming_identity(host_system, inner, received_identity, time_ticks, true);
                                } else {
                                    debug_event!(
                                        host_system,
                                        "[vl1] {} OK(WHOIS): bad identity: {}",
                                        self.identity.address.to_string(),
                                        r.err().unwrap().to_string()
                                    );
                                    return PacketHandlerResult::Error;
                                }
                            }
                        } else {
                            return PacketHandlerResult::Ok; // not invalid, just ignored
                        }
                    }

                    _ => {
                        return inner.handle_ok(
                            node,
                            self,
                            &source_path,
                            message_id,
                            ok_header.in_re_verb,
                            in_re_message_id,
                            payload,
                            &mut cursor,
                        );
                    }
                }
            }
        }
        return PacketHandlerResult::Error;
    }

    fn handle_incoming_whois<InnerProtocolImpl: InnerProtocol + ?Sized>(
        self: &Arc<Self>,
        host_system: &HostSystemImpl,
        inner: &InnerProtocolImpl,
        node: &Node<HostSystemImpl>,
        time_ticks: i64,
        message_id: MessageId,
        payload: &PacketBuffer,
    ) -> PacketHandlerResult {
        if node.this_node_is_root() || inner.should_communicate_with(&self.identity) {
            let init_packet = |packet: &mut PacketBuffer| {
                packet.set_size(v1::HEADER_SIZE);
                let mut f: &mut v1::message_component_structs::OkHeader = packet.append_struct_get_mut().unwrap();
                f.verb = verbs::VL1_OK;
                f.in_re_verb = verbs::VL1_WHOIS;
                f.in_re_message_id = message_id.to_ne_bytes();
            };

            let mut packet = host_system.get_buffer();
            init_packet(&mut packet);

            let mut addresses = payload.as_bytes();
            loop {
                if addresses.len() >= ADDRESS_SIZE {
                    if let Some(zt_address) = Address::from_bytes(&addresses[..ADDRESS_SIZE]) {
                        if let Some(peer) = node.peer(zt_address) {
                            if (packet.capacity() - packet.len()) < Identity::MAX_MARSHAL_SIZE {
                                self.send(host_system, None, node, time_ticks, packet);
                                packet = host_system.get_buffer();
                                init_packet(&mut packet);
                            }
                            if !peer.identity.write_public(packet.as_mut(), self.identity.p384.is_none()).is_ok() {
                                break;
                            }
                        }
                    }
                    addresses = &addresses[ADDRESS_SIZE..];
                } else {
                    break;
                }
            }

            self.send(host_system, None, node, time_ticks, packet);
        }
        return PacketHandlerResult::Ok;
    }

    fn handle_incoming_rendezvous(
        self: &Arc<Self>,
        host_system: &HostSystemImpl,
        node: &Node<HostSystemImpl>,
        time_ticks: i64,
        message_id: MessageId,
        source_path: &Arc<Path<HostSystemImpl>>,
        payload: &PacketBuffer,
    ) -> PacketHandlerResult {
        if node.is_peer_root(self) {}
        return PacketHandlerResult::Ok;
    }

    fn handle_incoming_echo<InnerProtocolImpl: InnerProtocol + ?Sized>(
        &self,
        host_system: &HostSystemImpl,
        inner: &InnerProtocolImpl,
        node: &Node<HostSystemImpl>,
        time_ticks: i64,
        message_id: MessageId,
        payload: &PacketBuffer,
    ) -> PacketHandlerResult {
        if inner.should_communicate_with(&self.identity) || node.is_peer_root(self) {
            let mut packet = host_system.get_buffer();
            packet.set_size(v1::HEADER_SIZE);
            {
                let mut f: &mut v1::message_component_structs::OkHeader = packet.append_struct_get_mut().unwrap();
                f.verb = verbs::VL1_OK;
                f.in_re_verb = verbs::VL1_ECHO;
                f.in_re_message_id = message_id.to_ne_bytes();
            }
            if packet.append_bytes(payload.as_bytes()).is_ok() {
                self.send(host_system, None, node, time_ticks, packet);
            }
        } else {
            debug_event!(
                host_system,
                "[vl1] dropping ECHO from {} due to lack of trust relationship",
                self.identity.address.to_string()
            );
        }
        return PacketHandlerResult::Ok;
    }

    fn handle_incoming_push_direct_paths(
        self: &Arc<Self>,
        host_system: &HostSystemImpl,
        node: &Node<HostSystemImpl>,
        time_ticks: i64,
        source_path: &Arc<Path<HostSystemImpl>>,
        payload: &PacketBuffer,
    ) -> PacketHandlerResult {
        PacketHandlerResult::Ok
    }

    fn handle_incoming_user_message(
        self: &Arc<Self>,
        host_system: &HostSystemImpl,
        node: &Node<HostSystemImpl>,
        time_ticks: i64,
        source_path: &Arc<Path<HostSystemImpl>>,
        payload: &PacketBuffer,
    ) -> PacketHandlerResult {
        PacketHandlerResult::Ok
    }
}

impl<HostSystemImpl: HostSystem + ?Sized> Hash for Peer<HostSystemImpl> {
    #[inline(always)]
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        state.write_u64(self.identity.address.into());
    }
}

impl<HostSystemImpl: HostSystem + ?Sized> PartialEq for Peer<HostSystemImpl> {
    #[inline(always)]
    fn eq(&self, other: &Self) -> bool {
        self.identity.fingerprint.eq(&other.identity.fingerprint)
    }
}

impl<HostSystemImpl: HostSystem + ?Sized> Eq for Peer<HostSystemImpl> {}

fn v1_proto_try_aead_decrypt(
    secret: &v1::SymmetricSecret,
    packet_frag0_payload_bytes: &[u8],
    packet_header: &v1::PacketHeader,
    fragments: &[Option<PooledPacketBuffer>],
    payload: &mut PacketBuffer,
) -> Option<MessageId> {
    let cipher = packet_header.cipher();
    match cipher {
        v1::CIPHER_NOCRYPT_POLY1305 | v1::CIPHER_SALSA2012_POLY1305 => {
            let _ = payload.append_bytes(packet_frag0_payload_bytes);
            for f in fragments.iter() {
                if let Some(f) = f.as_ref() {
                    if let Ok(f) = f.as_bytes_starting_at(v1::FRAGMENT_HEADER_SIZE) {
                        let _ = payload.append_bytes(f);
                    }
                }
            }

            let (mut salsa, poly1305_key) = v1_proto_salsa_poly_create(secret, packet_header, payload.len() + v1::HEADER_SIZE);
            let mac = poly1305::compute(&poly1305_key, &payload.as_bytes());
            if mac[0..8].eq(&packet_header.mac) {
                let message_id = u64::from_be_bytes(packet_header.id);
                if cipher == v1::CIPHER_SALSA2012_POLY1305 {
                    salsa.crypt_in_place(payload.as_bytes_mut());
                    Some(message_id)
                } else if (payload.u8_at(0).unwrap_or(0) & v1::VERB_MASK) == verbs::VL1_HELLO {
                    Some(message_id)
                } else {
                    // SECURITY: fail if there is no encryption and the message is not HELLO. No other types are allowed
                    // to be sent without full packet encryption.
                    None
                }
            } else {
                None
            }
        }

        v1::CIPHER_AES_GMAC_SIV => {
            let mut aes_gmac_siv = secret.aes_gmac_siv.get();
            aes_gmac_siv.decrypt_init(&packet_header.aes_gmac_siv_tag());

            aes_gmac_siv.decrypt_set_aad(&[
                packet_header.dest[0],
                packet_header.dest[1],
                packet_header.dest[2],
                packet_header.dest[3],
                packet_header.dest[4],
                packet_header.src[0],
                packet_header.src[1],
                packet_header.src[2],
                packet_header.src[3],
                packet_header.src[4],
                packet_header.flags_cipher_hops & v1::FLAGS_FIELD_MASK_HIDE_HOPS,
            ]);

            if let Ok(b) = payload.append_bytes_get_mut(packet_frag0_payload_bytes.len()) {
                aes_gmac_siv.decrypt(packet_frag0_payload_bytes, b);
            }
            for f in fragments.iter() {
                if let Some(f) = f.as_ref() {
                    if let Ok(f) = f.as_bytes_starting_at(v1::FRAGMENT_HEADER_SIZE) {
                        if let Ok(b) = payload.append_bytes_get_mut(f.len()) {
                            aes_gmac_siv.decrypt(f, b);
                        }
                    }
                }
            }

            if let Some(tag) = aes_gmac_siv.decrypt_finish() {
                // AES-GMAC-SIV encrypts the packet ID too as part of its computation of a single
                // opaque 128-bit tag, so to get the original packet ID we have to grab it from the
                // decrypted tag.
                Some(u64::from_be_bytes(*array_range::<u8, 16, 0, 8>(tag)))
            } else {
                None
            }
        }

        _ => None,
    }
}

fn v1_proto_salsa_poly_create(secret: &v1::SymmetricSecret, header: &v1::PacketHeader, packet_size: usize) -> (Salsa<12>, [u8; 32]) {
    // Create a per-packet key from the IV, source, destination, and packet size.
    let mut key: Secret<32> = secret.key.first_n_clone();
    let hb = header.as_bytes();
    for i in 0..18 {
        key.0[i] ^= hb[i];
    }
    key.0[18] ^= header.flags_cipher_hops & v1::FLAGS_FIELD_MASK_HIDE_HOPS;
    key.0[19] ^= packet_size as u8;
    key.0[20] ^= packet_size.wrapping_shr(8) as u8;

    let mut salsa = Salsa::<12>::new(&key.0, &header.id);
    let mut poly1305_key = [0_u8; 32];
    salsa.crypt_in_place(&mut poly1305_key);
    (salsa, poly1305_key)
}
