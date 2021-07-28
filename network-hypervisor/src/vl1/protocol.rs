use crate::vl1::constants::{HEADER_FLAGS_FIELD_MASK_CIPHER, HEADER_FLAGS_FIELD_MASK_HOPS, HEADER_FLAG_FRAGMENTED};
use std::ops::Not;
use crate::vl1::buffer::RawObject;

type PacketID = u64;

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

    #[inline(always)]
    pub fn hops(&self) -> u8 {
        self.flags_cipher_hops & HEADER_FLAGS_FIELD_MASK_HOPS
    }

    #[inline(always)]
    pub fn increment_hops(&mut self) {
        let f = self.flags_cipher_hops;
        self.flags_cipher_hops = (f & HEADER_FLAGS_FIELD_MASK_HOPS.not()) | ((f + 1) & HEADER_FLAGS_FIELD_MASK_HOPS);
    }

    #[inline(always)]
    pub fn is_fragmented(&self) -> bool {
        (self.flags_cipher_hops & HEADER_FLAG_FRAGMENTED) != 0
    }
}

#[derive(Clone)]
#[repr(packed)]
pub struct FragmentHeader {
    pub id: PacketID,
    pub dest: [u8; 5],
    pub fragment_indicator: u8,
    pub total_and_fragment_no: u8,
    pub hops: u8,
}

unsafe impl crate::vl1::buffer::RawObject for FragmentHeader {}

impl FragmentHeader {
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
        self.hops & HEADER_FLAGS_FIELD_MASK_HOPS
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
