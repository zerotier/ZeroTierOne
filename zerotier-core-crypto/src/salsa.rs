// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::convert::TryInto;
use std::ptr::{slice_from_raw_parts, slice_from_raw_parts_mut};

const CONSTANTS: [u32; 4] = [u32::from_le_bytes(*b"expa"), u32::from_le_bytes(*b"nd 3"), u32::from_le_bytes(*b"2-by"), u32::from_le_bytes(*b"te k")];

/// Salsa stream cipher implementation supporting 8, 12, or 20 rounds.
///
/// WARNING: this has a major limitation/caveat. If you call crypt() with plaintext whose
/// size is not a multiple of 64, subsequent calls to crypt() will not be properly aligned.
/// This is okay for uses in ZeroTier but might break other cases. Salsa is deprecated as
/// transport encryption in ZeroTier anyway, but is still used to derive addresses from
/// identity public keys.
pub struct Salsa<const ROUNDS: usize> {
    state: [u32; 16],
}

impl<const ROUNDS: usize> Salsa<ROUNDS> {
    /// Create a new Salsa cipher given a 256-bit key and a 64-bit IV.
    pub fn new(key: &[u8], iv: &[u8]) -> Self {
        assert!(ROUNDS == 8 || ROUNDS == 12 || ROUNDS == 20);
        assert!(key.len() >= 32);
        assert!(iv.len() >= 8);
        Self {
            state: [
                CONSTANTS[0],
                u32::from_le_bytes((&key[0..4]).try_into().unwrap()),
                u32::from_le_bytes((&key[4..8]).try_into().unwrap()),
                u32::from_le_bytes((&key[8..12]).try_into().unwrap()),
                u32::from_le_bytes((&key[12..16]).try_into().unwrap()),
                CONSTANTS[1],
                u32::from_le_bytes((&iv[0..4]).try_into().unwrap()),
                u32::from_le_bytes((&iv[4..8]).try_into().unwrap()),
                0,
                0,
                CONSTANTS[2],
                u32::from_le_bytes((&key[16..20]).try_into().unwrap()),
                u32::from_le_bytes((&key[20..24]).try_into().unwrap()),
                u32::from_le_bytes((&key[24..28]).try_into().unwrap()),
                u32::from_le_bytes((&key[28..32]).try_into().unwrap()),
                CONSTANTS[3],
            ],
        }
    }

