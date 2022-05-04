/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::convert::TryFrom;
use std::mem::MaybeUninit;

use crate::util::buffer::{Buffer, FlatBlob};
use crate::vl1::Address;

pub const VERB_VL1_NOP: u8 = 0x00;
pub const VERB_VL1_HELLO: u8 = 0x01;
pub const VERB_VL1_ERROR: u8 = 0x02;
pub const VERB_VL1_OK: u8 = 0x03;
pub const VERB_VL1_WHOIS: u8 = 0x04;
pub const VERB_VL1_RENDEZVOUS: u8 = 0x05;
pub const VERB_VL1_ECHO: u8 = 0x08;
pub const VERB_VL1_PUSH_DIRECT_PATHS: u8 = 0x10;
pub const VERB_VL1_USER_MESSAGE: u8 = 0x14;

/// Default maximum payload size for UDP transport.
///
/// This is small enough to traverse numerous weird networks including PPPoE and Google Cloud's
/// weird exit MTU restriction, but is large enough that a standard 2800 byte frame creates only
/// two fragments.
pub const UDP_DEFAULT_MTU: usize = 1432;

/// KBKDF usage label indicating a key used to HMAC packets for extended authentication.
pub const KBKDF_KEY_USAGE_LABEL_PACKET_HMAC: u8 = b'M';

/// KBKDF usage label for the first AES-GMAC-SIV key.
pub const KBKDF_KEY_USAGE_LABEL_AES_GMAC_SIV_K0: u8 = b'0';

/// KBKDF usage label for the second AES-GMAC-SIV key.
pub const KBKDF_KEY_USAGE_LABEL_AES_GMAC_SIV_K1: u8 = b'1';

/// KBKDF usage label for the private section of HELLOs.
pub const KBKDF_KEY_USAGE_LABEL_HELLO_PRIVATE_SECTION: u8 = b'h';

/// KBKDF usage label for the key used to advance the ratchet.
pub const KBKDF_KEY_USAGE_LABEL_EPHEMERAL_RATCHET_KEY: u8 = b'e';

/// Try to re-key ephemeral keys after this time.
pub const EPHEMERAL_SECRET_REKEY_AFTER_TIME: i64 = 300000; // 5 minutes

/// Maximum number of times to use an ephemeral secret before trying to replace it.
pub const EPHEMERAL_SECRET_REKEY_AFTER_USES: usize = 536870912; // 1/4 the NIST/FIPS security bound of 2^31

/// Ephemeral secret reject after time.
pub const EPHEMERAL_SECRET_REJECT_AFTER_TIME: i64 = EPHEMERAL_SECRET_REKEY_AFTER_TIME * 2;

/// Ephemeral secret reject after uses.
pub const EPHEMERAL_SECRET_REJECT_AFTER_USES: usize = 2147483648; // NIST/FIPS security bound

pub const SESSION_METADATA_INSTANCE_ID: &'static str = "i";
pub const SESSION_METADATA_CLOCK: &'static str = "t";
pub const SESSION_METADATA_SENT_TO: &'static str = "d";
pub const SESSION_METADATA_EPHEMERAL_CURRENT_SYMMETRIC_KEY_ID: &'static str = "e";
pub const SESSION_METADATA_EPHEMERAL_PUBLIC_OFFER: &'static str = "E";

/// Length of an address in bytes.
pub const ADDRESS_SIZE: usize = 5;

/// Length of an address in string format.
pub const ADDRESS_SIZE_STRING: usize = 10;

/// Prefix indicating reserved addresses (that can't actually be addresses).
pub const ADDRESS_RESERVED_PREFIX: u8 = 0xff;

/// Size of packet header that lies outside the encryption envelope.
pub const PACKET_HEADER_SIZE: usize = 27;

/// Minimum packet, which is the header plus a verb.
pub const PACKET_SIZE_MIN: usize = PACKET_HEADER_SIZE + 1;

/// Maximum size of an entire packet.
pub const PACKET_SIZE_MAX: usize = PACKET_HEADER_SIZE + PACKET_PAYLOAD_SIZE_MAX;

/// Maximum packet payload size including the verb/flags field.
///
/// This is large enough to carry "jumbo MTU" packets. The exact
/// value is because 10005+27 == 10032 which is divisible by 16. This
/// improves memory layout and alignment when buffers are allocated.
/// This value could technically be increased but it would require a
/// protocol version bump and only new nodes would be able to accept
/// the new size.
pub const PACKET_PAYLOAD_SIZE_MAX: usize = 10005;

/// Index of packet verb after header.
pub const PACKET_VERB_INDEX: usize = 27;

/// Index of destination in both fragment and full packet headers.
pub const PACKET_DESTINATION_INDEX: usize = 8;

/// Index of 8-byte MAC field in packet header.
pub const HEADER_MAC_FIELD_INDEX: usize = 19;

/// Mask to select cipher from header flags field.
pub const HEADER_FLAGS_FIELD_MASK_CIPHER: u8 = 0x30;

/// Mask to select packet hops from header flags field.
pub const HEADER_FLAGS_FIELD_MASK_HOPS: u8 = 0x07;

