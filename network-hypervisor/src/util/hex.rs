pub(crate) const HEX_CHARS: [u8; 16] = [ b'0', b'1', b'2', b'3', b'4', b'5', b'6', b'7', b'8', b'9', b'a', b'b', b'c', b'd', b'e', b'f'];

/// Encode a byte slice to a hexadecimal string.
pub fn to_string(b: &[u8]) -> String {
    let mut s = String::new();
    s.reserve(b.len() * 2);
    for c in b {
        let x = *c as usize;
        s.push(HEX_CHARS[x >> 4] as char);
        s.push(HEX_CHARS[x & 0xf] as char);
    }
    s
}

/// Encode an unsigned 64-bit value as a hexadecimal string.
pub fn to_string_u64(mut i: u64, skip_leading_zeroes: bool) -> String {
    let mut s = String::new();
    s.reserve(16);
    for _ in 0..16 {
        let ii = i >> 60;
        if ii != 0 || !s.is_empty() || !skip_leading_zeroes {
            s.push(HEX_CHARS[ii as usize] as char);
        }
        i = i.wrapping_shl(4);
    }
    s
}

/// Encode an unsigned 64-bit value as a hexadecimal ASCII string.
pub fn to_vec_u64(mut i: u64, skip_leading_zeroes: bool) -> Vec<u8> {
    let mut s = Vec::new();
    s.reserve(16);
    for _ in 0..16 {
        let ii = i >> 60;
        if ii != 0 || !s.is_empty() || !skip_leading_zeroes {
            s.push(HEX_CHARS[ii as usize]);
        }
        i = i.wrapping_shl(4);
    }
    s
}

/// Decode a hex string, ignoring all non-hexadecimal characters.
pub fn from_string(s: &str) -> Vec<u8> {
    let mut b: Vec<u8> = Vec::new();
    b.reserve((s.len() / 2) + 1);

    let mut byte = 0_u8;
    let mut have_8: bool = false;
    for cc in s.as_bytes() {
        let c = *cc;
        if c >= 48 && c <= 57 {
            byte = (byte.wrapping_shl(4)) | (c - 48);
            if have_8 {
                b.push(byte);
            }
            have_8 = !have_8;
        } else if c >= 65 && c <= 70 {
            byte = (byte.wrapping_shl(4)) | (c - 55);
            if have_8 {
                b.push(byte);
            }
            have_8 = !have_8;
        } else if c >= 97 && c <= 102 {
            byte = (byte.wrapping_shl(4)) | (c - 87);
            if have_8 {
                b.push(byte);
            }
            have_8 = !have_8;
        }
    }

    b
}

/// Encode bytes from 'b' into hex characters in 'dest'.
/// This will panic if the destination slice is smaller than twice the length of the source.
pub fn to_hex_bytes(b: &[u8], dest: &mut [u8]) {
    let mut j = 0;
    for c in b {
        let x = *c as usize;
        dest[j] = HEX_CHARS[x >> 4];
        dest[j + 1] = HEX_CHARS[x & 0xf];
        j += 2;
    }
}
