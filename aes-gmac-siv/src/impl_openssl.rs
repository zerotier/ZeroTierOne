// AES-GMAC-SIV implemented using OpenSSL.

use std::convert::TryInto;
use openssl::symm::{Crypter, Cipher, Mode};

/// AES-GMAC-SIV encryptor/decryptor.
#[repr(align(8))] // allow tag and tmp to be accessed as u64 arrays as well
pub struct AesGmacSiv {
    tag: [u8; 16],
    tmp: [u8; 16],
    k0: [u8; 32],
    k1: [u8; 32],
    ctr: Option<Crypter>,
    gmac: Option<Crypter>,
}

impl AesGmacSiv {
    /// Create a new keyed instance of AES-GMAC-SIV
    /// The key may be of size 16, 24, or 32 bytes (128, 192, or 256 bits). Any other size will panic.
    #[inline(always)]
    pub fn new(k0: &[u8], k1: &[u8]) -> Self {
        if k0.len() != 32 && k0.len() != 24 && k0.len() != 16 {
            panic!("AES supports 128, 192, or 256 bits keys");
        }
        if k1.len() != k0.len() {
            panic!("k0 and k1 must be of the same size");
        }
        AesGmacSiv {
            tag: [0_u8; 16],
            tmp: [0_u8; 16],
            k0: k0.try_into().unwrap(),
            k1: k1.try_into().unwrap(),
            ctr: None,
            gmac: None,
        }
    }

    /// Reset to prepare for another encrypt or decrypt operation.
    #[inline(always)]
    pub fn reset(&mut self) {
        let _ = self.ctr.take();
        let _ = self.gmac.take();
    }

    /// Initialize for encryption.
    #[inline(always)]
    pub fn encrypt_init(&mut self, iv: &[u8]) {
        self.tag[0..8].copy_from_slice(iv);
        self.tag[8..16].fill(0);
        let _ = self.gmac.replace(Crypter::new(Cipher::aes_256_gcm(), Mode::Encrypt, &self.k0, Some(&self.tag)).unwrap());
    }

    /// Set additional authenticated data (data to be authenticated but not encrypted).
    /// This can currently only be called once. Multiple calls will result in corrupt data.
    #[inline(always)]
    pub fn encrypt_set_aad(&mut self, data: &[u8]) {
        let _ = self.gmac.as_mut().unwrap().aad_update(data);
        let pad = data.len() & 0xf;
        if pad != 0 {
            let _ = self.gmac.as_mut().unwrap().aad_update(&crate::ZEROES[0..(16 - pad)]);
        }
    }

    /// Feed plaintext in for the first encryption pass.
    /// This may be called more than once.
    #[inline(always)]
    pub fn encrypt_first_pass(&mut self, plaintext: &[u8]) {
        let _ = self.gmac.as_mut().unwrap().aad_update(plaintext);
    }

    /// Finish first pass and begin second pass.
    #[inline(always)]
    pub fn encrypt_first_pass_finish(&mut self) {
        let gmac = self.gmac.as_mut().unwrap();
        let _ = gmac.finalize(&mut self.tmp);
        let _ = gmac.get_tag(&mut self.tmp);
        unsafe { // tag[8..16] = tmp[0..8] ^ tmp[8..16]
            let tmp = self.tmp.as_mut_ptr().cast::<u64>();
            *self.tag.as_mut_ptr().cast::<u64>().offset(1) = *tmp ^ *tmp.offset(1);
        }
        let mut tag_tmp = [0_u8; 32];
        let _ = Crypter::new(Cipher::aes_256_ecb(), Mode::Encrypt, &self.k1, None).unwrap().update(&self.tag, &mut tag_tmp);
        self.tag.copy_from_slice(&tag_tmp[0..16]);
        self.tmp.copy_from_slice(&self.tag);
        self.tmp[12] &= 0x7f;
        let _ = self.ctr.replace(Crypter::new(Cipher::aes_256_ctr(), Mode::Encrypt, &self.k1, Some(&self.tmp)).unwrap());
    }

    /// Feed plaintext for second pass and write ciphertext to supplied buffer.
    /// This may be called more than once.
    #[inline(always)]
    pub fn encrypt_second_pass(&mut self, plaintext: &[u8], ciphertext: &mut [u8]) {
        let _ = self.ctr.as_mut().unwrap().update(plaintext, ciphertext);
    }

    /// Encrypt plaintext in place.
    /// This may be called more than once.
    #[inline(always)]
    pub fn encrypt_second_pass_in_place(&mut self, plaintext_to_ciphertext: &mut [u8]) {
        let _ = self.ctr.as_mut().unwrap().update(unsafe { std::slice::from_raw_parts(plaintext_to_ciphertext.as_ptr(), plaintext_to_ciphertext.len()) }, plaintext_to_ciphertext);
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
        let _ = self.ctr.replace(Crypter::new(Cipher::aes_256_ctr(), Mode::Decrypt, &self.k1, Some(&self.tmp)).unwrap());
        let mut tag_tmp = [0_u8; 32];
        let _ = Crypter::new(Cipher::aes_256_ecb(), Mode::Decrypt, &self.k1, None).unwrap().update(&self.tag, &mut tag_tmp);
        self.tag.copy_from_slice(&tag_tmp[0..16]);
        unsafe { // tmp[0..8] = tag[0..8], tmp[8..16] = 0
            let tmp = self.tmp.as_mut_ptr().cast::<u64>();
            *tmp = *self.tag.as_mut_ptr().cast::<u64>();
            *tmp.offset(1) = 0;
        }
        let _ = self.gmac.replace(Crypter::new(Cipher::aes_256_gcm(), Mode::Encrypt, &self.k0, Some(&self.tmp)).unwrap());
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
        let _ = self.ctr.as_mut().unwrap().update(ciphertext, plaintext);
        let _ = self.gmac.as_mut().unwrap().aad_update(plaintext);
    }

    /// Decrypt ciphertext in place.
    /// This may be called more than once.
    #[inline(always)]
    pub fn decrypt_in_place(&mut self, ciphertext_to_plaintext: &mut [u8]) {
        let _ = self.ctr.as_mut().unwrap().update(unsafe { std::slice::from_raw_parts(ciphertext_to_plaintext.as_ptr(), ciphertext_to_plaintext.len()) }, ciphertext_to_plaintext);
        let _ = self.gmac.as_mut().unwrap().aad_update(ciphertext_to_plaintext);
    }

    /// Finish decryption and return true if authentication appears valid.
    /// If this returns false the message should be dropped.
    #[inline(always)]
    pub fn decrypt_finish(&mut self) -> bool {
        let gmac = self.gmac.as_mut().unwrap();
        let _ = gmac.finalize(&mut self.tmp);
        let _ = gmac.get_tag(&mut self.tmp);
        unsafe { // tag[8..16] == tmp[0..8] ^ tmp[8..16]
            let tmp = self.tmp.as_mut_ptr().cast::<u64>();
            *self.tag.as_mut_ptr().cast::<u64>().offset(1) == *tmp ^ *tmp.offset(1)
        }
    }
}
