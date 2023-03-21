// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::convert::TryFrom;
use std::mem::MaybeUninit;

use crate::vl1::Address;

use zerotier_utils::buffer::{Buffer, PooledBufferFactory};
use zerotier_utils::pool::{Pool, PoolFactory, Pooled};

use zerotier_crypto::aes_gmac_siv::AesGmacSiv;
use zerotier_crypto::hash::hmac_sha384;
use zerotier_crypto::secret::Secret;

/*
 * Legacy V1 protocol versions:
 *
 * 1  - 0.2.0 ... 0.2.5
 * 2  - 0.3.0 ... 0.4.5
 *    + Added signature and originating peer to multicast frame
 *    + Double size of multicast frame bloom filter
 * 3  - 0.5.0 ... 0.6.0
 *    + Yet another multicast redesign
 *    + New crypto completely changes key agreement cipher
 * 4  - 0.6.0 ... 1.0.6
 *    + BREAKING CHANGE: New identity format based on hashcash design
 * 5  - 1.1.0 ... 1.1.5
 *    + Supports echo
 *    + Supports in-band world (root server definition) updates
 *    + Clustering! (Though this will work with protocol v4 clients.)
 *    + Otherwise backward compatible with protocol v4
 * 6  - 1.1.5 ... 1.1.10
 *    + Network configuration format revisions including binary values
 * 7  - 1.1.10 ... 1.1.17
 *    + Introduce trusted paths for local SDN use
 * 8  - 1.1.17 ... 1.2.0
 *    + Multipart network configurations for large network configs
 *    + Tags and Capabilities
 *    + inline push of CertificateOfMembership deprecated
 * 9  - 1.2.0 ... 1.2.14
 * 10 - 1.4.0 ... 1.4.6
 *    + Contained early pre-alpha versions of multipath, which are deprecated
 * 11 - 1.6.0 ... 2.0.0
 *    + Supports and prefers AES-GMAC-SIV symmetric crypto, backported.
 */
pub const PROTOCOL_VERSION: u8 = 20;

/// Minimum peer protocol version supported.
///
/// We could probably push it back to 8 or 9 with some added support for sending Salsa/Poly packets.
pub const PROTOCOL_VERSION_MIN: u8 = 11;

/// V2 is this protocol version or higher.
pub const PROTOCOL_VERSION_V2: u8 = 20;

/// Size of a pooled packet buffer.
pub const PACKET_BUFFER_SIZE: usize = 16384;

/// Buffer sized for ZeroTier packets.
pub type PacketBuffer = Buffer<PACKET_BUFFER_SIZE>;

/// Factory type to supply to a new PacketBufferPool, used in PooledPacketBuffer and PacketBufferPool types.
pub type PacketBufferFactory = PooledBufferFactory<PACKET_BUFFER_SIZE>;

/// Packet buffer checked out of pool, automatically returns on drop.
pub type PooledPacketBuffer = Pooled<PacketBuffer, PacketBufferFactory>;

/// Source for instances of PacketBuffer
pub type PacketBufferPool = Pool<PacketBuffer, PacketBufferFactory>;

/// 64-bit message ID (obtained after AEAD decryption).
pub type MessageId = u64;

/// ZeroTier VL1 and VL2 wire protocol message types.
pub mod message_type {
    // VL1: Virtual Layer 1, the peer to peer network

    pub const VL1_NOP: u8 = 0x00;
    pub const VL1_HELLO: u8 = 0x01;
    pub const VL1_ERROR: u8 = 0x02;
    pub const VL1_OK: u8 = 0x03;
    pub const VL1_WHOIS: u8 = 0x04;
    pub const VL1_RENDEZVOUS: u8 = 0x05;
    pub const VL1_ECHO: u8 = 0x08;
    pub const VL1_PUSH_DIRECT_PATHS: u8 = 0x10;
    pub const VL1_USER_MESSAGE: u8 = 0x14;

    // VL2: Virtual Layer 2, the virtual Ethernet network

    pub const VL2_MULTICAST_LIKE: u8 = 0x09;
    pub const VL2_NETWORK_CREDENTIALS: u8 = 0x0a;
    pub const VL2_NETWORK_CONFIG_REQUEST: u8 = 0x0b;
    pub const VL2_NETWORK_CONFIG: u8 = 0x0c;
    pub const VL2_MULTICAST_GATHER: u8 = 0x0d;

