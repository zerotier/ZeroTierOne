use std::convert::TryInto;
use std::mem::MaybeUninit;
use std::ptr::copy_nonoverlapping;
use std::sync::Arc;
use std::sync::atomic::{AtomicI64, AtomicU64, AtomicU8, Ordering};

use parking_lot::Mutex;

use zerotier_core_crypto::aes_gmac_siv::{AesCtr, AesGmacSiv};
use zerotier_core_crypto::c25519::C25519KeyPair;
use zerotier_core_crypto::hash::{SHA384, SHA384_HASH_SIZE};
use zerotier_core_crypto::kbkdf::zt_kbkdf_hmac_sha384;
use zerotier_core_crypto::p521::P521KeyPair;
use zerotier_core_crypto::poly1305::Poly1305;
use zerotier_core_crypto::random::next_u64_secure;
use zerotier_core_crypto::salsa::Salsa;
use zerotier_core_crypto::secret::Secret;

use crate::{VERSION_MAJOR, VERSION_MINOR, VERSION_PROTO, VERSION_REVISION};
use crate::defaults::UDP_DEFAULT_MTU;
use crate::util::pool::{Pool, PoolFactory};
use crate::vl1::{Dictionary, Endpoint, Identity, InetAddress, Path};
use crate::vl1::buffer::Buffer;
use crate::vl1::node::*;
use crate::vl1::protocol::*;

/// Interval for servicing and background operations on peers.
pub(crate) const PEER_SERVICE_INTERVAL: i64 = 30000;

struct AesGmacSivPoolFactory(Secret<48>, Secret<48>);

impl PoolFactory<AesGmacSiv> for AesGmacSivPoolFactory {
    #[inline(always)]
    fn create(&self) -> AesGmacSiv { AesGmacSiv::new(&self.0.0[0..32], &self.1.0[0..32]) }

    #[inline(always)]
    fn reset(&self, obj: &mut AesGmacSiv) { obj.reset(); }
}

struct PeerSecret {
    // Time secret was created in ticks for ephemeral secrets, or -1 for static secrets.
    create_time_ticks: i64,

    // Number of times secret has been used to encrypt something during this session.
    encrypt_count: AtomicU64,

    // Raw secret itself.
    secret: Secret<48>,

    // Reusable AES-GMAC-SIV ciphers initialized with secret.
    // These can't be used concurrently so they're pooled to allow low-contention concurrency.
    aes: Pool<AesGmacSiv, AesGmacSivPoolFactory>,
}

struct EphemeralKeyPair {
    // Time ephemeral key pair was created.
    create_time_ticks: i64,

    // SHA384(c25519 public | p521 public)
    public_keys_hash: [u8; 48],

    // Curve25519 ECDH key pair.
    c25519: C25519KeyPair,

    // NIST P-521 ECDH key pair.
    p521: P521KeyPair,
}

/// A remote peer known to this node.
/// Sending-related and receiving-related fields are locked separately since concurrent
/// send/receive is not uncommon.
pub struct Peer {
    // This peer's identity.
    identity: Identity,

    // Static shared secret computed from agreement with identity.
    static_secret: PeerSecret,

    // Derived static secret (in initialized cipher) used to encrypt the dictionary part of HELLO.
    static_secret_hello_dictionary: Mutex<AesCtr>,

    // Derived static secret used to add full HMAC-SHA384 to packets, currently just HELLO.
    static_secret_packet_hmac: Secret<48>,

    // Latest ephemeral secret acknowledged with OK(HELLO).
    ephemeral_secret: Mutex<Option<Arc<PeerSecret>>>,

    // Either None or the current ephemeral key pair whose public keys are on offer.
    ephemeral_pair: Mutex<Option<EphemeralKeyPair>>,

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

