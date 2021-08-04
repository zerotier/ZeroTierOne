use std::sync::Arc;

use parking_lot::Mutex;
use aes_gmac_siv::AesGmacSiv;

use crate::crypto::c25519::C25519KeyPair;
use crate::crypto::kbkdf::zt_kbkdf_hmac_sha384;
use crate::crypto::p521::P521KeyPair;
use crate::crypto::poly1305::Poly1305;
use crate::crypto::random::next_u64_secure;
use crate::crypto::salsa::Salsa;
use crate::crypto::secret::Secret;
use crate::vl1::{Identity, Path};
use crate::vl1::buffer::Buffer;
use crate::vl1::constants::*;
use crate::vl1::node::*;
use crate::vl1::protocol::*;

struct PeerSecrets {
    // Time secret was created in ticks or -1 for static secrets.
    create_time_ticks: i64,

    // Number of time secret has been used to encrypt something during this session.
    encrypt_count: u64,

    // Raw secret itself.
    secret: Secret<48>,

    // Reusable AES-GMAC-SIV initialized with secret.
    aes: AesGmacSiv,
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

struct TxState {
    // Time we last sent something to this peer.
    last_send_time_ticks: i64,

    // Outgoing packet IV counter, starts at a random position.
    packet_iv_counter: u64,

    // Total bytes sent to this peer during this session.
    total_bytes: u64,

    // "Eternal" static secret created via identity agreement.
    static_secret: PeerSecrets,

    // The most recently negotiated ephemeral secret.
    ephemeral_secret: Option<PeerSecrets>,

    // The current ephemeral key pair we will share with HELLO.
    ephemeral_pair: Option<EphemeralKeyPair>,

    // Paths to this peer sorted in ascending order of path quality.
    paths: Vec<Arc<Path>>,
}

struct RxState {
    // Time we last received something (authenticated) from this peer.
    last_receive_time_ticks: i64,

    // Total bytes received from this peer during this session.
    total_bytes: u64,

    // "Eternal" static secret created via identity agreement.
    static_secret: PeerSecrets,

    // The most recently negotiated ephemeral secret.
    ephemeral_secret: Option<PeerSecrets>,

    // Remote version as major, minor, revision, build in most-to-least-significant 16-bit chunks.
    // This is the user-facing software version and is zero if not yet known.
    remote_version: u64,

    // Remote protocol version or zero if not yet known.
    remote_protocol_version: u8,
}

/// A remote peer known to this node.
/// Sending-related and receiving-related fields are locked separately since concurrent
/// send/receive is not uncommon.
pub struct Peer {
    // This peer's identity.
    identity: Identity,

    // Static shared secret computed from agreement with identity.
    static_secret: Secret<48>,

    // Derived static secret used to encrypt the dictionary part of HELLO.
    static_secret_hello_dictionary_encrypt: Secret<48>,

    // Derived static secret used to add full HMAC-SHA384 to packets, currently just HELLO.
    static_secret_packet_hmac: Secret<48>,

    // State used primarily when sending to this peer.
    tx: Mutex<TxState>,

