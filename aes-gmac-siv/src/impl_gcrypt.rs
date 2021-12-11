/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

// AES-GMAC-SIV implemented using libgcrypt.

use std::io::Write;

pub struct AesCtr(gcrypt::cipher::Cipher);

impl AesCtr {
    /// Construct a new AES-CTR cipher.
    /// Key must be 16, 24, or 32 bytes in length or a panic will occur.
    pub fn new(k: &[u8]) -> Self {
        if k.len() != 32 && k.len() != 24 && k.len() != 16 {
            panic!("AES supports 128, 192, or 256 bits keys");
        }
        AesCtr(gcrypt::cipher::Cipher::new(gcrypt::cipher::Algorithm::Aes, gcrypt::cipher::Mode::Ctr).unwrap())
    }

    /// Initialize AES-CTR for encryption or decryption with the given IV.
    /// If it's already been used, this also resets the cipher. There is no separate reset.
    #[inline(always)]
    pub fn init(&mut self, iv: &[u8]) {
        let _ = self.0.reset();
        if iv.len() == 16 {
            let _ = self.0.set_iv(iv);
        } else if iv.len() < 16 {
            let mut iv2 = [0_u8; 16];
            iv2[0..iv.len()].copy_from_slice(iv);
            let _ = self.0.set_iv(iv2);
        } else {
            panic!("CTR IV must be less than or equal to 16 bytes in length");
        }
    }

    /// Encrypt or decrypt (same operation with CTR mode)
    #[inline(always)]
    pub fn crypt(&mut self, input: &[u8], output: &mut [u8]) {
        let _ = self.0.encrypt(input, output);
    }

    /// Encrypt or decrypt in place (same operation with CTR mode)
    #[inline(always)]
    pub fn crypt_in_place(&mut self, data: &mut [u8]) {
        let _ = self.0.encrypt_inplace(data);
    }
}

#[repr(align(8))] // allow tag and tmp to be accessed as u64 arrays as well
pub struct AesGmacSiv {
    tag: [u8; 16],
    tmp: [u8; 16],
    ctr: gcrypt::cipher::Cipher,
    ecb: gcrypt::cipher::Cipher,
    gmac: gcrypt::mac::Mac,
}

impl AesGmacSiv {
    /// Create a new keyed instance of AES-GMAC-SIV
    /// The key may be of size 16, 24, or 32 bytes (128, 192, or 256 bits). Any other size will panic.
    pub fn new(k0: &[u8], k1: &[u8]) -> Self {
        if k0.len() != 32 && k0.len() != 24 && k0.len() != 16 {
            panic!("AES supports 128, 192, or 256 bits keys");
        }
        if k1.len() != k0.len() {
            panic!("k0 and k1 must be of the same size");
        }
        let mut c = AesGmacSiv {
            tag: [0_u8; 16],
            tmp: [0_u8; 16],
            ctr: gcrypt::cipher::Cipher::new(gcrypt::cipher::Algorithm::Aes, gcrypt::cipher::Mode::Ctr).unwrap(),
            ecb: gcrypt::cipher::Cipher::new(gcrypt::cipher::Algorithm::Aes, gcrypt::cipher::Mode::Ecb).unwrap(),
            gmac: gcrypt::mac::Mac::new(gcrypt::mac::Algorithm::GmacAes).unwrap(),
        };
        c.ctr.set_key(k1).expect("AES-CTR init failed");
        c.ecb.set_key(k1).expect("AES-ECB init failed");
        c.gmac.set_key(k0).expect("AES-GMAC init failed");
        c
    }

    /// Reset to prepare for another encrypt or decrypt operation.
    #[inline(always)]
    pub fn reset(&mut self) {
        let _ = self.ctr.reset();
        // self.ecb is stateless
        let _ = self.gmac.reset();
    }

    /// Initialize for encryption.
    #[inline(always)]
    pub fn encrypt_init(&mut self, iv: &[u8]) {
        self.tag[0..8].copy_from_slice(iv);
        self.tag[8..16].fill(0);
        let _ = self.gmac.set_iv(&self.tag);
    }

    /// Set additional authenticated data (data to be authenticated but not encrypted).
    /// This can currently only be called once. Multiple calls will result in corrupt data.
    #[inline(always)]
    pub fn encrypt_set_aad(&mut self, data: &[u8]) {
        let _ = self.gmac.update(data);
        let pad = data.len() & 0xf;
        if pad != 0 {
            let _ = self.gmac.update(&crate::ZEROES[0..(16 - pad)]);
        }
    }

