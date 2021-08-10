use std::sync::Arc;
use std::sync::atomic::{AtomicI64, AtomicU64, AtomicU8, Ordering};

use parking_lot::Mutex;
use aes_gmac_siv::{AesGmacSiv, AesCtr};

use crate::crypto::c25519::C25519KeyPair;
use crate::crypto::kbkdf::zt_kbkdf_hmac_sha384;
use crate::crypto::p521::P521KeyPair;
use crate::crypto::poly1305::Poly1305;
use crate::crypto::random::next_u64_secure;
use crate::crypto::salsa::Salsa;
use crate::crypto::secret::Secret;
use crate::util::pool::{Pool, PoolFactory};
use crate::vl1::{Identity, Path, Endpoint};
use crate::vl1::buffer::Buffer;
use crate::vl1::constants::*;
use crate::vl1::node::*;
use crate::vl1::protocol::*;

struct AesGmacSivPoolFactory(Secret<48>, Secret<48>);

impl PoolFactory<AesGmacSiv> for AesGmacSivPoolFactory {
    #[inline(always)]
    fn create(&self) -> AesGmacSiv {
        AesGmacSiv::new(&self.0.0[0..32], &self.1.0[0..32])
    }

    #[inline(always)]
    fn reset(&self, obj: &mut AesGmacSiv) {
        obj.reset();
    }
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

