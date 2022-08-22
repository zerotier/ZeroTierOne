// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

pub mod aes;
pub mod aes_gmac_siv;
pub mod hash;
pub mod hex;
pub mod kbkdf;
pub mod noise;
pub mod p384;
pub mod poly1305;
pub mod random;
pub mod salsa;
pub mod secret;
pub mod varint;
pub mod x25519;

mod p384_internal;

pub const ZEROES: [u8; 16] = [0_u8; 16];
