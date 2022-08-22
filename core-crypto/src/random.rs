// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::mem::MaybeUninit;
use std::sync::atomic::{AtomicU64, Ordering};

use openssl::rand::rand_bytes;

pub fn next_u32_secure() -> u32 {
    unsafe {
        let mut tmp: [u32; 1] = MaybeUninit::uninit().assume_init();
        assert!(rand_bytes(&mut *(tmp.as_mut_ptr().cast::<[u8; 4]>())).is_ok());
        tmp[0]
    }
}

pub fn next_u64_secure() -> u64 {
    unsafe {
        let mut tmp: [u64; 1] = MaybeUninit::uninit().assume_init();
        assert!(rand_bytes(&mut *(tmp.as_mut_ptr().cast::<[u8; 8]>())).is_ok());
        tmp[0]
    }
}

pub fn next_u128_secure() -> u128 {
    unsafe {
        let mut tmp: [u128; 1] = MaybeUninit::uninit().assume_init();
        assert!(rand_bytes(&mut *(tmp.as_mut_ptr().cast::<[u8; 16]>())).is_ok());
        tmp[0]
    }
}

#[inline(always)]
pub fn fill_bytes_secure(dest: &mut [u8]) {
    assert!(rand_bytes(dest).is_ok());
}

#[inline(always)]
pub fn get_bytes_secure<const COUNT: usize>() -> [u8; COUNT] {
    let mut tmp: [u8; COUNT] = unsafe { MaybeUninit::uninit().assume_init() };
    assert!(rand_bytes(&mut tmp).is_ok());
    tmp
}

pub struct SecureRandom;

impl Default for SecureRandom {
    #[inline(always)]
    fn default() -> Self {
        Self
    }
}

impl SecureRandom {
    #[inline(always)]
    pub fn get() -> Self {
        Self
    }
}

impl rand_core::RngCore for SecureRandom {
    #[inline(always)]
    fn next_u32(&mut self) -> u32 {
        next_u32_secure()
    }

    #[inline(always)]
    fn next_u64(&mut self) -> u64 {
        next_u64_secure()
    }

    #[inline(always)]
    fn fill_bytes(&mut self, dest: &mut [u8]) {
        fill_bytes_secure(dest);
    }

    #[inline(always)]
    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), rand_core::Error> {
        fill_bytes_secure(dest);
        Ok(())
    }
}

impl rand_core::CryptoRng for SecureRandom {}

impl rand_core_062::RngCore for SecureRandom {
    #[inline(always)]
    fn next_u32(&mut self) -> u32 {
        next_u32_secure()
    }

    #[inline(always)]
    fn next_u64(&mut self) -> u64 {
        next_u64_secure()
    }

    #[inline(always)]
    fn fill_bytes(&mut self, dest: &mut [u8]) {
        fill_bytes_secure(dest);
    }

    #[inline(always)]
    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), rand_core_062::Error> {
        fill_bytes_secure(dest);
        Ok(())
    }
}

impl rand_core_062::CryptoRng for SecureRandom {}

unsafe impl Sync for SecureRandom {}
unsafe impl Send for SecureRandom {}

/// Get a non-cryptographic random number.
pub fn xorshift64_random() -> u64 {
    static mut XORSHIFT64_STATE: AtomicU64 = AtomicU64::new(0);
    let mut x = unsafe { XORSHIFT64_STATE.load(Ordering::Relaxed) };
    while x == 0 {
        x = next_u64_secure();
    }
    x ^= x.wrapping_shl(13);
    x ^= x.wrapping_shr(7);
    x ^= x.wrapping_shl(17);
    unsafe { XORSHIFT64_STATE.store(x, Ordering::Relaxed) };
    x
}
