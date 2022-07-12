// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::mem::size_of;
use std::sync::atomic::AtomicU32;

use zerotier_core_crypto::aes_gmac_siv::{Aes, AesCtr};
use zerotier_core_crypto::hash::{hmac_sha384, SHA384};
use zerotier_core_crypto::kbkdf::zt_kbkdf_hmac_sha512;
use zerotier_core_crypto::p384::*;
use zerotier_core_crypto::pqc_kyber;
use zerotier_core_crypto::random;
use zerotier_core_crypto::secret::Secret;
use zerotier_core_crypto::x25519::*;

use crate::util::buffer::Buffer;
use crate::util::marshalable::Marshalable;
use crate::vl1::identity::Identity;
use crate::vl1::symmetricsecret::SymmetricSecret;

use parking_lot::RwLock;

/*

Basic outline of the ZeroTier V2 session protocol:

*** Three-way connection setup handshake:

(1) Initiator sends INIT:

[16]  random IV
[4]   session ID
[1]   FFFFTTTT where F == flags, T == message type (1 for INIT)
[1]   ZeroTier protocol version
[4]   reserved, always zero
[1]   field ID of unencrypted initial ephemeral key
[...] outer ephemeral public key (currently always NIST P-384)
-- begin AES-CTR encryption using ephemeral/static AES key
[...] additional tuples of field ID and field data
-- end AES-CTR encryption
[48]  HMAC-SHA384 using static/static HMAC key

Additional fields in INIT:
 - Optional: additional ephemeral public keys
 - Optional: first 16 bytes of SHA384 of "current" session key
 - Required: static ZeroTier identity of initiator
 - Required: timestamp

(2) Responder sends ACK:

[16]  random IV
[4]   session ID
[1]   FFFFTTTT where F == flags, T == message type (2 for ACK)
[1]   ZeroTier protocol version
[4]   reserved, always zero
-- begin AES-CTR encryption using same ephemeral/static AES key as INIT
[...] tuples of field ID and field data
-- end AES-CTR encryption
[48]  HMAC-SHA384 using static/static HMAC key

Fields in ACK:
 - Required: ephemeral public key matching at least one ephemeral sent
 - Optional: additional matching ephemeral keys
 - Optional: first 16 bytes of SHA384 of "current" session key
 - Required: timestamp
 - Required: echo of timestamp from INIT

(3) Initiator sends CONFIRM:

[16]  AES-GMAC-SIV opaque tag
[4]   session ID
[1]   FFFFTTTT where F == flags, T == message type (3 for CONFIRM)
-- begin AES-GMAC-SIV encryption
[...] tuples of field ID and field data

Fields in CONFIRM:
 - Required: echo of timestamp from ACK

CONFIRM is technically optional as a valid DATA message also counts as
confirmation, but having an explicit message allows for mutual exchange
of latency information and in the future other things.

*** DATA packets:

[16]  AES-GMAC-SIV opaque tag
[4]   session ID
[1]   FFFFTTTT where F == flags, T == message type (0 for DATA)
-- begin AES-GMAC-SIV encrypted data packet
[1]   LNNNNNNN where N == fragment number and L is set if it's the last fragment
[...] data payload, typically starting with a ZeroTier VL1/VL2 protocol verb

When AES-GMAC-SIV packets are decrypted and authenticated, a sequential
message ID is exposed. This is used as a counter to prohibit replay attacks
within a session.

*** SINGLETON packets:

A singleton packet has the same format as an INIT packet, but includes no
additional public keys or session key info. Instead it includes a data payload
field and it elicits no ACK response. The session ID must be zero.

Singleton packets can be used to send unidirectional sparse messages without
incurring the overhead of a full session. There is no replay attack prevention
in this case, so these messages should only be used for things that are
idempotent or have their own resistance to replay. There is also no automatic
fragmentation, so the full packet must fit in the underlying transport.

*** Notes:

The initiator creates one or more ephemeral public keys and sends the first of
these ephemeral keys in unencrypted form. Key agreement (or KEX if applicable) is
performed against the responder's static identity key by both the initiator and the
responder to create an ephemeral/static key that is only used for INIT and ACK and
not afterwords. (The ephemeral sent in the clear must have a counterpart in the
recipient's static identity.)

When the responder receives INIT it computes the session key as follows:

(1) A starting ratchet key is chosen. If INIT contains a hash of the current
    (being replaced) session key and it matches the one at the responder, a
    derived ratchet key from the current session is used. Otherwise a ratchet
    key derived from the static/static key (the permanent key) is used.
(2) For each ephemeral key supplied by the initiator, the responder optionally
    generates its own ephemeral counterpart. While the responder is not required
    to match all supplied keys it must compute and supply at least one to create
    a valid forward-secure session. The responder then sends these keys in an
    ACK message encrypted using the same key as INIT but authenticated via HMAC
    using the new session key. Once the responder generates its own ephemeral
    keys it may compute the session key in the same manner as the initiator.
(3) When the initiator receives ACK it can compute the session key. Starting
    with the ratchet key from step (1) the initator performs key agreement using
    each ephemeral key pair for which both sides have furnished a key. These are
    chained together using HMAC-SHA512(last, next) where the last key is the
    "key" in HMAC and the next key is the "message."

Key agreements in (3) are performed in the following order, skipping any where both
sides have not furnished a key:

(1) Curve25519 ECDH
(2) Kyber (768)
(3) NIST P-384 ECDH

The NIST key must be last for FIPS compliance reasons as it's a FIPS-compliant
algorithm and elliptic curve. FIPS allows HKDF using HMAC(salt, key) and allows
the salt to be anything, so we can use the results of previous non-FIPS agreements
as this "salt."

Kyber is a post-quantum algorithm, the first to be standardized by NIST. Its
purpose is to provide long-term forward secrecy against adversaries who warehouse
data in anticipation of future quantum computing capability. When enabled a future
QC adversary could de-anonymize identities by breaking e.g. NIST P-384 but could
still not decrypt actual session payload.

Kyber is a key encapsulation algorithm rather than a Diffie-Hellman style
algorithm. When used the initiator generates a key pair and then sends its public
key to the responder. The responder then uses this public key to generate a shared
secret that is sent back to the initiator. The responder does not have to generate
its own key pair for this exchange. The raw Kyber algorithm is used since the
authentication in this session protocol is provided by HMAC-SHA384 using identity
keys.

*** Flags:

 - 0x80 - use extended authentication: this flag is only used in DATA and is ignored
   in setup exchanges. It indicates that the packet is terminated by a 48-byte full
   HMAC-SHA384 using the HMAC key derived from the session key. Enabling this slows
   things down but significantly strengthens the authentication posture of the
   protocol. It's generally only used if required for compliance.

*** Anti-DPI Obfuscation:

Obfuscation is applied to all session packets by AES encrypting a single block (ECB)
starting at byte index 12 in each packet. This single block is then decrypted by
the receiving end. The key for AES encryption is the first 32 bytes of the fingerprint
of the receiving side's ZeroTier identity.

This technically serves no purpose in terms of cryptographic security or authentication.
Its purpose is to make it difficult for deep packet inspectors to easily detect ZeroTier
traffic. For a DPI to correctly classify ZeroTier traffic it must know the identity of
the recipient and perform one single AES decrypt per packet.

Starting at byte index 12 randomizes this AES block even if other fields such as the
session ID are the same, as this incorporates four bytes of the random IV or tag field.

*** Credits:

Designed by Adam Ierymenko with heavy influence from the Noise protocol specification by
Trevor Perrin and the Wireguard VPN protocol by Jason Donenfeld.

*/