    /// Feed plaintext in for the first encryption pass.
    /// This may be called more than once.
    #[inline(always)]
    pub fn encrypt_first_pass(&mut self, plaintext: &[u8]) {
        let _ = self.gmac.update(plaintext);
    }

    /// Finish first pass and begin second pass.
    #[inline(always)]
    pub fn encrypt_first_pass_finish(&mut self) {
        let _ = self.gmac.flush();
        let _ = self.gmac.get_mac(&mut self.tmp);
        unsafe { // tag[8..16] = tmp[0..8] ^ tmp[8..16]
            let tmp = self.tmp.as_mut_ptr().cast::<u64>();
            *self.tag.as_mut_ptr().cast::<u64>().offset(1) = *tmp ^ *tmp.offset(1);
        }
        let _ = self.ecb.encrypt_inplace(&mut self.tag);
        self.tmp.copy_from_slice(&self.tag);
        self.tmp[12] &= 0x7f;
        let _ = self.ctr.set_ctr(&self.tmp);
    }

    /// Feed plaintext for second pass and write ciphertext to supplied buffer.
    /// This may be called more than once.
    #[inline(always)]
    pub fn encrypt_second_pass(&mut self, plaintext: &[u8], ciphertext: &mut [u8]) {
        let _ = self.ctr.encrypt(plaintext, ciphertext);
    }

    /// Encrypt plaintext in place.
    /// This may be called more than once.
    #[inline(always)]
    pub fn encrypt_second_pass_in_place(&mut self, plaintext_to_ciphertext: &mut [u8]) {
        let _ = self.ctr.encrypt_inplace(plaintext_to_ciphertext);
    }

    /// Finish second pass and return a reference to the tag for this message.
    /// The tag returned remains valid until reset() is called.
    #[inline(always)]
    pub fn encrypt_second_pass_finish(&mut self) -> &[u8; 16] {
        return &self.tag;
    }

    #[inline(always)]
    fn decrypt_init_internal(&mut self) {
        self.tmp[12] &= 0x7f;
        let _ = self.ctr.set_ctr(&self.tmp);
        let _ = self.ecb.decrypt_inplace(&mut self.tag);
        unsafe { // tmp[0..8] = tag[0..8], tmp[8..16] = 0
            let tmp = self.tmp.as_mut_ptr().cast::<u64>();
            *tmp = *self.tag.as_mut_ptr().cast::<u64>();
            *tmp.offset(1) = 0;
        }
        let _ = self.gmac.set_iv(&self.tmp);
    }

    /// Initialize this cipher for decryption.
    /// The supplied tag must be 16 bytes in length. Any other length will panic.
    #[inline(always)]
    pub fn decrypt_init(&mut self, tag: &[u8]) {
        self.tmp.copy_from_slice(tag);
        self.tag.copy_from_slice(tag);
        self.decrypt_init_internal();
    }

    /// Set additional authenticated data to be checked.
    #[inline(always)]
    pub fn decrypt_set_aad(&mut self, data: &[u8]) {
        self.encrypt_set_aad(data);
    }

    /// Decrypt ciphertext and write to plaintext.
    /// This may be called more than once.
    #[inline(always)]
    pub fn decrypt(&mut self, ciphertext: &[u8], plaintext: &mut [u8]) {
        let _ = self.ctr.decrypt(ciphertext, plaintext);
        let _ = self.gmac.update(plaintext);
    }

    /// Decrypt ciphertext in place.
    /// This may be called more than once.
    #[inline(always)]
    pub fn decrypt_in_place(&mut self, ciphertext_to_plaintext: &mut [u8]) {
        let _ = self.ctr.decrypt_inplace(ciphertext_to_plaintext);
        let _ = self.gmac.update(ciphertext_to_plaintext);
    }

    /// Finish decryption and return true if authentication appears valid.
    /// If this returns false the message should be dropped.
    #[inline(always)]
    pub fn decrypt_finish(&mut self) -> bool {
        let _ = self.gmac.flush();
        let _ = self.gmac.get_mac(&mut self.tmp);
        unsafe { // tag[8..16] == tmp[0..8] ^ tmp[8..16]
            let tmp = self.tmp.as_mut_ptr().cast::<u64>();
            *self.tag.as_mut_ptr().cast::<u64>().offset(1) == *tmp ^ *tmp.offset(1)
        }
    }
}