/// Mask to select packet hops from header flags field.
pub const HEADER_FLAGS_FIELD_MASK_HIDE_HOPS: u8 = 0xf8;

/// Index of hops/flags field
pub const HEADER_FLAGS_FIELD_INDEX: usize = 18;

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
pub const PACKET_FRAGMENT_SIZE_MIN: usize = 16;

/// Size of fragment header after which data begins.
pub const FRAGMENT_HEADER_SIZE: usize = 16;

/// Maximum allowed number of fragments.
pub const PACKET_FRAGMENT_COUNT_MAX: usize = 8;

/// Time after which an incomplete fragmented packet expires.
pub const PACKET_FRAGMENT_EXPIRATION: i64 = 1500;

/// Maximum number of inbound fragmented packets to handle at once per path.
/// This is a sanity limit to prevent memory exhaustion due to DOS attacks or broken peers.
pub const PACKET_FRAGMENT_MAX_INBOUND_PACKETS_PER_PATH: usize = 256;

/// Index of packet fragment indicator byte to detect fragments.
pub const PACKET_FRAGMENT_INDICATOR_INDEX: usize = 13;

/// Byte found at FRAGMENT_INDICATOR_INDEX to indicate a fragment.
pub const PACKET_FRAGMENT_INDICATOR: u8 = 0xff;

/// Verb (inner) flag indicating that the packet's payload (after the verb) is LZ4 compressed.
pub const VERB_FLAG_COMPRESSED: u8 = 0x80;

/// Verb (inner) flag indicating that payload is authenticated with HMAC-SHA384.
pub const VERB_FLAG_EXTENDED_AUTHENTICATION: u8 = 0x40;

/// Mask to get only the verb from the verb + verb flags byte.
pub const VERB_MASK: u8 = 0x1f;

/// Maximum number of verbs that the protocol can support.
pub const VERB_MAX_COUNT: usize = 32;

/// Maximum number of packet hops allowed by the protocol.
pub const PROTOCOL_MAX_HOPS: u8 = 7;

/// Maximum number of hops to allow.
pub const FORWARD_MAX_HOPS: u8 = 3;

/// Maximum difference between current packet ID counter and OK/ERROR in-re packet ID.
pub const PACKET_RESPONSE_COUNTER_DELTA_MAX: u64 = 1024;

/// Frequency for WHOIS retries
pub const WHOIS_RETRY_INTERVAL: i64 = 1000;

/// Maximum number of WHOIS retries
pub const WHOIS_RETRY_MAX: u16 = 3;

/// Maximum number of packets to queue up behind a WHOIS.
pub const WHOIS_MAX_WAITING_PACKETS: usize = 64;

/// Keepalive interval for paths in milliseconds.
pub const PATH_KEEPALIVE_INTERVAL: i64 = 20000;

/// Proof of work difficulty (threshold) for identity generation.
pub const IDENTITY_POW_THRESHOLD: u8 = 17;

/// Compress a packet and return true if compressed.
/// The 'dest' buffer must be empty (will panic otherwise). A return value of false indicates an error or
/// that the data was not compressible. The state of the destination buffer is undefined on a return
/// value of false.
pub fn compress_packet(src: &[u8], dest: &mut Buffer<{ PACKET_SIZE_MAX }>) -> bool {
    if src.len() > PACKET_VERB_INDEX {
        debug_assert!(dest.is_empty());
        let cs = {
            let d = dest.as_bytes_mut();
            d[0..PACKET_VERB_INDEX].copy_from_slice(&src[0..PACKET_VERB_INDEX]);
            d[PACKET_VERB_INDEX] = src[PACKET_VERB_INDEX] | VERB_FLAG_COMPRESSED;
            lz4_flex::block::compress_into(&src[PACKET_VERB_INDEX + 1..], &mut d[PACKET_VERB_INDEX + 1..])
        };
        if cs.is_ok() {
            let cs = cs.unwrap();
            if cs > 0 && cs < (src.len() - PACKET_VERB_INDEX) {
                unsafe { dest.set_size_unchecked(PACKET_VERB_INDEX + 1 + cs) };
                return true;
            }
        }
    }
    return false;
}

/// ZeroTier unencrypted outer packet header
///
/// This is the header for a complete packet. If the fragmented flag is set, it will
/// arrive with one or more fragments that must be assembled to complete it.
#[repr(C, packed)]
pub struct PacketHeader {
    pub id: [u8; 8],
    pub dest: [u8; 5],
    pub src: [u8; 5],
    pub flags_cipher_hops: u8,
    pub mac: [u8; 8],
}

unsafe impl FlatBlob for PacketHeader {}

impl PacketHeader {
    #[inline(always)]
    pub fn cipher(&self) -> u8 {
        self.flags_cipher_hops & HEADER_FLAGS_FIELD_MASK_CIPHER
    }

    #[inline(always)]
    pub fn hops(&self) -> u8 {
        self.flags_cipher_hops & HEADER_FLAGS_FIELD_MASK_HOPS
    }