pub const SESSION_SETUP_PACKET_SIZE_MAX: usize = 1400;
pub const SESSION_PACKET_SIZE_MIN: usize = 28;
pub const SESSION_DATA_HEADER_SIZE: usize = 22;
pub const SESSION_DATA_PAYLOAD_SIZE_MIN: usize = SESSION_PACKET_SIZE_MIN - SESSION_DATA_HEADER_SIZE;

const FLAGS_TYPE_INDEX: usize = 20;
const FLAGS_TYPE_TYPE_MASK: u8 = 0x0f;

const MESSAGE_TYPE_DATA: u8 = 0x00;
const MESSAGE_TYPE_INIT: u8 = 0x01;
const MESSAGE_TYPE_ACK: u8 = 0x02;
const MESSAGE_TYPE_CONFIRM: u8 = 0x03;
const MESSAGE_TYPE_SINGLETON: u8 = 0x04;

const MESSAGE_FLAGS_EXTENDED_AUTH: u8 = 0x80;

const FIELD_DATA: u8 = 0x00;
const FIELD_INITIATOR_IDENTITY: u8 = 0x01;
const FIELD_EPHEMERAL_C25519: u8 = 0x02;
const FIELD_EPHEMERAL_NISTP384: u8 = 0x03;
const FIELD_EPHEMERAL_KYBER_PUBLIC: u8 = 0x04;
const FIELD_EPHEMERAL_KYBER_ENCAPSULATED_SECRET: u8 = 0x05;
const FIELD_CURRENT_SESSION_KEY_HASH: u8 = 0x06;
const FIELD_TIMESTAMP: u8 = 0x07;
const FIELD_TIMESTAMP_ECHO: u8 = 0x08;

