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
pub mod iblt;

pub use zerotier_core_crypto::hex;
pub use zerotier_core_crypto::varint;

pub(crate) const ZEROES: [u8; 64] = [0_u8; 64];

/// Obtain a reference to a sub-array within an existing array.
#[inline(always)]
pub(crate) fn array_range<T, const A: usize, const START: usize, const LEN: usize>(a: &[T; A]) -> &[T; LEN] {
    assert!((START + LEN) <= A);
    unsafe { &*a.as_ptr().add(START).cast::<[T; LEN]>() }
}

/// Obtain a reference to a sub-array within an existing array.
#[inline(always)]
pub(crate) fn array_range_mut<T, const A: usize, const START: usize, const LEN: usize>(a: &mut [T; A]) -> &mut [T; LEN] {
    assert!((START + LEN) <= A);
    unsafe { &mut *a.as_mut_ptr().add(START).cast::<[T; LEN]>() }
}

/// Cast a u64 to a byte array.
#[inline(always)]
pub(crate) fn u64_as_bytes(i: &u64) -> &[u8; 8] { unsafe { &*(i as *const u64).cast() } }

lazy_static! {
    static ref HIGHWAYHASHER_KEY: [u64; 4] = [zerotier_core_crypto::random::next_u64_secure(), zerotier_core_crypto::random::next_u64_secure(), zerotier_core_crypto::random::next_u64_secure(), zerotier_core_crypto::random::next_u64_secure()];
}

/// Get an instance of HighwayHasher initialized with a secret per-process random salt.
/// The random salt is generated at process start and so will differ for each invocation of whatever process this is inside.
#[inline(always)]
pub(crate) fn highwayhasher() -> highway::HighwayHasher { highway::HighwayHasher::new(highway::Key(HIGHWAYHASHER_KEY.clone())) }

#[inline(always)]
pub(crate) fn u128_from_2xu64_ne(x: [u64; 2]) -> u128 { unsafe { std::mem::transmute(x) } }

/// Non-cryptographic 64-bit bit mixer for things like local hashing.
#[inline(always)]
pub(crate) fn hash64_noncrypt(mut x: u64) -> u64 {
    x ^= x.wrapping_shr(30);
    x = x.wrapping_mul(0xbf58476d1ce4e5b9);
    x ^= x.wrapping_shr(27);
    x = x.wrapping_mul(0x94d049bb133111eb);
    x ^ x.wrapping_shr(31)
}

/// A hasher for maps that just returns u64 values as-is.
/// Used with things like ZeroTier addresses and network IDs that are already randomly distributed.
#[derive(Copy, Clone)]
pub(crate) struct U64NoOpHasher(u64);

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
