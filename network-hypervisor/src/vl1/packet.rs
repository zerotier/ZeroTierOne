use crate::vl1::buffer::{Buffer, RawObject};
use crate::vl1::protocol::{HEADER_FLAGS_FIELD_MASK_CIPHER, HEADER_FLAGS_FIELD_MASK_HOPS, HEADER_FLAG_FRAGMENTED};
use std::ops::Not;

type PacketID = u64;

#[derive(Clone)]
#[repr(packed)]
pub struct Header {
    pub id: [u8; 8],
    pub dest: [u8; 5],
    pub src: [u8; 5],
    pub flags_cipher_hops: u8,
    pub message_auth: [u8; 8],
}

unsafe impl RawObject for Header {}

impl Header {
    /// Get this packet's ID as a u64.
    /// While this returns u64, the returned integer contains raw packet ID bytes
    /// in "native" byte order and should be treated conceptually like [u8; 8].
    /// In particular, greater/less than comparisons may differ across architectures.
    #[inline(always)]
    pub fn to_id(&self) -> PacketID {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64", target_arch = "aarch64"))]
        unsafe { *self.id.as_ptr().cast::<u64>() }
        #[cfg(not(any(target_arch = "x86", target_arch = "x86_64", target_arch = "aarch64")))]
        u64::from_ne_bytes(self.id.clone())
    }

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

/// Packet is a Buffer with the packet Header and the packet max payload size.
pub type Packet = Buffer<Header, { crate::vl1::protocol::PACKET_SIZE_MAX }>;

#[derive(Clone)]
#[repr(packed)]
pub struct FragmentHeader {
    pub id: [u8; 8],
    pub dest: [u8; 5],
    pub fragment_indicator: u8,
    pub total_and_fragment_no: u8,
    pub hops: u8,
}

unsafe impl RawObject for FragmentHeader {}

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
    use crate::vl1::packet::{Header, FragmentHeader};
    use crate::vl1::protocol::{PACKET_HEADER_SIZE, FRAGMENT_SIZE_MIN};

    #[test]
    fn object_sizing() {
        assert_eq!(size_of::<Header>(), PACKET_HEADER_SIZE);
        assert_eq!(size_of::<FragmentHeader>(), FRAGMENT_SIZE_MIN);
    }
}
