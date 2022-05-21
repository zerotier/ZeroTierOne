// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::hash::{Hash, Hasher};
use std::mem::MaybeUninit;
use std::sync::atomic::{AtomicI64, AtomicU64, AtomicU8, Ordering};
use std::sync::{Arc, Weak};

use parking_lot::{Mutex, RwLock};

use zerotier_core_crypto::aes_gmac_siv::AesCtr;
use zerotier_core_crypto::hash::*;
use zerotier_core_crypto::poly1305::Poly1305;
use zerotier_core_crypto::random::{get_bytes_secure, next_u64_secure};
use zerotier_core_crypto::salsa::Salsa;
use zerotier_core_crypto::secret::Secret;

use crate::util::byte_array_range;
use crate::util::marshalable::Marshalable;
use crate::vl1::node::*;
use crate::vl1::protocol::*;
use crate::vl1::symmetricsecret::{EphemeralSymmetricSecret, SymmetricSecret};
use crate::vl1::{Dictionary, Endpoint, Identity, Path};
use crate::{VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION};

pub(crate) const SERVICE_INTERVAL_MS: i64 = security_constants::EPHEMERAL_SECRET_REKEY_AFTER_TIME / 10;

struct PeerPath<SI: SystemInterface> {
    path: Weak<Path<SI>>,
    last_receive_time_ticks: i64,
}

/// A remote peer known to this node.
///
/// Equality and hashing is implemented in terms of the identity.
pub struct Peer<SI: SystemInterface> {
    // This peer's identity.
    pub(crate) identity: Identity,

    // Static shared secret computed from agreement with identity.
    identity_symmetric_key: SymmetricSecret,

    // Latest ephemeral secret or None if not yet negotiated.
    ephemeral_symmetric_key: RwLock<Option<EphemeralSymmetricSecret>>,

    // Paths sorted in descending order of quality / preference.
    paths: Mutex<Vec<PeerPath<SI>>>,

    // Statistics and times of events.
    pub(crate) last_send_time_ticks: AtomicI64,
    pub(crate) last_receive_time_ticks: AtomicI64,
    pub(crate) last_hello_reply_time_ticks: AtomicI64,
    pub(crate) last_forward_time_ticks: AtomicI64,

    // Counter for assigning sequential message IDs.
    message_id_counter: AtomicU64,

    // Remote peer version information.
    remote_version: AtomicU64,
    remote_protocol_version: AtomicU8,
}

/// Create initialized instances of Salsa20/12 and Poly1305 for a packet.
/// This is deprecated and is not used with AES-GMAC-SIV.
fn salsa_poly_create(secret: &SymmetricSecret, header: &PacketHeader, packet_size: usize) -> (Salsa<12>, Poly1305) {
    // Create a per-packet key from the IV, source, destination, and packet size.
    let mut key: Secret<32> = secret.key.first_n();
    let hb = header.as_bytes();
    for i in 0..18 {
        key.0[i] ^= hb[i];
    }
    key.0[18] ^= hb[packet_constants::FLAGS_FIELD_INDEX] & packet_constants::FLAGS_FIELD_MASK_HIDE_HOPS;
    key.0[19] ^= (packet_size >> 8) as u8;
    key.0[20] ^= packet_size as u8;

    let mut salsa = Salsa::<12>::new(&key.0, &header.id);
    let mut poly1305_key = [0_u8; 32];
    salsa.crypt_in_place(&mut poly1305_key);
    (salsa, Poly1305::new(&poly1305_key).unwrap())
}