    pub fn name(verb: u8) -> &'static str {
        match verb {
            VL1_NOP => "VL1_NOP",
            VL1_HELLO => "VL1_HELLO",
            VL1_ERROR => "VL1_ERROR",
            VL1_OK => "VL1_OK",
            VL1_WHOIS => "VL1_WHOIS",
            VL1_RENDEZVOUS => "VL1_RENDEZVOUS",
            VL1_ECHO => "VL1_ECHO",
            VL1_PUSH_DIRECT_PATHS => "VL1_PUSH_DIRECT_PATHS",
            VL1_USER_MESSAGE => "VL1_USER_MESSAGE",
            VL2_MULTICAST_LIKE => "VL2_MULTICAST_LIKE",
            VL2_NETWORK_CONFIG_REQUEST => "VL2_NETWORK_CONFIG_REQUEST",
            VL2_NETWORK_CONFIG => "VL2_NETWORK_CONFIG",
            VL2_MULTICAST_GATHER => "VL2_MULTICAST_GATHER",
            _ => "???",
        }
    }
}

/// Verb (inner) flag indicating that the packet's payload (after the verb) is LZ4 compressed.
pub const MESSAGE_FLAG_COMPRESSED: u8 = 0x80;

/// Mask to get only the verb from the verb + verb flags byte.
pub const MESSAGE_TYPE_MASK: u8 = 0x1f;

/// Default maximum payload size for UDP transport.
///
/// This is small enough to traverse numerous weird networks including PPPoE and Google Cloud's
/// weird exit MTU restriction, but is large enough that a standard 2800 byte frame creates only
/// two fragments.
pub const UDP_DEFAULT_MTU: usize = 1432;

/// Default MTU inside VL2 virtual networks.
pub const ZEROTIER_VIRTUAL_NETWORK_DEFAULT_MTU: usize = 2800;

/// Default multicast limit if not set in the network.
pub const DEFAULT_MULTICAST_LIMIT: usize = 32;

/// Length of an address in bytes.
pub const ADDRESS_SIZE: usize = 5;

/// Length of an address in string format.
pub const ADDRESS_SIZE_STRING: usize = 10;

/// Bit mask for address bits in a u64.
pub const ADDRESS_MASK: u64 = 0xffffffffff;

pub mod v1 {
    use super::*;

    /// Size of packet header that lies outside the encryption envelope.
    pub const HEADER_SIZE: usize = 27;

    /// Maximum packet payload size including the verb/flags field.
    ///
    /// This is large enough to carry "jumbo MTU" packets. The exact
    /// value is because 10005+27 == 10032 which is divisible by 16. This
    /// improves memory layout and alignment when buffers are allocated.
    /// This value could technically be increased but it would require a
    /// protocol version bump and only new nodes would be able to accept
    /// the new size.
    pub const PAYLOAD_SIZE_MAX: usize = 10005;

    /// Minimum packet, which is the header plus a verb.
    pub const SIZE_MIN: usize = HEADER_SIZE + 1;

    /// Maximum size of an entire packet.
    pub const SIZE_MAX: usize = HEADER_SIZE + PAYLOAD_SIZE_MAX;

    /// Index of packet verb after header.
    pub const VERB_INDEX: usize = 27;

    /// Index of destination in both fragment and full packet headers.
    pub const DESTINATION_INDEX: usize = 8;

    /// Index of 8-byte MAC field in packet header (also size of header minus MAC).
    pub const MAC_FIELD_INDEX: usize = 19;

    /// Mask to select cipher from header flags field.
    pub const FLAGS_FIELD_MASK_CIPHER: u8 = 0x38;

    /// Mask to select packet hops from header flags field.
    pub const FLAGS_FIELD_MASK_HOPS: u8 = 0x07;

    /// Mask to select packet hops from header flags field.
    pub const FLAGS_FIELD_MASK_HIDE_HOPS: u8 = 0xf8;

    /// Index of hops/flags field
    pub const FLAGS_FIELD_INDEX: usize = 18;

