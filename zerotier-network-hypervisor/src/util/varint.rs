/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::io::{Read, Write};

pub fn write<W: Write>(w: &mut W, mut v: u64) -> std::io::Result<()> {
    let mut b = [0_u8; 10];
    let mut i = 10;
    loop {
        if v > 0x7f {
            i -= 1;
            b[i] = (v as u8) & 0x7f;
            v >>= 7;
        } else {
            i -= 1;
            b[i] = (v as u8) | 0x80;
            break;
        }
    }
    w.write_all(&b[i..])
}

pub fn read<R: Read>(r: &mut R) -> std::io::Result<u64> {
    let mut v = 0_u64;
    let mut buf = [0_u8; 1];
    loop {
        v <<= 7;
        let _ = r.read_exact(&mut buf)?;
        let b = buf[0];
        if b <= 0x7f {
            v |= b as u64;
        } else {
            v |= (b & 0x7f) as u64;
            return Ok(v);
        }
    }
}

pub(crate) fn read_from_bytes(r: &[u8], cursor: &mut usize) -> std::io::Result<u64> {
    let mut v = 0_u64;
    let mut c = *cursor;
    while c < r.len() {
        v <<= 7;
        let b = unsafe { *r.get_unchecked(c) };
        c += 1;
        if b <= 0x7f {
            v |= b as u64;
        } else {
            v |= (b & 0x7f) as u64;
            *cursor = c;
            return Ok(v);
        }
    }
    *cursor = c;
    return Err(std::io::Error::new(std::io::ErrorKind::UnexpectedEof, "incomplete varint"));
}
