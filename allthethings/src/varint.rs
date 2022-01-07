/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use smol::io::{AsyncReadExt, AsyncWrite, AsyncRead, AsyncWriteExt};

pub use zerotier_core_crypto::varint::*;

pub async fn async_write<W: AsyncWrite + Unpin>(w: &mut W, mut v: u64) -> smol::io::Result<()> {
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
    w.write_all(&b[0..i]).await
}

pub async fn async_read<R: AsyncRead + Unpin>(r: &mut R) -> smol::io::Result<u64> {
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
