/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::io::{Read, Write};

/// Write a variable length integer, which can consume up to 10 bytes.
pub fn write<W: Write>(w: &mut W, mut v: u64) -> std::io::Result<()> {
    let mut b = [0_u8; 10];
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
    w.write_all(&b[0..i])
}

/// Read a variable length integer, returning the value and the number of bytes written.
pub fn read<R: Read>(r: &mut R) -> std::io::Result<(u64, usize)> {
    let mut v = 0_u64;
    let mut buf = [0_u8; 1];
    let mut pos = 0;
    let mut i = 0_usize;
    loop {
        let _ = r.read_exact(&mut buf)?;
        let b = buf[0];
        i += 1;
        if b <= 0x7f {
            v |= (b as u64).wrapping_shl(pos);
            pos += 7;
        } else {
            v |= ((b & 0x7f) as u64).wrapping_shl(pos);
            return Ok((v, i));
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::varint::*;

    #[test]
    fn varint() {
        let mut t: Vec<u8> = Vec::new();
        for i in 0..131072 {
            t.clear();
            let ii = (u64::MAX / 131072) * i;
            assert!(write(&mut t, ii).is_ok());
            let mut t2 = t.as_slice();
            assert_eq!(read(&mut t2).unwrap().0, ii);
        }
    }
}
