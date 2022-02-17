/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::mem::MaybeUninit;
use openssl::rand::rand_bytes;

pub struct SecureRandom;

#[inline(always)]
pub fn next_u32_secure() -> u32 {
    unsafe {
        let mut tmp: [u32; 1] = MaybeUninit::uninit().assume_init();
        assert!(rand_bytes(&mut *(tmp.as_mut_ptr().cast::<[u8; 4]>())).is_ok());
        tmp[0]
    }
}

#[inline(always)]
pub fn next_u64_secure() -> u64 {
    unsafe {
        let mut tmp: [u64; 1] = MaybeUninit::uninit().assume_init();
        assert!(rand_bytes(&mut *(tmp.as_mut_ptr().cast::<[u8; 8]>())).is_ok());
        tmp[0]
    }
}

#[inline(always)]
pub fn fill_bytes_secure(dest: &mut [u8]) {
    assert!(rand_bytes(dest).is_ok());
}

impl SecureRandom {
    #[inline(always)]
    pub fn get() -> Self { Self }
}

impl rand_core::RngCore for SecureRandom {
    #[inline(always)]
    fn next_u32(&mut self) -> u32 { next_u32_secure() }

    #[inline(always)]
    fn next_u64(&mut self) -> u64 { next_u64_secure() }

    #[inline(always)]
    fn fill_bytes(&mut self, dest: &mut [u8]) { fill_bytes_secure(dest); }

    #[inline(always)]
    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), rand_core::Error> {
        rand_bytes(dest).map_err(|e| rand_core::Error::new(Box::new(e)))
    }
}

impl rand_core::CryptoRng for SecureRandom {}

unsafe impl Send for SecureRandom {}

static mut XORSHIFT64_STATE: u64 = 0;

/// Get a non-cryptographic random number.
pub fn xorshift64_random() -> u64 {
    let mut x = unsafe { XORSHIFT64_STATE };
    while x == 0 {
        x = next_u64_secure();
    }
    x ^= x.wrapping_shl(13);
    x ^= x.wrapping_shr(7);
    x ^= x.wrapping_shl(17);
    unsafe { XORSHIFT64_STATE = x };
    x
}
