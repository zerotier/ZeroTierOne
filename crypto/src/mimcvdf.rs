/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

/*
 * MIMC is a hash function originally designed for use with STARK and SNARK proofs. It's based
 * on modular multiplication and exponentiation instead of the usual bit twiddling or ARX
 * operations that underpin more common hash algorithms.
 *
 * It's useful as a verifiable delay function because it can be computed in both directions with
 * one direction taking orders of magnitude longer than the other. The "backward" direction is
 * used as the delay function as it requires modular exponentiation which is inherently more
 * compute intensive. The "forward" direction simply requires modular cubing which is two modular
 * multiplications and is much faster.
 *
 * It's also nice because it's incredibly simple with a tiny code footprint.
 *
 * This is used for anti-DOS and anti-spamming delay functions. It's not used for anything
 * really "cryptographically hard," and if it were broken cryptographically it would still be
 * useful as a VDF as long as the break didn't yield a significantly faster way of computing a
 * delay proof than the straightforward iterative way implemented here.
 *
 * Here are two references on MIMC with the first being the original paper and the second being
 * a blog post describing its use as a VDF.
 *
 * https://eprint.iacr.org/2016/492.pdf
 * https://vitalik.ca/general/2018/07/21/starks_part_3.html
 */

// p = 2^127 - 39, the largest 127-bit prime of the form 6k + 5
const PRIME: u128 = 170141183460469231731687303715884105689;

// (2p - 1) / 3
const PRIME_2P_MINUS_1_DIV_3: u128 = 113427455640312821154458202477256070459;

// Randomly generated round constants, each modulo PRIME.
const K_COUNT_MASK: usize = 31;
const K: [u128; 32] = [
    0x1fdd07a761b611bb1ab9419a70599a7c,
    0x23056b05d5c6b925e333d7418047650a,
    0x77a638f9b437a307f8866fbd2672c705,
    0x60213dab83bab91d1c310bd87e9da332,
    0xf56bc883301ab373179e46b098b7a7,
    0x7914a0dbd2f971344173b350c28a838,
    0x44bb64af5e446e6ebdc068d10d318f26,
    0x1bca1921fd328bb725ae0cbcbc20a263,
    0xafa963242f5216a7da1cd5328b23659,
    0x7fe17c43782b883a63ee0a790e0b2b77,
    0x23bb62abf728bf453200ee528f902c33,
    0x75ec0c055be14955db6878567e3c0465,
    0x7902bb57876e0b08b4de02a66755e5d7,
    0xe5d7094f37b615f5a1e1594b0390de8,
    0x12d4ddee90653a26f5de63ff4651f2d,
    0xce4a15bc35633b5ed8bcae2c93d739c,
    0x23f25b935e52df87255db8c608ef9ab4,
    0x611a08d7464fb984c98104d77f1609a7,
    0x7aa825876a7f6acde5efa57992da9c43,
    0x2be9686f630fa28a0a0e1081a59755b4,
    0x50060dac9ac4656ba3f8ee7592f4e28a,
    0x4113abff6f5bb303eac2ca809d4d529d,
    0x2af9d01d4e753feb5834c14ca0543397,
    0x73c2d764691ced2b823dda887e22ae85,
    0x5b53dcd4750ff888dca2497cec4dacb7,
    0x5d8984a52c2d8f3cc9bcf61ef29f8a1,
    0x588d8cc99533d649aabb5f0f552140e,
    0x4dae04985fde8c8464ba08aaa7d8761e,
    0x53f0c4740b8c3bda3fc05109b9a2b71,
    0x3e918c88a6795e3bf840e0b74d91b9d7,
    0x1dbcb30d724f11200aebb1dff87def91,
    0x6086b0af0e1e68558170239d23be9780,
];

fn mulmod<const M: u128>(mut a: u128, mut b: u128) -> u128 {
    let mut res: u128 = 0;
    a %= M;
    loop {
        if (b & 1) != 0 {
            res = res.wrapping_add(a) % M;
        }
        b = b.wrapping_shr(1);
        if b != 0 {
            a = a.wrapping_shl(1) % M;
        } else {
            return res;
        }
    }
}

#[inline(always)]
fn powmod<const M: u128>(mut base: u128, mut exp: u128) -> u128 {
    let mut res: u128 = 1;
    loop {
        if (exp & 1) != 0 {
            res = mulmod::<M>(base, res);
        }
        exp = exp.wrapping_shr(1);
        if exp != 0 {
            base = mulmod::<M>(base, base);
        } else {
            return res;
        }
    }
}

/// Compute MIMC for the given number of iterations and return a proof that can be checked much more quickly.
pub fn delay(mut input: u128, rounds: usize) -> u128 {
    debug_assert!(rounds > 0);
    input %= PRIME;
    for r in 1..(rounds + 1) {
        input = powmod::<PRIME>(input ^ K[(rounds - r) & K_COUNT_MASK], PRIME_2P_MINUS_1_DIV_3);
    }
    input
}

/// Quickly verify the result of delay() given the returned proof, original input, and original number of rounds.
pub fn verify(mut proof: u128, original_input: u128, rounds: usize) -> bool {
    debug_assert!(rounds > 0);
    for r in 0..rounds {
        proof = mulmod::<PRIME>(proof, mulmod::<PRIME>(proof, proof)) ^ K[r & K_COUNT_MASK];
    }
    proof == (original_input % PRIME)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn delay_and_verify() {
        for i in 1..5 {
            let input = (crate::random::xorshift64_random() as u128).wrapping_mul(crate::random::xorshift64_random() as u128);
            let proof = delay(input, i * 3);
            //println!("{}", proof);
            assert!(verify(proof, input, i * 3));
        }
    }
}
