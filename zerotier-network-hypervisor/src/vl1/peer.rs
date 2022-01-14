/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::convert::TryInto;
use std::mem::MaybeUninit;
use std::num::NonZeroI64;

use std::sync::Arc;
use std::sync::atomic::{AtomicI64, AtomicU64, AtomicU8, Ordering};

use arc_swap::ArcSwapOption;
use parking_lot::Mutex;

use zerotier_core_crypto::hash::{SHA384, SHA384_HASH_SIZE};
use zerotier_core_crypto::poly1305::Poly1305;
use zerotier_core_crypto::random::next_u64_secure;
use zerotier_core_crypto::salsa::Salsa;
use zerotier_core_crypto::secret::Secret;

use crate::{PacketBuffer, VERSION_MAJOR, VERSION_MINOR, VERSION_PROTO, VERSION_REVISION};
use crate::util::{array_range, u64_as_bytes};
use crate::util::buffer::Buffer;
use crate::vl1::{Endpoint, Identity, InetAddress, Path, ephemeral};
use crate::vl1::ephemeral::EphemeralSymmetricSecret;
use crate::vl1::identity::{IDENTITY_ALGORITHM_ALL, IDENTITY_ALGORITHM_X25519};
use crate::vl1::node::*;
use crate::vl1::protocol::*;
use crate::vl1::symmetricsecret::SymmetricSecret;

/// A remote peer known to this node.
/// Sending-related and receiving-related fields are locked separately since concurrent
/// send/receive is not uncommon.
pub struct Peer {
    // This peer's identity.
    identity: Identity,

    // Static shared secret computed from agreement with identity.
    static_secret: SymmetricSecret,

    // Latest ephemeral secret or None if not yet negotiated.
    ephemeral_secret: ArcSwapOption<EphemeralSymmetricSecret>,

    // Paths sorted in descending order of quality / preference.
    paths: Mutex<Vec<Arc<Path>>>,

    // Local external address most recently reported by this peer (IP transport only).
    reported_local_ip: Mutex<Option<InetAddress>>,

    // Statistics and times of events.
    last_send_time_ticks: AtomicI64,
    last_receive_time_ticks: AtomicI64,
    last_forward_time_ticks: AtomicI64,
    total_bytes_sent: AtomicU64,
    total_bytes_sent_indirect: AtomicU64,
    total_bytes_received: AtomicU64,
    total_bytes_received_indirect: AtomicU64,
    total_bytes_forwarded: AtomicU64,

    // Counter for assigning sequential message IDs.
    message_id_counter: AtomicU64,

    // Remote peer version information.
    remote_version: AtomicU64,
    remote_protocol_version: AtomicU8,
}

/// Derive per-packet key for Sals20/12 encryption (and Poly1305 authentication).
///
/// This effectively adds a few additional bits of entropy to the IV from packet
/// characteristics such as its size and direction of communication. It also
/// effectively incorporates header information as AAD, since if the header info
/// is different the key will be wrong and MAC will fail.
///
/// This is only used for Salsa/Poly modes.
#[inline(always)]
fn salsa_derive_per_packet_key(key: &Secret<48>, header: &PacketHeader, packet_size: usize) -> Secret<48> {
    let hb = header.as_bytes();
    let mut k = key.clone();
    for i in 0..18 {
        k.0[i] ^= hb[i];
    }
    k.0[18] ^= hb[HEADER_FLAGS_FIELD_INDEX] & HEADER_FLAGS_FIELD_MASK_HIDE_HOPS;
    k.0[19] ^= (packet_size >> 8) as u8;
    k.0[20] ^= packet_size as u8;
    k
}

/// Create initialized instances of Salsa20/12 and Poly1305 for a packet.
#[inline(always)]
fn salsa_poly_create(secret: &SymmetricSecret, header: &PacketHeader, packet_size: usize) -> (Salsa, Poly1305) {
    let key = salsa_derive_per_packet_key(&secret.key, header, packet_size);
    let mut salsa = Salsa::new(&key.0[0..32], &header.id, true).unwrap();
    let mut poly1305_key = [0_u8; 32];
    salsa.crypt_in_place(&mut poly1305_key);
    (salsa, Poly1305::new(&poly1305_key).unwrap())
}