    // Counter for assigning packet IV's a.k.a. PacketIDs.
    packet_id_counter: AtomicU64,

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
fn salsa_poly_create(secret: &PeerSecret, header: &PacketHeader, packet_size: usize) -> (Salsa, Poly1305) {
    let key = salsa_derive_per_packet_key(&secret.secret, header, packet_size);
    let mut salsa = Salsa::new(&key.0[0..32], header.id_bytes(), true).unwrap();
    let mut poly1305_key = [0_u8; 32];
    salsa.crypt_in_place(&mut poly1305_key);
    (salsa, Poly1305::new(&poly1305_key).unwrap())
}

impl Peer {
    pub(crate) const INTERVAL: i64 = PEER_SERVICE_INTERVAL;

    /// Create a new peer.
    /// This only returns None if this_node_identity does not have its secrets or if some
    /// fatal error occurs performing key agreement between the two identities.
    pub(crate) fn new(this_node_identity: &Identity, id: Identity) -> Option<Peer> {
        this_node_identity.agree(&id).map(|static_secret| {
            let aes_factory = AesGmacSivPoolFactory(
                zt_kbkdf_hmac_sha384(&static_secret.0, KBKDF_KEY_USAGE_LABEL_AES_GMAC_SIV_K0, 0, 0),
                zt_kbkdf_hmac_sha384(&static_secret.0, KBKDF_KEY_USAGE_LABEL_AES_GMAC_SIV_K1, 0, 0));
            let static_secret_hello_dictionary = zt_kbkdf_hmac_sha384(&static_secret.0, KBKDF_KEY_USAGE_LABEL_HELLO_DICTIONARY_ENCRYPT, 0, 0);
            let static_secret_packet_hmac = zt_kbkdf_hmac_sha384(&static_secret.0, KBKDF_KEY_USAGE_LABEL_PACKET_HMAC, 0, 0);
            Peer {
                identity: id,
                static_secret: PeerSecret {
                    create_time_ticks: -1,
                    encrypt_count: AtomicU64::new(0),
                    secret: static_secret,
                    aes: Pool::new(4, aes_factory),
                },
                static_secret_hello_dictionary: Mutex::new(AesCtr::new(&static_secret_hello_dictionary.0[0..32])),
                static_secret_packet_hmac,
                ephemeral_secret: Mutex::new(None),
                ephemeral_pair: Mutex::new(None),
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
                packet_id_counter: AtomicU64::new(next_u64_secure()),
                remote_version: AtomicU64::new(0),
                remote_protocol_version: AtomicU8::new(0),
            }
        })
    }

    /// Get the next packet ID / IV.
    #[inline(always)]
    pub(crate) fn next_packet_id(&self) -> PacketID { self.packet_id_counter.fetch_add(1, Ordering::Relaxed) }

