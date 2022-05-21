/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

pub mod buffer;
pub(crate) mod gate;
pub mod marshalable;
pub(crate) mod pool;

pub use zerotier_core_crypto::hex;
pub use zerotier_core_crypto::varint;

pub(crate) const ZEROES: [u8; 64] = [0_u8; 64];

/// Obtain a reference to a sub-array within an existing byte array.
#[inline(always)]
pub(crate) fn byte_array_range<const A: usize, const START: usize, const LEN: usize>(a: &[u8; A]) -> &[u8; LEN] {
    assert!((START + LEN) <= A);
    unsafe { &*a.as_ptr().add(START).cast::<[u8; LEN]>() }
}

/// A super-minimal hasher for u64 keys for keys already fairly randomly distributed like addresses and network IDs.
#[derive(Copy, Clone)]
pub(crate) struct U64NoOpHasher(u64);

impl U64NoOpHasher {
    #[inline(always)]
    pub fn new() -> Self {
        Self(0)
    }
}

impl std::hash::Hasher for U64NoOpHasher {
    #[inline(always)]
    fn finish(&self) -> u64 {
        self.0.wrapping_add(self.0.wrapping_shr(32))
    }

    #[inline(always)]
    fn write_u64(&mut self, i: u64) {
        self.0 = self.0.wrapping_add(i);
    }

    #[inline(always)]
    fn write_i64(&mut self, i: i64) {
        self.0 = self.0.wrapping_add(i as u64);
    }

    #[inline(always)]
    fn write_usize(&mut self, i: usize) {
        self.0 = self.0.wrapping_add(i as u64);
    }

    #[inline(always)]
    fn write_isize(&mut self, i: isize) {
        self.0 = self.0.wrapping_add(i as u64);
    }

    #[inline(always)]
    fn write_u32(&mut self, i: u32) {
        self.0 = self.0.wrapping_add(i as u64);
    }

    #[inline(always)]
    fn write_i32(&mut self, i: i32) {
        self.0 = self.0.wrapping_add(i as u64);
    }

    #[inline(always)]
    fn write(&mut self, _: &[u8]) {
        panic!("U64NoOpHasher should only be used with u64 and i64 types");
    }
}

impl std::hash::BuildHasher for U64NoOpHasher {
    type Hasher = Self;

    #[inline(always)]
    fn build_hasher(&self) -> Self::Hasher {
        Self(0)
    }
}
