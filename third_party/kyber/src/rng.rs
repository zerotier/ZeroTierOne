#![cfg(not(feature = "KATs"))]
use rand_core::*;

// Fills buffer x with len bytes, RNG must satisfy the
// RngCore trait and CryptoRng marker trait requirements
pub fn randombytes<R>(x: &mut [u8], len: usize, rng: &mut R)
where
    R: RngCore + CryptoRng,
{
    rng.fill_bytes(&mut x[..len]);
}