#[derive(Clone, Copy)]
#[repr(C, packed)]
struct InitAckSingletonHeader {
    iv: [u8; 16],
    session_id: u32,
    flags_type: u8,
    protocol_version: u8,
    zero: [u8; 4],
}

#[derive(Clone, Copy)]
#[repr(C, packed)]
struct InitSingletonHeader {
    h: InitAckSingletonHeader,
    outer_ephemeral_field_id: u8,
    outer_ephemeral: [u8; P384_PUBLIC_KEY_SIZE],
}

#[derive(Clone, Copy)]
#[repr(C, packed)]
struct ConfirmDataHeader {
    tag: [u8; 16],
    session_id: u32,
    flags_type: u8,
}

struct InitiatorOfferedKeys {
    p384: P384KeyPair,
    kyber: Option<pqc_kyber::Keypair>,
    ratchet_starting_key: Secret<64>,
}

struct Keys {
    /// Keys offered by local node and sent to remote, generated by initiate().
    local_offered: Option<Box<InitiatorOfferedKeys>>,

    /// Key resulting from agreement between the outer (unencrypted) ephemeral sent with INIT and the recipient's static identity key.
    setup_key: Option<Secret<32>>,

    /// Final key ratcheted from previous or starting key via agreement between all matching ephemeral pairs.
    session_key: Option<SymmetricSecret>,
}

#[cfg(any(target_arch = "x86", target_arch = "x86_64", target_arch = "aarch64", target_arch = "powerpc64"))]
#[inline(always)]
fn zero_is_zero(z: &[u8; 4]) -> bool {
    unsafe { *(z as *const [u8; 4]).cast::<u32>() == 0 }
}

#[cfg(not(any(target_arch = "x86", target_arch = "x86_64", target_arch = "aarch64", target_arch = "powerpc64")))]
#[inline(always)]
fn zero_is_zero(z: &[u8; 4]) -> bool {
    u32::from_ne_bytes(*z) == 0
}

/// ZeroTier V2 forward-secure session
///
/// The current version always uses NIST P-384 as the outer ephemeral key and optionally
/// Kyber for the internal ephemeral key. Curve25519 is supported if sent by the remote
/// side though.
///
/// The RD template argument is used to specify a type to be attached to the session such
/// as a ZeroTier peer.
#[allow(unused)]
pub(crate) struct Session<RD> {
    /// Arbitrary object that may be attached by external code to this session (e.g. a peer).
    pub related_data: RwLock<Option<RD>>,

    /// Session keys of various types.
    keys: RwLock<Keys>,

    /// Timestamp when session was created.
    creation_time: i64,

    /// A random number added to sent timestamps to not reveal exact local tick counter.
    latency_timestamp_delta: u32,

    /// Number of times session key has been used to encrypt data.
    encrypt_uses: AtomicU32,

    /// Number of times session key has been used to decrypt data.
    decrypt_uses: AtomicU32,

    /// Most recent measured latency in milliseconds.
    latency: AtomicU32,

    /// Random session ID generated by initiator.
    pub id: u32,
}

pub(crate) trait SessionContext<RD> {
    /// Iterate through all sessions matching an ID until the supplied function returns false.
    fn sessions_with_id<F: FnMut(&Session<RD>) -> bool>(&self, id: u32, f: F);
}