/// Attempt AEAD packet encryption and MAC validation. Returns message ID on success.
fn try_aead_decrypt(secret: &SymmetricSecret, packet_frag0_payload_bytes: &[u8], header: &PacketHeader, fragments: &[Option<PooledPacketBuffer>], payload: &mut PacketBuffer) -> Option<u64> {
    packet_frag0_payload_bytes.get(0).map_or(None, |verb| {
        match header.cipher() {
            security_constants::CIPHER_NOCRYPT_POLY1305 => {
                if (verb & packet_constants::VERB_MASK) == verbs::VL1_HELLO {
                    let mut total_packet_len = packet_frag0_payload_bytes.len() + packet_constants::HEADER_SIZE;
                    for f in fragments.iter() {
                        total_packet_len += f.as_ref().map_or(0, |f| f.len());
                    }
                    let _ = payload.append_bytes(packet_frag0_payload_bytes);
                    for f in fragments.iter() {
                        let _ = f.as_ref().map(|f| f.as_bytes_starting_at(packet_constants::HEADER_SIZE).map(|f| payload.append_bytes(f)));
                    }
                    let (_, mut poly) = salsa_poly_create(secret, header, total_packet_len);
                    poly.update(payload.as_bytes());
                    if poly.finish()[0..8].eq(&header.mac) {
                        Some(u64::from_ne_bytes(header.id))
                    } else {
                        None
                    }
                } else {
                    // Only HELLO is permitted without payload encryption. Drop other packet types if sent this way.
                    None
                }
            }

            security_constants::CIPHER_SALSA2012_POLY1305 => {
                let mut total_packet_len = packet_frag0_payload_bytes.len() + packet_constants::HEADER_SIZE;
                for f in fragments.iter() {
                    total_packet_len += f.as_ref().map_or(0, |f| f.len());
                }
                let (mut salsa, mut poly) = salsa_poly_create(secret, header, total_packet_len);
                poly.update(packet_frag0_payload_bytes);
                let _ = payload.append_bytes_get_mut(packet_frag0_payload_bytes.len()).map(|b| salsa.crypt(packet_frag0_payload_bytes, b));
                for f in fragments.iter() {
                    let _ = f.as_ref().map(|f| {
                        f.as_bytes_starting_at(packet_constants::FRAGMENT_HEADER_SIZE).map(|f| {
                            poly.update(f);
                            let _ = payload.append_bytes_get_mut(f.len()).map(|b| salsa.crypt(f, b));
                        })
                    });
                }
                if poly.finish()[0..8].eq(&header.mac) {
                    Some(u64::from_ne_bytes(header.id))
                } else {
                    None
                }
            }

            security_constants::CIPHER_AES_GMAC_SIV => {
                let mut aes = secret.aes_gmac_siv.get();
                aes.decrypt_init(&header.aes_gmac_siv_tag());
                aes.decrypt_set_aad(&header.aad_bytes());
                // NOTE: if there are somehow missing fragments this part will silently fail,
                // but the packet will fail MAC check in decrypt_finish() so meh.
                let _ = payload.append_bytes_get_mut(packet_frag0_payload_bytes.len()).map(|b| aes.decrypt(packet_frag0_payload_bytes, b));
                for f in fragments.iter() {
                    f.as_ref().map(|f| {
                        f.as_bytes_starting_at(packet_constants::FRAGMENT_HEADER_SIZE).map(|f| {
                            let _ = payload.append_bytes_get_mut(f.len()).map(|b| aes.decrypt(f, b));
                        })
                    });
                }
                aes.decrypt_finish().map_or(None, |tag| {
                    // AES-GMAC-SIV encrypts the packet ID too as part of its computation of a single
                    // opaque 128-bit tag, so to get the original packet ID we have to grab it from the
                    // decrypted tag.
                    Some(u64::from_ne_bytes(*byte_array_range::<16, 0, 8>(tag)))
                })
            }

            _ => None,
        }
    })
}