/// Attempt AEAD packet encryption and MAC validation.
fn try_aead_decrypt(secret: &SymmetricSecret, packet_frag0_payload_bytes: &[u8], header: &PacketHeader, fragments: &[Option<PacketBuffer>], payload: &mut Buffer<PACKET_SIZE_MAX>) -> Option<u64> {
    packet_frag0_payload_bytes.get(0).map_or(None, |verb| {
        match header.cipher() {
            CIPHER_NOCRYPT_POLY1305 => {
                if (verb & VERB_MASK) == VERB_VL1_HELLO {
                    let mut total_packet_len = packet_frag0_payload_bytes.len() + PACKET_HEADER_SIZE;
                    for f in fragments.iter() {
                        total_packet_len += f.as_ref().map_or(0, |f| f.len());
                    }
                    let _ = payload.append_bytes(packet_frag0_payload_bytes);
                    for f in fragments.iter() {
                        let _ = f.as_ref().map(|f| f.as_bytes_starting_at(FRAGMENT_HEADER_SIZE).map(|f| payload.append_bytes(f)));
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

            CIPHER_SALSA2012_POLY1305 => {
                let mut total_packet_len = packet_frag0_payload_bytes.len() + PACKET_HEADER_SIZE;
                for f in fragments.iter() {
                    total_packet_len += f.as_ref().map_or(0, |f| f.len());
                }
                let (mut salsa, mut poly) = salsa_poly_create(secret, header, total_packet_len);
                poly.update(packet_frag0_payload_bytes);
                let _ = payload.append_bytes_get_mut(packet_frag0_payload_bytes.len()).map(|b| salsa.crypt(packet_frag0_payload_bytes, b));
                for f in fragments.iter() {
                    let _ = f.as_ref().map(|f| f.as_bytes_starting_at(FRAGMENT_HEADER_SIZE).map(|f| {
                        poly.update(f);
                        let _ = payload.append_bytes_get_mut(f.len()).map(|b| salsa.crypt(f, b));
                    }));
                }
                if poly.finish()[0..8].eq(&header.mac) {
                    Some(u64::from_ne_bytes(header.id))
                } else {
                    None
                }
            }

            CIPHER_AES_GMAC_SIV => {
                let mut aes = secret.aes_gmac_siv.get();
                aes.decrypt_init(&header.aes_gmac_siv_tag());
                aes.decrypt_set_aad(&header.aad_bytes());
                // NOTE: if there are somehow missing fragments this part will silently fail,
                // but the packet will fail MAC check in decrypt_finish() so meh.
                let _ = payload.append_bytes_get_mut(packet_frag0_payload_bytes.len()).map(|b| aes.decrypt(packet_frag0_payload_bytes, b));
                for f in fragments.iter() {
                    f.as_ref().map(|f| {
                        f.as_bytes_starting_at(FRAGMENT_HEADER_SIZE).map(|f| {
                            let _ = payload.append_bytes_get_mut(f.len()).map(|b| aes.decrypt(f, b));
                        })
                    });
                }
                aes.decrypt_finish().map_or(None, |tag| {
                    // AES-GMAC-SIV encrypts the packet ID too as part of its computation of a single
                    // opaque 128-bit tag, so to get the original packet ID we have to grab it from the
                    // decrypted tag.
                    Some(u64::from_ne_bytes(*array_range::<u8, 16, 0, 8>(tag)))
                })
            }

            _ => None,
        }
    })
}

impl Peer {
    /// Create a new peer.
    /// This only returns None if this_node_identity does not have its secrets or if some
    /// fatal error occurs performing key agreement between the two identities.
    pub(crate) fn new(this_node_identity: &Identity, id: Identity) -> Option<Peer> {
        this_node_identity.agree(&id).map(|static_secret| -> Peer {
            Peer {
                identity: id,
                static_secret: SymmetricSecret::new(static_secret),
                ephemeral_secret: ArcSwapOption::const_empty(),
                paths: Mutex::new(Vec::new()),
                reported_local_ip: Mutex::new(None),
                last_send_time_ticks: AtomicI64::new(0),
                last_receive_time_ticks: AtomicI64::new(0),
                last_forward_time_ticks: AtomicI64::new(0),
                total_bytes_sent: AtomicU64::new(0),
                total_bytes_sent_indirect: AtomicU64::new(0),
                total_bytes_received: AtomicU64::new(0),
                total_bytes_received_indirect: AtomicU64::new(0),
                total_bytes_forwarded: AtomicU64::new(0),
                message_id_counter: AtomicU64::new(next_u64_secure()),
                remote_version: AtomicU64::new(0),
                remote_protocol_version: AtomicU8::new(0),
            }
        })
    }

    /// Get the next message ID.
    #[inline(always)]
    pub(crate) fn next_message_id(&self) -> u64 { self.message_id_counter.fetch_add(1, Ordering::Relaxed) }

    /// Receive, decrypt, authenticate, and process an incoming packet from this peer.
    /// If the packet comes in multiple fragments, the fragments slice should contain all
    /// those fragments after the main packet header and first chunk.
    pub(crate) fn receive<CI: SystemInterface, PH: VL1VirtualInterface>(&self, node: &Node, ci: &CI, ph: &PH, time_ticks: i64, source_endpoint: &Endpoint, source_path: &Arc<Path>, header: &PacketHeader, packet: &Buffer<{ PACKET_SIZE_MAX }>, fragments: &[Option<PacketBuffer>]) {
        let _ = packet.as_bytes_starting_at(PACKET_VERB_INDEX).map(|packet_frag0_payload_bytes| {
            let mut payload: Buffer<PACKET_SIZE_MAX> = unsafe { Buffer::new_without_memzero() };

            let (forward_secrecy, mut message_id) = if let Some(ephemeral_secret) = self.ephemeral_secret.load_full() {
                if let Some(message_id) = try_aead_decrypt(&ephemeral_secret.secret, packet_frag0_payload_bytes, header, fragments, &mut payload) {
                    ephemeral_secret.decrypt_uses.fetch_add(1, Ordering::Relaxed);
                    (true, message_id)
                } else {
                    (false, 0)
                }
            } else {
                (false, 0)
            };
            if !forward_secrecy {
                if let Some(message_id2) = try_aead_decrypt(&self.static_secret, packet_frag0_payload_bytes, header, fragments, &mut payload) {
                    message_id = message_id2;
                } else {
                    // Packet failed to decrypt using either ephemeral or permament key, reject.
                    return;
                }
            }
            debug_assert!(!payload.is_empty());

            // ---------------------------------------------------------------
            // If we made it here it decrypted and passed authentication.
            // ---------------------------------------------------------------

            self.last_receive_time_ticks.store(time_ticks, Ordering::Relaxed);
            self.total_bytes_received.fetch_add((payload.len() + PACKET_HEADER_SIZE) as u64, Ordering::Relaxed);
            source_path.log_receive_authenticated_packet(payload.len() + PACKET_HEADER_SIZE, source_endpoint);

            let mut verb = payload.as_bytes()[0];

            // If this flag is set, the end of the payload is a full HMAC-SHA384 authentication
            // tag for much stronger authentication than is offered by the packet MAC.
            let extended_authentication = (verb & VERB_FLAG_EXTENDED_AUTHENTICATION) != 0;
            if extended_authentication {
                if payload.len() >= (1 + SHA384_HASH_SIZE) {
                    let actual_end_of_payload = payload.len() - SHA384_HASH_SIZE;
                    let hmac = SHA384::hmac_multipart(self.static_secret.packet_hmac_key.as_ref(), &[u64_as_bytes(&message_id), payload.as_bytes()]);
                    if !hmac.eq(&(payload.as_bytes()[actual_end_of_payload..])) {
                        return;
                    }
                    payload.set_size(actual_end_of_payload);
                } else {
                    return;
                }
            }

            if (verb & VERB_FLAG_COMPRESSED) != 0 {
                let mut decompressed_payload: [u8; PACKET_SIZE_MAX] = unsafe { MaybeUninit::uninit().assume_init() };
                decompressed_payload[0] = verb;
                let dlen = lz4_flex::block::decompress_into(&payload.as_bytes()[1..], &mut decompressed_payload[1..]);
                if dlen.is_ok() {
                    payload.set_to(&decompressed_payload[0..(dlen.unwrap() + 1)]);
                } else {
                    return;
                }
            }

            // For performance reasons we let VL2 handle packets first. It returns false
            // if it didn't handle the packet, in which case it's handled at VL1. This is
            // because the most performance critical path is the handling of the ???_FRAME
            // verbs, which are in VL2.
            verb &= VERB_MASK; // mask off flags
            if !ph.handle_packet(self, source_path, forward_secrecy, extended_authentication, verb, &payload) {
                match verb {
                    //VERB_VL1_NOP => {}
                    VERB_VL1_HELLO => self.receive_hello(ci, node, time_ticks, source_path, &payload),
                    VERB_VL1_ERROR => self.receive_error(ci, ph, node, time_ticks, source_path, forward_secrecy, extended_authentication, &payload),
                    VERB_VL1_OK => self.receive_ok(ci, ph, node, time_ticks, source_path, forward_secrecy, extended_authentication, &payload),
                    VERB_VL1_WHOIS => self.receive_whois(ci, node, time_ticks, source_path, &payload),
                    VERB_VL1_RENDEZVOUS => self.receive_rendezvous(ci, node, time_ticks, source_path, &payload),
                    VERB_VL1_ECHO => self.receive_echo(ci, node, time_ticks, source_path, &payload),
                    VERB_VL1_PUSH_DIRECT_PATHS => self.receive_push_direct_paths(ci, node, time_ticks, source_path, &payload),
                    VERB_VL1_USER_MESSAGE => self.receive_user_message(ci, node, time_ticks, source_path, &payload),
                    _ => {}
                }
            } else {
                #[cfg(debug)] {
                    if match verb {
                        VERB_VL1_NOP | VERB_VL1_HELLO | VERB_VL1_ERROR | VERB_VL1_OK | VERB_VL1_WHOIS | VERB_VL1_RENDEZVOUS | VERB_VL1_ECHO | VERB_VL1_PUSH_DIRECT_PATHS | VERB_VL1_USER_MESSAGE => true,
                        _ => false
                    } {
                        panic!("The next layer handled a VL1 packet! It should not do this.");
                    }
                }
            }
        });
    }

    fn send_to_endpoint<CI: SystemInterface>(&self, ci: &CI, endpoint: &Endpoint, local_socket: Option<NonZeroI64>, local_interface: Option<NonZeroI64>, packet: &Buffer<{ PACKET_SIZE_MAX }>) -> bool {
        debug_assert!(packet.len() <= PACKET_SIZE_MAX);
        debug_assert!(packet.len() >= PACKET_SIZE_MIN);
        match endpoint {
            Endpoint::Ip(_) | Endpoint::IpUdp(_) | Endpoint::Ethernet(_) | Endpoint::Bluetooth(_) | Endpoint::WifiDirect(_) => {
                let packet_size = packet.len();
                if packet_size > UDP_DEFAULT_MTU {
                    let bytes = packet.as_bytes();
                    if !ci.wire_send(endpoint, local_socket, local_interface, &[&bytes[0..UDP_DEFAULT_MTU]], 0) {
                        return false;
                    }

                    let mut pos = UDP_DEFAULT_MTU;

                    let overrun_size = (packet_size - UDP_DEFAULT_MTU) as u32;
                    let fragment_count = (overrun_size / (UDP_DEFAULT_MTU - FRAGMENT_HEADER_SIZE) as u32) + (((overrun_size % (UDP_DEFAULT_MTU - FRAGMENT_HEADER_SIZE) as u32) != 0) as u32);
                    debug_assert!(fragment_count <= PACKET_FRAGMENT_COUNT_MAX as u32);

                    let mut header = FragmentHeader {
                        id: unsafe { *packet.as_bytes().as_ptr().cast::<[u8; 8]>() },
                        dest: bytes[PACKET_DESTINATION_INDEX..PACKET_DESTINATION_INDEX + ADDRESS_SIZE].try_into().unwrap(),
                        fragment_indicator: PACKET_FRAGMENT_INDICATOR,
                        total_and_fragment_no: ((fragment_count + 1) << 4) as u8,
                        reserved_hops: 0,
                    };

                    let mut chunk_size = (packet_size - pos).min(UDP_DEFAULT_MTU - FRAGMENT_HEADER_SIZE);
                    loop {
                        header.total_and_fragment_no += 1;
                        let next_pos = pos + chunk_size;
                        if !ci.wire_send(endpoint, local_socket, local_interface, &[header.as_bytes(), &bytes[pos..next_pos]], 0) {
                            return false;
                        }
                        pos = next_pos;
                        if pos < packet_size {
                            chunk_size = (packet_size - pos).min(UDP_DEFAULT_MTU - FRAGMENT_HEADER_SIZE);
                        } else {
                            return true;
                        }
                    }
                } else {
                    return ci.wire_send(endpoint, local_socket, local_interface, &[packet.as_bytes()], 0);
                }
            }
            _ => {
                return ci.wire_send(endpoint, local_socket, local_interface, &[packet.as_bytes()], 0);
            }
        }
    }

    /// Send a packet to this peer.
    ///
    /// This will go directly if there is an active path, or otherwise indirectly
    /// via a root or some other route.
    pub(crate) fn send<CI: SystemInterface>(&self, ci: &CI, node: &Node, time_ticks: i64, packet: &Buffer<{ PACKET_SIZE_MAX }>) -> bool {
        self.path(node).map_or(false, |path| {
            if self.send_to_endpoint(ci, path.endpoint().as_ref(), path.local_socket(), path.local_interface(), packet) {
                self.last_send_time_ticks.store(time_ticks, Ordering::Relaxed);
                self.total_bytes_sent.fetch_add(packet.len() as u64, Ordering::Relaxed);
                true
            } else {
                false
            }
        })
    }

    /// Forward a packet to this peer.
    ///
    /// This is called when we receive a packet not addressed to this node and
    /// want to pass it along.
    ///
    /// This doesn't fragment large packets since fragments are forwarded individually.
    /// Intermediates don't need to adjust fragmentation.
    pub(crate) fn forward<CI: SystemInterface>(&self, ci: &CI, time_ticks: i64, packet: &Buffer<{ PACKET_SIZE_MAX }>) -> bool {
        self.direct_path().map_or(false, |path| {
            if ci.wire_send(path.endpoint().as_ref(), path.local_socket(), path.local_interface(), &[packet.as_bytes()], 0) {
                self.last_forward_time_ticks.store(time_ticks, Ordering::Relaxed);
                self.total_bytes_forwarded.fetch_add(packet.len() as u64, Ordering::Relaxed);
                true
            } else {
                false
            }
        })
    }

    /// Send a HELLO to this peer.
    ///
    /// If explicit_endpoint is not None the packet will be sent directly to this endpoint.
    /// Otherwise it will be sent via the best direct or indirect path known.
    pub(crate) fn send_hello<CI: SystemInterface>(&self, ci: &CI, node: &Node, explicit_endpoint: Option<&Endpoint>) -> bool {
        let mut packet: Buffer<{ PACKET_SIZE_MAX }> = Buffer::new();
        let time_ticks = ci.time_ticks();

        let message_id = self.next_message_id();
        {
            let packet_header: &mut PacketHeader = packet.append_struct_get_mut().unwrap();
            packet_header.id = message_id.to_ne_bytes(); // packet ID and message ID are the same when Poly1305 MAC is used
            packet_header.dest = self.identity.address.to_bytes();
            packet_header.src = node.identity.address.to_bytes();
            packet_header.flags_cipher_hops = CIPHER_NOCRYPT_POLY1305;
        }
        {
            let hello_fixed_headers: &mut message_component_structs::HelloFixedHeaderFields = packet.append_struct_get_mut().unwrap();
            hello_fixed_headers.verb = VERB_VL1_HELLO | VERB_FLAG_EXTENDED_AUTHENTICATION;
            hello_fixed_headers.version_proto = VERSION_PROTO;
            hello_fixed_headers.version_major = VERSION_MAJOR;
            hello_fixed_headers.version_minor = VERSION_MINOR;
            hello_fixed_headers.version_revision = (VERSION_REVISION as u16).to_be_bytes();
            hello_fixed_headers.timestamp = (time_ticks as u64).to_be_bytes();
        }

        assert!(self.identity.marshal(&mut packet, IDENTITY_ALGORITHM_ALL, false).is_ok());
        if self.identity.algorithms() == IDENTITY_ALGORITHM_X25519 {
            // LEGACY: append an extra zero when marshaling identities containing only
            // x25519 keys. This is interpreted as an empty InetAddress by old nodes.
            // This isn't needed if a NIST P-521 key or other new key types are present.
            // See comments before IDENTITY_CIPHER_SUITE_EC_NIST_P521 in identity.rs.
            assert!(packet.append_u8(0).is_ok());
        }

        assert!(packet.append_u64(0).is_ok()); // reserved, must be zero for legacy compatibility
        assert!(packet.append_u64(node.instance_id).is_ok());

        // LEGACY: create a 16-bit encrypted field that specifies zero "moons." This is ignored now
        // but causes old nodes to be able to parse this packet properly. This is not significant in
        // terms of encryption or authentication and can disappear once old versions are dead. Newer
        // versions ignore these bytes.
        let zero_moon_count = packet.append_bytes_fixed_get_mut::<2>().unwrap();
        let mut salsa_iv = message_id.to_ne_bytes();
        salsa_iv[7] &= 0xf8;
        Salsa::new(&self.static_secret.key.0[0..32], &salsa_iv, true).unwrap().crypt(&[0_u8, 0_u8], zero_moon_count);

        // Size of dictionary with optional fields, currently none. For future use.
        assert!(packet.append_u16(0).is_ok());

        // Add full HMAC for strong authentication with newer nodes.
        assert!(packet.append_bytes_fixed(&SHA384::hmac_multipart(&self.static_secret.packet_hmac_key.0, &[u64_as_bytes(&message_id), &packet.as_bytes()[PACKET_HEADER_SIZE..]])).is_ok());

        // LEGACY: set MAC field in header with poly1305 for older nodes.
        // Newer nodes use the HMAC for stronger verification.
        let (_, mut poly) = salsa_poly_create(&self.static_secret, packet.struct_at::<PacketHeader>(0).unwrap(), packet.len());
        poly.update(packet.as_bytes_starting_at(PACKET_HEADER_SIZE).unwrap());
        packet.as_mut_range_fixed::<HEADER_MAC_FIELD_INDEX, { HEADER_MAC_FIELD_INDEX + 8 }>().copy_from_slice(&poly.finish()[0..8]);

        self.last_send_time_ticks.store(time_ticks, Ordering::Relaxed);
        self.total_bytes_sent.fetch_add(packet.len() as u64, Ordering::Relaxed);

        explicit_endpoint.map_or_else(|| {
            self.path(node).map_or(false, |path| {
                path.log_send_anything(time_ticks);
                self.send_to_endpoint(ci, path.endpoint().as_ref(), path.local_socket(), path.local_interface(), &packet)
            })
        }, |endpoint| {
            self.send_to_endpoint(ci, endpoint, None, None, &packet)
        })
    }

    pub(crate) const CALL_EVERY_INTERVAL_MS: i64 = EPHEMERAL_SECRET_REKEY_AFTER_TIME / 10;

    /// Called every INTERVAL during background tasks.
    #[inline(always)]
    pub(crate) fn call_every_interval<CI: SystemInterface>(&self, ct: &CI, time_ticks: i64) {}

    #[inline(always)]
    fn receive_hello<CI: SystemInterface>(&self, ci: &CI, node: &Node, time_ticks: i64, source_path: &Arc<Path>, payload: &Buffer<{ PACKET_SIZE_MAX }>) {}

    #[inline(always)]
    fn receive_error<CI: SystemInterface, PH: VL1VirtualInterface>(&self, ci: &CI, ph: &PH, node: &Node, time_ticks: i64, source_path: &Arc<Path>, forward_secrecy: bool, extended_authentication: bool, payload: &Buffer<{ PACKET_SIZE_MAX }>) {
        let mut cursor: usize = 0;
        let _ = payload.read_struct::<message_component_structs::ErrorHeader>(&mut cursor).map(|error_header| {
            let in_re_message_id = u64::from_ne_bytes(error_header.in_re_message_id);
            let current_packet_id_counter = self.message_id_counter.load(Ordering::Relaxed);
            if current_packet_id_counter.wrapping_sub(in_re_message_id) <= PACKET_RESPONSE_COUNTER_DELTA_MAX {
                match error_header.in_re_verb {
                    _ => {
                        ph.handle_error(self, source_path, forward_secrecy, extended_authentication, error_header.in_re_verb, in_re_message_id, error_header.error_code, payload, &mut cursor);
                    }
                }
            }
        });
    }

    #[inline(always)]
    fn receive_ok<CI: SystemInterface, PH: VL1VirtualInterface>(&self, ci: &CI, ph: &PH, node: &Node, time_ticks: i64, source_path: &Arc<Path>, forward_secrecy: bool, extended_authentication: bool, payload: &Buffer<{ PACKET_SIZE_MAX }>) {
        let mut cursor: usize = 0;
        let _ = payload.read_struct::<message_component_structs::OkHeader>(&mut cursor).map(|ok_header| {
            let in_re_message_id = u64::from_ne_bytes(ok_header.in_re_message_id);
            let current_packet_id_counter = self.message_id_counter.load(Ordering::Relaxed);
            if current_packet_id_counter.wrapping_sub(in_re_message_id) <= PACKET_RESPONSE_COUNTER_DELTA_MAX {
                match ok_header.in_re_verb {
                    VERB_VL1_HELLO => {
                    }
                    VERB_VL1_WHOIS => {
                    }
                    _ => {
                        ph.handle_ok(self, source_path, forward_secrecy, extended_authentication, ok_header.in_re_verb, in_re_message_id, payload, &mut cursor);
                    }
                }
            }
        });
    }

    #[inline(always)]
    fn receive_whois<CI: SystemInterface>(&self, ci: &CI, node: &Node, time_ticks: i64, source_path: &Arc<Path>, payload: &Buffer<{ PACKET_SIZE_MAX }>) {}

    #[inline(always)]
    fn receive_rendezvous<CI: SystemInterface>(&self, ci: &CI, node: &Node, time_ticks: i64, source_path: &Arc<Path>, payload: &Buffer<{ PACKET_SIZE_MAX }>) {}

    #[inline(always)]
    fn receive_echo<CI: SystemInterface>(&self, ci: &CI, node: &Node, time_ticks: i64, source_path: &Arc<Path>, payload: &Buffer<{ PACKET_SIZE_MAX }>) {}

    #[inline(always)]
    fn receive_push_direct_paths<CI: SystemInterface>(&self, ci: &CI, node: &Node, time_ticks: i64, source_path: &Arc<Path>, payload: &Buffer<{ PACKET_SIZE_MAX }>) {}

    #[inline(always)]
    fn receive_user_message<CI: SystemInterface>(&self, ci: &CI, node: &Node, time_ticks: i64, source_path: &Arc<Path>, payload: &Buffer<{ PACKET_SIZE_MAX }>) {}

    /// Get current best path or None if there are no direct paths to this peer.
    #[inline(always)]
    pub fn direct_path(&self) -> Option<Arc<Path>> {
        self.paths.lock().first().map(|p| p.clone())
    }

    /// Get either the current best direct path or an indirect path.
    pub fn path(&self, node: &Node) -> Option<Arc<Path>> {
        self.direct_path().map_or_else(|| node.root().map_or(None, |root| root.direct_path().map_or(None, |bp| Some(bp))), |bp| Some(bp))
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
}
