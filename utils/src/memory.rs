// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

#[cfg(any(target_arch = "x86", target_arch = "x86_64", target_arch = "aarch64"))]
#[allow(unused)]
mod fast_int_memory_access {
    #[inline(always)]
    pub fn u64_from_le_bytes(b: &[u8]) -> u64 {
        assert!(b.len() >= 8);
        unsafe { *b.as_ptr().cast() }
    }

    #[inline(always)]
    pub fn u32_from_le_bytes(b: &[u8]) -> u32 {
        assert!(b.len() >= 4);
        unsafe { *b.as_ptr().cast() }
    }

    #[inline(always)]
    pub fn u16_from_le_bytes(b: &[u8]) -> u16 {
        assert!(b.len() >= 2);
        unsafe { *b.as_ptr().cast() }
    }

    #[inline(always)]
    pub fn i64_from_le_bytes(b: &[u8]) -> i64 {
        assert!(b.len() >= 8);
        unsafe { *b.as_ptr().cast() }
    }

    #[inline(always)]
    pub fn i32_from_le_bytes(b: &[u8]) -> i32 {
        assert!(b.len() >= 4);
        unsafe { *b.as_ptr().cast() }
    }

    #[inline(always)]
    pub fn i16_from_le_bytes(b: &[u8]) -> i16 {
        assert!(b.len() >= 2);
        unsafe { *b.as_ptr().cast() }
    }

    #[inline(always)]
    pub fn u64_from_be_bytes(b: &[u8]) -> u64 {
        assert!(b.len() >= 8);
        unsafe { *b.as_ptr().cast::<u64>() }.swap_bytes()
    }

    #[inline(always)]
    pub fn u32_from_be_bytes(b: &[u8]) -> u32 {
        assert!(b.len() >= 4);
        unsafe { *b.as_ptr().cast::<u32>() }.swap_bytes()
    }

    #[inline(always)]
    pub fn u16_from_be_bytes(b: &[u8]) -> u16 {
        assert!(b.len() >= 2);
        unsafe { *b.as_ptr().cast::<u16>() }.swap_bytes()
    }

    #[inline(always)]
    pub fn i64_from_be_bytes(b: &[u8]) -> i64 {
        assert!(b.len() >= 8);
        unsafe { *b.as_ptr().cast::<i64>() }.swap_bytes()
    }

    #[inline(always)]
    pub fn i32_from_be_bytes(b: &[u8]) -> i32 {
        assert!(b.len() >= 4);
        unsafe { *b.as_ptr().cast::<i32>() }.swap_bytes()
    }

    #[inline(always)]
    pub fn i16_from_be_bytes(b: &[u8]) -> i16 {
        assert!(b.len() >= 2);
        unsafe { *b.as_ptr().cast::<i16>() }.swap_bytes()
    }
}

#[cfg(not(any(target_arch = "x86", target_arch = "x86_64", target_arch = "aarch64")))]
#[allow(unused)]
mod fast_int_memory_access {
    #[inline(always)]
    pub fn u64_from_le_bytes(b: &[u8]) -> u64 {
        u64::from_le_bytes(b[..8].try_into().unwrap())
    }

    #[inline(always)]
    pub fn u32_from_le_bytes(b: &[u8]) -> u32 {
        u32::from_le_bytes(b[..4].try_into().unwrap())
    }

    #[inline(always)]
    pub fn u16_from_le_bytes(b: &[u8]) -> u16 {
        u16::from_le_bytes(b[..2].try_into().unwrap())
    }

    #[inline(always)]
    pub fn i64_from_le_bytes(b: &[u8]) -> i64 {
        i64::from_le_bytes(b[..8].try_into().unwrap())
    }

    #[inline(always)]
    pub fn i32_from_le_bytes(b: &[u8]) -> i32 {
        i32::from_le_bytes(b[..4].try_into().unwrap())
    }

    #[inline(always)]
    pub fn i16_from_le_bytes(b: &[u8]) -> i16 {
        i16::from_le_bytes(b[..2].try_into().unwrap())
    }

    #[inline(always)]
    pub fn u64_from_be_bytes(b: &[u8]) -> u64 {
        u64::from_be_bytes(b[..8].try_into().unwrap())
    }

    #[inline(always)]
    pub fn u32_from_be_bytes(b: &[u8]) -> u32 {
        u32::from_be_bytes(b[..4].try_into().unwrap())
    }

    #[inline(always)]
    pub fn u16_from_be_bytes(b: &[u8]) -> u16 {
        u16::from_be_bytes(b[..2].try_into().unwrap())
    }

    #[inline(always)]
    pub fn i64_from_be_bytes(b: &[u8]) -> i64 {
        i64::from_be_bytes(b[..8].try_into().unwrap())
    }

    #[inline(always)]
    pub fn i32_from_be_bytes(b: &[u8]) -> i32 {
        i32::from_be_bytes(b[..4].try_into().unwrap())
    }

    #[inline(always)]
    pub fn i16_from_be_bytes(b: &[u8]) -> i16 {
        i16::from_be_bytes(b[..2].try_into().unwrap())
    }
}

pub use fast_int_memory_access::*;