impl<SI: SystemInterface> Peer<SI> {
    /// Create a new peer.
    ///
    /// This only returns None if this_node_identity does not have its secrets or if some
    /// fatal error occurs performing key agreement between the two identities.
    pub(crate) fn new(this_node_identity: &Identity, id: Identity, time_clock: i64) -> Option<Peer<SI>> {
        this_node_identity.agree(&id).map(|static_secret| -> Self {
            /*
             * SECURITY NOTE:
             *
             * The message ID counter is initialized from the number of minutes since the Unix epoch (according to
             * the current clock) in the most significant 26 bits followed by two zero bits followed by 36 random
             * bits.
             *
             * The nature of AES-GMAC-SIV means that message ID duplication is not particularly dangerous, but we
             * still want to avoid it. If the clock is at least marginally correct this will mean that message IDs
             * will remain unique for over a hundred years. Message IDs are kept secret as well because they are
             * encrypted along with a GMAC code to form an opaque 128-bit packet tag.
             */
            Self {
                identity: id,
                identity_symmetric_key: SymmetricSecret::new(static_secret),
                ephemeral_symmetric_key: RwLock::new(None),
                paths: Mutex::new(Vec::with_capacity(4)),
                last_send_time_ticks: AtomicI64::new(0),
                last_receive_time_ticks: AtomicI64::new(0),
                last_forward_time_ticks: AtomicI64::new(0),
                last_hello_reply_time_ticks: AtomicI64::new(0),
                message_id_counter: AtomicU64::new(((time_clock as u64) / 60000).wrapping_shl(38) ^ next_u64_secure().wrapping_shr(28)),
                remote_version: AtomicU64::new(0),
                remote_protocol_version: AtomicU8::new(0),
            }
        })
    }

    /// Get the next message ID for sending a message to this peer.
    #[inline(always)]
    pub(crate) fn next_message_id(&self) -> u64 {
        // SECURITY NOTE: uses the strictest memory ordering to avoid duplicate IDs on loose architectures like ARM64.
        self.message_id_counter.fetch_add(1, Ordering::SeqCst)
    }

    /// Receive, decrypt, authenticate, and process an incoming packet from this peer.
    ///
    /// If the packet comes in multiple fragments, the fragments slice should contain all
    /// those fragments after the main packet header and first chunk.
    pub(crate) fn receive<VI: InnerProtocolInterface>(&self, node: &Node<SI>, si: &SI, vi: &VI, time_ticks: i64, source_path: &Arc<Path<SI>>, header: &PacketHeader, frag0: &PacketBuffer, fragments: &[Option<PooledPacketBuffer>]) {
        if let Ok(packet_frag0_payload_bytes) = frag0.as_bytes_starting_at(packet_constants::VERB_INDEX) {
            let mut payload = unsafe { PacketBuffer::new_without_memzero() };

            // First try decrypting and authenticating with an ephemeral secret if one is negotiated.
            let (forward_secrecy, mut message_id) = if let Some(ephemeral_secret) = self.ephemeral_symmetric_key.read().as_ref() {
                if let Some(message_id) = try_aead_decrypt(&ephemeral_secret.secret, packet_frag0_payload_bytes, header, fragments, &mut payload) {
                    // Decryption successful with ephemeral secret
                    ephemeral_secret.decrypt_uses.fetch_add(1, Ordering::Relaxed);
                    (true, message_id)
                } else {
                    // Decryption failed with ephemeral secret, which may indicate that it's obsolete.
                    (false, 0)
                }
            } else {
                // There is no ephemeral secret negotiated (yet?).
                (false, 0)
            };

            // Then try the permanent secret.
            if !forward_secrecy {
                if let Some(message_id2) = try_aead_decrypt(&self.identity_symmetric_key, packet_frag0_payload_bytes, header, fragments, &mut payload) {
                    // Decryption successful with static secret.
                    message_id = message_id2;
                } else {
                    // Packet failed to decrypt using either ephemeral or permament key, reject.
                    return;
                }
            }

            if let Ok(mut verb) = payload.u8_at(0) {
                self.last_receive_time_ticks.store(time_ticks, Ordering::Relaxed);

                let extended_authentication = (verb & packet_constants::VERB_FLAG_EXTENDED_AUTHENTICATION) != 0;
                if extended_authentication {
                    if payload.len() >= SHA512_HASH_SIZE {
                        let actual_end_of_payload = payload.len() - SHA512_HASH_SIZE;
                        let mut hmac = HMACSHA512::new(self.identity_symmetric_key.packet_hmac_key.as_bytes());
                        hmac.update(&message_id.to_ne_bytes());
                        hmac.update(&payload.as_bytes()[..actual_end_of_payload]);
                        if !hmac.finish().eq(&payload.as_bytes()[actual_end_of_payload..]) {
                            return;
                        }
                        payload.set_size(actual_end_of_payload);
                    } else {
                        return;
                    }
                }

                // ---------------------------------------------------------------
                // If we made it here it decrypted and passed authentication.
                // ---------------------------------------------------------------

                if (verb & packet_constants::VERB_FLAG_COMPRESSED) != 0 {
                    let mut decompressed_payload: [u8; packet_constants::SIZE_MAX] = unsafe { MaybeUninit::uninit().assume_init() };
                    decompressed_payload[0] = verb;
                    if let Ok(dlen) = lz4_flex::block::decompress_into(&payload.as_bytes()[1..], &mut decompressed_payload[1..]) {
                        payload.set_to(&decompressed_payload[..(dlen + 1)]);
                    } else {
                        return;
                    }
                }

                let source_path_ptr = Arc::as_ptr(source_path);
                for p in self.paths.lock().iter_mut() {
                    if Weak::as_ptr(&p.path) == source_path_ptr {
                        p.last_receive_time_ticks = time_ticks;
                        break;
                    }
                }

                // For performance reasons we let VL2 handle packets first. It returns false
                // if it didn't handle the packet, in which case it's handled at VL1. This is
                // because the most performance critical path is the handling of the ???_FRAME
                // verbs, which are in VL2.
                verb &= packet_constants::VERB_MASK; // mask off flags
                if !vi.handle_packet(self, source_path, forward_secrecy, extended_authentication, verb, &payload) {
                    match verb {
                        //VERB_VL1_NOP => {}
                        verbs::VL1_HELLO => self.receive_hello(si, node, time_ticks, source_path, &payload),
                        verbs::VL1_ERROR => self.receive_error(si, vi, node, time_ticks, source_path, forward_secrecy, extended_authentication, &payload),
                        verbs::VL1_OK => self.receive_ok(si, vi, node, time_ticks, source_path, forward_secrecy, extended_authentication, &payload),
                        verbs::VL1_WHOIS => self.receive_whois(si, node, time_ticks, source_path, &payload),
                        verbs::VL1_RENDEZVOUS => self.receive_rendezvous(si, node, time_ticks, source_path, &payload),
                        verbs::VL1_ECHO => self.receive_echo(si, node, time_ticks, source_path, &payload),
                        verbs::VL1_PUSH_DIRECT_PATHS => self.receive_push_direct_paths(si, node, time_ticks, source_path, &payload),
                        verbs::VL1_USER_MESSAGE => self.receive_user_message(si, node, time_ticks, source_path, &payload),
                        _ => {}
                    }
                }
            }
        }
    }