    #[inline(always)]
    pub fn increment_hops(&mut self) -> u8 {
        let f = self.flags_cipher_hops;
        let h = (f + 1) & HEADER_FLAGS_FIELD_MASK_HOPS;
        self.flags_cipher_hops = (f & HEADER_FLAGS_FIELD_MASK_HIDE_HOPS) | h;
        h
    }

    #[inline(always)]
    pub fn is_fragmented(&self) -> bool {
        (self.flags_cipher_hops & HEADER_FLAG_FRAGMENTED) != 0
    }

    #[inline(always)]
    pub fn as_bytes(&self) -> &[u8; PACKET_HEADER_SIZE] {
        unsafe { &*(self as *const Self).cast::<[u8; PACKET_HEADER_SIZE]>() }
    }

    #[inline(always)]
    pub fn aad_bytes(&self) -> [u8; 11] {
        let mut id = unsafe { MaybeUninit::<[u8; 11]>::uninit().assume_init() };
        id[0..5].copy_from_slice(&self.dest);
        id[5..10].copy_from_slice(&self.src);
        id[10] = self.flags_cipher_hops & HEADER_FLAGS_FIELD_MASK_HIDE_HOPS;
        id
    }

    #[inline(always)]
    pub fn aes_gmac_siv_tag(&self) -> [u8; 16] {
        let mut id = unsafe { MaybeUninit::<[u8; 16]>::uninit().assume_init() };
        id[0..8].copy_from_slice(&self.id);
        id[8..16].copy_from_slice(&self.mac);
        id
    }
}

/// ZeroTier fragment header
///
/// Fragments are indicated by byte 0xff at the start of the source address, which
/// is normally illegal since addresses can't begin with that. Fragmented packets
/// will arrive with the first fragment carrying a normal header with the fragment
/// bit set and remaining fragments being these.
#[repr(C, packed)]
pub struct FragmentHeader {
    pub id: [u8; 8],               // (outer) packet ID
    pub dest: [u8; 5],             // destination address
    pub fragment_indicator: u8,    // always 0xff in fragments
    pub total_and_fragment_no: u8, // TTTTNNNN (fragment number, total fragments)
    pub reserved_hops: u8,         // rrrrrHHH (3 hops bits, rest reserved)
}

unsafe impl FlatBlob for FragmentHeader {}

impl FragmentHeader {
    #[inline(always)]
    pub fn is_fragment(&self) -> bool {
        self.fragment_indicator == PACKET_FRAGMENT_INDICATOR
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
        self.reserved_hops & HEADER_FLAGS_FIELD_MASK_HOPS
    }

    #[inline(always)]
    pub fn increment_hops(&mut self) -> u8 {
        let f = self.reserved_hops;
        let h = (f + 1) & HEADER_FLAGS_FIELD_MASK_HOPS;
        self.reserved_hops = (f & HEADER_FLAGS_FIELD_MASK_HIDE_HOPS) | h;
        h
    }

    #[inline(always)]
    pub fn as_bytes(&self) -> &[u8; FRAGMENT_HEADER_SIZE] {
        unsafe { &*(self as *const Self).cast::<[u8; FRAGMENT_HEADER_SIZE]>() }
    }
}

pub(crate) mod message_component_structs {
    use crate::util::buffer::FlatBlob;

    #[repr(C, packed)]
    pub struct OkHeader {
        pub in_re_verb: u8,
        pub in_re_message_id: [u8; 8],
    }

    unsafe impl FlatBlob for OkHeader {}

    #[repr(C, packed)]
    pub struct ErrorHeader {
        pub in_re_verb: u8,
        pub in_re_message_id: [u8; 8],
        pub error_code: u8,
    }

    unsafe impl FlatBlob for ErrorHeader {}

    #[repr(C, packed)]
    pub struct HelloFixedHeaderFields {
        pub verb: u8,
        pub version_proto: u8,
        pub version_major: u8,
        pub version_minor: u8,
        pub version_revision: [u8; 2], // u16
        pub timestamp: [u8; 8],        // u64
    }

    unsafe impl FlatBlob for HelloFixedHeaderFields {}

    #[repr(C, packed)]
    pub struct OkHelloFixedHeaderFields {
        pub timestamp_echo: [u8; 8], // u64
        pub version_proto: u8,
        pub version_major: u8,
        pub version_minor: u8,
        pub version_revision: [u8; 2], // u16
    }

    unsafe impl FlatBlob for OkHelloFixedHeaderFields {}
}

#[cfg(test)]
mod tests {
    use std::mem::size_of;

    use crate::vl1::protocol::*;

    #[test]
    fn representation() {
        assert_eq!(size_of::<message_component_structs::OkHeader>(), 9);
        assert_eq!(size_of::<message_component_structs::ErrorHeader>(), 10);
        assert_eq!(size_of::<PacketHeader>(), PACKET_HEADER_SIZE);
        assert_eq!(size_of::<FragmentHeader>(), FRAGMENT_HEADER_SIZE);

        let mut foo = [0_u8; 32];
        unsafe {
            (*foo.as_mut_ptr().cast::<PacketHeader>()).src[0] = 0xff;
            assert_eq!((*foo.as_ptr().cast::<FragmentHeader>()).fragment_indicator, 0xff);
        }
    }
}
