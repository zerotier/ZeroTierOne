/// Minimum size of a valid physical ZSSP packet or packet fragment.
pub const MIN_PACKET_SIZE: usize = HEADER_SIZE + AES_GCM_TAG_SIZE;

/// Minimum physical MTU for ZSSP to function.
pub const MIN_TRANSPORT_MTU: usize = 64;

/// Minimum recommended interval between calls to service() on each session, in milliseconds.
pub const SERVICE_INTERVAL: u64 = 10000;

/// Maximum number of fragments for data packets.
pub(crate) const MAX_FRAGMENTS: usize = 48; // hard protocol max: 63

/// Maximum number of fragments for key exchange packets (can be smaller to save memory, only a few needed)
pub(crate) const KEY_EXCHANGE_MAX_FRAGMENTS: usize = 2; // enough room for p384 + ZT identity + kyber1024 + tag/hmac/etc.

/// Start attempting to rekey after a key has been used to send packets this many times.
/// This is 1/4 the recommended NIST limit for AES-GCM key lifetimes under most conditions.
pub(crate) const REKEY_AFTER_USES: u64 = 536870912;

/// Hard expiration after this many uses.
///
/// Use of the key beyond this point is prohibited. If we reach this number of key uses
/// the key will be destroyed in memory and the session will cease to function. A hard
/// error is also generated.
pub(crate) const EXPIRE_AFTER_USES: u64 = REKEY_AFTER_USES * 2;

/// Start attempting to rekey after a key has been in use for this many milliseconds.
pub(crate) const REKEY_AFTER_TIME_MS: i64 = 1000 * 60 * 60; // 1 hour

/// Maximum random jitter to add to rekey-after time.
pub(crate) const REKEY_AFTER_TIME_MS_MAX_JITTER: u32 = 1000 * 60 * 10; // 10 minutes

/// Version 0: AES-256-GCM + NIST P-384 + optional Kyber1024 PQ forward secrecy
pub(crate) const SESSION_PROTOCOL_VERSION: u8 = 0x00;

/// Secondary key type: none, use only P-384 for forward secrecy.
pub(crate) const HYBRID_KEY_TYPE_NONE: u8 = 0;

/// Secondary key type: Kyber1024, PQ forward secrecy enabled.
pub(crate) const HYBRID_KEY_TYPE_KYBER1024: u8 = 1;

/// Size of packet header
pub(crate) const HEADER_SIZE: usize = 16;

/// Start of single block AES encryption of a portion of the header (and some data).
pub(crate) const HEADER_CHECK_ENCRYPT_START: usize = 6;

/// End of single block AES encryption of a portion of the header (and some data).
pub(crate) const HEADER_CHECK_ENCRYPT_END: usize = 22;

pub(crate) const AES_KEY_SIZE: usize = 32;
pub(crate) const AES_HEADER_CHECK_KEY_SIZE: usize = 16;
pub(crate) const AES_GCM_TAG_SIZE: usize = 16;
pub(crate) const AES_GCM_NONCE_SIZE: usize = 12;

/// Size of a session ID, which behaves a bit like a TCP port number.
///
/// This is large since some ZeroTier nodes handle huge numbers of links, like roots and controllers.
pub(crate) const SESSION_ID_SIZE: usize = 6;

/// Maximum difference between out-of-order incoming packet counters, and size of deduplication buffer.
pub(crate) const COUNTER_WINDOW_MAX_OUT_OF_ORDER: usize = 16;

/// Maximum skip-ahead for counter.
///
/// This is huge (2^24) because its real purpose is to filter out bad packets where decryption of
/// the counter yields an invalid value.
pub(crate) const COUNTER_WINDOW_MAX_SKIP_AHEAD: u64 = 16777216;

// Key usage labels for sub-key derivation using NIST-style KBKDF (basically just HMAC KDF).
pub(crate) const KBKDF_KEY_USAGE_LABEL_KEX_PAYLOAD_ENCRYPTION: u8 = b'M'; // intermediate keys used in key exchanges
pub(crate) const KBKDF_KEY_USAGE_LABEL_HEADER_CHECK: u8 = b'H'; // AES-based header check code generation
pub(crate) const KBKDF_KEY_USAGE_LABEL_AES_GCM_ALICE_TO_BOB: u8 = b'A'; // AES-GCM in A->B direction
pub(crate) const KBKDF_KEY_USAGE_LABEL_AES_GCM_BOB_TO_ALICE: u8 = b'B'; // AES-GCM in B->A direction

// AES key size for header check code generation
pub(crate) const HEADER_CHECK_AES_KEY_SIZE: usize = 16;

/// Aribitrary starting value for master key derivation.
///
/// It doesn't matter very much what this is but it's good for it to be unique. It should
/// be changed if this code is changed in any cryptographically meaningful way like changing
/// the primary algorithm from NIST P-384 or the transport cipher from AES-GCM.
pub(crate) const INITIAL_KEY: [u8; 64] = [
    // macOS command line to generate:
    // echo -n 'ZSSP_Noise_XKpsk2_NISTP384_?KYBER1024_AESGCM_SHA512' | shasum -a 512  | cut -d ' ' -f 1 | xxd -r -p | xxd -i
    0xc7, 0xde, 0xa3, 0xbe, 0x84, 0xe5, 0x91, 0x25, 0x30, 0x59, 0xc1, 0xc9, 0x5d, 0x22, 0xf5, 0x5a, 0xd0, 0x67, 0x9e, 0xf9, 0xf6, 0xbb,
    0xc0, 0x2a, 0x7f, 0xd0, 0x12, 0xb2, 0x0f, 0xed, 0x64, 0x7a, 0x86, 0x9f, 0x82, 0x19, 0xca, 0x84, 0xad, 0xf6, 0x61, 0xda, 0x59, 0xcc,
    0x40, 0xcf, 0x57, 0x68, 0x3e, 0xe4, 0xd6, 0xe7, 0xd1, 0xad, 0xe9, 0x56, 0x50, 0xf2, 0x38, 0x22, 0x88, 0xa3, 0x5c, 0x7f,
];
