// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::collections::HashMap;
use std::hash::{Hash, Hasher};
use std::sync::atomic::{AtomicI64, AtomicU64, Ordering};
use std::sync::{Arc, Weak};

use parking_lot::{Mutex, RwLock};

use zerotier_core_crypto::random::next_u64_secure;
use zerotier_core_crypto::salsa::Salsa;
use zerotier_core_crypto::secret::Secret;

use crate::util::buffer::BufferReader;
use crate::util::byte_array_range;
use crate::util::canonicalobject::CanonicalObject;
use crate::util::debug_event;
use crate::util::marshalable::Marshalable;
use crate::vl1::address::Address;
use crate::vl1::careof::CareOf;
use crate::vl1::node::*;
use crate::vl1::protocol::*;
use crate::vl1::rootset::RootSet;
use crate::vl1::symmetricsecret::SymmetricSecret;
use crate::vl1::{Dictionary, Endpoint, Identity, Path};
use crate::{VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION};

pub(crate) const SERVICE_INTERVAL_MS: i64 = security_constants::EPHEMERAL_SECRET_REKEY_AFTER_TIME / 10;

struct PeerPath<SI: SystemInterface> {
    path: Weak<Path<SI>>,
    canonical_instance_id: usize,
    last_receive_time_ticks: i64,
}

struct RemoteNodeInfo {
    remote_instance_id: [u8; 16],
    reported_local_endpoints: HashMap<Endpoint, i64>,
    care_of: Option<CareOf>,
    remote_version: u64,
    remote_protocol_version: u8,
}

/// A remote peer known to this node.
///
/// Equality and hashing is implemented in terms of the identity.
pub struct Peer<SI: SystemInterface> {
    canonical: CanonicalObject,

    // This peer's identity.
    pub identity: Identity,

    // Static shared secret computed from agreement with identity.
    identity_symmetric_key: SymmetricSecret,

    // Paths sorted in descending order of quality / preference.
    paths: Mutex<Vec<PeerPath<SI>>>,

    // Statistics and times of events.
    pub(crate) last_send_time_ticks: AtomicI64,
    pub(crate) last_receive_time_ticks: AtomicI64,
    pub(crate) last_hello_reply_time_ticks: AtomicI64,
    pub(crate) last_forward_time_ticks: AtomicI64,
    pub(crate) last_incoming_message_id: AtomicU64,
    pub(crate) create_time_ticks: i64,

    // A random offset added to ticks sent to this peer to avoid disclosing the actual tick count.
    random_ticks_offset: u64,

    // Counter for assigning sequential message IDs.
    message_id_counter: AtomicU64,

    // Other information reported by remote node.
    remote_node_info: RwLock<RemoteNodeInfo>,
}

