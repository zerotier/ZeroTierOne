use crate::crypto::hash::SHA384;

/// Derive a key using KBKDF prefaced by the bytes 'ZT' for use in ZeroTier.
/// This is a fixed cost key derivation function used to derive sub-keys from a single original
/// shared secret for different uses, such as the K0/K1 in AES-GMAC-SIV.
/// Key must be 384 bits in length.
pub fn zt_kbkdf_hmac_sha384(key: &[u8], label: u8, context: u8, iter: u32) -> [u8; 48] {
    debug_assert!(key.len() == 48);
    // HMAC'd message is: preface | iteration[4], preface[2], label, 0x00, context, hash size[4]
    // See: https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-108.pdf
    SHA384::hmac(key, &[(iter >> 24) as u8, (iter >> 16) as u8, (iter >> 8) as u8, iter as u8, b'Z', b'T', label, 0, context, 0, 0, 0x01, 0x80])
}
