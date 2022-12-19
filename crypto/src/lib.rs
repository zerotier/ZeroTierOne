// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

pub mod aes;
pub mod aes_gmac_siv;
pub mod hash;
pub mod p384;
pub mod poly1305;
pub mod random;
pub mod salsa;
pub mod secret;
pub mod verified;
pub mod x25519;

pub const ZEROES: [u8; 64] = [0_u8; 64];

/// Constant time byte slice equality.
pub fn secure_eq(a: &[u8], b: &[u8]) -> bool {
    if a.len() == b.len() {
        let mut x = 0u8;
        for (aa, bb) in a.iter().zip(b.iter()) {
            x |= *aa ^ *bb;
        }
        x == 0
    } else {
        false
    }
}
