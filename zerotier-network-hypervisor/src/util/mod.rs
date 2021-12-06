/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

pub mod pool;
pub mod gate;
pub mod buffer;

pub use zerotier_core_crypto::hex;
pub use zerotier_core_crypto::varint;

pub(crate) const ZEROES: [u8; 64] = [0_u8; 64];

/// Obtain a reference to a sub-array within an existing array.
/// Attempts to violate array bounds will panic or fail to compile.
#[inline(always)]
pub(crate) fn array_range<T, const A: usize, const START: usize, const LEN: usize>(a: &[T; A]) -> &[T; LEN] {
    assert!((START + LEN) <= A);
    unsafe { &*a.as_ptr().add(std::mem::size_of::<T>() * start_index).cast::<[T; LEN]>() }
}

/// Cast a u64 reference to a byte array in place.
/// Going the other direction is not safe on some architectures, but this should be safe everywhere.
#[inline(always)]
pub(crate) fn u64_as_bytes(i: &u64) -> &[u8; 8] { unsafe { &*(i as *const u64).cast() } }

/// A hasher for maps that just returns u64 values as-is.
///
/// This should be used only for things like ZeroTier addresses that are already random
/// and that aren't vulnerable to malicious crafting of identifiers.
#[derive(Copy, Clone)]
pub struct U64NoOpHasher(u64);

impl U64NoOpHasher {
    #[inline(always)]
    pub fn new() -> Self { Self(0) }
}

impl std::hash::Hasher for U64NoOpHasher {
    #[inline(always)]
    fn finish(&self) -> u64 { self.0 }

    #[inline(always)]
    fn write(&mut self, _: &[u8]) {
        panic!("U64NoOpHasher should only be used with u64 and i64 types");
    }

    #[inline(always)]
    fn write_u64(&mut self, i: u64) { self.0 += i; }

    #[inline(always)]
    fn write_i64(&mut self, i: i64) { self.0 += i as u64; }
}

impl std::hash::BuildHasher for U64NoOpHasher {
    type Hasher = Self;

    #[inline(always)]
    fn build_hasher(&self) -> Self::Hasher { Self(0) }
}