    /// Minimum size of a fragment.
    pub const FRAGMENT_SIZE_MIN: usize = 16;

    /// Size of fragment header after which data begins.
    pub const FRAGMENT_HEADER_SIZE: usize = 16;

    /// Maximum allowed number of fragments.
    pub const FRAGMENT_COUNT_MAX: usize = 8;

    /// Time after which an incomplete fragmented packet expires.
    pub const FRAGMENT_EXPIRATION: i64 = 1500;

    /// Maximum number of inbound fragmented packets to handle at once per path.
    /// This is a sanity limit to prevent memory exhaustion due to DOS attacks or broken peers.
    pub const FRAGMENT_MAX_INBOUND_PACKETS_PER_PATH: usize = 256;

    /// Index of packet fragment indicator byte to detect fragments.
    pub const FRAGMENT_INDICATOR_INDEX: usize = 13;

    /// Byte found at FRAGMENT_INDICATOR_INDEX to indicate a fragment.
    pub const FRAGMENT_INDICATOR: u8 = 0xff;

    /// Header (outer) flag indicating that this packet has additional fragments.
    pub const HEADER_FLAG_FRAGMENTED: u8 = 0x40;

    /// Packet is not encrypted but contains a Poly1305 MAC of the plaintext.
    /// Poly1305 is initialized with Salsa20/12 in the same manner as SALSA2012_POLY1305.
    pub const CIPHER_NOCRYPT_POLY1305: u8 = 0x00;

    /// Packet is encrypted and authenticated with Salsa20/12 and Poly1305.
    /// Construction is the same as that which is used in the NaCl secret box functions.
    pub const CIPHER_SALSA2012_POLY1305: u8 = 0x08;

    /// Formerly 'NONE' which is deprecated; reserved for future use.
    pub const CIPHER_RESERVED: u8 = 0x10;

    /// Packet is encrypted and authenticated with AES-GMAC-SIV (AES-256).
    pub const CIPHER_AES_GMAC_SIV: u8 = 0x18;

    /// KBKDF usage label indicating a key used to HMAC packets for extended authentication.
    pub const KBKDF_KEY_USAGE_LABEL_PACKET_HMAC: u8 = b'm';

    /// KBKDF usage label for the first AES-GMAC-SIV key.
    pub const KBKDF_KEY_USAGE_LABEL_AES_GMAC_SIV_K0: u8 = b'0';

    /// KBKDF usage label for the second AES-GMAC-SIV key.
    pub const KBKDF_KEY_USAGE_LABEL_AES_GMAC_SIV_K1: u8 = b'1';

    /// Maximum number of packet hops allowed by the protocol.
    pub const PROTOCOL_MAX_HOPS: u8 = 7;

    /// Maximum number of hops to allow.
    pub const FORWARD_MAX_HOPS: u8 = 3;

    /// Set header flag indicating that a packet is fragmented.
    ///
    /// This will panic if the buffer provided doesn't contain a proper header.
    #[inline(always)]
    pub(crate) fn set_packet_fragment_flag<const S: usize>(pkt: &mut Buffer<S>) {
        pkt.as_bytes_mut()[FLAGS_FIELD_INDEX] |= HEADER_FLAG_FRAGMENTED;
    }

    /// ZeroTier unencrypted outer packet header
    ///
    /// This is the header for a complete packet. If the fragmented flag is set, it will
    /// arrive with one or more fragments that must be assembled to complete it.
    #[derive(Clone, Copy)]
    #[repr(C, packed)]
    pub struct PacketHeader {
        pub id: [u8; 8],
        pub dest: [u8; 5],
        pub src: [u8; 5],
        pub flags_cipher_hops: u8,
        pub mac: [u8; 8],
    }

    impl PacketHeader {
        #[inline(always)]
        pub fn packet_id(&self) -> u64 {
            u64::from_ne_bytes(self.id)
        }

        #[inline(always)]
        pub fn cipher(&self) -> u8 {
            self.flags_cipher_hops & FLAGS_FIELD_MASK_CIPHER
        }

        #[inline(always)]
        pub fn hops(&self) -> u8 {
            self.flags_cipher_hops & FLAGS_FIELD_MASK_HOPS
        }