impl<RD> Session<RD> {
    /// Create an initiator session and return it and the packet to be sent.
    pub fn initiate(
        local_identity: &Identity,
        remote_identity: &Identity,
        obfuscation_key: &Aes,
        static_key: &SymmetricSecret,
        current_session: Option<&Self>,
        current_time: i64,
    ) -> Option<(Self, Buffer<SESSION_SETUP_PACKET_SIZE_MAX>)> {
        let mut packet: Buffer<SESSION_SETUP_PACKET_SIZE_MAX> = Buffer::new();

        let mut id = random::next_u32_secure();
        id |= (id == 0) as u32;

        let ephemeral_p384 = P384KeyPair::generate();
        {
            let h: &mut InitSingletonHeader = packet.append_struct_get_mut().unwrap();
            random::fill_bytes_secure(&mut h.h.iv);
            h.h.session_id = id; // actually [u8; 4] so endian is irrelevant
            h.h.flags_type = MESSAGE_FLAGS_EXTENDED_AUTH | MESSAGE_TYPE_INIT;
            h.h.protocol_version = crate::vl1::protocol::PROTOCOL_VERSION;
            h.outer_ephemeral_field_id = FIELD_EPHEMERAL_NISTP384;
            h.outer_ephemeral = *ephemeral_p384.public_key_bytes();
        }

        assert!(packet.append_u8(FIELD_INITIATOR_IDENTITY).is_ok());
        assert!(local_identity.marshal(&mut packet).is_ok());

        let ephemeral_kyber = pqc_kyber::keypair(&mut random::SecureRandom::get());
        assert!(packet.append_u8(FIELD_EPHEMERAL_KYBER_PUBLIC).is_ok());
        assert!(packet.append_bytes_fixed(&ephemeral_kyber.public).is_ok());

        let ratchet_starting_key = current_session
            .and_then(|cs| {
                cs.keys.read().session_key.as_ref().map(|cs_key| {
                    assert!(packet.append_u8(FIELD_CURRENT_SESSION_KEY_HASH).is_ok());
                    assert!(packet.append_bytes_fixed(&cs_key.key_hash).is_ok());
                    zt_kbkdf_hmac_sha512(cs_key.key.as_bytes(), crate::vl1::protocol::security_constants::KBKDF_KEY_USAGE_LABEL_RATCHET_KEY)
                })
            })
            .unwrap_or_else(|| zt_kbkdf_hmac_sha512(static_key.key.as_bytes(), crate::vl1::protocol::security_constants::KBKDF_KEY_USAGE_LABEL_RATCHET_KEY));

        let latency_timestamp_delta = random::next_u32_secure();
        assert!(packet.append_u8(FIELD_TIMESTAMP).is_ok());
        assert!(packet.append_u64((current_time as u64).wrapping_add(latency_timestamp_delta as u64)).is_ok());

        let setup_key;
        if let Some(responder_p384) = remote_identity.p384.as_ref() {
            if let Some(sk) = ephemeral_p384.agree(&responder_p384.ecdh) {
                setup_key = Secret(SHA384::hash(sk.as_bytes())[..32].try_into().unwrap());
                AesCtr::new(setup_key.as_bytes()).crypt_in_place(&mut packet.as_bytes_mut()[size_of::<InitSingletonHeader>()..]);
            } else {
                return None;
            }
        } else {
            return None;
        };

        assert!(packet.append_bytes(&hmac_sha384(static_key.packet_hmac_key.as_bytes(), packet.as_bytes())).is_ok());

        obfuscation_key.encrypt_block_in_place(&mut packet.as_bytes_mut()[12..28]);

        return Some((
            Self {
                related_data: RwLock::new(None),
                keys: RwLock::new(Keys {
                    local_offered: Some(Box::new(InitiatorOfferedKeys {
                        p384: ephemeral_p384,
                        kyber: Some(ephemeral_kyber),
                        ratchet_starting_key,
                    })),
                    setup_key: Some(setup_key),
                    session_key: None,
                }),
                creation_time: current_time,
                latency_timestamp_delta,
                encrypt_uses: AtomicU32::new(0),
                decrypt_uses: AtomicU32::new(0),
                latency: AtomicU32::new(0),
                id,
            },
            packet,
        ));
    }

