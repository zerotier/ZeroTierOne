/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use gcrypt::rand::{Level, randomize};

/// Secure random source based on the desired third party library (gcrypt).
pub struct SecureRandom;

impl SecureRandom {
    #[inline(always)]
    pub fn get() -> Self { Self }
}

impl rand_core::RngCore for SecureRandom {
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
    fn fill_bytes(&mut self, dest: &mut [u8]) { randomize(Level::Strong, dest); }

    #[inline(always)]
    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), rand_core::Error> {
        randomize(Level::Strong, dest);
        Ok(())
    }
}

impl rand_core::CryptoRng for SecureRandom {}

#[inline(always)]
pub fn next_u32_secure() -> u32 {
    let mut tmp = 0_u32;
    randomize(Level::Strong, unsafe { &mut *(&mut tmp as *mut u32).cast::<[u8; 4]>() });
    tmp
}

#[inline(always)]
pub fn next_u64_secure() -> u64 {
    let mut tmp = 0_u64;
    randomize(Level::Strong, unsafe { &mut *(&mut tmp as *mut u64).cast::<[u8; 8]>() });
    tmp
}

#[inline(always)]
pub fn fill_bytes_secure(dest: &mut [u8]) { randomize(Level::Strong, dest); }
