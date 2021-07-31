/// Length of an address in bytes.
pub const ADDRESS_SIZE: usize = 5;

/// Size of packet header that lies outside the encryption envelope.
pub const PACKET_HEADER_SIZE: usize = 27;

/// Maximum packet payload size including the verb/flags field.
/// This is large enough to carry "jumbo MTU" packets. The size is
/// odd because 10005+27 == 10032 which is divisible by 16. This
/// improves memory layout and alignment when buffers are allocated.
/// This value could technically be increased but it would require a
/// protocol version bump and only new nodes would be able to accept
/// the new size.
pub const PACKET_PAYLOAD_SIZE_MAX: usize = 10005;

/// Minimum packet, which is the header plus a verb.
pub const PACKET_SIZE_MIN: usize = PACKET_HEADER_SIZE + 1;

/// Maximum size of an entire packet.
pub const PACKET_SIZE_MAX: usize = PACKET_HEADER_SIZE + PACKET_PAYLOAD_SIZE_MAX;

/// Mask to select cipher from header flags field.
pub const HEADER_FLAGS_FIELD_MASK_CIPHER: u8 = 0x30;

/// Mask to select packet hops from header flags field.
pub const HEADER_FLAGS_FIELD_MASK_HOPS: u8 = 0x07;

/// Packet is not encrypted but contains a Poly1305 MAC of the plaintext.
/// Poly1305 is initialized with Salsa20/12 in the same manner as SALSA2012_POLY1305.
pub const CIPHER_NOCRYPT_POLY1305: u8 = 0x00;

/// Packet is encrypted and authenticated with Salsa20/12 and Poly1305.
/// Construction is the same as that which is used in the NaCl secret box functions.
pub const CIPHER_SALSA2012_POLY1305: u8 = 0x10;

/// Formerly 'NONE' which is deprecated; reserved for future use.
pub const CIPHER_RESERVED: u8 = 0x20;

/// Packet is encrypted and authenticated with AES-GMAC-SIV (AES-256).
pub const CIPHER_AES_GMAC_SIV: u8 = 0x30;

/// Header (outer) flag indicating that this packet has additional fragments.
pub const HEADER_FLAG_FRAGMENTED: u8 = 0x40;

/// Minimum size of a fragment.
pub const FRAGMENT_SIZE_MIN: usize = 16;

/// Maximum allowed number of fragments.
pub const FRAGMENT_COUNT_MAX: usize = 16;

/// Maximum number of fragmented packets in flight from a peer.
/// Usually there should only be one at a time, so this is overkill.
pub const PEER_DEFRAGMENT_MAX_PACKETS_IN_FLIGHT: usize = 4;

/// Verb (inner) flag indicating that the packet's payload (after the verb) is LZ4 compressed.
pub const VERB_FLAG_COMPRESSED: u8 = 0x80;

/// Maximum number of packet hops allowed by the protocol.
pub const PROTOCOL_MAX_HOPS: usize = 7;

/// Index of packet fragment indicator byte to detect fragments.
pub const FRAGMENT_INDICATOR_INDEX: usize = 13;

/// Byte found at FRAGMENT_INDICATOR_INDEX to indicate a fragment.
pub const FRAGMENT_INDICATOR: u8 = 0xff;

/// Prefix indicating reserved addresses (that can't actually be addresses).
pub const ADDRESS_RESERVED_PREFIX: u8 = 0xff;