        #[inline(always)]
        pub fn increment_hops(&mut self) -> u8 {
            let f = self.flags_cipher_hops;
            let h = (f + 1) & FLAGS_FIELD_MASK_HOPS;
            self.flags_cipher_hops = (f & FLAGS_FIELD_MASK_HIDE_HOPS) | h;
            h
        }

        #[inline(always)]
        pub fn is_fragmented(&self) -> bool {
            (self.flags_cipher_hops & HEADER_FLAG_FRAGMENTED) != 0
        }

        #[inline(always)]
        pub fn as_bytes(&self) -> &[u8; HEADER_SIZE] {
            unsafe { &*(self as *const Self).cast::<[u8; HEADER_SIZE]>() }
        }

        #[inline(always)]
        pub fn aes_gmac_siv_tag(&self) -> [u8; 16] {
            let mut id = 0u128.to_ne_bytes();
            id[0..8].copy_from_slice(&self.id);
            id[8..16].copy_from_slice(&self.mac);
            id
        }
    }

    #[inline(always)]
    pub fn get_packet_aad_bytes(destination: &Address, source: &Address, flags_cipher_hops: u8) -> [u8; 11] {
        let mut id = [0u8; 11];
        id[0..5].copy_from_slice(destination.legacy_address().as_bytes());
        id[5..10].copy_from_slice(source.legacy_address().as_bytes());
        id[10] = flags_cipher_hops & FLAGS_FIELD_MASK_HIDE_HOPS;
        id
    }

    /// ZeroTier fragment header
    ///
    /// Fragments are indicated by byte 0xff at the start of the source address, which
    /// is normally illegal since addresses can't begin with that. Fragmented packets
    /// will arrive with the first fragment carrying a normal header with the fragment
    /// bit set and remaining fragments being these.
    #[derive(Clone, Copy)]
    #[repr(C, packed)]
    pub struct FragmentHeader {
        pub id: [u8; 8],               // (outer) packet ID
        pub dest: [u8; 5],             // destination address
        pub fragment_indicator: u8,    // always 0xff in fragments
        pub total_and_fragment_no: u8, // TTTTNNNN (fragment number, total fragments)
        pub reserved_hops: u8,         // rrrrrHHH (3 hops bits, rest reserved)
    }

    impl FragmentHeader {
        #[inline(always)]
        pub fn packet_id(&self) -> u64 {
            u64::from_ne_bytes(self.id)
        }

        #[inline(always)]
        pub fn is_fragment(&self) -> bool {
            self.fragment_indicator == FRAGMENT_INDICATOR
        }

        #[inline(always)]
        pub fn total_fragments(&self) -> u8 {
            self.total_and_fragment_no >> 4
        }

        #[inline(always)]
        pub fn fragment_no(&self) -> u8 {
            self.total_and_fragment_no & 0x0f
        }

        #[inline(always)]
        pub fn hops(&self) -> u8 {
            self.reserved_hops & FLAGS_FIELD_MASK_HOPS
        }

        #[inline(always)]
        pub fn increment_hops(&mut self) -> u8 {
            let f = self.reserved_hops;
            let h = (f + 1) & FLAGS_FIELD_MASK_HOPS;
            self.reserved_hops = (f & FLAGS_FIELD_MASK_HIDE_HOPS) | h;
            h
        }

        #[inline(always)]
        pub fn as_bytes(&self) -> &[u8; FRAGMENT_HEADER_SIZE] {
            unsafe { &*(self as *const Self).cast::<[u8; FRAGMENT_HEADER_SIZE]>() }
        }
    }

    /// Flat packed structs for fixed length header blocks in messages.
    pub mod message_component_structs {
        #[derive(Clone, Copy)]
        #[repr(C, packed)]
        pub(crate) struct HelloFixedHeaderFields {
            pub verb: u8,
            pub version_proto: u8,
            pub version_major: u8,
            pub version_minor: u8,
            pub version_revision: [u8; 2], // u16
            pub timestamp: [u8; 8],        // u64
        }