    pub fn receive<const L: usize, SC: SessionContext<RD>>(
        local_identity: &Identity,
        obfuscation_key: &Aes,
        static_key: &SymmetricSecret,
        current_time: i64,
        sc: &SC,
        packet: &mut Buffer<L>,
    ) -> bool {
        if packet.len() >= SESSION_PACKET_SIZE_MIN {
            obfuscation_key.decrypt_block_in_place(&mut packet.as_bytes_mut()[12..28]);
            let flags = packet.u8_at(FLAGS_TYPE_INDEX).unwrap();
            let message_type = flags & FLAGS_TYPE_TYPE_MASK;
            match message_type {
                MESSAGE_TYPE_DATA | MESSAGE_TYPE_CONFIRM => if let Ok(header) = packet.struct_at::<ConfirmDataHeader>(0) {},

                MESSAGE_TYPE_INIT | MESSAGE_TYPE_ACK | MESSAGE_TYPE_SINGLETON => {
                    if let Ok(header) = packet.struct_at::<InitAckSingletonHeader>(0) {
                        if zero_is_zero(&header.zero) {
                            let (
                                mut remote_identity,
                                mut remote_offered_c25519,
                                mut remote_offered_nistp384,
                                mut remote_offered_kyber_public,
                                mut remote_timestamp,
                                mut remote_session_key_hash,
                            ) = (None, None, None, None, -1, None);

                            let mut cursor = size_of::<InitAckSingletonHeader>();
                            loop {
                                if let Ok(field_type) = packet.read_u8(&mut cursor) {
                                    match field_type {
                                        FIELD_DATA => {}
                                        FIELD_INITIATOR_IDENTITY => {
                                            if let Ok(id) = Identity::unmarshal(packet, &mut cursor) {
                                                remote_identity = Some(id);
                                            } else {
                                                return false;
                                            }
                                        }
                                        FIELD_EPHEMERAL_C25519 => {
                                            if let Ok(k) = packet.read_bytes_fixed::<C25519_PUBLIC_KEY_SIZE>(&mut cursor) {
                                                remote_offered_c25519 = Some(k);
                                            } else {
                                                return false;
                                            }
                                        }
                                        FIELD_EPHEMERAL_NISTP384 => {
                                            if let Ok(k) = packet.read_bytes_fixed::<P384_PUBLIC_KEY_SIZE>(&mut cursor) {
                                                remote_offered_nistp384 = Some(k);
                                            } else {
                                                return false;
                                            }
                                        }
                                        FIELD_EPHEMERAL_KYBER_PUBLIC => {
                                            if let Ok(k) = packet.read_bytes_fixed::<{ pqc_kyber::KYBER_PUBLICKEYBYTES }>(&mut cursor) {
                                                remote_offered_kyber_public = Some(k);
                                            } else {
                                                return false;
                                            }
                                        }
                                        FIELD_EPHEMERAL_KYBER_ENCAPSULATED_SECRET => {}
                                        FIELD_CURRENT_SESSION_KEY_HASH => {
                                            if let Ok(k) = packet.read_bytes_fixed::<16>(&mut cursor) {
                                                remote_session_key_hash = Some(k);
                                            } else {
                                                return false;
                                            }
                                        }
                                        FIELD_TIMESTAMP => {
                                            if let Ok(ts) = packet.read_varint(&mut cursor) {
                                                remote_timestamp = ts as i64;
                                            } else {
                                                return false;
                                            }
                                        }
                                        FIELD_TIMESTAMP_ECHO => {
                                            if let Ok(ts) = packet.read_varint(&mut cursor) {
                                            } else {
                                                return false;
                                            }
                                        }
                                        _ => {}
                                    }

                                    if message_type == MESSAGE_TYPE_INIT {}
                                } else {
                                    break;
                                }
                            }
                        }
                    }
                }

                _ => {}
            }
        }
        return false;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn sizing() {
        assert_eq!(size_of::<InitAckSingletonHeader>(), 26);
        assert_eq!(size_of::<InitSingletonHeader>(), 26 + 1 + P384_PUBLIC_KEY_SIZE);
        assert_eq!(size_of::<ConfirmDataHeader>(), 21);
    }
}