    /// Receive, decrypt, authenticate, and process an incoming packet from this peer.
    /// If the packet comes in multiple fragments, the fragments slice should contain all
    /// those fragments after the main packet header and first chunk.
    pub(crate) fn receive<CI: VL1CallerInterface, PH: VL1PacketHandler>(&self, node: &Node, ci: &CI, ph: &PH, time_ticks: i64, source_path: &Arc<Path>, header: &PacketHeader, packet: &Buffer<{ PACKET_SIZE_MAX }>, fragments: &[Option<PacketBuffer>]) {
        let _ = packet.as_bytes_starting_at(PACKET_VERB_INDEX).map(|packet_frag0_payload_bytes| {
            let mut payload = node.get_packet_buffer();

            let mut forward_secrecy = true; // set to false below if ephemeral fails
            let mut packet_id = header.id as u64;
            let cipher = header.cipher();
            let ephemeral_secret = self.ephemeral_secret.lock().clone();
            for secret in [ephemeral_secret.as_ref().map_or(&self.static_secret, |s| s.as_ref()), &self.static_secret] {
                match cipher {
                    CIPHER_NOCRYPT_POLY1305 => {
                        if (packet_frag0_payload_bytes[0] & VERB_MASK) == VERB_VL1_HELLO {
                            let _ = payload.append_bytes(packet_frag0_payload_bytes);
                            for f in fragments.iter() {
                                let _ = f.as_ref().map(|f| f.as_bytes_starting_at(FRAGMENT_HEADER_SIZE).map(|f| payload.append_bytes(f)));
                            }

                            // FIPS note: for FIPS purposes the HMAC-SHA384 tag at the end of V2 HELLOs
                            // will be considered the "real" handshake authentication. This authentication
                            // is technically deprecated in V2.
                            let (_, mut poly) = salsa_poly_create(secret, header, packet.len());
                            poly.update(payload.as_bytes());

                            if poly.finish()[0..8].eq(&header.message_auth) {
                                break;
                            }
                        } else {
                            // Only HELLO is permitted without payload encryption. Drop other packet types if sent this way.
                            return;
                        }
                    }

                    CIPHER_SALSA2012_POLY1305 => {
                        let (mut salsa, mut poly) = salsa_poly_create(secret, header, packet.len());
                        poly.update(packet_frag0_payload_bytes);
                        let _ = payload.append_and_init_bytes(packet_frag0_payload_bytes.len(), |b| salsa.crypt(packet_frag0_payload_bytes, b));
                        for f in fragments.iter() {
                            let _ = f.as_ref().map(|f| f.as_bytes_starting_at(FRAGMENT_HEADER_SIZE).map(|f| {
                                poly.update(f);
                                let _ = payload.append_and_init_bytes(f.len(), |b| salsa.crypt(f, b));
                            }));
                        }
                        if poly.finish()[0..8].eq(&header.message_auth) {
                            break;
                        }
                    }

                    CIPHER_AES_GMAC_SIV => {
                        let mut aes = secret.aes.get();
                        aes.decrypt_init(&header.aes_gmac_siv_tag());
                        aes.decrypt_set_aad(&header.aad_bytes());
                        let _ = payload.append_and_init_bytes(packet_frag0_payload_bytes.len(), |b| aes.decrypt(packet_frag0_payload_bytes, b));
                        for f in fragments.iter() {
                            let _ = f.as_ref().map(|f| f.as_bytes_starting_at(FRAGMENT_HEADER_SIZE).map(|f| payload.append_and_init_bytes(f.len(), |b| aes.decrypt(f, b))));
                        }
                        let tag = aes.decrypt_finish();
                        if tag.is_some() {
                            // For AES-GMAC-SIV we need to grab the original packet ID from the decrypted tag.
                            let tag = tag.unwrap();
                            unsafe { copy_nonoverlapping(tag.as_ptr(), (&mut packet_id as *mut u64).cast(), 8) };
                            break;
                        }
                    }

                    _ => {
                        return;
                    }
                }

                if (secret as *const PeerSecret) == (&self.static_secret as *const PeerSecret) {
                    // If the static secret failed to authenticate it means we either didn't have an
                    // ephemeral key or the ephemeral also failed (as it's tried first).
                    return;
                } else {
                    // If ephemeral failed, static secret will be tried. Set forward secrecy to false.
                    forward_secrecy = false;
                    payload.clear();
                }
            }
            drop(ephemeral_secret);

            // If decryption and authentication succeeded, the code above will break out of the
            // for loop and end up here. Otherwise it returns from the whole function.

            self.last_receive_time_ticks.store(time_ticks, Ordering::Relaxed);
            self.total_bytes_received.fetch_add((payload.len() + PACKET_HEADER_SIZE) as u64, Ordering::Relaxed);

            let _ = payload.u8_at(0).map(|mut verb| {
                let extended_authentication = (verb & VERB_FLAG_EXTENDED_AUTHENTICATION) != 0;
                if extended_authentication {
                    let auth_bytes = payload.as_bytes();
                    if auth_bytes.len() >= (1 + SHA384_HASH_SIZE) {
                        let packet_hmac_start = auth_bytes.len() - SHA384_HASH_SIZE;
                        if !SHA384::hmac(self.static_secret_packet_hmac.as_ref(), &auth_bytes[1..packet_hmac_start]).eq(&auth_bytes[packet_hmac_start..]) {
                            return;
                        }
                        let new_len = payload.len() - SHA384_HASH_SIZE;
                        payload.set_size(new_len);
                    } else {
                        return;
                    }
                }

                if (verb & VERB_FLAG_COMPRESSED) != 0 {
                    let mut decompressed_payload = node.get_packet_buffer();
                    let _ = decompressed_payload.append_u8(verb);
                    let dlen = lz4_flex::block::decompress_into(&payload.as_bytes()[1..], &mut decompressed_payload.as_bytes_mut(), 1);
                    if dlen.is_ok() {
                        decompressed_payload.set_size(dlen.unwrap());
                        payload = decompressed_payload;
                    } else {
                        return;
                    }
                }

                // For performance reasons we let VL2 handle packets first. It returns false
                // if it didn't handle the packet, in which case it's handled at VL1. This is
                // because the most performance critical path is the handling of the ???_FRAME
                // verbs, which are in VL2.
                verb &= VERB_MASK;
                if !ph.handle_packet(self, source_path, forward_secrecy, extended_authentication, verb, payload.as_ref()) {
                    match verb {
                        //VERB_VL1_NOP => {}
                        VERB_VL1_HELLO => self.receive_hello(ci, node, time_ticks, source_path, payload.as_ref()),
                        VERB_VL1_ERROR => self.receive_error(ci, ph, node, time_ticks, source_path, forward_secrecy, extended_authentication, payload.as_ref()),
                        VERB_VL1_OK => self.receive_ok(ci, ph, node, time_ticks, source_path, forward_secrecy, extended_authentication, payload.as_ref()),
                        VERB_VL1_WHOIS => self.receive_whois(ci, node, time_ticks, source_path, payload.as_ref()),
                        VERB_VL1_RENDEZVOUS => self.receive_rendezvous(ci, node, time_ticks, source_path, payload.as_ref()),
                        VERB_VL1_ECHO => self.receive_echo(ci, node, time_ticks, source_path, payload.as_ref()),
                        VERB_VL1_PUSH_DIRECT_PATHS => self.receive_push_direct_paths(ci, node, time_ticks, source_path, payload.as_ref()),
                        VERB_VL1_USER_MESSAGE => self.receive_user_message(ci, node, time_ticks, source_path, payload.as_ref()),
                        _ => {}
                    }
                }
            });
        });
    }

