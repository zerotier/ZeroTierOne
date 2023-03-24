/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::io::Write;

use crate::error::InvalidParameterError;

// All unambiguous letters, thus easy to type on the alphabetic keyboards on phones without extra shift taps.
const BASE24_ALPHABET: [u8; 24] = *(b"abcdefghjkmnopqrstuvwxyz"); // avoids 'i' and 'l'
const BASE24_ALPHABET_INV: [u8; 26] = [
    0, 1, 2, 3, 4, 5, 6, 7, 255, 8, 9, 255, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
];

/// Encode a byte slice into base24 ASCII format (no padding)
pub fn encode_into(mut b: &[u8], s: &mut String) {
    while b.len() >= 4 {
        let mut n = u32::from_le_bytes(b[..4].try_into().unwrap());
        for _ in 0..6 {
            s.push(BASE24_ALPHABET[(n % 24) as usize] as char);
            n /= 24;
        }
        s.push(BASE24_ALPHABET[n as usize] as char);
        b = &b[4..];
    }

    if !b.is_empty() {
        let mut n = 0u32;
        for i in 0..b.len() {
            n |= (b[i] as u32).wrapping_shl((i as u32) * 8);
        }
        for _ in 0..(b.len() * 2) {
            s.push(BASE24_ALPHABET[(n % 24) as usize] as char);
            n /= 24;
        }
    }
}

fn decode_up_to_u32(s: &[u8]) -> Result<u32, InvalidParameterError> {
    let mut n = 0u32;
    for c in s.iter().rev() {
        let mut c = *c;
        if c >= 97 && c <= 122 {
            c -= 97;
        } else if c >= 65 && c <= 90 {
            c -= 65;
        } else {
            return Err(InvalidParameterError("invalid base24 character"));
        }
        let i = BASE24_ALPHABET_INV[c as usize];
        if i == 255 {
            return Err(InvalidParameterError("invalid base24 character"));
        }
        n *= 24;
        n = n.wrapping_add(i as u32);
    }
    return Ok(n);
}

/// Decode a base24 ASCII slice into bytes (no padding, length determines output length)
pub fn decode_into(s: &str, b: &mut Vec<u8>) -> Result<(), InvalidParameterError> {
    let mut s = s.as_bytes();

    while s.len() >= 7 {
        let _ = b.write_all(&decode_up_to_u32(&s[..7])?.to_le_bytes());
        s = &s[7..];
    }

    if !s.is_empty() {
        let _ = b.write_all(
            &decode_up_to_u32(s)?.to_le_bytes()[..match s.len() {
                2 => 1,
                4 => 2,
                6 => 3,
                _ => return Err(InvalidParameterError("invalid base24 length")),
            }],
        );
    }

    return Ok(());
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn base24_encode_decode() {
        let mut tmp = [0xffu8; 256];
        for _ in 0..3 {
            let mut s = String::with_capacity(1024);
            let mut v: Vec<u8> = Vec::with_capacity(256);
            for i in 1..256 {
                s.clear();
                encode_into(&tmp[..i], &mut s);
                //println!("{}", s);
                v.clear();
                decode_into(s.as_str(), &mut v).expect("decode error");
                assert!(v.as_slice().eq(&tmp[..i]));
            }
            for b in tmp.iter_mut() {
                *b -= 3;
            }
        }
    }
}
