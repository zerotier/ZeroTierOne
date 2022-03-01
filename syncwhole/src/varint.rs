/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2022 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::io::{Read, Write};
use tokio::io::{AsyncRead, AsyncReadExt, AsyncWrite, AsyncWriteExt};

const VARINT_MAX_SIZE_BYTES: usize = 10;

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

#[inline(always)]
pub async fn write_async<W: AsyncWrite + Unpin>(w: &mut W, v: u64) -> std::io::Result<()> {
    let mut b = [0_u8; VARINT_MAX_SIZE_BYTES];
    let i = encode(&mut b, v);
    w.write_all(&b[0..i]).await
}

pub async fn read_async<R: AsyncRead + Unpin>(r: &mut R) -> std::io::Result<u64> {
    let mut v = 0_u64;
    let mut buf = [0_u8; 1];
    let mut pos = 0;
    loop {
        let _ = r.read_exact(&mut buf).await?;
        let b = buf[0];
        if b <= 0x7f {
            v |= (b as u64).wrapping_shl(pos);
            pos += 7;
        } else {
            v |= ((b & 0x7f) as u64).wrapping_shl(pos);
            return Ok(v);
        }
    }
}

#[inline(always)]
pub fn write<W: Write>(w: &mut W, v: u64) -> std::io::Result<()> {
    let mut b = [0_u8; VARINT_MAX_SIZE_BYTES];
    let i = encode(&mut b, v);
    w.write_all(&b[0..i])
}

pub fn read<R: Read>(r: &mut R) -> std::io::Result<u64> {
    let mut v = 0_u64;
    let mut buf = [0_u8; 1];
    let mut pos = 0;
    loop {
        let _ = r.read_exact(&mut buf)?;
        let b = buf[0];
        if b <= 0x7f {
            v |= (b as u64).wrapping_shl(pos);
            pos += 7;
        } else {
            v |= ((b & 0x7f) as u64).wrapping_shl(pos);
            return Ok(v);
        }
    }
}