    // Paths sorted in ascending order of quality / preference.
    paths: Mutex<Vec<Arc<Path>>>,

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
    packet_iv_counter: AtomicU64,

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
                last_send_time_ticks: AtomicI64::new(0),
                last_receive_time_ticks: AtomicI64::new(0),
                last_forward_time_ticks: AtomicI64::new(0),
                total_bytes_sent: AtomicU64::new(0),
                total_bytes_sent_indirect: AtomicU64::new(0),
                total_bytes_received: AtomicU64::new(0),
                total_bytes_received_indirect: AtomicU64::new(0),
                total_bytes_forwarded: AtomicU64::new(0),
                packet_iv_counter: AtomicU64::new(next_u64_secure()),
                remote_version: AtomicU64::new(0),
                remote_protocol_version: AtomicU8::new(0),
            }
        })
    }

    /// Receive, decrypt, authenticate, and process an incoming packet from this peer.
    /// If the packet comes in multiple fragments, the fragments slice should contain all
    /// those fragments after the main packet header and first chunk.
    pub(crate) fn receive<CI: VL1CallerInterface, PH: VL1PacketHandler>(&self, node: &Node, ci: &CI, ph: &PH, time_ticks: i64, source_path: &Arc<Path>, header: &PacketHeader, packet: &Buffer<{ PACKET_SIZE_MAX }>, fragments: &[Option<PacketBuffer>]) {
        let _ = packet.as_bytes_starting_at(PACKET_VERB_INDEX).map(|packet_frag0_payload_bytes| {
            let mut payload: Buffer<{ PACKET_SIZE_MAX }> = Buffer::new();

            let cipher = header.cipher();
            let mut forward_secrecy = true;
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
                            // will be considered the "real" handshake authentication.
                            let key = salsa_derive_per_packet_key(&secret.secret, header, payload.len());
                            let mut salsa = Salsa::new(&key.0[0..32], header.id_bytes(), true).unwrap();
                            let mut poly1305_key = [0_u8; 32];
                            salsa.crypt_in_place(&mut poly1305_key);
                            let mut poly = Poly1305::new(&poly1305_key).unwrap();
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
                        let key = salsa_derive_per_packet_key(&secret.secret, header, payload.len());
                        let mut salsa = Salsa::new(&key.0[0..32], header.id_bytes(), true).unwrap();
                        let mut poly1305_key = [0_u8; 32];
                        salsa.crypt_in_place(&mut poly1305_key);
                        let mut poly = Poly1305::new(&poly1305_key).unwrap();

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

                        if aes.decrypt_finish() {
                            break;
                        }
                    }

                    _ => {
                        // Unrecognized or unsupported cipher type.
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
                    let _ = payload.set_size(0);
                }
            }
            drop(ephemeral_secret);

            // If decryption and authentication succeeded, the code above will break out of the
            // for loop and end up here. Otherwise it returns from the whole function.

            self.last_receive_time_ticks.store(time_ticks, Ordering::Relaxed);
            let _ = self.total_bytes_received.fetch_add((payload.len() + PACKET_HEADER_SIZE) as u64, Ordering::Relaxed);

            let _ = payload.u8_at(0).map(|verb| {
                // For performance reasons we let VL2 handle packets first. It returns false
                // if it didn't handle the packet, in which case it's handled at VL1.
                if !ph.handle_packet(self, source_path, forward_secrecy, verb, &payload) {
                    match verb {
                        //VERB_VL1_NOP => {}
                        VERB_VL1_HELLO => self.receive_hello(ci, node, time_ticks, source_path, &payload),
                        VERB_VL1_ERROR => self.receive_error(ci, node, time_ticks, source_path, &payload),
                        VERB_VL1_OK => self.receive_ok(ci, node, time_ticks, source_path, &payload),
                        VERB_VL1_WHOIS => self.receive_whois(ci, node, time_ticks, source_path, &payload),
                        VERB_VL1_RENDEZVOUS => self.receive_rendezvous(ci, node, time_ticks, source_path, &payload),
                        VERB_VL1_ECHO => self.receive_echo(ci, node, time_ticks, source_path, &payload),
                        VERB_VL1_PUSH_DIRECT_PATHS => self.receive_push_direct_paths(ci, node, time_ticks, source_path, &payload),
                        VERB_VL1_USER_MESSAGE => self.receive_user_message(ci, node, time_ticks, source_path, &payload),
                        _ => {}
                    }
                }
            });
        });
    }

    /// Send a packet to this peer.
    ///
    /// This will go directly if there is an active path, or otherwise indirectly
    /// via a root or some other route.
    pub(crate) fn send<CI: VL1CallerInterface>(&self, ci: &CI, time_ticks: i64, data: PacketBuffer) {
        self.last_send_time_ticks.store(time_ticks, Ordering::Relaxed);
        let _ = self.total_bytes_sent.fetch_add(data.len() as u64, Ordering::Relaxed);
    }

    /// Forward a packet to this peer.
    ///
    /// This is called when we receive a packet not addressed to this node and
    /// want to pass it along.
    ///
    /// This doesn't support fragmenting since fragments are forwarded individually.
    /// Intermediates don't need to adjust fragmentation.
    pub(crate) fn forward<CI: VL1CallerInterface>(&self, ci: &CI, time_ticks: i64, data: PacketBuffer) {
        self.last_forward_time_ticks.store(time_ticks, Ordering::Relaxed);
        let _ = self.total_bytes_forwarded.fetch_add(data.len() as u64, Ordering::Relaxed);
        todo!()
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

    /// Called every INTERVAL during background tasks.
    #[inline(always)]
    pub fn on_interval<CI: VL1CallerInterface>(&self, ct: &CI, time_ticks: i64) {
    }

    #[inline(always)]
    fn receive_hello<CI: VL1CallerInterface>(&self, ci: &CI, node: &Node, time_ticks: i64, source_path: &Arc<Path>, packet: &Buffer<{ PACKET_SIZE_MAX }>) {
    }

    #[inline(always)]
    fn receive_error<CI: VL1CallerInterface>(&self, ci: &CI, node: &Node, time_ticks: i64, source_path: &Arc<Path>, packet: &Buffer<{ PACKET_SIZE_MAX }>) {
    }

    #[inline(always)]
    fn receive_ok<CI: VL1CallerInterface>(&self, ci: &CI, node: &Node, time_ticks: i64, source_path: &Arc<Path>, packet: &Buffer<{ PACKET_SIZE_MAX }>) {
    }

    #[inline(always)]
    fn receive_whois<CI: VL1CallerInterface>(&self, ci: &CI, node: &Node, time_ticks: i64, source_path: &Arc<Path>, packet: &Buffer<{ PACKET_SIZE_MAX }>) {
    }

    #[inline(always)]
    fn receive_rendezvous<CI: VL1CallerInterface>(&self, ci: &CI, node: &Node, time_ticks: i64, source_path: &Arc<Path>, packet: &Buffer<{ PACKET_SIZE_MAX }>) {
    }

    #[inline(always)]
    fn receive_echo<CI: VL1CallerInterface>(&self, ci: &CI, node: &Node, time_ticks: i64, source_path: &Arc<Path>, packet: &Buffer<{ PACKET_SIZE_MAX }>) {
    }

    #[inline(always)]
    fn receive_push_direct_paths<CI: VL1CallerInterface>(&self, ci: &CI, node: &Node, time_ticks: i64, source_path: &Arc<Path>, packet: &Buffer<{ PACKET_SIZE_MAX }>) {
    }

    #[inline(always)]
    fn receive_user_message<CI: VL1CallerInterface>(&self, ci: &CI, node: &Node, time_ticks: i64, source_path: &Arc<Path>, packet: &Buffer<{ PACKET_SIZE_MAX }>) {
    }
}