    fn send_to_endpoint(&self, si: &SI, endpoint: &Endpoint, local_socket: Option<&SI::LocalSocket>, local_interface: Option<&SI::LocalInterface>, packet: &PacketBuffer) -> bool {
        match endpoint {
            Endpoint::Ip(_) | Endpoint::IpUdp(_) | Endpoint::Ethernet(_) | Endpoint::Bluetooth(_) | Endpoint::WifiDirect(_) => {
                let packet_size = packet.len();
                if packet_size > UDP_DEFAULT_MTU {
                    let bytes = packet.as_bytes();
                    if !si.wire_send(endpoint, local_socket, local_interface, &[&bytes[0..UDP_DEFAULT_MTU]], 0) {
                        return false;
                    }

                    let mut pos = UDP_DEFAULT_MTU;

                    let overrun_size = (packet_size - UDP_DEFAULT_MTU) as u32;
                    let fragment_count = (overrun_size / (UDP_DEFAULT_MTU - packet_constants::FRAGMENT_HEADER_SIZE) as u32) + (((overrun_size % (UDP_DEFAULT_MTU - packet_constants::FRAGMENT_HEADER_SIZE) as u32) != 0) as u32);
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
                        if !si.wire_send(endpoint, local_socket, local_interface, &[header.as_bytes(), &bytes[pos..next_pos]], 0) {
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
                    return si.wire_send(endpoint, local_socket, local_interface, &[packet.as_bytes()], 0);
                }
            }
            _ => {
                return si.wire_send(endpoint, local_socket, local_interface, &[packet.as_bytes()], 0);
            }
        }
    }

    /// Send a packet to this peer.
    ///
    /// This will go directly if there is an active path, or otherwise indirectly
    /// via a root or some other route.
    pub(crate) fn send(&self, si: &SI, node: &Node<SI>, time_ticks: i64, packet: &PacketBuffer) -> bool {
        if let Some(path) = self.path(node) {
            if self.send_to_endpoint(si, &path.endpoint, Some(&path.local_socket), Some(&path.local_interface), packet) {
                self.last_send_time_ticks.store(time_ticks, Ordering::Relaxed);
                return true;
            }
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
    pub(crate) fn forward(&self, si: &SI, time_ticks: i64, packet: &PacketBuffer) -> bool {
        if let Some(path) = self.direct_path() {
            if si.wire_send(&path.endpoint, Some(&path.local_socket), Some(&path.local_interface), &[packet.as_bytes()], 0) {
                self.last_forward_time_ticks.store(time_ticks, Ordering::Relaxed);
                return true;
            }
        }
        return false;
    }

    /// Send a HELLO to this peer.
    ///
    /// If explicit_endpoint is not None the packet will be sent directly to this endpoint.
    /// Otherwise it will be sent via the best direct or indirect path known.
    ///
    /// Unlike other messages HELLO is sent partially in the clear and always with the long-lived
    /// static identity key.
    pub(crate) fn send_hello(&self, si: &SI, node: &Node<SI>, explicit_endpoint: Option<&Endpoint>) -> bool {
        let mut path = None;
        let destination = explicit_endpoint.map_or_else(
            || {
                self.path(node).map_or(None, |p| {
                    let _ = path.insert(p.clone());
                    Some(p.endpoint.clone())
                })
            },
            |endpoint| Some(endpoint.clone()),
        );
        if destination.is_none() {
            return false;
        }
        let destination = destination.unwrap();

        let mut packet = PacketBuffer::new();
        let time_ticks = si.time_ticks();
        let message_id = self.next_message_id();

        {
            let packet_header: &mut PacketHeader = packet.append_struct_get_mut().unwrap();
            packet_header.id = message_id.to_ne_bytes(); // packet ID and message ID are the same when Poly1305 MAC is used
            packet_header.dest = self.identity.address.to_bytes();
            packet_header.src = node.identity.address.to_bytes();
            packet_header.flags_cipher_hops = security_constants::CIPHER_NOCRYPT_POLY1305;
        }

        {
            let hello_fixed_headers: &mut message_component_structs::HelloFixedHeaderFields = packet.append_struct_get_mut().unwrap();
            hello_fixed_headers.verb = verbs::VL1_HELLO | packet_constants::VERB_FLAG_EXTENDED_AUTHENTICATION;
            hello_fixed_headers.version_proto = PROTOCOL_VERSION;
            hello_fixed_headers.version_major = VERSION_MAJOR;
            hello_fixed_headers.version_minor = VERSION_MINOR;
            hello_fixed_headers.version_revision = (VERSION_REVISION as u16).to_be_bytes();
            hello_fixed_headers.timestamp = (time_ticks as u64).to_be_bytes();
        }

        // Full identity of the node establishing the session.
        assert!(self.identity.marshal_with_options(&mut packet, Identity::ALGORITHM_ALL, false).is_ok());

        // 8 reserved bytes, must be zero for compatibility with old nodes.
        assert!(packet.append_padding(0, 8).is_ok());

        // Generate a 12-byte nonce for the private section of HELLO.
        let mut nonce = get_bytes_secure::<12>();

        // LEGACY: create a 16-bit encrypted field that specifies zero "moons." Current nodes ignore this
        // and treat it as part of the random AES-CTR nonce, but old versions need it to parse the packet
        // correctly.
        let mut salsa_iv = message_id.to_ne_bytes();
        salsa_iv[7] &= 0xf8;
        Salsa::<12>::new(&self.identity_symmetric_key.key.0[0..32], &salsa_iv).crypt(&[0_u8, 0_u8], &mut nonce[8..10]);

        // Append 12-byte AES-CTR nonce.
        assert!(packet.append_bytes_fixed(&nonce).is_ok());

        // Add session meta-data, which is encrypted using plain AES-CTR. No authentication (AEAD) is needed
        // because the whole packet is authenticated. Data in the session is not technically secret in a
        // cryptographic sense but we encrypt it for privacy and as a defense in depth.
        let mut fields = Dictionary::new();
        fields.set_u64(session_metadata::INSTANCE_ID, node.instance_id);
        fields.set_u64(session_metadata::CLOCK, si.time_clock() as u64);
        fields.set_bytes(session_metadata::SENT_TO, destination.to_buffer::<{ Endpoint::MAX_MARSHAL_SIZE }>().unwrap().as_bytes().to_vec());
        let fields = fields.to_bytes();
        assert!(fields.len() <= 0xffff); // sanity check, should be impossible
        assert!(packet.append_u16(fields.len() as u16).is_ok()); // prefix with unencrypted size
        let private_section_start = packet.len();
        assert!(packet.append_bytes(fields.as_slice()).is_ok());
        let mut aes = AesCtr::new(&self.identity_symmetric_key.hello_private_section_key.as_bytes()[0..32]);
        aes.init(&nonce);
        aes.crypt_in_place(&mut packet.as_mut()[private_section_start..]);
        drop(aes);
        drop(fields);

        // Seal packet with HMAC-SHA512 extended authentication.
        let mut hmac = HMACSHA512::new(self.identity_symmetric_key.packet_hmac_key.as_bytes());
        hmac.update(&message_id.to_ne_bytes());
        hmac.update(&packet.as_bytes()[packet_constants::HEADER_SIZE..]);
        assert!(packet.append_bytes_fixed(&hmac.finish()).is_ok());

        // Set poly1305 in header, which is the only authentication for old nodes.
        let (_, mut poly) = salsa_poly_create(&self.identity_symmetric_key, packet.struct_at::<PacketHeader>(0).unwrap(), packet.len());
        poly.update(packet.as_bytes_starting_at(packet_constants::HEADER_SIZE).unwrap());
        packet.as_mut()[packet_constants::MAC_FIELD_INDEX..packet_constants::MAC_FIELD_INDEX + 8].copy_from_slice(&poly.finish()[0..8]);

        self.last_send_time_ticks.store(time_ticks, Ordering::Relaxed);

        path.map_or_else(
            || self.send_to_endpoint(si, &destination, None, None, &packet),
            |p| {
                if self.send_to_endpoint(si, &destination, Some(&p.local_socket), Some(&p.local_interface), &packet) {
                    p.log_send_anything(time_ticks);
                    true
                } else {
                    false
                }
            },
        )
    }

    fn receive_hello(&self, si: &SI, node: &Node<SI>, time_ticks: i64, source_path: &Arc<Path<SI>>, payload: &PacketBuffer) {}

    fn receive_error<PH: InnerProtocolInterface>(&self, si: &SI, ph: &PH, node: &Node<SI>, time_ticks: i64, source_path: &Arc<Path<SI>>, forward_secrecy: bool, extended_authentication: bool, payload: &PacketBuffer) {
        let mut cursor: usize = 1;
        if let Ok(error_header) = payload.read_struct::<message_component_structs::ErrorHeader>(&mut cursor) {
            let in_re_message_id = u64::from_ne_bytes(error_header.in_re_message_id);
            let current_packet_id_counter = self.message_id_counter.load(Ordering::Relaxed);
            if current_packet_id_counter.wrapping_sub(in_re_message_id) <= PACKET_RESPONSE_COUNTER_DELTA_MAX {
                match error_header.in_re_verb {
                    _ => {
                        ph.handle_error(self, source_path, forward_secrecy, extended_authentication, error_header.in_re_verb, in_re_message_id, error_header.error_code, payload, &mut cursor);
                    }
                }
            }
        }
    }

    fn receive_ok<PH: InnerProtocolInterface>(&self, si: &SI, ph: &PH, node: &Node<SI>, time_ticks: i64, source_path: &Arc<Path<SI>>, forward_secrecy: bool, extended_authentication: bool, payload: &PacketBuffer) {
        let mut cursor: usize = 1;
        if let Ok(ok_header) = payload.read_struct::<message_component_structs::OkHeader>(&mut cursor) {
            let in_re_message_id = u64::from_ne_bytes(ok_header.in_re_message_id);
            let current_packet_id_counter = self.message_id_counter.load(Ordering::Relaxed);
            if current_packet_id_counter.wrapping_sub(in_re_message_id) <= PACKET_RESPONSE_COUNTER_DELTA_MAX {
                match ok_header.in_re_verb {
                    verbs::VL1_HELLO => {
                        // TODO
                    }
                    verbs::VL1_WHOIS => {}
                    _ => {
                        ph.handle_ok(self, source_path, forward_secrecy, extended_authentication, ok_header.in_re_verb, in_re_message_id, payload, &mut cursor);
                    }
                }
            }
        }
    }

    fn receive_whois(&self, si: &SI, node: &Node<SI>, time_ticks: i64, source_path: &Arc<Path<SI>>, payload: &PacketBuffer) {}

    fn receive_rendezvous(&self, si: &SI, node: &Node<SI>, time_ticks: i64, source_path: &Arc<Path<SI>>, payload: &PacketBuffer) {}

    fn receive_echo(&self, si: &SI, node: &Node<SI>, time_ticks: i64, source_path: &Arc<Path<SI>>, payload: &PacketBuffer) {}

    fn receive_push_direct_paths(&self, si: &SI, node: &Node<SI>, time_ticks: i64, source_path: &Arc<Path<SI>>, payload: &PacketBuffer) {}

    fn receive_user_message(&self, si: &SI, node: &Node<SI>, time_ticks: i64, source_path: &Arc<Path<SI>>, payload: &PacketBuffer) {}

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

    /// Get either the current best direct path or an indirect path.
    pub fn path(&self, node: &Node<SI>) -> Option<Arc<Path<SI>>> {
        let direct_path = self.direct_path();
        if direct_path.is_some() {
            return direct_path;
        }
        if let Some(root) = node.root() {
            return root.direct_path();
        }
        return None;
    }

    /// Get the remote version of this peer: major, minor, revision, and build.
    /// Returns None if it's not yet known.
    pub fn version(&self) -> Option<[u16; 4]> {
        let rv = self.remote_version.load(Ordering::Relaxed);
        if rv != 0 {
            Some([(rv >> 48) as u16, (rv >> 32) as u16, (rv >> 16) as u16, rv as u16])
        } else {
            None
        }
    }

    /// Get the remote protocol version of this peer or None if not yet known.
    pub fn protocol_version(&self) -> Option<u8> {
        let pv = self.remote_protocol_version.load(Ordering::Relaxed);
        if pv != 0 {
            Some(pv)
        } else {
            None
        }
    }

    pub(crate) fn service(&self, _: &SI, _: &Node<SI>, time_ticks: i64) -> bool {
        let mut paths = self.paths.lock();
        if (time_ticks - self.last_receive_time_ticks.load(Ordering::Relaxed)) < PEER_EXPIRATION_TIME {
            paths.retain(|p| ((time_ticks - p.last_receive_time_ticks) < PEER_EXPIRATION_TIME) && (p.path.strong_count() > 0));
            paths.sort_unstable_by(|a, b| a.last_receive_time_ticks.cmp(&b.last_receive_time_ticks).reverse());
            true
        } else {
            paths.clear();
            false
        }
    }
}

impl<SI: SystemInterface> PartialEq for Peer<SI> {
    #[inline(always)]
    fn eq(&self, other: &Self) -> bool {
        self.identity.eq(&other.identity)
    }
}

impl<SI: SystemInterface> Eq for Peer<SI> {}

impl<SI: SystemInterface> Hash for Peer<SI> {
    #[inline(always)]
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.identity.hash(state);
    }
}
