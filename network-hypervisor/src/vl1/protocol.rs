use std::mem::MaybeUninit;

use crate::vl1::Address;
use crate::vl1::buffer::RawObject;
use crate::vl1::constants::*;

pub const VERB_VL1_NOP: u8 = 0x00;
pub const VERB_VL1_HELLO: u8 = 0x01;
pub const VERB_VL1_ERROR: u8 = 0x02;
pub const VERB_VL1_OK: u8 = 0x03;
pub const VERB_VL1_WHOIS: u8 = 0x04;
pub const VERB_VL1_RENDEZVOUS: u8 = 0x05;
pub const VERB_VL1_ECHO: u8 = 0x08;
pub const VERB_VL1_PUSH_DIRECT_PATHS: u8 = 0x10;
pub const VERB_VL1_USER_MESSAGE: u8 = 0x14;
pub const VERB_VL1_REMOTE_TRACE: u8 = 0x15;

/// A unique packet identifier, also the cryptographic nonce.
///
/// Packet IDs are stored as u64s for efficiency but they should be treated as
/// [u8; 8] fields in that their endianness is "wire" endian. If for some reason
/// packet IDs need to be portably compared or shared across systems they should
/// be treated as bytes not integers.
pub type PacketID = u64;

/// ZeroTier unencrypted outer packet header
///
/// This is the header for a complete packet. If the fragmented flag is set, it will
/// arrive with one or more fragments that must be assembled to complete it.
#[repr(packed)]
pub struct PacketHeader {
    pub id: PacketID,
    pub dest: [u8; 5],
    pub src: [u8; 5],
    pub flags_cipher_hops: u8,
    pub message_auth: [u8; 8],
}

unsafe impl RawObject for PacketHeader {}

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
    pub fn increment_hops(&mut self) {
        let f = self.flags_cipher_hops;
        self.flags_cipher_hops = (f & HEADER_FLAGS_FIELD_MASK_HIDE_HOPS) | ((f + 1) & HEADER_FLAGS_FIELD_MASK_HOPS);
    }

    #[inline(always)]
    pub fn is_fragmented(&self) -> bool {
        (self.flags_cipher_hops & HEADER_FLAG_FRAGMENTED) != 0
    }

    #[inline(always)]
    pub fn destination(&self) -> Address {
        Address::from(&self.dest)
    }

    #[inline(always)]
    pub fn source(&self) -> Address {
        Address::from(&self.src)
    }

    #[inline(always)]
    pub fn id_bytes(&self) -> &[u8; 8] {
        unsafe { &*(self as *const Self).cast::<[u8; 8]>() }
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
        id[0..8].copy_from_slice(self.id_bytes());
        id[8..16].copy_from_slice(&self.message_auth);
        id
    }
}

/// ZeroTier fragment header
///
/// Fragments are indicated by byte 0xff at the start of the source address, which
/// is normally illegal since addresses can't begin with that. Fragmented packets
/// will arrive with the first fragment carrying a normal header with the fragment
/// bit set and remaining fragments being these.
#[repr(packed)]
pub struct FragmentHeader {
    pub id: PacketID,              // packet ID
    pub dest: [u8; 5],             // destination address
    pub fragment_indicator: u8,    // always 0xff in fragments
    pub total_and_fragment_no: u8, // TTTTNNNN (fragment number, total fragments)
    pub reserved_hops: u8,         // rrrrrHHH (3 hops bits, rest reserved)
}

unsafe impl RawObject for FragmentHeader {}

impl FragmentHeader {
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
        self.reserved_hops & HEADER_FLAGS_FIELD_MASK_HOPS
    }

    #[inline(always)]
    pub fn increment_hops(&mut self) {
        let f = self.reserved_hops;
        self.reserved_hops = (f & HEADER_FLAGS_FIELD_MASK_HIDE_HOPS) | ((f + 1) & HEADER_FLAGS_FIELD_MASK_HOPS);
    }

    #[inline(always)]
    pub fn destination(&self) -> Address {
        Address::from(&self.dest)
    }
}

#[cfg(test)]
mod tests {
    use std::mem::size_of;

    use crate::vl1::constants::{FRAGMENT_HEADER_SIZE, PACKET_HEADER_SIZE};
    use crate::vl1::protocol::{FragmentHeader, PacketHeader};

    #[test]
    fn representation() {
        assert_eq!(size_of::<PacketHeader>(), PACKET_HEADER_SIZE);
        assert_eq!(size_of::<FragmentHeader>(), FRAGMENT_HEADER_SIZE);

        let mut foo = [0_u8; 32];
        unsafe {
            (*foo.as_mut_ptr().cast::<PacketHeader>()).src[0] = 0xff;
            assert_eq!((*foo.as_ptr().cast::<FragmentHeader>()).fragment_indicator, 0xff);
        }

        let bar = PacketHeader{
            id: 0x0102030405060708_u64.to_be(),
            dest: [0_u8; 5],
            src: [0_u8; 5],
            flags_cipher_hops: 0,
            message_auth: [0_u8; 8],
        };
        assert_eq!(bar.id_bytes().clone(), [1_u8, 2, 3, 4, 5, 6, 7, 8]);
    }
}
