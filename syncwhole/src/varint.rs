/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2022 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

pub const VARINT_MAX_SIZE_BYTES: usize = 10;

pub fn encode(b: &mut [u8], mut v: u64) -> usize {
    let mut i = 0;
    loop {
        if v > 0x7f {
            b[i] = (v as u8) & 0x7f;
            i += 1;
            v = v.wrapping_shr(7);
        } else {
            b[i] = (v as u8) | 0x80;
            i += 1;
            break;
        }
    }
    i
}

pub fn decode(b: &[u8]) -> (u64, usize) {
    let mut v = 0_u64;
    let mut pos = 0;
    let mut l = 0;
    let bl = b.len();
    while l < bl {
        let x = b[l];
        l += 1;
        if x <= 0x7f {
            v |= (x as u64).wrapping_shl(pos);
            pos += 7;
        } else {
            v |= ((x & 0x7f) as u64).wrapping_shl(pos);
            return (v, l);
        }
    }
    return (0, 0);
}