    fn send_to_endpoint<CI: VL1CallerInterface>(&self, ci: &CI, endpoint: &Endpoint, local_socket: Option<i64>, local_interface: Option<i64>, packet_id: PacketID, packet: &Buffer<{ PACKET_SIZE_MAX }>) -> bool {
        debug_assert!(packet.len() <= PACKET_SIZE_MAX);
        if matches!(endpoint, Endpoint::IpUdp(_)) {
            let packet_size = packet.len();
            if packet_size > UDP_DEFAULT_MTU {
                let bytes = packet.as_bytes();
                if !ci.wire_send(endpoint, local_socket, local_interface, &[&bytes[0..UDP_DEFAULT_MTU]], 0) {
                    return false;
                }

                let mut pos = UDP_DEFAULT_MTU;

                let fragment_count = (((packet_size - UDP_DEFAULT_MTU) as u32) / ((UDP_DEFAULT_MTU - FRAGMENT_HEADER_SIZE) as u32)) + ((((packet_size - UDP_DEFAULT_MTU) as u32) % ((UDP_DEFAULT_MTU - FRAGMENT_HEADER_SIZE) as u32)) != 0) as u32;
                debug_assert!(fragment_count <= FRAGMENT_COUNT_MAX as u32);

                let mut header = FragmentHeader {
                    id: packet_id,
                    dest: bytes[PACKET_DESTINATION_INDEX..PACKET_DESTINATION_INDEX + ADDRESS_SIZE].try_into().unwrap(),
                    fragment_indicator: FRAGMENT_INDICATOR,
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
            }
        }
        return ci.wire_send(endpoint, local_socket, local_interface, &[packet.as_bytes()], 0);
    }

    /// Send a packet to this peer.
    ///
    /// This will go directly if there is an active path, or otherwise indirectly
    /// via a root or some other route.
    pub(crate) fn send<CI: VL1CallerInterface>(&self, ci: &CI, node: &Node, time_ticks: i64, packet_id: PacketID, packet: &Buffer<{ PACKET_SIZE_MAX }>) -> bool {
        self.path(node).map_or(false, |path| {
            if self.send_to_endpoint(ci, path.endpoint(), Some(path.local_socket()), Some(path.local_interface()), packet_id, packet) {
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
    pub(crate) fn forward<CI: VL1CallerInterface>(&self, ci: &CI, time_ticks: i64, packet: &Buffer<{ PACKET_SIZE_MAX }>) -> bool {
        self.direct_path().map_or(false, |path| {
            if ci.wire_send(path.endpoint(), Some(path.local_socket()), Some(path.local_interface()), &[packet.as_bytes()], 0) {
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
    /// If try_new_endpoint is not None the packet will be sent directly to this endpoint.
    /// Otherwise it will be sent via the best direct or indirect path.
    ///
    /// This has its own send logic so it can handle either an explicit endpoint or a
    /// known one.
    pub(crate) fn send_hello<CI: VL1CallerInterface>(&self, ci: &CI, node: &Node, explicit_endpoint: Option<Endpoint>) -> bool {
        let path = if explicit_endpoint.is_none() { self.path(node) } else { None };
        explicit_endpoint.as_ref().map_or_else(|| Some(path.as_ref().unwrap().endpoint()), |ep| Some(ep)).map_or(false, |endpoint| {
            let mut packet: Buffer<{ PACKET_SIZE_MAX }> = Buffer::new();
            let time_ticks = ci.time_ticks();

            let packet_id = self.next_packet_id();
            debug_assert!(packet.append_and_init_struct(|header: &mut PacketHeader| {
                header.id = packet_id;
                header.dest = self.identity.address().to_bytes();
                header.src = node.address().to_bytes();
                header.flags_cipher_hops = CIPHER_NOCRYPT_POLY1305;
            }).is_ok());
            debug_assert!(packet.append_and_init_struct(|header: &mut message_component_structs::HelloFixedHeaderFields| {
                header.verb = VERB_VL1_HELLO | VERB_FLAG_EXTENDED_AUTHENTICATION;
                header.version_proto = VERSION_PROTO;
                header.version_major = VERSION_MAJOR;
                header.version_minor = VERSION_MINOR;
                header.version_revision = (VERSION_REVISION as u16).to_be();
                header.timestamp = (time_ticks as u64).to_be();
            }).is_ok());

            debug_assert!(self.identity.marshal(&mut packet, false).is_ok());
            debug_assert!(endpoint.marshal(&mut packet).is_ok());

            let aes_ctr_iv_position = packet.len();
            debug_assert!(packet.append_and_init_bytes_fixed(|iv: &mut [u8; 18]| {
                crate::crypto::random::fill_bytes_secure(&mut iv[0..16]);
                iv[12] &= 0x7f; // mask off MSB of counter in iv to play nice with some AES-CTR implementations

                // LEGACY: create a 16-bit encrypted field that specifies zero moons. This is ignored by v2
                // but causes v1 nodes to be able to parse this packet properly. This is not significant in
                // terms of encryption or authentication.
                let mut salsa_iv = packet_id.to_ne_bytes();
                salsa_iv[7] &= 0xf8;
                Salsa::new(&self.static_secret.secret.0[0..32], &salsa_iv, true).unwrap().crypt(&[0_u8, 0_u8], &mut salsa_iv[16..18]);
            }).is_ok());

            let dict_start_position = packet.len();
            let mut dict = Dictionary::new();
            dict.set_u64(HELLO_DICT_KEY_INSTANCE_ID, node.instance_id);
            dict.set_u64(HELLO_DICT_KEY_CLOCK, ci.time_clock() as u64);
            let _ = node.locator().map(|loc| {
                let mut tmp: Buffer<{ PACKET_SIZE_MAX }> = Buffer::new();
                debug_assert!(loc.marshal(&mut tmp).is_ok());
                dict.set_bytes(HELLO_DICT_KEY_LOCATOR, tmp.as_bytes().to_vec());
            });
            let _ = self.ephemeral_pair.lock().as_ref().map(|ephemeral_pair| {
                dict.set_bytes(HELLO_DICT_KEY_EPHEMERAL_C25519, ephemeral_pair.c25519.public_bytes().to_vec());
                dict.set_bytes(HELLO_DICT_KEY_EPHEMERAL_P521, ephemeral_pair.p521.public_key_bytes().to_vec());
            });
            if node.is_peer_root(self) {
                // If the peer is a root we include some extra information for diagnostic and statistics
                // purposes such as the CPU type, bits, and OS info. This is not sent to other peers.
                dict.set_str(HELLO_DICT_KEY_SYS_ARCH, std::env::consts::ARCH);
                #[cfg(target_pointer_width = "32")] {
                    dict.set_u64(HELLO_DICT_KEY_SYS_BITS, 32);
                }
                #[cfg(target_pointer_width = "64")] {
                    dict.set_u64(HELLO_DICT_KEY_SYS_BITS, 64);
                }
                dict.set_str(HELLO_DICT_KEY_OS_NAME, std::env::consts::OS);
            }
            let mut flags = String::new();
            if node.fips_mode() {
                flags.push('F');
            }
            dict.set_str(HELLO_DICT_KEY_FLAGS, flags.as_str());
            debug_assert!(dict.write_to(&mut packet).is_ok());

            let mut dict_aes = self.static_secret_hello_dictionary.lock();
            dict_aes.init(&packet.as_bytes()[aes_ctr_iv_position..aes_ctr_iv_position + 16]);
            dict_aes.crypt_in_place(&mut packet.as_bytes_mut()[dict_start_position..]);
            drop(dict_aes);

            debug_assert!(packet.append_u16(0).is_ok());

            debug_assert!(packet.append_bytes_fixed(&SHA384::hmac(self.static_secret_packet_hmac.as_ref(), packet.as_bytes_starting_at(PACKET_HEADER_SIZE).unwrap())).is_ok());

            let (_, mut poly) = salsa_poly_create(&self.static_secret, packet.struct_at::<PacketHeader>(0).unwrap(), packet.len());
            poly.update(packet.as_bytes_starting_at(PACKET_HEADER_SIZE).unwrap());
            packet.as_bytes_mut()[HEADER_MAC_FIELD_INDEX..HEADER_MAC_FIELD_INDEX + 8].copy_from_slice(&poly.finish()[0..8]);

            self.static_secret.encrypt_count.fetch_add(1, Ordering::Relaxed);
            self.last_send_time_ticks.store(time_ticks, Ordering::Relaxed);
            self.total_bytes_sent.fetch_add(packet.len() as u64, Ordering::Relaxed);

            path.as_ref().map_or_else(|| {
                self.send_to_endpoint(ci, endpoint, None, None, packet_id, &packet)
            }, |path| {
                path.log_send(time_ticks);
                self.send_to_endpoint(ci, endpoint, Some(path.local_socket()), Some(path.local_interface()), packet_id, &packet)
            })
        })
    }

    /// Called every INTERVAL during background tasks.
    #[inline(always)]
    pub(crate) fn on_interval<CI: VL1CallerInterface>(&self, ct: &CI, time_ticks: i64) {}

    #[inline(always)]
    fn receive_hello<CI: VL1CallerInterface>(&self, ci: &CI, node: &Node, time_ticks: i64, source_path: &Arc<Path>, payload: &Buffer<{ PACKET_SIZE_MAX }>) {}

    #[inline(always)]
    fn receive_error<CI: VL1CallerInterface, PH: VL1PacketHandler>(&self, ci: &CI, ph: &PH, node: &Node, time_ticks: i64, source_path: &Arc<Path>, forward_secrecy: bool, extended_authentication: bool, payload: &Buffer<{ PACKET_SIZE_MAX }>) {
        let mut cursor: usize = 0;
        let _ = payload.read_struct::<message_component_structs::ErrorHeader>(&mut cursor).map(|error_header| {
            let in_re_packet_id = error_header.in_re_packet_id;
            let current_packet_id_counter = self.packet_id_counter.load(Ordering::Relaxed);
            if current_packet_id_counter.checked_sub(in_re_packet_id).map_or_else(|| {
                (!in_re_packet_id).wrapping_add(current_packet_id_counter) < PACKET_RESPONSE_COUNTER_DELTA_MAX
            }, |packets_ago| {
                packets_ago <= PACKET_RESPONSE_COUNTER_DELTA_MAX
            }) {
                match error_header.in_re_verb {
                    _ => {
                        ph.handle_error(self, source_path, forward_secrecy, extended_authentication, error_header.in_re_verb, in_re_packet_id, error_header.error_code, payload, &mut cursor);
                    }
                }
            }
        });
    }

    #[inline(always)]
    fn receive_ok<CI: VL1CallerInterface, PH: VL1PacketHandler>(&self, ci: &CI, ph: &PH, node: &Node, time_ticks: i64, source_path: &Arc<Path>, forward_secrecy: bool, extended_authentication: bool, payload: &Buffer<{ PACKET_SIZE_MAX }>) {
        let mut cursor: usize = 0;
        let _ = payload.read_struct::<message_component_structs::OkHeader>(&mut cursor).map(|ok_header| {
            let in_re_packet_id = ok_header.in_re_packet_id;
            let current_packet_id_counter = self.packet_id_counter.load(Ordering::Relaxed);
            if current_packet_id_counter.checked_sub(in_re_packet_id).map_or_else(|| {
                (!in_re_packet_id).wrapping_add(current_packet_id_counter) < PACKET_RESPONSE_COUNTER_DELTA_MAX
            }, |packets_ago| {
                packets_ago <= PACKET_RESPONSE_COUNTER_DELTA_MAX
            }) {
                match ok_header.in_re_verb {
                    VERB_VL1_HELLO => {
                    }
                    VERB_VL1_WHOIS => {
                    }
                    _ => {
                        ph.handle_ok(self, source_path, forward_secrecy, extended_authentication, ok_header.in_re_verb, in_re_packet_id, payload, &mut cursor);
                    }
                }
            }
        });
    }

    #[inline(always)]
    fn receive_whois<CI: VL1CallerInterface>(&self, ci: &CI, node: &Node, time_ticks: i64, source_path: &Arc<Path>, payload: &Buffer<{ PACKET_SIZE_MAX }>) {}

    #[inline(always)]
    fn receive_rendezvous<CI: VL1CallerInterface>(&self, ci: &CI, node: &Node, time_ticks: i64, source_path: &Arc<Path>, payload: &Buffer<{ PACKET_SIZE_MAX }>) {}

    #[inline(always)]
    fn receive_echo<CI: VL1CallerInterface>(&self, ci: &CI, node: &Node, time_ticks: i64, source_path: &Arc<Path>, payload: &Buffer<{ PACKET_SIZE_MAX }>) {}

    #[inline(always)]
    fn receive_push_direct_paths<CI: VL1CallerInterface>(&self, ci: &CI, node: &Node, time_ticks: i64, source_path: &Arc<Path>, payload: &Buffer<{ PACKET_SIZE_MAX }>) {}

    #[inline(always)]
    fn receive_user_message<CI: VL1CallerInterface>(&self, ci: &CI, node: &Node, time_ticks: i64, source_path: &Arc<Path>, payload: &Buffer<{ PACKET_SIZE_MAX }>) {}

    /// Get current best path or None if there are no direct paths to this peer.
    pub fn direct_path(&self) -> Option<Arc<Path>> { self.paths.lock().first().map(|p| p.clone()) }

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