    pub fn crypt(&mut self, mut plaintext: &[u8], mut ciphertext: &mut [u8]) {
        let (j0, j1, j2, j3, j4, j5, j6, j7, mut j8, mut j9, j10, j11, j12, j13, j14, j15) = (
            self.state[0],
            self.state[1],
            self.state[2],
            self.state[3],
            self.state[4],
            self.state[5],
            self.state[6],
            self.state[7],
            self.state[8],
            self.state[9],
            self.state[10],
            self.state[11],
            self.state[12],
            self.state[13],
            self.state[14],
            self.state[15],
        );

        while !plaintext.is_empty() {
            let (mut x0, mut x1, mut x2, mut x3, mut x4, mut x5, mut x6, mut x7, mut x8, mut x9, mut x10, mut x11, mut x12, mut x13, mut x14, mut x15) =
                (j0, j1, j2, j3, j4, j5, j6, j7, j8, j9, j10, j11, j12, j13, j14, j15);

            for _ in 0..(ROUNDS / 2) {
                x4 ^= x0.wrapping_add(x12).rotate_left(7);
                x8 ^= x4.wrapping_add(x0).rotate_left(9);
                x12 ^= x8.wrapping_add(x4).rotate_left(13);
                x0 ^= x12.wrapping_add(x8).rotate_left(18);
                x9 ^= x5.wrapping_add(x1).rotate_left(7);
                x13 ^= x9.wrapping_add(x5).rotate_left(9);
                x1 ^= x13.wrapping_add(x9).rotate_left(13);
                x5 ^= x1.wrapping_add(x13).rotate_left(18);
                x14 ^= x10.wrapping_add(x6).rotate_left(7);
                x2 ^= x14.wrapping_add(x10).rotate_left(9);
                x6 ^= x2.wrapping_add(x14).rotate_left(13);
                x10 ^= x6.wrapping_add(x2).rotate_left(18);
                x3 ^= x15.wrapping_add(x11).rotate_left(7);
                x7 ^= x3.wrapping_add(x15).rotate_left(9);
                x11 ^= x7.wrapping_add(x3).rotate_left(13);
                x15 ^= x11.wrapping_add(x7).rotate_left(18);
                x1 ^= x0.wrapping_add(x3).rotate_left(7);
                x2 ^= x1.wrapping_add(x0).rotate_left(9);
                x3 ^= x2.wrapping_add(x1).rotate_left(13);
                x0 ^= x3.wrapping_add(x2).rotate_left(18);
                x6 ^= x5.wrapping_add(x4).rotate_left(7);
                x7 ^= x6.wrapping_add(x5).rotate_left(9);
                x4 ^= x7.wrapping_add(x6).rotate_left(13);
                x5 ^= x4.wrapping_add(x7).rotate_left(18);
                x11 ^= x10.wrapping_add(x9).rotate_left(7);
                x8 ^= x11.wrapping_add(x10).rotate_left(9);
                x9 ^= x8.wrapping_add(x11).rotate_left(13);
                x10 ^= x9.wrapping_add(x8).rotate_left(18);
                x12 ^= x15.wrapping_add(x14).rotate_left(7);
                x13 ^= x12.wrapping_add(x15).rotate_left(9);
                x14 ^= x13.wrapping_add(x12).rotate_left(13);
                x15 ^= x14.wrapping_add(x13).rotate_left(18);
            }

            x0 = x0.wrapping_add(j0);
            x1 = x1.wrapping_add(j1);
            x2 = x2.wrapping_add(j2);
            x3 = x3.wrapping_add(j3);
            x4 = x4.wrapping_add(j4);
            x5 = x5.wrapping_add(j5);
            x6 = x6.wrapping_add(j6);
            x7 = x7.wrapping_add(j7);
            x8 = x8.wrapping_add(j8);
            x9 = x9.wrapping_add(j9);
            x10 = x10.wrapping_add(j10);
            x11 = x11.wrapping_add(j11);
            x12 = x12.wrapping_add(j12);
            x13 = x13.wrapping_add(j13);
            x14 = x14.wrapping_add(j14);
            x15 = x15.wrapping_add(j15);

            j8 = j8.wrapping_add(1);
            j9 = j9.wrapping_add((j8 == 0) as u32);

            if plaintext.len() >= 64 {
                #[cfg(any(target_arch = "x86", target_arch = "x86_64", target_arch = "aarch64", target_arch = "powerpc64"))]
                {
                    // Slightly faster keystream XOR for little-endian platforms with unaligned load/store.
                    unsafe {
                        *ciphertext.as_mut_ptr().cast::<u32>() = *plaintext.as_ptr().cast::<u32>() ^ x0;
                        *ciphertext.as_mut_ptr().cast::<u32>().add(1) = *plaintext.as_ptr().cast::<u32>().add(1) ^ x1;
                        *ciphertext.as_mut_ptr().cast::<u32>().add(2) = *plaintext.as_ptr().cast::<u32>().add(2) ^ x2;
                        *ciphertext.as_mut_ptr().cast::<u32>().add(3) = *plaintext.as_ptr().cast::<u32>().add(3) ^ x3;
                        *ciphertext.as_mut_ptr().cast::<u32>().add(4) = *plaintext.as_ptr().cast::<u32>().add(4) ^ x4;
                        *ciphertext.as_mut_ptr().cast::<u32>().add(5) = *plaintext.as_ptr().cast::<u32>().add(5) ^ x5;
                        *ciphertext.as_mut_ptr().cast::<u32>().add(6) = *plaintext.as_ptr().cast::<u32>().add(6) ^ x6;
                        *ciphertext.as_mut_ptr().cast::<u32>().add(7) = *plaintext.as_ptr().cast::<u32>().add(7) ^ x7;
                        *ciphertext.as_mut_ptr().cast::<u32>().add(8) = *plaintext.as_ptr().cast::<u32>().add(8) ^ x8;
                        *ciphertext.as_mut_ptr().cast::<u32>().add(9) = *plaintext.as_ptr().cast::<u32>().add(9) ^ x9;
                        *ciphertext.as_mut_ptr().cast::<u32>().add(10) = *plaintext.as_ptr().cast::<u32>().add(10) ^ x10;
                        *ciphertext.as_mut_ptr().cast::<u32>().add(11) = *plaintext.as_ptr().cast::<u32>().add(11) ^ x11;
                        *ciphertext.as_mut_ptr().cast::<u32>().add(12) = *plaintext.as_ptr().cast::<u32>().add(12) ^ x12;
                        *ciphertext.as_mut_ptr().cast::<u32>().add(13) = *plaintext.as_ptr().cast::<u32>().add(13) ^ x13;
                        *ciphertext.as_mut_ptr().cast::<u32>().add(14) = *plaintext.as_ptr().cast::<u32>().add(14) ^ x14;
                        *ciphertext.as_mut_ptr().cast::<u32>().add(15) = *plaintext.as_ptr().cast::<u32>().add(15) ^ x15;
                    }
                }
                #[cfg(not(any(target_arch = "x86", target_arch = "x86_64", target_arch = "aarch64")))]
                {
                    // Portable keystream XOR with alignment-safe access and native to little-endian conversion.
                    let keystream = [
                        x0.to_le(),
                        x1.to_le(),
                        x2.to_le(),
                        x3.to_le(),
                        x4.to_le(),
                        x5.to_le(),
                        x6.to_le(),
                        x7.to_le(),
                        x8.to_le(),
                        x9.to_le(),
                        x10.to_le(),
                        x11.to_le(),
                        x12.to_le(),
                        x13.to_le(),
                        x14.to_le(),
                        x15.to_le(),
                    ];
                    for i in 0..64 {
                        ciphertext[i] = plaintext[i] ^ unsafe { *keystream.as_ptr().cast::<u8>().add(i) };
                    }
                }

                plaintext = &plaintext[64..];
                ciphertext = &mut ciphertext[64..];
            } else {
                let keystream = [
                    x0.to_le(),
                    x1.to_le(),
                    x2.to_le(),
                    x3.to_le(),
                    x4.to_le(),
                    x5.to_le(),
                    x6.to_le(),
                    x7.to_le(),
                    x8.to_le(),
                    x9.to_le(),
                    x10.to_le(),
                    x11.to_le(),
                    x12.to_le(),
                    x13.to_le(),
                    x14.to_le(),
                    x15.to_le(),
                ];
                for i in 0..plaintext.len() {
                    ciphertext[i] = plaintext[i] ^ unsafe { *keystream.as_ptr().cast::<u8>().add(i) };
                }
                break;
            }
        }

        self.state[8] = j8;
        self.state[9] = j9;
    }

