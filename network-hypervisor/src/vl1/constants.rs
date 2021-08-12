/// Length of an address in bytes.
pub const ADDRESS_SIZE: usize = 5;

/// Prefix indicating reserved addresses (that can't actually be addresses).
pub const ADDRESS_RESERVED_PREFIX: u8 = 0xff;

/// KBKDF usage label indicating a key used to encrypt the dictionary inside HELLO.
pub const KBKDF_KEY_USAGE_LABEL_HELLO_DICTIONARY_ENCRYPT: u8 = b'H';

/// KBKDF usage label indicating a key used to HMAC packets, which is currently only used for HELLO.
pub const KBKDF_KEY_USAGE_LABEL_PACKET_HMAC: u8 = b'M';

/// KBKDF usage label for the first AES-GMAC-SIV key.
pub const KBKDF_KEY_USAGE_LABEL_AES_GMAC_SIV_K0: u8 = b'0';

/// KBKDF usage label for the second AES-GMAC-SIV key.
pub const KBKDF_KEY_USAGE_LABEL_AES_GMAC_SIV_K1: u8 = b'1';

/// KBKDF usage label for acknowledgement of a shared secret.
pub const KBKDF_KEY_USAGE_LABEL_EPHEMERAL_ACK: u8 = b'A';

/// Size of packet header that lies outside the encryption envelope.
pub const PACKET_HEADER_SIZE: usize = 27;

/// Maximum packet payload size including the verb/flags field.
///
/// This is large enough to carry "jumbo MTU" packets. The exact
/// value is because 10005+27 == 10032 which is divisible by 16. This
/// improves memory layout and alignment when buffers are allocated.
/// This value could technically be increased but it would require a
/// protocol version bump and only new nodes would be able to accept
/// the new size.
pub const PACKET_PAYLOAD_SIZE_MAX: usize = 10005;

/// Minimum packet, which is the header plus a verb.
pub const PACKET_SIZE_MIN: usize = PACKET_HEADER_SIZE + 1;

/// Maximum size of an entire packet.
pub const PACKET_SIZE_MAX: usize = PACKET_HEADER_SIZE + PACKET_PAYLOAD_SIZE_MAX;

/// Maximum number of inbound fragmented packets to handle at once per path.
/// This is a sanity limit to prevent memory exhaustion due to DOS attacks or broken peers.
pub const FRAGMENT_MAX_INBOUND_PACKETS_PER_PATH: usize = 256;

/// Time after which an incomplete fragmented packet expires.
pub const FRAGMENT_EXPIRATION: i64 = 1500;

/// Frequency for WHOIS retries
pub const WHOIS_RETRY_INTERVAL: i64 = 1000;

/// Maximum number of WHOIS retries
pub const WHOIS_RETRY_MAX: u16 = 3;

/// Maximum number of packets to queue up behind a WHOIS.
pub const WHOIS_MAX_WAITING_PACKETS: usize = 64;

/// Maximum number of endpoints allowed in a Locator.
pub const LOCATOR_MAX_ENDPOINTS: usize = 32;

/// Keepalive interval for paths in milliseconds.
pub const PATH_KEEPALIVE_INTERVAL: i64 = 20000;

/// Interval for servicing and background operations on peers.
pub const PEER_SERVICE_INTERVAL: i64 = 30000;