/// Attempt AEAD packet encryption and MAC validation. Returns message ID on success.
fn try_aead_decrypt(
    secret: &SymmetricSecret,
    packet_frag0_payload_bytes: &[u8],
    packet_header: &PacketHeader,
    fragments: &[Option<PooledPacketBuffer>],
    payload: &mut PacketBuffer,
) -> Option<MessageId> {
    let cipher = packet_header.cipher();
    match cipher {
        security_constants::CIPHER_NOCRYPT_POLY1305 | security_constants::CIPHER_SALSA2012_POLY1305 => {
            let _ = payload.append_bytes(packet_frag0_payload_bytes);
            for f in fragments.iter() {
                if let Some(f) = f.as_ref() {
                    if let Ok(f) = f.as_bytes_starting_at(packet_constants::FRAGMENT_HEADER_SIZE) {
                        let _ = payload.append_bytes(f);
                    }
                }
            }

            let (mut salsa, poly1305_key) = salsa_poly_create(secret, packet_header, payload.len() + packet_constants::HEADER_SIZE);
            let mac = zerotier_core_crypto::poly1305::compute(&poly1305_key, &payload.as_bytes());
            if mac[0..8].eq(&packet_header.mac) {
                let message_id = u64::from_ne_bytes(packet_header.id);
                if cipher == security_constants::CIPHER_SALSA2012_POLY1305 {
                    salsa.crypt_in_place(payload.as_bytes_mut());
                    Some(message_id)
                } else if (payload.u8_at(0).unwrap_or(0) & packet_constants::VERB_MASK) == verbs::VL1_HELLO {
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

        security_constants::CIPHER_AES_GMAC_SIV => {
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
                packet_header.flags_cipher_hops & packet_constants::FLAGS_FIELD_MASK_HIDE_HOPS,
            ]);

            if let Ok(b) = payload.append_bytes_get_mut(packet_frag0_payload_bytes.len()) {
                aes_gmac_siv.decrypt(packet_frag0_payload_bytes, b);
            }
            for f in fragments.iter() {
                if let Some(f) = f.as_ref() {
                    if let Ok(f) = f.as_bytes_starting_at(packet_constants::FRAGMENT_HEADER_SIZE) {
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
                Some(u64::from_ne_bytes(*byte_array_range::<16, 0, 8>(tag)))
            } else {
                None
            }
        }

        _ => None,
    }
}

/// Create initialized instances of Salsa20/12 and Poly1305 for a packet.
/// (Note that this is a legacy cipher suite.)
fn salsa_poly_create(secret: &SymmetricSecret, header: &PacketHeader, packet_size: usize) -> (Salsa<12>, [u8; 32]) {
    // Create a per-packet key from the IV, source, destination, and packet size.
    let mut key: Secret<32> = secret.key.first_n();
    let hb = header.as_bytes();
    for i in 0..18 {
        key.0[i] ^= hb[i];
    }
    key.0[18] ^= header.flags_cipher_hops & packet_constants::FLAGS_FIELD_MASK_HIDE_HOPS;
    key.0[19] ^= packet_size as u8;
    key.0[20] ^= packet_size.wrapping_shr(8) as u8;

    let mut salsa = Salsa::<12>::new(&key.0, &header.id);
    let mut poly1305_key = [0_u8; 32];
    salsa.crypt_in_place(&mut poly1305_key);
    (salsa, poly1305_key)
}

/// Sort a list of paths by quality or priority, with best paths first.
fn prioritize_paths<SI: SystemInterface>(paths: &mut Vec<PeerPath<SI>>) {
    paths.sort_unstable_by(|a, b| a.last_receive_time_ticks.cmp(&b.last_receive_time_ticks).reverse());
}

impl<SI: SystemInterface> Peer<SI> {
    /// Create a new peer.
    ///
    /// This only returns None if this_node_identity does not have its secrets or if some
    /// fatal error occurs performing key agreement between the two identities.
    pub(crate) fn new(this_node_identity: &Identity, id: Identity, time_ticks: i64) -> Option<Peer<SI>> {
        this_node_identity.agree(&id).map(|static_secret| -> Self {
            Self {
                canonical: CanonicalObject::new(),
                identity: id,
                identity_symmetric_key: SymmetricSecret::new(static_secret),
                paths: Mutex::new(Vec::with_capacity(4)),
                last_send_time_ticks: AtomicI64::new(crate::util::NEVER_HAPPENED_TICKS),
                last_receive_time_ticks: AtomicI64::new(crate::util::NEVER_HAPPENED_TICKS),
                last_forward_time_ticks: AtomicI64::new(crate::util::NEVER_HAPPENED_TICKS),
                last_hello_reply_time_ticks: AtomicI64::new(crate::util::NEVER_HAPPENED_TICKS),
                last_incoming_message_id: AtomicU64::new(0),
                create_time_ticks: time_ticks,
                random_ticks_offset: next_u64_secure(),
                message_id_counter: AtomicU64::new(next_u64_secure()),
                remote_node_info: RwLock::new(RemoteNodeInfo {
                    remote_instance_id: [0_u8; 16],
                    reported_local_endpoints: HashMap::new(),
                    care_of: None,
                    remote_version: 0,
                    remote_protocol_version: 0,
                }),
            }
        })
    }

    /// Get the remote version of this peer: major, minor, revision, and build.
    /// Returns None if it's not yet known.
    pub fn version(&self) -> Option<[u16; 4]> {
        let rv = self.remote_node_info.read().remote_version;
        if rv != 0 {
            Some([rv.wrapping_shr(48) as u16, rv.wrapping_shr(32) as u16, rv.wrapping_shr(16) as u16, rv as u16])
        } else {
            None
        }
    }

    /// Get the remote protocol version of this peer or None if not yet known.
    pub fn protocol_version(&self) -> Option<u8> {
        let pv = self.remote_node_info.read().remote_protocol_version;
        if pv != 0 {
            Some(pv)
        } else {
            None
        }
    }

    /// Get the next message ID for sending a message to this peer.
    #[inline(always)]
    pub(crate) fn next_message_id(&self) -> MessageId {
        self.message_id_counter.fetch_add(1, Ordering::SeqCst)
    }

    /// Get current best path or None if there are no direct paths to this peer.
    pub fn direct_path(&self) -> Option<Arc<Path<SI>>> {
        for p in self.paths.lock().iter() {
            let pp = p.path.upgrade();
            if pp.is_some() {
                return pp;
            }
        }
        return None;
    }

    /// Get either the current best direct path or an indirect path via e.g. a root.
    pub fn path(&self, node: &Node<SI>) -> Option<Arc<Path<SI>>> {
        let direct_path = self.direct_path();
        if direct_path.is_some() {
            return direct_path;
        }
        if let Some(root) = node.best_root() {
            return root.direct_path();
        }
        return None;
    }

    pub(crate) fn learn_path(&self, si: &SI, new_path: &Arc<Path<SI>>, time_ticks: i64) {
        let mut paths = self.paths.lock();

        match &new_path.endpoint {
            Endpoint::IpUdp(new_ip) => {
                // If this is an IpUdp endpoint, scan the existing paths and replace any that come from
                // the same IP address but a different port. This prevents the accumulation of duplicate
                // paths to the same peer over different ports.
                for pi in paths.iter_mut() {
                    if pi.canonical_instance_id == new_path.canonical.canonical_instance_id() {
                        return;
                    }
                    if let Some(p) = pi.path.upgrade() {
                        match &p.endpoint {
                            Endpoint::IpUdp(existing_ip) => {
                                if existing_ip.ip_bytes().eq(new_ip.ip_bytes()) {
                                    debug_event!(
                                        si,
                                        "[vl1] {} replacing path {} with {} (same IP, different port)",
                                        self.identity.address.to_string(),
                                        p.endpoint.to_string(),
                                        new_path.endpoint.to_string()
                                    );
                                    pi.path = Arc::downgrade(new_path);
                                    pi.canonical_instance_id = new_path.canonical.canonical_instance_id();
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
                    if pi.canonical_instance_id == new_path.canonical.canonical_instance_id() {
                        return;
                    }
                }
            }
        }

        // Learn new path if it's not a duplicate or should not replace an existing path.
        debug_event!(si, "[vl1] {} learned new path: {}", self.identity.address.to_string(), new_path.endpoint.to_string());
        paths.push(PeerPath::<SI> {
            path: Arc::downgrade(new_path),
            canonical_instance_id: new_path.canonical.canonical_instance_id(),
            last_receive_time_ticks: time_ticks,
        });
        prioritize_paths(&mut paths);
    }

    /// Called every SERVICE_INTERVAL_MS by the background service loop in Node.
    pub(crate) fn service(&self, _: &SI, _: &Node<SI>, time_ticks: i64) -> bool {
        {
            let mut paths = self.paths.lock();
            paths.retain(|p| ((time_ticks - p.last_receive_time_ticks) < PEER_EXPIRATION_TIME) && (p.path.strong_count() > 0));
            prioritize_paths(&mut paths);
        }
        self.remote_node_info.write().reported_local_endpoints.retain(|_, ts| (time_ticks - *ts) < PEER_EXPIRATION_TIME);
        (time_ticks - self.last_receive_time_ticks.load(Ordering::Relaxed).max(self.create_time_ticks)) < PEER_EXPIRATION_TIME
    }

    /// Send to an endpoint, fragmenting if needed.
    ///
    /// This does not set the fragmentation field in the packet header, MAC, or encrypt the packet. The sender
    /// must do that while building the packet. The fragmentation flag must be set if fragmentation will be needed.
    async fn internal_send(
        &self,
        si: &SI,
        endpoint: &Endpoint,
        local_socket: Option<&SI::LocalSocket>,
        local_interface: Option<&SI::LocalInterface>,
        max_fragment_size: usize,
        packet: &PacketBuffer,
    ) -> bool {
        let packet_size = packet.len();
        if packet_size > max_fragment_size {
            let bytes = packet.as_bytes();
            if !si.wire_send(endpoint, local_socket, local_interface, &[&bytes[0..UDP_DEFAULT_MTU]], 0).await {
                return false;
            }
            let mut pos = UDP_DEFAULT_MTU;

            let overrun_size = (packet_size - UDP_DEFAULT_MTU) as u32;
            let fragment_count =
                (overrun_size / (UDP_DEFAULT_MTU - packet_constants::FRAGMENT_HEADER_SIZE) as u32) + (((overrun_size % (UDP_DEFAULT_MTU - packet_constants::FRAGMENT_HEADER_SIZE) as u32) != 0) as u32);
            debug_assert!(fragment_count <= packet_constants::FRAGMENT_COUNT_MAX as u32);

            let mut header = FragmentHeader {
                id: *packet.bytes_fixed_at(0).unwrap(),
                dest: *packet.bytes_fixed_at(packet_constants::DESTINATION_INDEX).unwrap(),
                fragment_indicator: packet_constants::FRAGMENT_INDICATOR,
                total_and_fragment_no: ((fragment_count + 1) << 4) as u8,
                reserved_hops: 0,
            };

            let mut chunk_size = (packet_size - pos).min(UDP_DEFAULT_MTU - packet_constants::HEADER_SIZE);
            loop {
                header.total_and_fragment_no += 1;
                let next_pos = pos + chunk_size;
                if !si.wire_send(endpoint, local_socket, local_interface, &[header.as_bytes(), &bytes[pos..next_pos]], 0).await {
                    return false;
                }
                pos = next_pos;
                if pos < packet_size {
                    chunk_size = (packet_size - pos).min(UDP_DEFAULT_MTU - packet_constants::HEADER_SIZE);
                } else {
                    return true;
                }
            }
        } else {
            return si.wire_send(endpoint, local_socket, local_interface, &[packet.as_bytes()], 0).await;
        }
    }

    /// Send a packet to this peer, returning true on (potential) success.
    ///
    /// This will go directly if there is an active path, or otherwise indirectly
    /// via a root or some other route.
    ///
    /// It encrypts and sets the MAC and cipher fields and packet ID and other things.
    pub(crate) async fn send(&self, si: &SI, path: Option<&Arc<Path<SI>>>, node: &Node<SI>, time_ticks: i64, packet: &mut PacketBuffer) -> bool {
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

        let max_fragment_size = if path.endpoint.requires_fragmentation() { UDP_DEFAULT_MTU } else { usize::MAX };
        let flags_cipher_hops =
            if packet.len() > max_fragment_size { packet_constants::HEADER_FLAG_FRAGMENTED | security_constants::CIPHER_AES_GMAC_SIV } else { security_constants::CIPHER_AES_GMAC_SIV };

        let mut aes_gmac_siv = self.identity_symmetric_key.aes_gmac_siv.get();
        aes_gmac_siv.encrypt_init(&self.next_message_id().to_ne_bytes());
        aes_gmac_siv.encrypt_set_aad(&get_packet_aad_bytes(self.identity.address, node.identity.address, flags_cipher_hops));
        if let Ok(payload) = packet.as_bytes_starting_at_mut(packet_constants::HEADER_SIZE) {
            aes_gmac_siv.encrypt_first_pass(payload);
            aes_gmac_siv.encrypt_first_pass_finish();
            aes_gmac_siv.encrypt_second_pass_in_place(payload);
            let tag = aes_gmac_siv.encrypt_second_pass_finish();
            let header = packet.struct_mut_at::<PacketHeader>(0).unwrap();
            header.id = *byte_array_range::<16, 0, 8>(tag);
            header.dest = self.identity.address.to_bytes();
            header.src = node.identity.address.to_bytes();
            header.flags_cipher_hops = flags_cipher_hops;
            header.mac = *byte_array_range::<16, 8, 8>(tag);
        } else {
            return false;
        }

        if self.internal_send(si, &path.endpoint, Some(&path.local_socket), Some(&path.local_interface), max_fragment_size, packet).await {
            self.last_send_time_ticks.store(time_ticks, Ordering::Relaxed);
            return true;
        }

        return false;
    }

    /// Forward a packet to this peer.
    ///
    /// This is called when we receive a packet not addressed to this node and
    /// want to pass it along.
    ///
    /// This doesn't fragment large packets since fragments are forwarded individually.
    /// Intermediates don't need to adjust fragmentation.
    pub(crate) async fn forward(&self, si: &SI, time_ticks: i64, packet: &PacketBuffer) -> bool {
        if let Some(path) = self.direct_path() {
            if si.wire_send(&path.endpoint, Some(&path.local_socket), Some(&path.local_interface), &[packet.as_bytes()], 0).await {
                self.last_forward_time_ticks.store(time_ticks, Ordering::Relaxed);
                return true;
            }
        }
        return false;
    }

    pub(crate) fn create_session_metadata(&self, node: &Node<SI>, direct_source_report: Option<&Endpoint>) -> Vec<u8> {
        let mut session_metadata = Dictionary::new();
        session_metadata.set_bytes(session_metadata::INSTANCE_ID, node.instance_id.to_vec());
        session_metadata.set_bytes(session_metadata::CARE_OF, node.care_of_bytes());
        if let Some(direct_source) = direct_source_report {
            session_metadata.set_bytes(session_metadata::DIRECT_SOURCE, direct_source.to_buffer::<{ Endpoint::MAX_MARSHAL_SIZE }>().unwrap().as_bytes().to_vec());
        }
        if let Some(my_root_sets) = node.my_root_sets() {
            session_metadata.set_bytes(session_metadata::ROOT_SET_UPDATES, my_root_sets);
        }
        session_metadata.to_bytes()
    }

    /// Send a HELLO to this peer.
    ///
    /// If explicit_endpoint is not None the packet will be sent directly to this endpoint.
    /// Otherwise it will be sent via the best direct or indirect path known.
    ///
    /// Unlike other messages HELLO is sent partially in the clear and always with the long-lived
    /// static identity key. Authentication in old versions is via Poly1305 and in new versions
    /// via HMAC-SHA512.
    pub(crate) async fn send_hello(&self, si: &SI, node: &Node<SI>, explicit_endpoint: Option<&Endpoint>) -> bool {
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

        let max_fragment_size = if destination.requires_fragmentation() { UDP_DEFAULT_MTU } else { usize::MAX };
        let time_ticks = si.time_ticks();

        let mut packet = PacketBuffer::new();
        {
            let message_id = self.next_message_id();

            {
                let f: &mut (PacketHeader, message_component_structs::HelloFixedHeaderFields) = packet.append_struct_get_mut().unwrap();
                f.0.id = message_id.to_ne_bytes();
                f.0.dest = self.identity.address.to_bytes();
                f.0.src = node.identity.address.to_bytes();
                f.0.flags_cipher_hops = security_constants::CIPHER_NOCRYPT_POLY1305;
                f.1.verb = verbs::VL1_HELLO | packet_constants::VERB_FLAG_EXTENDED_AUTHENTICATION;
                f.1.version_proto = PROTOCOL_VERSION;
                f.1.version_major = VERSION_MAJOR;
                f.1.version_minor = VERSION_MINOR;
                f.1.version_revision = VERSION_REVISION.to_be_bytes();
                f.1.timestamp = (time_ticks as u64).wrapping_add(self.random_ticks_offset).to_be_bytes();
            }

            debug_assert_eq!(packet.len(), 41);
            assert!(packet.append_bytes((&node.identity.to_public_bytes()).into()).is_ok());

            let (_, poly1305_key) = salsa_poly_create(&self.identity_symmetric_key, packet.struct_at::<PacketHeader>(0).unwrap(), packet.len());
            let mac = zerotier_core_crypto::poly1305::compute(&poly1305_key, packet.as_bytes_starting_at(packet_constants::HEADER_SIZE).unwrap());
            packet.as_mut()[packet_constants::MAC_FIELD_INDEX..packet_constants::MAC_FIELD_INDEX + 8].copy_from_slice(&mac[0..8]);

            self.last_send_time_ticks.store(time_ticks, Ordering::Relaxed);

            debug_event!(si, "HELLO -> {} @ {} ({} bytes)", self.identity.address.to_string(), destination.to_string(), packet.len());
        }

        if let Some(p) = path.as_ref() {
            if self.internal_send(si, destination, Some(&p.local_socket), Some(&p.local_interface), max_fragment_size, &packet).await {
                p.log_send_anything(time_ticks);
                true
            } else {
                false
            }
        } else {
            self.internal_send(si, destination, None, None, max_fragment_size, &packet).await
        }
    }

    /// Receive, decrypt, authenticate, and process an incoming packet from this peer.
    ///
    /// If the packet comes in multiple fragments, the fragments slice should contain all
    /// those fragments after the main packet header and first chunk.
    ///
    /// This returns true if the packet decrypted and passed authentication.
    pub(crate) async fn receive<PH: InnerProtocolInterface>(
        &self,
        node: &Node<SI>,
        si: &SI,
        ph: &PH,
        time_ticks: i64,
        source_path: &Arc<Path<SI>>,
        packet_header: &PacketHeader,
        frag0: &PacketBuffer,
        fragments: &[Option<PooledPacketBuffer>],
    ) -> bool {
        if let Ok(packet_frag0_payload_bytes) = frag0.as_bytes_starting_at(packet_constants::VERB_INDEX) {
            let mut payload = PacketBuffer::new();

            let message_id = if let Some(message_id2) = try_aead_decrypt(&self.identity_symmetric_key, packet_frag0_payload_bytes, packet_header, fragments, &mut payload) {
                // Decryption successful with static secret.
                message_id2
            } else {
                // Packet failed to decrypt using either ephemeral or permament key, reject.
                debug_event!(si, "[vl1] #{:0>16x} failed authentication", u64::from_be_bytes(packet_header.id));
                return false;
            };

            if let Ok(mut verb) = payload.u8_at(0) {
                if (verb & packet_constants::VERB_FLAG_COMPRESSED) != 0 {
                    let mut decompressed_payload = [0_u8; packet_constants::SIZE_MAX];
                    decompressed_payload[0] = verb;
                    if let Ok(dlen) = lz4_flex::block::decompress_into(&payload.as_bytes()[1..], &mut decompressed_payload[1..]) {
                        payload.set_to(&decompressed_payload[..(dlen + 1)]);
                    } else {
                        return false;
                    }
                }

                // ---------------------------------------------------------------
                // If we made it here it decrypted and passed authentication.
                // ---------------------------------------------------------------

                self.last_receive_time_ticks.store(time_ticks, Ordering::Relaxed);

                let mut path_is_known = false;
                for p in self.paths.lock().iter_mut() {
                    if p.canonical_instance_id == source_path.canonical.canonical_instance_id() {
                        p.last_receive_time_ticks = time_ticks;
                        path_is_known = true;
                        break;
                    }
                }

                verb &= packet_constants::VERB_MASK; // mask off flags
                debug_event!(si, "[vl1] #{:0>16x} decrypted and authenticated, verb: {} ({:0>2x})", u64::from_be_bytes(packet_header.id), verbs::name(verb), verb as u32);

                if match verb {
                    verbs::VL1_NOP => true,
                    verbs::VL1_HELLO => self.handle_incoming_hello(si, ph, node, time_ticks, message_id, source_path, packet_header.hops(), &payload).await,
                    verbs::VL1_ERROR => self.handle_incoming_error(si, ph, node, time_ticks, source_path, &payload).await,
                    verbs::VL1_OK => self.handle_incoming_ok(si, ph, node, time_ticks, source_path, packet_header.hops(), path_is_known, &payload).await,
                    verbs::VL1_WHOIS => self.handle_incoming_whois(si, ph, node, time_ticks, message_id, &payload).await,
                    verbs::VL1_RENDEZVOUS => self.handle_incoming_rendezvous(si, node, time_ticks, message_id, source_path, &payload).await,
                    verbs::VL1_ECHO => self.handle_incoming_echo(si, ph, node, time_ticks, message_id, &payload).await,
                    verbs::VL1_PUSH_DIRECT_PATHS => self.handle_incoming_push_direct_paths(si, node, time_ticks, source_path, &payload).await,
                    verbs::VL1_USER_MESSAGE => self.handle_incoming_user_message(si, node, time_ticks, source_path, &payload).await,
                    _ => ph.handle_packet(self, &source_path, verb, &payload).await,
                } {
                    // This needs to be saved AFTER processing the packet since some message types may use it to try to filter for replays.
                    self.last_incoming_message_id.store(message_id, Ordering::Relaxed);

                    return true;
                }
            }
        }
        return false;
    }

    async fn handle_incoming_hello<PH: InnerProtocolInterface>(
        &self,
        si: &SI,
        ph: &PH,
        node: &Node<SI>,
        time_ticks: i64,
        message_id: MessageId,
        source_path: &Arc<Path<SI>>,
        hops: u8,
        payload: &PacketBuffer,
    ) -> bool {
        if !(ph.has_trust_relationship(&self.identity) || node.this_node_is_root() || node.is_peer_root(self)) {
            debug_event!(si, "[vl1] dropping HELLO from {} due to lack of trust relationship", self.identity.address.to_string());
            return true; // packet wasn't invalid, just ignored
        }

        let mut cursor = 0;
        if let Ok(hello_fixed_headers) = payload.read_struct::<message_component_structs::HelloFixedHeaderFields>(&mut cursor) {
            if let Ok(identity) = Identity::read_bytes(&mut BufferReader::new(payload, &mut cursor)) {
                if identity.eq(&self.identity) {
                    {
                        let mut remote_node_info = self.remote_node_info.write();
                        remote_node_info.remote_protocol_version = hello_fixed_headers.version_proto;
                        remote_node_info.remote_version = (hello_fixed_headers.version_major as u64).wrapping_shl(48)
                            | (hello_fixed_headers.version_minor as u64).wrapping_shl(32)
                            | (u16::from_be_bytes(hello_fixed_headers.version_revision) as u64).wrapping_shl(16);
                    }

                    let mut packet = PacketBuffer::new();
                    packet.set_size(packet_constants::HEADER_SIZE);
                    {
                        let f: &mut (message_component_structs::OkHeader, message_component_structs::OkHelloFixedHeaderFields) = packet.append_struct_get_mut().unwrap();
                        f.0.verb = verbs::VL1_OK;
                        f.0.in_re_verb = verbs::VL1_HELLO;
                        f.0.in_re_message_id = message_id.to_ne_bytes();
                        f.1.timestamp_echo = hello_fixed_headers.timestamp;
                        f.1.version_proto = PROTOCOL_VERSION;
                        f.1.version_major = VERSION_MAJOR;
                        f.1.version_minor = VERSION_MINOR;
                        f.1.version_revision = VERSION_REVISION.to_be_bytes();
                    }
                    if hello_fixed_headers.version_proto >= 20 {
                        let session_metadata = self.create_session_metadata(node, if hops == 0 { Some(&source_path.endpoint) } else { None });
                        assert!(session_metadata.len() <= 0xffff); // sanity check, should be impossible
                        assert!(packet.append_u16(session_metadata.len() as u16).is_ok());
                        assert!(packet.append_bytes(session_metadata.as_slice()).is_ok());
                    }

                    return self.send(si, Some(source_path), node, time_ticks, &mut packet).await;
                }
            }
        }
        return false;
    }

    async fn handle_incoming_error<PH: InnerProtocolInterface>(&self, _si: &SI, ph: &PH, _node: &Node<SI>, _time_ticks: i64, source_path: &Arc<Path<SI>>, payload: &PacketBuffer) -> bool {
        let mut cursor = 0;
        if let Ok(error_header) = payload.read_struct::<message_component_structs::ErrorHeader>(&mut cursor) {
            let in_re_message_id: MessageId = u64::from_ne_bytes(error_header.in_re_message_id);
            if self.message_id_counter.load(Ordering::Relaxed).wrapping_sub(in_re_message_id) <= PACKET_RESPONSE_COUNTER_DELTA_MAX {
                match error_header.in_re_verb {
                    _ => {
                        return ph.handle_error(self, &source_path, error_header.in_re_verb, in_re_message_id, error_header.error_code, payload, &mut cursor).await;
                    }
                }
            }
        }
        return false;
    }

    async fn handle_incoming_ok<PH: InnerProtocolInterface>(
        &self,
        si: &SI,
        ph: &PH,
        node: &Node<SI>,
        time_ticks: i64,
        source_path: &Arc<Path<SI>>,
        hops: u8,
        path_is_known: bool,
        payload: &PacketBuffer,
    ) -> bool {
        let mut cursor = 0;
        if let Ok(ok_header) = payload.read_struct::<message_component_structs::OkHeader>(&mut cursor) {
            let in_re_message_id: MessageId = u64::from_ne_bytes(ok_header.in_re_message_id);
            if self.message_id_counter.load(Ordering::Relaxed).wrapping_sub(in_re_message_id) <= PACKET_RESPONSE_COUNTER_DELTA_MAX {
                match ok_header.in_re_verb {
                    verbs::VL1_HELLO => {
                        if let Ok(ok_hello_fixed_header_fields) = payload.read_struct::<message_component_structs::OkHelloFixedHeaderFields>(&mut cursor) {
                            if ok_hello_fixed_header_fields.version_proto >= 20 {
                                // V2 nodes send a whole dictionary for easy extensibiility.
                                if let Ok(session_metadata_len) = payload.read_u16(&mut cursor) {
                                    if session_metadata_len > 0 {
                                        if let Ok(session_metadata) = payload.read_bytes(session_metadata_len as usize, &mut cursor) {
                                            if let Some(session_metadata) = Dictionary::from_bytes(session_metadata) {
                                                let mut remote_node_info = self.remote_node_info.write();

                                                if hops == 0 {
                                                    if let Some(reported_endpoint) = session_metadata.get_bytes(session_metadata::DIRECT_SOURCE) {
                                                        if let Some(reported_endpoint) = Endpoint::from_bytes(reported_endpoint) {
                                                            #[cfg(debug_assertions)]
                                                            let reported_endpoint2 = reported_endpoint.clone();
                                                            if remote_node_info.reported_local_endpoints.insert(reported_endpoint, time_ticks).is_none() {
                                                                #[cfg(debug_assertions)]
                                                                debug_event!(
                                                                    si,
                                                                    "[vl1] {} reported new remote perspective, local endpoint: {}",
                                                                    self.identity.address.to_string(),
                                                                    reported_endpoint2.to_string()
                                                                );
                                                            }
                                                        }
                                                    }
                                                }

                                                if let Some(care_of) = session_metadata.get_bytes(session_metadata::CARE_OF) {
                                                    if let Some(care_of) = CareOf::from_bytes(care_of) {
                                                        let _ = remote_node_info.care_of.insert(care_of);
                                                    }
                                                }

                                                if let Some(instance_id) = session_metadata.get_bytes(session_metadata::INSTANCE_ID) {
                                                    remote_node_info.remote_instance_id.fill(0);
                                                    let l = instance_id.len().min(remote_node_info.remote_instance_id.len());
                                                    (&mut remote_node_info.remote_instance_id[..l]).copy_from_slice(&instance_id[..l]);
                                                }

                                                if let Some(root_set_updates) = session_metadata.get_bytes(session_metadata::ROOT_SET_UPDATES) {
                                                    let mut tmp = PacketBuffer::new();
                                                    if root_set_updates.len() <= tmp.capacity() {
                                                        tmp.set_to(root_set_updates);
                                                        let mut cursor = 0;
                                                        while cursor < tmp.len() {
                                                            if let Ok(rs) = RootSet::unmarshal(&tmp, &mut cursor) {
                                                                // This checks the origin node and only allows members of root sets to update them.
                                                                node.remote_update_root_set(&self.identity, rs);
                                                            } else {
                                                                break;
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            } else {
                                // V1 nodes just append the endpoint to which they sent the packet, and we can still use that.
                                if hops == 0 {
                                    if let Ok(reported_endpoint) = Endpoint::unmarshal(&payload, &mut cursor) {
                                        #[cfg(debug_assertions)]
                                        let reported_endpoint2 = reported_endpoint.clone();
                                        if self.remote_node_info.write().reported_local_endpoints.insert(reported_endpoint, time_ticks).is_none() {
                                            #[cfg(debug_assertions)]
                                            debug_event!(si, "[vl1] {} reported new remote perspective, local endpoint: {}", self.identity.address.to_string(), reported_endpoint2.to_string());
                                        }
                                    }
                                }
                            }

                            if hops == 0 && !path_is_known {
                                self.learn_path(si, source_path, time_ticks);
                            }

                            self.last_hello_reply_time_ticks.store(time_ticks, Ordering::Relaxed);
                        }
                    }

                    verbs::VL1_WHOIS => {
                        if node.is_peer_root(self) {
                            while cursor < payload.len() {
                                if let Ok(_whois_response) = Identity::read_bytes(&mut BufferReader::new(payload, &mut cursor)) {
                                    // TODO
                                } else {
                                    break;
                                }
                            }
                        } else {
                            return true; // not invalid, just ignored
                        }
                    }

                    _ => {
                        return ph.handle_ok(self, &source_path, ok_header.in_re_verb, in_re_message_id, payload, &mut cursor).await;
                    }
                }
            }
        }
        return false;
    }

    async fn handle_incoming_whois<PH: InnerProtocolInterface>(&self, si: &SI, ph: &PH, node: &Node<SI>, time_ticks: i64, message_id: MessageId, payload: &PacketBuffer) -> bool {
        if node.this_node_is_root() || ph.has_trust_relationship(&self.identity) {
            let mut packet = PacketBuffer::new();
            packet.set_size(packet_constants::HEADER_SIZE);
            {
                let mut f: &mut message_component_structs::OkHeader = packet.append_struct_get_mut().unwrap();
                f.verb = verbs::VL1_OK;
                f.in_re_verb = verbs::VL1_WHOIS;
                f.in_re_message_id = message_id.to_ne_bytes();
            }

            let mut cursor = 0;
            while cursor < payload.len() {
                if let Ok(zt_address) = Address::unmarshal(payload, &mut cursor) {
                    if let Some(peer) = node.peer(zt_address) {
                        if !packet.append_bytes((&peer.identity.to_public_bytes()).into()).is_ok() {
                            debug_event!(si, "unexpected error serializing an identity into a WHOIS packet response");
                            return false;
                        }
                    }
                }
            }

            self.send(si, None, node, time_ticks, &mut packet).await
        } else {
            true // packet wasn't invalid, just ignored
        }
    }

    #[allow(unused)]
    async fn handle_incoming_rendezvous(&self, si: &SI, node: &Node<SI>, time_ticks: i64, message_id: MessageId, source_path: &Arc<Path<SI>>, payload: &PacketBuffer) -> bool {
        if node.is_peer_root(self) {}
        return true;
    }

    async fn handle_incoming_echo<PH: InnerProtocolInterface>(&self, si: &SI, ph: &PH, node: &Node<SI>, time_ticks: i64, message_id: MessageId, payload: &PacketBuffer) -> bool {
        if ph.has_trust_relationship(&self.identity) || node.is_peer_root(self) {
            let mut packet = PacketBuffer::new();
            packet.set_size(packet_constants::HEADER_SIZE);
            {
                let mut f: &mut message_component_structs::OkHeader = packet.append_struct_get_mut().unwrap();
                f.verb = verbs::VL1_OK;
                f.in_re_verb = verbs::VL1_ECHO;
                f.in_re_message_id = message_id.to_ne_bytes();
            }
            if packet.append_bytes(payload.as_bytes()).is_ok() {
                self.send(si, None, node, time_ticks, &mut packet).await
            } else {
                false
            }
        } else {
            debug_event!(si, "[vl1] dropping ECHO from {} due to lack of trust relationship", self.identity.address.to_string());
            true // packet wasn't invalid, just ignored
        }
    }

    #[allow(unused)]
    async fn handle_incoming_push_direct_paths(&self, si: &SI, node: &Node<SI>, time_ticks: i64, source_path: &Arc<Path<SI>>, payload: &PacketBuffer) -> bool {
        false
    }

    #[allow(unused)]
    async fn handle_incoming_user_message(&self, si: &SI, node: &Node<SI>, time_ticks: i64, source_path: &Arc<Path<SI>>, payload: &PacketBuffer) -> bool {
        false
    }
}

impl<SI: SystemInterface> PartialEq for Peer<SI> {
    #[inline(always)]
    fn eq(&self, other: &Self) -> bool {
        self.canonical.eq(&other.canonical)
    }
}

impl<SI: SystemInterface> Eq for Peer<SI> {}

impl<SI: SystemInterface> Hash for Peer<SI> {
    #[inline(always)]
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.canonical.canonical_instance_id().hash(state);
    }
}
