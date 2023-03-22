use std::sync::{Mutex};

use libc::c_int;

use crate::error::{cvt, ErrorStack};

/// Fill buffer with cryptographically strong pseudo-random bytes.
fn rand_bytes(buf: &mut [u8]) -> Result<(), ErrorStack> {
    unsafe {
        assert!(buf.len() <= c_int::max_value() as usize);
        cvt(ffi::RAND_bytes(buf.as_mut_ptr(), buf.len() as c_int)).map(|_| ())
    }
}

pub fn next_u32_secure() -> u32 {
    unsafe {
        let mut tmp = [0u32; 1];
        rand_bytes(&mut *(tmp.as_mut_ptr().cast::<[u8; 4]>())).unwrap();
        tmp[0]
    }
}

pub fn next_u64_secure() -> u64 {
    unsafe {
        let mut tmp = [0u64; 1];
        rand_bytes(&mut *(tmp.as_mut_ptr().cast::<[u8; 8]>())).unwrap();
        tmp[0]
    }
}

pub fn next_u128_secure() -> u128 {
    unsafe {
        let mut tmp = [0u128; 1];
        rand_bytes(&mut *(tmp.as_mut_ptr().cast::<[u8; 16]>())).unwrap();
        tmp[0]
    }
}

#[inline(always)]
pub fn fill_bytes_secure(dest: &mut [u8]) {
    rand_bytes(dest).unwrap();
}

#[inline(always)]
pub fn get_bytes_secure<const COUNT: usize>() -> [u8; COUNT] {
    let mut tmp = [0u8; COUNT];
    rand_bytes(&mut tmp).unwrap();
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

/// ed25519-dalek still uses rand_core 0.5.1, and that version is incompatible with 0.6.4, so we need to import and implement both.
impl rand_core_051::RngCore for SecureRandom {
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
    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), rand_core_051::Error> {
        fill_bytes_secure(dest);
        Ok(())
    }
}

impl rand_core::CryptoRng for SecureRandom {}
impl rand_core_051::CryptoRng for SecureRandom {}

unsafe impl Sync for SecureRandom {}
unsafe impl Send for SecureRandom {}

/// Get a non-cryptographic random number.
pub fn xorshift64_random() -> u64 {
    static mut XORSHIFT64_STATE: Mutex<u64> = Mutex::new(0);
    let mut state = unsafe { XORSHIFT64_STATE.lock().unwrap() };
    let mut x = *state;
    while x == 0 {
        x = next_u64_secure();
    }
    x ^= x.wrapping_shl(13);
    x ^= x.wrapping_shr(7);
    x ^= x.wrapping_shl(17);
    *state = x;
    x
}
