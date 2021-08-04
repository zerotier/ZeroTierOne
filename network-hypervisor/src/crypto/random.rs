use rand_core::{RngCore, Error};
use rand_core::CryptoRng;
use gcrypt::rand::{Level, randomize};

pub struct SecureRandom;

impl SecureRandom {
    #[inline(always)]
    pub fn get() -> Self {
        Self
    }
}

impl RngCore for SecureRandom {
    #[inline(always)]
    fn next_u32(&mut self) -> u32 {
        let mut tmp = 0_u32;
        randomize(Level::Strong, unsafe { &mut *(&mut tmp as *mut u32).cast::<[u8; 4]>() });
        tmp
    }

    #[inline(always)]
    fn next_u64(&mut self) -> u64 {
        let mut tmp = 0_u64;
        randomize(Level::Strong, unsafe { &mut *(&mut tmp as *mut u64).cast::<[u8; 8]>() });
        tmp
    }

    #[inline(always)]
    fn fill_bytes(&mut self, dest: &mut [u8]) {
        randomize(Level::Strong, dest);
    }

    #[inline(always)]
    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        randomize(Level::Strong, dest);
        Ok(())
    }
}

impl CryptoRng for SecureRandom {}

#[inline(always)]
pub(crate) fn next_u32_secure() -> u32 {
    let mut tmp = 0_u32;
    randomize(Level::Strong, unsafe { &mut *(&mut tmp as *mut u32).cast::<[u8; 4]>() });
    tmp
}

#[inline(always)]
pub(crate) fn next_u64_secure() -> u64 {
    let mut tmp = 0_u64;
    randomize(Level::Strong, unsafe { &mut *(&mut tmp as *mut u64).cast::<[u8; 8]>() });
    tmp
}

#[inline(always)]
pub(crate) fn fill_bytes_secure(dest: &mut [u8]) {
    randomize(Level::Strong, dest);
}