        #[derive(Clone, Copy)]
        #[repr(C, packed)]
        pub(crate) struct OkHelloFixedHeaderFields {
            pub timestamp_echo: [u8; 8], // u64
            pub version_proto: u8,
            pub version_major: u8,
            pub version_minor: u8,
            pub version_revision: [u8; 2], // u16
        }
    }

    /// Packet fragment re-assembler and container.
    ///
    /// Performance note: PacketBuffer is Pooled<Buffer> which is NotNull<*mut Buffer>.
    /// That means Option<PacketBuffer> is just a pointer, since NotNull permits the
    /// compiler to optimize out any additional state in Option.
    pub(crate) struct FragmentedPacket {
        pub ts_ticks: i64,
        pub frags: [Option<PooledPacketBuffer>; FRAGMENT_COUNT_MAX],
        pub have: u8,
        pub expecting: u8,
    }

    impl FragmentedPacket {
        #[inline]
        pub fn new(ts: i64) -> Self {
            // 'have' and 'expecting' must be expanded if this is >8
            debug_assert!(v1::FRAGMENT_COUNT_MAX <= 8);

            Self {
                ts_ticks: ts,
                frags: Default::default(),
                have: 0,
                expecting: 0,
            }
        }

        /// Add a fragment to this fragment set and return true if all fragments are present.
        #[inline(always)]
        pub fn add_fragment(&mut self, frag: PooledPacketBuffer, no: u8, expecting: u8) -> bool {
            if let Some(entry) = self.frags.get_mut(no as usize) {
                /*
                 * This works by setting bit N in the 'have' bit mask and then setting X bits
                 * in 'expecting' if the 'expecting' field is non-zero. Since the packet head
                 * does not carry the expecting fragment count (it will be provided as zero) and
                 * all subsequent fragments should have the same fragment count, this will yield
                 * a 'have' of 1 and an 'expecting' of 0 after the head arrives. Then 'expecting'
                 * will be set to the right bit pattern by the first fragment and 'true' will get
                 * returned once all fragments have arrived and therefore all flags in 'have' are
                 * set.
                 *
                 * Receipt of a four-fragment packet would look like:
                 *
                 * after head      : have == 0x01, expecting == 0x00 -> false
                 * after fragment 1: have == 0x03, expecting == 0x0f -> false
                 * after fragment 2: have == 0x07, expecting == 0x0f -> false
                 * after fragment 3: have == 0x0f, expecting == 0x0f -> true (done!)
                 *
                 * This algorithm is just a few instructions in ASM and also correctly handles
                 * duplicated packet fragments. If all fragments never arrive receipt eventually
                 * times out and this is discarded.
                 */

                let _ = entry.insert(frag);

                self.have |= 1_u8.wrapping_shl(no as u32);
                self.expecting |= 0xff_u8.wrapping_shr(8 - (expecting as u32));
                self.have == self.expecting
            } else {
                false
            }
        }
    }

    /// A V1 symmetric secret with master key and sub-keys for AES-GMAC-SIV
    pub(crate) struct SymmetricSecret {
        pub key: Secret<64>,
        pub aes_gmac_siv: Pool<AesGmacSiv, AesGmacSivPoolFactory>,
    }

    fn zt_kbkdf_hmac_sha384(key: &[u8], label: u8) -> Secret<48> {
        Secret(hmac_sha384(key, &[0, 0, 0, 0, b'Z', b'T', label, 0, 0, 0, 0, 0x01, 0x80]))
    }

    impl SymmetricSecret {
        /// Create a new symmetric secret, deriving all sub-keys and such.
        pub fn new(key: Secret<64>) -> SymmetricSecret {
            let aes_factory = AesGmacSivPoolFactory(
                zt_kbkdf_hmac_sha384(&key.0[..48], v1::KBKDF_KEY_USAGE_LABEL_AES_GMAC_SIV_K0).first_n_clone(),
                zt_kbkdf_hmac_sha384(&key.0[..48], v1::KBKDF_KEY_USAGE_LABEL_AES_GMAC_SIV_K1).first_n_clone(),
            );
            SymmetricSecret { key, aes_gmac_siv: Pool::new(2, aes_factory) }
        }
    }

    pub(crate) struct AesGmacSivPoolFactory(Secret<32>, Secret<32>);

