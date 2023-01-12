/// Minimum size of a valid physical ZSSP packet or packet fragment.
pub const MIN_PACKET_SIZE: usize = HEADER_SIZE + AES_GCM_TAG_SIZE;

/// Minimum physical MTU for ZSSP to function.
pub const MIN_TRANSPORT_MTU: usize = 64;

/// Minimum recommended interval between calls to service() on each session, in milliseconds.
pub const SERVICE_INTERVAL: u64 = 10000;

/// Setting this to true enables kyber1024 post-quantum forward secrecy.
///
/// Kyber1024 is used for data forward secrecy but not authentication. Authentication would
/// require Kyber1024 in identities, which would make them huge, and isn't needed for our
/// threat model which is data warehousing today to decrypt tomorrow. Breaking authentication
/// is only relevant today, not in some mid to far future where a QC that can break 384-bit ECC
/// exists.
///
/// This is normally enabled but could be disabled at build time for e.g. very small devices.
/// It might not even be necessary there to disable it since it's not that big and is usually
/// faster than NIST P-384 ECDH.
pub(crate) const JEDI: bool = true;

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

/// Size of AES-GCM keys (256 bits)
pub(crate) const AES_KEY_SIZE: usize = 32;

/// Size of AES-GCM MAC tags
pub(crate) const AES_GCM_TAG_SIZE: usize = 16;

/// Size of HMAC-SHA384 MAC tags
pub(crate) const HMAC_SIZE: usize = 48;

/// Size of a session ID, which behaves a bit like a TCP port number.
///
/// This is large since some ZeroTier nodes handle huge numbers of links, like roots and controllers.
pub(crate) const SESSION_ID_SIZE: usize = 6;

/// Maximum difference between out-of-order incoming packet counters, and size of deduplication buffer.
pub(crate) const COUNTER_WINDOW_MAX_OUT_OF_ORDER: usize = 16;

// Packet types can range from 0 to 15 (4 bits) -- 0-3 are defined and 4-15 are reserved for future use
pub(crate) const PACKET_TYPE_DATA: u8 = 0;
pub(crate) const PACKET_TYPE_INITIAL_KEY_OFFER: u8 = 1; // "alice"
pub(crate) const PACKET_TYPE_KEY_COUNTER_OFFER: u8 = 2; // "bob"

// Key usage labels for sub-key derivation using NIST-style KBKDF (basically just HMAC KDF).
pub(crate) const KBKDF_KEY_USAGE_LABEL_HMAC: u8 = b'M'; // HMAC-SHA384 authentication for key exchanges
pub(crate) const KBKDF_KEY_USAGE_LABEL_HEADER_CHECK: u8 = b'H'; // AES-based header check code generation
pub(crate) const KBKDF_KEY_USAGE_LABEL_AES_GCM_ALICE_TO_BOB: u8 = b'A'; // AES-GCM in A->B direction
pub(crate) const KBKDF_KEY_USAGE_LABEL_AES_GCM_BOB_TO_ALICE: u8 = b'B'; // AES-GCM in B->A direction
pub(crate) const KBKDF_KEY_USAGE_LABEL_RATCHETING: u8 = b'R'; // Key input for next ephemeral ratcheting

// AES key size for header check code generation
pub(crate) const HEADER_CHECK_AES_KEY_SIZE: usize = 16;

/// Aribitrary starting value for master key derivation.
///
/// It doesn't matter very much what this is but it's good for it to be unique. It should
/// be changed if this code is changed in any cryptographically meaningful way like changing
/// the primary algorithm from NIST P-384 or the transport cipher from AES-GCM.
pub(crate) const INITIAL_KEY: [u8; 64] = [
    // macOS command line to generate:
    // echo -n 'ZSSP_Noise_IKpsk2_NISTP384_?KYBER1024_AESGCM_SHA512' | shasum -a 512  | cut -d ' ' -f 1 | xxd -r -p | xxd -i
    0x35, 0x6a, 0x75, 0xc0, 0xbf, 0xbe, 0xc3, 0x59, 0x70, 0x94, 0x50, 0x69, 0x4c, 0xa2, 0x08, 0x40, 0xc7, 0xdf, 0x67, 0xa8, 0x68, 0x52,
    0x6e, 0xd5, 0xdd, 0x77, 0xec, 0x59, 0x6f, 0x8e, 0xa1, 0x99, 0xb4, 0x32, 0x85, 0xaf, 0x7f, 0x0d, 0xa9, 0x6c, 0x01, 0xfb, 0x72, 0x46,
    0xc0, 0x09, 0x58, 0xb8, 0xe0, 0xa8, 0xcf, 0xb1, 0x58, 0x04, 0x6e, 0x32, 0xba, 0xa8, 0xb8, 0xf9, 0x0a, 0xa4, 0xbf, 0x36,
];
