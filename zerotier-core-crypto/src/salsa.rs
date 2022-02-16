/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::convert::TryInto;
use std::ptr::{slice_from_raw_parts, slice_from_raw_parts_mut};

const CONSTANTS: [u32; 4] = [ u32::from_le_bytes(*b"expa"), u32::from_le_bytes(*b"nd 3"), u32::from_le_bytes(*b"2-by"), u32::from_le_bytes(*b"te k") ];

/// Salsa stream cipher implementation supporting 8, 12, or 20 rounds.
///
/// WARNING: this has a major limitation/caveat. If you call crypt() with plaintext whose
/// size is not a multiple of 64, subsequent calls to crypt() will not be properly aligned.
/// This is okay for uses in ZeroTier but might break other cases. Salsa is deprecated as
/// transport encryption in ZeroTier anyway, but is still used to derive addresses from
/// identity public keys.
pub struct Salsa<const ROUNDS: usize> {
    state: [u32; 16]
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
                CONSTANTS[3]
            ]
        }
    }

    pub fn crypt(&mut self, mut plaintext: &[u8], mut ciphertext: &mut [u8]) {
        let (j0, j1, j2, j3, j4, j5, j6, j7, mut j8, mut j9, j10, j11, j12, j13, j14, j15) = (self.state[0], self.state[1], self.state[2], self.state[3], self.state[4], self.state[5], self.state[6], self.state[7], self.state[8], self.state[9], self.state[10], self.state[11], self.state[12], self.state[13], self.state[14], self.state[15]);
        loop {
            let (mut x0, mut x1, mut x2, mut x3, mut x4, mut x5, mut x6, mut x7, mut x8, mut x9, mut x10, mut x11, mut x12, mut x13, mut x14, mut x15) = (j0, j1, j2, j3, j4, j5, j6, j7, j8, j9, j10, j11, j12, j13, j14, j15);

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

            if plaintext.len() >= 64 {
                #[cfg(any(target_arch = "x86", target_arch = "x86_64", target_arch = "aarch64"))] {
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
                #[cfg(not(any(target_arch = "x86", target_arch = "x86_64", target_arch = "aarch64")))] {
                    // Portable keystream XOR with alignment-safe access and native to little-endian conversion.
                    ciphertext[0..4].copy_from_slice(&(u32::from_ne_bytes(unsafe { *plaintext.as_ptr().cast::<[u8; 4]>() }) ^ x0.to_le()).to_ne_bytes());
                    ciphertext[4..8].copy_from_slice(&(u32::from_ne_bytes(unsafe { *plaintext.as_ptr().add(4).cast::<[u8; 4]>() }) ^ x1.to_le()).to_ne_bytes());
                    ciphertext[8..12].copy_from_slice(&(u32::from_ne_bytes(unsafe { *plaintext.as_ptr().add(8).cast::<[u8; 4]>() }) ^ x2.to_le()).to_ne_bytes());
                    ciphertext[12..16].copy_from_slice(&(u32::from_ne_bytes(unsafe { *plaintext.as_ptr().add(12).cast::<[u8; 4]>() }) ^ x3.to_le()).to_ne_bytes());
                    ciphertext[16..20].copy_from_slice(&(u32::from_ne_bytes(unsafe { *plaintext.as_ptr().add(16).cast::<[u8; 4]>() }) ^ x4.to_le()).to_ne_bytes());
                    ciphertext[20..24].copy_from_slice(&(u32::from_ne_bytes(unsafe { *plaintext.as_ptr().add(20).cast::<[u8; 4]>() }) ^ x5.to_le()).to_ne_bytes());
                    ciphertext[24..28].copy_from_slice(&(u32::from_ne_bytes(unsafe { *plaintext.as_ptr().add(24).cast::<[u8; 4]>() }) ^ x6.to_le()).to_ne_bytes());
                    ciphertext[28..32].copy_from_slice(&(u32::from_ne_bytes(unsafe { *plaintext.as_ptr().add(28).cast::<[u8; 4]>() }) ^ x7.to_le()).to_ne_bytes());
                    ciphertext[32..36].copy_from_slice(&(u32::from_ne_bytes(unsafe { *plaintext.as_ptr().add(32).cast::<[u8; 4]>() }) ^ x8.to_le()).to_ne_bytes());
                    ciphertext[36..40].copy_from_slice(&(u32::from_ne_bytes(unsafe { *plaintext.as_ptr().add(36).cast::<[u8; 4]>() }) ^ x9.to_le()).to_ne_bytes());
                    ciphertext[40..44].copy_from_slice(&(u32::from_ne_bytes(unsafe { *plaintext.as_ptr().add(40).cast::<[u8; 4]>() }) ^ x10.to_le()).to_ne_bytes());
                    ciphertext[44..48].copy_from_slice(&(u32::from_ne_bytes(unsafe { *plaintext.as_ptr().add(44).cast::<[u8; 4]>() }) ^ x11.to_le()).to_ne_bytes());
                    ciphertext[48..52].copy_from_slice(&(u32::from_ne_bytes(unsafe { *plaintext.as_ptr().add(48).cast::<[u8; 4]>() }) ^ x12.to_le()).to_ne_bytes());
                    ciphertext[52..56].copy_from_slice(&(u32::from_ne_bytes(unsafe { *plaintext.as_ptr().add(52).cast::<[u8; 4]>() }) ^ x13.to_le()).to_ne_bytes());
                    ciphertext[56..60].copy_from_slice(&(u32::from_ne_bytes(unsafe { *plaintext.as_ptr().add(56).cast::<[u8; 4]>() }) ^ x14.to_le()).to_ne_bytes());
                    ciphertext[60..64].copy_from_slice(&(u32::from_ne_bytes(unsafe { *plaintext.as_ptr().add(60).cast::<[u8; 4]>() }) ^ x15.to_le()).to_ne_bytes());
                }
                plaintext = &plaintext[64..];
                ciphertext = &mut ciphertext[64..];

                j8 = j8.wrapping_add(1);
                j9 = j9.wrapping_add((j8 == 0) as u32);
            } else {
                if !plaintext.is_empty() {
                    let remainder = [x0.to_le(), x1.to_le(), x2.to_le(), x3.to_le(), x4.to_le(), x5.to_le(), x6.to_le(), x7.to_le(), x8.to_le(), x9.to_le(), x10.to_le(), x11.to_le(), x12.to_le(), x13.to_le(), x14.to_le(), x15.to_le()];
                    for i in 0..plaintext.len() {
                        ciphertext[i] = plaintext[i] ^ unsafe { *remainder.as_ptr().cast::<u8>().add(i) };
                    }
                }

                self.state[8] = j8;
                self.state[9] = j9;

                break;
            }
        }
    }

    #[inline(always)]
    pub fn crypt_in_place(&mut self, data: &mut [u8]) {
        unsafe {
            self.crypt(&*slice_from_raw_parts(data.as_ptr(), data.len()), &mut *slice_from_raw_parts_mut(data.as_mut_ptr(), data.len()))
        }
    }
}
