use crate::error::InvalidParameterError;

/// URL-safe base64 alphabet
const ALPHABET: [u8; 64] = *b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
const ALPHABET_INV: [u8; 256] = [
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 62, 255, 255, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 255, 255,
    255, 255, 255, 255, 255, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 255, 255, 255, 255, 63,
    255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
];

pub fn encode_into(mut b: &[u8], s: &mut String) {
    while b.len() >= 3 {
        let bits = (b[0] as usize) | (b[1] as usize).wrapping_shl(8) | (b[2] as usize).wrapping_shl(16);
        b = &b[3..];
        let (i0, i1, i2, i3) = (bits & 63, bits.wrapping_shr(6) & 63, bits.wrapping_shr(12) & 63, bits.wrapping_shr(18));
        s.push(ALPHABET[i0] as char);
        s.push(ALPHABET[i1] as char);
        s.push(ALPHABET[i2] as char);
        s.push(ALPHABET[i3] as char);
    }
    if b.len() == 2 {
        let bits = (b[0] as usize) | (b[1] as usize).wrapping_shl(8);
        s.push(ALPHABET[bits & 63] as char);
        s.push(ALPHABET[bits.wrapping_shr(6) & 63] as char);
        s.push(ALPHABET[bits.wrapping_shr(12)] as char);
    } else if b.len() == 1 {
        let bits = b[0] as usize;
        s.push(ALPHABET[bits & 63] as char);
        s.push(ALPHABET[bits.wrapping_shr(6)] as char);
    }
}

pub fn decode_into(mut s: &[u8], b: &mut Vec<u8>) -> Result<(), InvalidParameterError> {
    while s.len() >= 4 {
        let (i0, i1, i2, i3) = (
            ALPHABET_INV[s[0] as usize],
            ALPHABET_INV[s[1] as usize],
            ALPHABET_INV[s[2] as usize],
            ALPHABET_INV[s[3] as usize],
        );
        s = &s[4..];
        if (i0 | i1 | i2 | i3) > 64 {
            return Err(InvalidParameterError("invalid base64 string"));
        }
        let bits = (i0 as usize) | (i1 as usize).wrapping_shl(6) | (i2 as usize).wrapping_shl(12) | (i3 as usize).wrapping_shl(18);
        b.push((bits & 0xff) as u8);
        b.push((bits.wrapping_shr(8) & 0xff) as u8);
        b.push((bits.wrapping_shr(16) & 0xff) as u8);
    }
    match s.len() {
        1 => return Err(InvalidParameterError("invalid base64 string")),
        2 => {
            let (i0, i1) = (ALPHABET_INV[s[0] as usize], ALPHABET_INV[s[1] as usize]);
            if (i0 | i1) > 64 {
                return Err(InvalidParameterError("invalid base64 string"));
            }
            let bits = (i0 as usize) | (i1 as usize).wrapping_shl(6);
            b.push((bits & 0xff) as u8);
        }
        3 => {
            let (i0, i1, i2) = (ALPHABET_INV[s[0] as usize], ALPHABET_INV[s[1] as usize], ALPHABET_INV[s[2] as usize]);
            if (i0 | i1 | i2) > 64 {
                return Err(InvalidParameterError("invalid base64 string"));
            }
            let bits = (i0 as usize) | (i1 as usize).wrapping_shl(6) | (i2 as usize).wrapping_shl(12);
            b.push((bits & 0xff) as u8);
            b.push((bits.wrapping_shr(8) & 0xff) as u8);
        }
        _ => {}
    }
    Ok(())
}

pub fn encode(b: &[u8]) -> String {
    let mut tmp = String::with_capacity(((b.len() / 3) * 4) + 3);
    encode_into(b, &mut tmp);
    tmp
}

pub fn decode(s: &[u8]) -> Result<Vec<u8>, InvalidParameterError> {
    let mut tmp = Vec::with_capacity(((s.len() / 4) * 3) + 3);
    decode_into(s, &mut tmp)?;
    Ok(tmp)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn encode_decode() {
        let mut tmp = [0xffu8; 256];
        for _ in 0..7 {
            let mut s = String::with_capacity(1024);
            let mut v: Vec<u8> = Vec::with_capacity(256);
            for i in 1..256 {
                s.clear();
                encode_into(&tmp[..i], &mut s);
                //println!("{}", s);
                v.clear();
                decode_into(s.as_str().as_bytes(), &mut v).expect("decode error");
                assert!(v.as_slice().eq(&tmp[..i]));
            }
            for b in tmp.iter_mut() {
                *b -= 13;
            }
        }
    }
}