    impl PoolFactory<AesGmacSiv> for AesGmacSivPoolFactory {
        #[inline(always)]
        fn create(&self) -> AesGmacSiv {
            AesGmacSiv::new(self.0.as_bytes(), self.1.as_bytes())
        }

        #[inline(always)]
        fn reset(&self, obj: &mut AesGmacSiv) {
            obj.reset();
        }
    }
}

/// Compress a packet payload, returning the new size of the payload or the same size if unchanged.
///
/// This also sets the VERB_FLAG_COMPRESSED flag in the verb, which is assumed to be the first byte.
/// If compression fails for some reason or does not yield a result that is actually smaller, the
/// buffer is left unchanged and its size is returned. If compression succeeds the buffer's data after
/// the first byte (the verb) is rewritten with compressed data and the new size of the payload
/// (including the verb) is returned.
pub fn compress(payload: &mut [u8]) -> usize {
    if payload.len() > 32 {
        let mut tmp = [0u8; 65536];
        if let Ok(mut compressed_size) = lz4_flex::block::compress_into(&payload[1..], &mut tmp) {
            if compressed_size < (payload.len() - 1) {
                payload[0] |= MESSAGE_FLAG_COMPRESSED;
                payload[1..(1 + compressed_size)].copy_from_slice(&tmp[..compressed_size]);
                return 1 + compressed_size;
            }
        }
    }
    return payload.len();
}

#[derive(Clone, Copy)]
#[repr(C, packed)]
pub struct OkHeader {
    pub verb: u8,
    pub in_re_verb: u8,
    pub in_re_message_id: [u8; 8],
}

#[derive(Clone, Copy)]
#[repr(C, packed)]
pub struct ErrorHeader {
    pub verb: u8,
    pub in_re_verb: u8,
    pub in_re_message_id: [u8; 8],
    pub error_code: u8,
}

/// Frequency for WHOIS retries in milliseconds.
pub(crate) const WHOIS_RETRY_INTERVAL: i64 = 2000;

/// Maximum number of WHOIS retries
pub(crate) const WHOIS_RETRY_COUNT_MAX: u16 = 3;

/// Maximum number of packets to queue up behind a WHOIS.
pub(crate) const WHOIS_MAX_WAITING_PACKETS: usize = 32;

/// Keepalive interval for paths in milliseconds.
pub(crate) const PATH_KEEPALIVE_INTERVAL: i64 = 20000;

/// Path object expiration time in milliseconds since last receive.
pub(crate) const PATH_EXPIRATION_TIME: i64 = (PATH_KEEPALIVE_INTERVAL * 2) + 10000;

/// How often to send HELLOs to roots, which is more often than normal peers.
pub(crate) const ROOT_HELLO_INTERVAL: i64 = PATH_KEEPALIVE_INTERVAL * 2;

/// How often to send HELLOs to roots when we are offline.
pub(crate) const ROOT_HELLO_SPAM_INTERVAL: i64 = 5000;

/// How often to send HELLOs to regular peers.
pub(crate) const PEER_HELLO_INTERVAL_MAX: i64 = 300000;

/// Timeout for path association with peers and for peers themselves.
pub(crate) const PEER_EXPIRATION_TIME: i64 = (PEER_HELLO_INTERVAL_MAX * 2) + 10000;

// Multicast LIKE expire time in milliseconds.
pub const VL2_DEFAULT_MULTICAST_LIKE_EXPIRE: i64 = 600000;

#[cfg(test)]
mod tests {
    use std::mem::size_of;

    use crate::protocol::*;

    #[test]
    fn representation() {
        assert_eq!(size_of::<OkHeader>(), 10);
        assert_eq!(size_of::<ErrorHeader>(), 11);
        assert_eq!(size_of::<v1::PacketHeader>(), v1::HEADER_SIZE);
        assert_eq!(size_of::<v1::FragmentHeader>(), v1::FRAGMENT_HEADER_SIZE);

        let mut foo = [0_u8; 32];
        unsafe {
            (*foo.as_mut_ptr().cast::<v1::PacketHeader>()).src[0] = 0xff;
            assert_eq!((*foo.as_ptr().cast::<v1::FragmentHeader>()).fragment_indicator, 0xff);
        }
    }
}
