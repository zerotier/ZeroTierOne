// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::mem::size_of;

// Version for architectures that definitely don't care about unaligned memory access.
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
    pub fn u128_from_ne_bytes(b: &[u8]) -> u128 {
        assert!(b.len() >= 16);
        unsafe { *b.as_ptr().cast() }
    }

    #[inline(always)]
    pub fn u64_from_ne_bytes(b: &[u8]) -> u64 {
        assert!(b.len() >= 8);
        unsafe { *b.as_ptr().cast() }
    }

    #[inline(always)]
    pub fn u32_from_ne_bytes(b: &[u8]) -> u32 {
        assert!(b.len() >= 4);
        unsafe { *b.as_ptr().cast() }
    }

    #[inline(always)]
    pub fn u16_from_ne_bytes(b: &[u8]) -> u16 {
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
}

// Version for architectures that might care about unaligned memory access.
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
    pub fn u128_from_ne_bytes(b: &[u8]) -> u64 {
        u128::from_ne_bytes(b[..16].try_into().unwrap())
    }

    #[inline(always)]
    pub fn u64_from_ne_bytes(b: &[u8]) -> u64 {
        u64::from_ne_bytes(b[..8].try_into().unwrap())
    }

    #[inline(always)]
    pub fn u32_from_ne_bytes(b: &[u8]) -> u32 {
        u32::from_ne_bytes(b[..4].try_into().unwrap())
    }

    #[inline(always)]
    pub fn u16_from_ne_bytes(b: &[u8]) -> u16 {
        u16::from_ne_bytes(b[..2].try_into().unwrap())
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
}

pub use fast_int_memory_access::*;

/// Obtain a view into an array cast as another array.
/// This will panic if the template parameters would result in out of bounds access.
#[inline(always)]
pub fn array_range<T: Copy, const S: usize, const START: usize, const LEN: usize>(a: &[T; S]) -> &[T; LEN] {
    assert!((START + LEN) <= S);
    unsafe { &*a.as_ptr().add(START).cast::<[T; LEN]>() }
}

/// Get a reference to a raw object as a byte array.
/// The template parameter S must equal the size of the object in bytes or this will panic.
#[inline(always)]
pub fn as_byte_array<T: Copy, const S: usize>(o: &T) -> &[u8; S] {
    assert_eq!(S, size_of::<T>());
    unsafe { &*(o as *const T).cast() }
}

/// Get a byte array as a flat object.
///
/// WARNING: while this is technically safe, care must be taken if the object requires aligned access.
pub fn as_flat_object<T: Copy, const S: usize>(b: &[u8; S]) -> &T {
    assert!(std::mem::size_of::<T>() <= S);
    unsafe { &*b.as_ptr().cast() }
}
