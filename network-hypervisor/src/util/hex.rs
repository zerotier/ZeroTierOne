pub(crate) const HEX_CHARS: [u8; 16] = [ b'0', b'1', b'2', b'3', b'4', b'5', b'6', b'7', b'8', b'9', b'a', b'b', b'c', b'd', b'e', b'f'];

/// Encode a binary string to a series of hex bytes.
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