    // State used primarily when receiving from this peer.
    rx: Mutex<RxState>,
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

impl Peer {
    /// Create a new peer.
    /// This only returns None if this_node_identity does not have its secrets or if some
    /// fatal error occurs performing key agreement between the two identities.
    pub(crate) fn new(this_node_identity: &Identity, id: Identity) -> Option<Peer> {
        this_node_identity.agree(&id).map(|static_secret| {
            let aes_k0 = zt_kbkdf_hmac_sha384(&static_secret.0, KBKDF_KEY_USAGE_LABEL_AES_GMAC_SIV_K0, 0, 0);
            let aes_k1 = zt_kbkdf_hmac_sha384(&static_secret.0, KBKDF_KEY_USAGE_LABEL_AES_GMAC_SIV_K1, 0, 0);
            let static_secret_hello_dictionary_encrypt = zt_kbkdf_hmac_sha384(&static_secret.0, KBKDF_KEY_USAGE_LABEL_HELLO_DICTIONARY_ENCRYPT, 0, 0);
            let static_secret_packet_hmac = zt_kbkdf_hmac_sha384(&static_secret.0, KBKDF_KEY_USAGE_LABEL_PACKET_HMAC, 0, 0);
            Peer {
                identity: id,
                static_secret: static_secret.clone(),
                static_secret_hello_dictionary_encrypt,
                static_secret_packet_hmac,
                tx: Mutex::new(TxState {
                    last_send_time_ticks: 0,
                    packet_iv_counter: next_u64_secure(),
                    total_bytes: 0,
                    static_secret: PeerSecrets {
                        create_time_ticks: -1,
                        encrypt_count: 0,
                        secret: static_secret.clone(),
                        aes: AesGmacSiv::new(&aes_k0.0, &aes_k1.0),
                    },
                    ephemeral_secret: None,
                    paths: Vec::with_capacity(4),
                    ephemeral_pair: None,
                }),
                rx: Mutex::new(RxState {
                    last_receive_time_ticks: 0,
                    total_bytes: 0,
                    static_secret: PeerSecrets {
                        create_time_ticks: -1,
                        encrypt_count: 0,
                        secret: static_secret,
                        aes: AesGmacSiv::new(&aes_k0.0, &aes_k1.0),
                    },
                    ephemeral_secret: None,
                    remote_version: 0,
                    remote_protocol_version: 0,
                }),
            }
        })
    }

