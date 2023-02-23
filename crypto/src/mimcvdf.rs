/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

/*
 * https://eprint.iacr.org/2016/492.pdf
 * https://vitalik.ca/general/2018/07/21/starks_part_3.html
 */

// 2^127 - 39
const PRIME: u128 = 170141183460469231731687303715884105689;
// 2p-1/3
const PRIME_2P_MINUS_1_DIV_3: u128 = 113427455640312821154458202477256070459;

const K_COUNT_MASK: usize = 63;
const K: [u64; 64] = [
    0x921cdfd99022340f,
    0xe7c65f78c70afaa8,
    0x72793744494c4fda,
    0x67759e2688bc9c0a,
    0x7681a224661f0ac0,
    0xa7b81b099925a2bf,
    0x16d43792e66b030a,
    0x841bd90742d26ee9,
    0xb1346ec08db97053,
    0xd044229c1173d972,
    0xf4813498dfdead0e,
    0xe46dca4c237d2c28,
    0xac64872778089599,
    0x67be75af74416e74,
    0xb9dec3aefd3ae012,
    0xf0497147953c4276,
    0xf6ac07fd3944177d,
    0xccf1c28813eb589b,
    0x49abb5e2b0bff5bd,
    0xd5c15eeb39587d69,
    0x9c6ff50ee6898649,
    0x763f3b25524a0fbf,
    0xa6029c37f715c02c,
    0xe458a5902b2b5629,
    0x8e4d6be6a1ba32c5,
    0x052aba0b61738f20,
    0xc18a6901fa026b12,
    0x137df11cf1dbe811,
    0x5da0310e419be602,
    0xc66ddec578f52891,
    0xe4eae4efc0f0d54f,
    0xf9d488269f118012,
    0xcf9b5108f66e77d1,
    0x443ba29939f5a657,
    0xa4e4b7d28c51e5c2,
    0xe030d1772f112c01,
    0xe136f0cf8da5e172,
    0x3e9ee638f9663dc2,
    0xbc5c1db73e639dfd,
    0xa9fbbaa873fedf73,
    0xffb2a5247d10ab8f,
    0x06e6f3b5ae4b67ac,
    0x475e7d427d331282,
    0xcac6237c40a9d653,
    0xe9a15c1d177beefa,
    0xa14ef2111c2175a3,
    0x8427d4b68982fc21,
    0x12171e2a55d43343,
    0x37715fdea87a0a60,
    0x24bc5d28cff8ecad,
    0x92276e4118304e62,
    0x824b66792f58dd45,
    0xe43973cf253b6947,
    0xd0db2c5a2a4f064d,
    0x734cdb241520ad04,
    0xcec4f2ce5013069e,
    0x2741c83c07bbf9e0,
    0x284be707dcbda1a4,
    0xd602f3d8545799b2,
    0xea3977f56573b4d2,
    0x0723fda64d57d0c6,
    0x04dc344d0dde863a,
    0x7584143462914be4,
    0x111307f7823dfcc6,
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

pub fn delay(mut input: u128, rounds: usize) -> u128 {
    debug_assert!(rounds > 0);
    input %= PRIME;
    for r in 1..(rounds + 1) {
        input = powmod::<PRIME>(input ^ (K[(rounds - r) & K_COUNT_MASK] as u128), PRIME_2P_MINUS_1_DIV_3);
    }
    input
}

pub fn verify(mut proof: u128, expected: u128, rounds: usize) -> bool {
    debug_assert!(rounds > 0);
    for r in 0..rounds {
        proof = mulmod::<PRIME>(proof, mulmod::<PRIME>(proof, proof)) ^ (K[r & K_COUNT_MASK] as u128);
    }
    proof == (expected % PRIME)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn delay_and_verify() {
        for i in 1..5 {
            let input = (crate::random::xorshift64_random() as u128).wrapping_mul(crate::random::xorshift64_random() as u128);
            let proof = delay(input, i * 3);
            assert!(verify(proof, input, i * 3));
        }
    }
}
