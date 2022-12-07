// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::io::{Read, Write};

pub const VARINT_MAX_SIZE_BYTES: usize = 10;

/// Encode an integer as a varint.
///
/// WARNING: if the supplied byte slice does not have at least 10 bytes available this may panic.
/// This is checked in debug mode by an assertion.
pub fn encode(b: &mut [u8], mut v: u64) -> usize {
    debug_assert!(b.len() >= VARINT_MAX_SIZE_BYTES);
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

/// Write a variable length integer, which can consume up to 10 bytes.
#[inline(always)]
pub fn write<W: Write>(w: &mut W, v: u64) -> std::io::Result<()> {
    let mut b = [0_u8; VARINT_MAX_SIZE_BYTES];
    let i = encode(&mut b, v);
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

/// A container for an encoded varint. Use as_ref() to get bytes.
pub struct Encoded([u8; VARINT_MAX_SIZE_BYTES], u8);

impl Encoded {
    #[inline(always)]
    pub fn from(v: u64) -> Encoded {
        let mut e = Encoded([0_u8; VARINT_MAX_SIZE_BYTES], 0);
        e.1 = encode(&mut e.0, v) as u8;
        e
    }
}

impl AsRef<[u8]> for Encoded {
    #[inline(always)]
    fn as_ref(&self) -> &[u8] {
        &self.0[0..(self.1 as usize)]
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