    /// Receive, decrypt, authenticate, and process an incoming packet from this peer.
    /// If the packet comes in multiple fragments, the fragments slice should contain all
    /// those fragments after the main packet header and first chunk.
    pub(crate) fn receive<CI: VL1CallerInterface, PH: VL1PacketHandler>(&self, node: &Node, ci: &CI, ph: &PH, time_ticks: i64, source_path: &Arc<Path>, header: &PacketHeader, packet: &Buffer<{ PACKET_SIZE_MAX }>, fragments: &[Option<PacketBuffer>]) {
        let packet_frag0_payload_bytes = packet.as_bytes_after(PACKET_VERB_INDEX).unwrap_or(&[]);
        if !packet_frag0_payload_bytes.is_empty() {
            let mut payload: Buffer<{ PACKET_SIZE_MAX }> = Buffer::new();
            let mut rx = self.rx.lock();

            // When handling incoming packets we try any current ephemeral secret first, and if that
            // fails we fall back to the static secret. If decryption with an ephemeral secret succeeds
            // the forward secrecy flag in the receive path is set.
            let forward_secrecy = {
                let mut secret = &mut rx.static_secret;
                loop {
                    match header.cipher() {
                        CIPHER_NOCRYPT_POLY1305 => {
                            // Only HELLO is allowed in the clear (but still authenticated).
                            if (packet_frag0_payload_bytes[0] & VERB_MASK) == VERB_VL1_HELLO {
                                let _ = payload.append_bytes(packet_frag0_payload_bytes);

                                for f in fragments.iter() {
                                    let _ = f.as_ref().map(|f| {
                                        let _ = f.as_bytes_after(FRAGMENT_HEADER_SIZE).map(|f| {
                                            let _ = payload.append_bytes(f);
                                        });
                                    });
                                }

                                // FIPS note: for FIPS purposes the HMAC-SHA384 tag at the end of V2 HELLOs
                                // will be considered the "real" handshake authentication.
                                let key = salsa_derive_per_packet_key(&secret.secret, header, payload.len());
                                let mut salsa = Salsa::new(&key.0[0..32], header.id_bytes(), true).unwrap();
                                let mut poly1305_key = [0_u8; 32];
                                salsa.crypt_in_place(&mut poly1305_key);
                                let mut poly = Poly1305::new(&poly1305_key).unwrap();
                                poly.update(packet_frag0_payload_bytes);

                                if poly.finish()[0..8].eq(&header.message_auth) {
                                    break;
                                }
                            }
                        }

                        CIPHER_SALSA2012_POLY1305 => {
                            // FIPS note: support for this mode would have to be disabled in FIPS compliant
                            // modes of operation.
                            let key = salsa_derive_per_packet_key(&secret.secret, header, payload.len());
                            let mut salsa = Salsa::new(&key.0[0..32], header.id_bytes(), true).unwrap();
                            let mut poly1305_key = [0_u8; 32];
                            salsa.crypt_in_place(&mut poly1305_key);
                            let mut poly = Poly1305::new(&poly1305_key).unwrap();

                            poly.update(packet_frag0_payload_bytes);
                            let _ = payload.append_and_init_bytes(packet_frag0_payload_bytes.len(), |b| salsa.crypt(packet_frag0_payload_bytes, b));
                            for f in fragments.iter() {
                                let _ = f.as_ref().map(|f| {
                                    let _ = f.as_bytes_after(FRAGMENT_HEADER_SIZE).map(|f| {
                                        poly.update(f);
                                        let _ = payload.append_and_init_bytes(f.len(), |b| salsa.crypt(f, b));
                                    });
                                });
                            }

                            if poly.finish()[0..8].eq(&header.message_auth) {
                                break;
                            }
                        }

                        CIPHER_AES_GMAC_SIV => {
                            secret.aes.reset();
                            secret.aes.decrypt_init(&header.aes_gmac_siv_tag());
                            secret.aes.decrypt_set_aad(&header.aad_bytes());

                            let _ = payload.append_and_init_bytes(packet_frag0_payload_bytes.len(), |b| secret.aes.decrypt(packet_frag0_payload_bytes, b));
                            for f in fragments.iter() {
                                let _ = f.as_ref().map(|f| {
                                    let _ = f.as_bytes_after(FRAGMENT_HEADER_SIZE).map(|f| {
                                        let _ = payload.append_and_init_bytes(f.len(), |b| secret.aes.decrypt(f, b));
                                    });
                                });
                            }

                            if secret.aes.decrypt_finish() {
                                break;
                            }
                        }

                        _ => {}
                    }

                    if (secret as *const PeerSecrets) != (&rx.static_secret as *const PeerSecrets) {
                        payload.clear();
                        secret = &mut rx.static_secret;
                    } else {
                        // Both ephemeral (if any) and static secret have failed, drop packet.
                        return;
                    }
                }
                (secret as *const PeerSecrets) != (&(rx.static_secret) as *const PeerSecrets)
            };

            // If we make it here we've successfully decrypted and authenticated the packet.

            rx.last_receive_time_ticks = time_ticks;
            rx.total_bytes += payload.len() as u64;

            // Unlock rx state mutex.
            drop(rx);

            let _ = payload.u8_at(0).map(|verb| {
                // For performance reasons we let VL2 handle packets first. It returns false
                // if it didn't pick up anything.
                if !ph.handle_packet(self, source_path, forward_secrecy, verb, &payload) {
                    match verb {
                        VERB_VL1_NOP => {}
                        VERB_VL1_HELLO => {}
                        VERB_VL1_ERROR => {}
                        VERB_VL1_OK => {}
                        VERB_VL1_WHOIS => {}
                        VERB_VL1_RENDEZVOUS => {}
                        VERB_VL1_ECHO => {}
                        VERB_VL1_PUSH_DIRECT_PATHS => {}
                        VERB_VL1_USER_MESSAGE => {}
                        VERB_VL1_REMOTE_TRACE => {}
                        _ => {}
                    }
                }
            });
        }
    }

    /// Get the remote version of this peer: major, minor, revision, and build.
    /// Returns None if it's not yet known.
    pub fn version(&self) -> Option<[u16; 4]> {
        let rv = self.rx.lock().remote_version;
        if rv != 0 {
            Some([(rv >> 48) as u16, (rv >> 32) as u16, (rv >> 16) as u16, rv as u16])
        } else {
            None
        }
    }

    /// Get the remote protocol version of this peer or None if not yet known.
    pub fn protocol_version(&self) -> Option<u8> {
        let pv = self.rx.lock().remote_protocol_version;
        if pv != 0 {
            Some(pv)
        } else {
            None
        }
    }
}
