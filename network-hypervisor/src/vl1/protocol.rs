use std::ops::Not;

use crate::vl1::buffer::{RawObject, Buffer};
use crate::vl1::constants::{HEADER_FLAGS_FIELD_MASK_CIPHER, HEADER_FLAGS_FIELD_MASK_HOPS, HEADER_FLAG_FRAGMENTED, FRAGMENT_INDICATOR};
use crate::vl1::Address;

/// A unique packet identifier, also the cryptographic nonce.
/// Packet IDs are stored as u64s for efficiency but they should be treated as
/// [u8; 8] fields in that their endianness is "wire" endian. If for some reason
/// packet IDs need to be portably compared or shared across systems they should
/// be treated as bytes not integers.
pub type PacketID = u64;

/// ZeroTier unencrypted outer header
/// This is the header for a complete packet. If the fragmented flag is set, it will
/// arrive with one or more fragments that must be assembled to complete it.
#[derive(Clone)]
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

    /// Get this packet's hops field.
    /// This is the only field in the unencrypted header that is not authenticated, allowing intermediate
    /// nodes to increment it as they forward packets between indirectly connected peers.
    #[inline(always)]
    pub fn hops(&self) -> u8 {
        self.flags_cipher_hops & HEADER_FLAGS_FIELD_MASK_HOPS
    }

    #[inline(always)]
    pub fn increment_hops(&mut self) {
        let f = self.flags_cipher_hops;
        self.flags_cipher_hops = (f & HEADER_FLAGS_FIELD_MASK_HOPS.not()) | ((f + 1) & HEADER_FLAGS_FIELD_MASK_HOPS);
    }

    /// If true, this packet requires one or more fragments to fully assemble.
    /// The one with the full header is always fragment 0. Note that is_fragment() is checked first
    /// to see if this IS a fragment.
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
}

/// ZeroTier fragment header
/// Fragments are indicated by byte 0xff at the start of the source address, which
/// is normally illegal since addresses can't begin with that. Fragmented packets
/// will arrive with the first fragment carrying a normal header with the fragment
/// bit set and remaining fragments being these.
#[derive(Clone)]
#[repr(packed)]
pub struct FragmentHeader {
    pub id: PacketID,              // packet ID
    pub dest: [u8; 5],             // destination address
    pub fragment_indicator: u8,    // always 0xff in fragments
    pub total_and_fragment_no: u8, // TTTTNNNN (fragment number, total fragments)
    pub reserved_hops: u8,         // rrrrrHHH (3 hops bits, rest reserved)
}

unsafe impl crate::vl1::buffer::RawObject for FragmentHeader {}

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
        self.reserved_hops = (f & HEADER_FLAGS_FIELD_MASK_HOPS.not()) | ((f + 1) & HEADER_FLAGS_FIELD_MASK_HOPS);
    }

    #[inline(always)]
    pub fn destination(&self) -> Address {
        Address::from(&self.dest)
    }
}

#[cfg(test)]
mod tests {
    use std::mem::size_of;
    use crate::vl1::protocol::{PacketHeader, FragmentHeader};
    use crate::vl1::constants::{PACKET_HEADER_SIZE, FRAGMENT_SIZE_MIN};

    #[test]
    fn object_sizing() {
        assert_eq!(size_of::<PacketHeader>(), PACKET_HEADER_SIZE);
        assert_eq!(size_of::<FragmentHeader>(), FRAGMENT_SIZE_MIN);
    }
}
