use std::io::Write;

use super::arrayvec::ArrayVec;
use super::memory;

const MAX_LENGTH_WORDS: usize = 128;

/// Encode a byte array into a base62 string.
///
/// The pad_output_to_length parameter outputs base62 zeroes at the end to ensure that the output
/// string is at least a given length. Set this to zero if you don't want to pad the output. This
/// has no effect on decoded output length.
pub fn encode_into(b: &[u8], s: &mut String, pad_output_to_length: usize) {
    assert!(b.len() <= MAX_LENGTH_WORDS * 4);
    let mut n: ArrayVec<u32, MAX_LENGTH_WORDS> = ArrayVec::new();

    let mut i = 0;
    let len_words = b.len() & usize::MAX.wrapping_shl(2);
    while i < len_words {
        n.push(u32::from_le(memory::load_raw(&b[i..])));
        i += 4;
    }
    if i < b.len() {
        let mut w = 0u32;
        let mut shift = 0u32;
        while i < b.len() {
            w |= (b[i] as u32).wrapping_shl(shift);
            i += 1;
            shift += 8;
        }
        n.push(w);
    }

    let mut string_len = 0;
    while !n.is_empty() {
        s.push(b"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"[big_div_rem::<MAX_LENGTH_WORDS, 62>(&mut n) as usize] as char);
        string_len += 1;
    }
    while string_len < pad_output_to_length {
        s.push('0');
        string_len += 1;
    }
}

/// Decode Base62 into a vector or other output.
///
/// Note that base62 doesn't have a way to know the output length. Decoding may be short if there were
/// trailing zeroes in the input. The output length parameter specifies the expected length of the
/// output, which will be zero padded if decoded data does not reach it. If decoded data exceeds this
/// length an error is returned.
pub fn decode_into<W: Write>(s: &[u8], b: &mut W, output_length: usize) -> std::io::Result<()> {
    let mut n: ArrayVec<u32, MAX_LENGTH_WORDS> = ArrayVec::new();

    for c in s.iter().rev() {
        let mut c = *c as u32;
        // 0..9, A..Z, or a..z
        if c >= 48 && c <= 57 {
            c -= 48;
        } else if c >= 65 && c <= 90 {
            c -= 65 - 10;
        } else if c >= 97 && c <= 122 {
            c -= 97 - (10 + 26);
        } else {
            return Err(std::io::Error::new(std::io::ErrorKind::InvalidInput, "invalid base62"));
        }
        big_mul::<MAX_LENGTH_WORDS, 62>(&mut n);
        big_add(&mut n, c);
    }

    let mut bc = output_length;
    for w in n.iter() {
        if bc > 0 {
            let l = bc.min(4);
            b.write_all(&w.to_le_bytes()[..l])?;
            bc -= l;
        } else {
            return Err(std::io::Error::new(std::io::ErrorKind::Other, "data too large"));
        }
    }
    while bc > 0 {
        b.write_all(&[0])?;
        bc -= 1;
    }

    return Ok(());
}

#[inline]
pub fn decode_into_slice(s: &[u8], mut b: &mut [u8]) -> std::io::Result<()> {
    let l = b.len();
    decode_into(s, &mut b, l)
}

/// Decode into and return an array whose length is the desired output_length.
/// None is returned if there is an error.
#[inline]
pub fn decode<const L: usize>(s: &[u8]) -> Option<[u8; L]> {
    let mut buf = [0u8; L];
    let mut w = &mut buf[..];
    if decode_into(s, &mut w, L).is_ok() {
        Some(buf)
    } else {
        None
    }
}

#[inline(always)]
fn big_div_rem<const C: usize, const D: u64>(n: &mut ArrayVec<u32, C>) -> u32 {
    while let Some(&0) = n.last() {
        n.pop();
    }
    let mut rem = 0;
    for word in n.iter_mut().rev() {
        let temp = (rem as u64).wrapping_shl(32) | (*word as u64);
        let (a, b) = (temp / D, temp % D);
        *word = a as u32;
        rem = b as u32;
    }
    while let Some(&0) = n.last() {
        n.pop();
    }
    rem
}

#[inline(always)]
fn big_add<const C: usize>(n: &mut ArrayVec<u32, C>, i: u32) {
    let mut carry = i as u64;
    for word in n.iter_mut() {
        let res = (*word as u64).wrapping_add(carry);
        *word = res as u32;
        carry = res.wrapping_shr(32);
    }
    if carry > 0 {
        n.push(carry as u32);
    }
}

#[inline(always)]
fn big_mul<const C: usize, const M: u64>(n: &mut ArrayVec<u32, C>) {
    while let Some(&0) = n.last() {
        n.pop();
    }
    let mut carry = 0;
    for word in n.iter_mut() {
        let temp = (*word as u64).wrapping_mul(M).wrapping_add(carry);
        *word = temp as u32;
        carry = temp.wrapping_shr(32);
    }
    if carry != 0 {
        n.push(carry as u32);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn div_rem() {
        let mut n = ArrayVec::<u32, 4>::new();
        n.push_slice(&[0xdeadbeef, 0xfeedfeed, 0xcafebabe, 0xf00dd00d]);
        let rem = big_div_rem::<4, 63>(&mut n);
        let nn = n.as_ref();
        assert!(nn[0] == 0xaa23440b && nn[1] == 0xa696103c && nn[2] == 0x89513fea && nn[3] == 0x03cf7514 && rem == 58);
    }

    #[test]
    fn encode_decode() {
        let mut test = [0xff; 64];
        for tl in 1..64 {
            let test = &mut test[..tl];
            test.fill(0xff);
            let mut b = Vec::with_capacity(1024);
            for _ in 0..10 {
                let mut s = String::with_capacity(1024);
                encode_into(&test, &mut s, 86);
                b.clear();
                //println!("{}", s);
                assert!(decode_into(s.as_bytes(), &mut b, test.len()).is_ok());
                assert_eq!(b.as_slice(), test);
                for c in test.iter_mut() {
                    *c = crate::rand() as u8;
                }
            }
        }
    }
}