    #[inline(always)]
    pub fn crypt_in_place(&mut self, data: &mut [u8]) {
        unsafe { self.crypt(&*slice_from_raw_parts(data.as_ptr(), data.len()), &mut *slice_from_raw_parts_mut(data.as_mut_ptr(), data.len())) }
    }
}

#[cfg(test)]
mod tests {
    use crate::salsa::*;

    const SALSA_20_TV0_KEY: [u8; 32] = [
        0x0f, 0x62, 0xb5, 0x08, 0x5b, 0xae, 0x01, 0x54, 0xa7, 0xfa, 0x4d, 0xa0, 0xf3, 0x46, 0x99, 0xec, 0x3f, 0x92, 0xe5, 0x38, 0x8b, 0xde, 0x31, 0x84, 0xd7, 0x2a, 0x7d, 0xd0,
        0x23, 0x76, 0xc9, 0x1c,
    ];
    const SALSA_20_TV0_IV: [u8; 8] = [0x28, 0x8f, 0xf6, 0x5d, 0xc4, 0x2b, 0x92, 0xf9];
    const SALSA_20_TV0_KS: [u8; 64] = [
        0x5e, 0x5e, 0x71, 0xf9, 0x01, 0x99, 0x34, 0x03, 0x04, 0xab, 0xb2, 0x2a, 0x37, 0xb6, 0x62, 0x5b, 0xf8, 0x83, 0xfb, 0x89, 0xce, 0x3b, 0x21, 0xf5, 0x4a, 0x10, 0xb8, 0x10,
        0x66, 0xef, 0x87, 0xda, 0x30, 0xb7, 0x76, 0x99, 0xaa, 0x73, 0x79, 0xda, 0x59, 0x5c, 0x77, 0xdd, 0x59, 0x54, 0x2d, 0xa2, 0x08, 0xe5, 0x95, 0x4f, 0x89, 0xe4, 0x0e, 0xb7,
        0xaa, 0x80, 0xa8, 0x4a, 0x61, 0x76, 0x66, 0x3f,
    ];

    #[test]
    fn salsa20() {
        let mut s20 = Salsa::<20>::new(&SALSA_20_TV0_KEY, &SALSA_20_TV0_IV);
        let mut ks = [0_u8; 64];
        s20.crypt_in_place(&mut ks);
        assert_eq!(ks, SALSA_20_TV0_KS);

        let mut s20 = Salsa::<20>::new(&SALSA_20_TV0_KEY, &SALSA_20_TV0_IV);
        let mut ks = [0_u8; 32];
        s20.crypt_in_place(&mut ks);
        assert_eq!(ks, &SALSA_20_TV0_KS[..32]);
    }
}
