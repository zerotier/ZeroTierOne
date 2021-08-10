// AES-GMAC-SIV implemented using MacOS/iOS CommonCrypto (MacOS 10.13 or newer required).

use std::os::raw::{c_void, c_int};
use std::ptr::{null_mut, null};

#[allow(non_upper_case_globals)]
const kCCModeECB: i32 = 1;
#[allow(non_upper_case_globals)]
const kCCModeCTR: i32 = 4;
#[allow(non_upper_case_globals)]
const kCCModeGCM: i32 = 11;
#[allow(non_upper_case_globals)]
const kCCEncrypt: i32 = 0;
#[allow(non_upper_case_globals)]
const kCCDecrypt: i32 = 1;
#[allow(non_upper_case_globals)]
const kCCAlgorithmAES: i32 = 0;
#[allow(non_upper_case_globals)]
const kCCOptionECBMode: i32 = 2;

extern "C" {
    fn CCCryptorCreateWithMode(op: i32, mode: i32, alg: i32, padding: i32, iv: *const c_void, key: *const c_void, key_len: usize, tweak: *const c_void, tweak_len: usize, num_rounds: c_int, options: i32, cryyptor_ref: *mut *mut c_void) -> i32;
    fn CCCryptorUpdate(cryptor_ref: *mut c_void, data_in: *const c_void, data_in_len: usize, data_out: *mut c_void, data_out_len: usize, data_out_written: *mut usize) -> i32;
    fn CCCryptorReset(cryptor_ref: *mut c_void, iv: *const c_void) -> i32;
    fn CCCryptorRelease(cryptor_ref: *mut c_void) -> i32;
    fn CCCryptorGCMSetIV(cryptor_ref: *mut c_void, iv: *const c_void, iv_len: usize) -> i32;
    fn CCCryptorGCMAddAAD(cryptor_ref: *mut c_void, aad: *const c_void, len: usize) -> i32;
    fn CCCryptorGCMFinalize(cryptor_ref: *mut c_void, tag: *mut c_void, tag_len: usize) -> i32;
    fn CCCryptorGCMReset(cryptor_ref: *mut c_void) -> i32;
}

pub struct AesCtr(*mut c_void);

impl Drop for AesCtr {
    fn drop(&mut self) {
        if !self.0.is_null() {
            unsafe {
                CCCryptorRelease(self.0);
            }
        }
    }
}

impl AesCtr {
    /// Construct a new AES-CTR cipher.
    /// Key must be 16, 24, or 32 bytes in length or a panic will occur.
    #[inline(always)]
    pub fn new(k: &[u8]) -> Self {
        if k.len() != 32 && k.len() != 24 && k.len() != 16 {
            panic!("AES supports 128, 192, or 256 bits keys");
        }
        unsafe {
            let mut ptr: *mut c_void = null_mut();
            let result = CCCryptorCreateWithMode(kCCEncrypt, kCCModeCTR, kCCAlgorithmAES, 0, crate::ZEROES.as_ptr().cast(), k.as_ptr().cast(), k.len(), null(), 0, 0, 0, &mut ptr);
            if result != 0 {
                panic!("CCCryptorCreateWithMode for CTR mode returned {}", result);
            }
            AesCtr(ptr)
        }
    }

    /// Initialize AES-CTR for encryption or decryption with the given IV.
    /// If it's already been used, this also resets the cipher. There is no separate reset.
    #[inline(always)]
    pub fn init(&mut self, iv: &[u8]) {
        unsafe {
            if iv.len() == 16 {
                if CCCryptorReset(self.0, iv.as_ptr().cast()) != 0 {
                    panic!("CCCryptorReset for CTR mode failed (old MacOS bug)");
                }
            } else if iv.len() < 16 {
                let mut iv2 = [0_u8; 16];
                iv2[0..iv.len()].copy_from_slice(iv);
                if CCCryptorReset(self.0, iv2.as_ptr().cast()) != 0 {
                    panic!("CCCryptorReset for CTR mode failed (old MacOS bug)");
                }
            } else {
                panic!("CTR IV must be less than or equal to 16 bytes in length");
            }
        }
    }

    /// Encrypt or decrypt (same operation with CTR mode)
    #[inline(always)]
    pub fn crypt(&mut self, input: &[u8], output: &mut [u8]) {
        unsafe {
            assert!(output.len() >= input.len());
            let mut data_out_written: usize = 0;
            CCCryptorUpdate(self.0, input.as_ptr().cast(), input.len(), output.as_mut_ptr().cast(), output.len(), &mut data_out_written);
        }
    }

    /// Encrypt or decrypt in place (same operation with CTR mode)
    #[inline(always)]
    pub fn crypt_in_place(&mut self, data: &mut [u8]) {
        unsafe {
            let mut data_out_written: usize = 0;
            CCCryptorUpdate(self.0, data.as_ptr().cast(), data.len(), data.as_mut_ptr().cast(), data.len(), &mut data_out_written);
        }
    }
}

#[repr(align(8))]
pub struct AesGmacSiv {
    tag: [u8; 16],
    tmp: [u8; 16],
    ctr: *mut c_void,
    ecb_enc: *mut c_void,
    ecb_dec: *mut c_void,
    gmac: *mut c_void,
}

impl Drop for AesGmacSiv {
    fn drop(&mut self) {
        unsafe {
            if !self.ctr.is_null() {
                CCCryptorRelease(self.ctr);
            }
            if !self.ecb_enc.is_null() {
                CCCryptorRelease(self.ecb_enc);
            }
            if !self.ecb_dec.is_null() {
                CCCryptorRelease(self.ecb_dec);
            }
            if !self.gmac.is_null() {
                CCCryptorRelease(self.gmac);
            }
        }
    }
}

impl AesGmacSiv {
    /// Create a new keyed instance of AES-GMAC-SIV
    /// The key may be of size 16, 24, or 32 bytes (128, 192, or 256 bits). Any other size will panic.
    /// Two keys are required: one for GMAC and one for AES-CTR.
    #[inline(always)]
    pub fn new(k0: &[u8], k1: &[u8]) -> Self {
        if k0.len() != 32 && k0.len() != 24 && k0.len() != 16 {
            panic!("AES supports 128, 192, or 256 bits keys");
        }
        if k1.len() != k0.len() {
            panic!("k0 and k1 must be of the same size");
        }
        let mut c: AesGmacSiv = AesGmacSiv {
            tag: [0_u8; 16],
            tmp: [0_u8; 16],
            ctr: null_mut(),
            ecb_enc: null_mut(),
            ecb_dec: null_mut(),
            gmac: null_mut(),
        };
        unsafe {
            let result = CCCryptorCreateWithMode(kCCEncrypt, kCCModeCTR, kCCAlgorithmAES, 0, crate::ZEROES.as_ptr().cast(), k1.as_ptr().cast(), k1.len(), null(), 0, 0, 0, &mut c.ctr);
            if result != 0 {
                panic!("CCCryptorCreateWithMode for CTR mode returned {}", result);
            }
            let result = CCCryptorCreateWithMode(kCCEncrypt, kCCModeECB, kCCAlgorithmAES, 0, crate::ZEROES.as_ptr().cast(), k1.as_ptr().cast(), k1.len(), null(), 0, 0, kCCOptionECBMode, &mut c.ecb_enc);
            if result != 0 {
                panic!("CCCryptorCreateWithMode for ECB encrypt mode returned {}", result);
            }
            let result = CCCryptorCreateWithMode(kCCDecrypt, kCCModeECB, kCCAlgorithmAES, 0, crate::ZEROES.as_ptr().cast(), k1.as_ptr().cast(), k1.len(), null(), 0, 0, kCCOptionECBMode, &mut c.ecb_dec);
            if result != 0 {
                panic!("CCCryptorCreateWithMode for ECB decrypt mode returned {}", result);
            }
            let result = CCCryptorCreateWithMode(kCCEncrypt, kCCModeGCM, kCCAlgorithmAES, 0, crate::ZEROES.as_ptr().cast(), k0.as_ptr().cast(), k0.len(), null(), 0, 0, 0, &mut c.gmac);
            if result != 0 {
                panic!("CCCryptorCreateWithMode for GCM (GMAC) mode returned {}", result);
            }
        }
        c
    }

    /// Reset to prepare for another encrypt or decrypt operation.
    #[inline(always)]
    pub fn reset(&mut self) {
        unsafe {
            CCCryptorGCMReset(self.gmac);
        }
    }

    /// Initialize for encryption.
    #[inline(always)]
    pub fn encrypt_init(&mut self, iv: &[u8]) {
        self.tag[0..8].copy_from_slice(iv);
        self.tag[8..16].fill(0);
        unsafe {
            CCCryptorGCMSetIV(self.gmac, self.tag.as_ptr().cast(), 16);
        }
    }

    /// Set additional authenticated data (data to be authenticated but not encrypted).
    /// This can currently only be called once. Multiple calls will result in corrupt data.
    #[inline(always)]
    pub fn encrypt_set_aad(&mut self, data: &[u8]) {
        unsafe {
            CCCryptorGCMAddAAD(self.gmac, data.as_ptr().cast(), data.len());
        }
        let pad = data.len() & 0xf;
        if pad != 0 {
            unsafe {
                CCCryptorGCMAddAAD(self.gmac, crate::ZEROES.as_ptr().cast(), 16 - pad);
            }
        }
    }

    /// Feed plaintext in for the first encryption pass.
    /// This may be called more than once.
    #[inline(always)]
    pub fn encrypt_first_pass(&mut self, plaintext: &[u8]) {
        unsafe {
            CCCryptorGCMAddAAD(self.gmac, plaintext.as_ptr().cast(), plaintext.len());
        }
    }

    /// Finish first pass and begin second pass.
    #[inline(always)]
    pub fn encrypt_first_pass_finish(&mut self) {
        unsafe {
            CCCryptorGCMFinalize(self.gmac, self.tmp.as_mut_ptr().cast(), 16);
            let tmp = self.tmp.as_mut_ptr().cast::<u64>();
            *self.tag.as_mut_ptr().cast::<u64>().offset(1) = *tmp ^ *tmp.offset(1);
            let mut data_out_written: usize = 0;
            CCCryptorUpdate(self.ecb_enc, self.tag.as_ptr().cast(), 16, self.tag.as_mut_ptr().cast(), 16, &mut data_out_written);
        }
        self.tmp.copy_from_slice(&self.tag);
        self.tmp[12] &= 0x7f;
        unsafe {
            if CCCryptorReset(self.ctr, self.tmp.as_ptr().cast()) != 0 {
                panic!("CCCryptorReset for CTR mode failed (old MacOS bug)");
            }
        }
    }

    /// Feed plaintext for second pass and write ciphertext to supplied buffer.
    /// This may be called more than once.
    #[inline(always)]
    pub fn encrypt_second_pass(&mut self, plaintext: &[u8], ciphertext: &mut [u8]) {
        unsafe {
            assert!(ciphertext.len() >= plaintext.len());
            let mut data_out_written: usize = 0;
            CCCryptorUpdate(self.ctr, plaintext.as_ptr().cast(), plaintext.len(), ciphertext.as_mut_ptr().cast(), ciphertext.len(), &mut data_out_written);
        }
    }

    /// Encrypt plaintext in place.
    /// This may be called more than once.
    #[inline(always)]
    pub fn encrypt_second_pass_in_place(&mut self, plaintext_to_ciphertext: &mut [u8]) {
        unsafe {
            let mut data_out_written: usize = 0;
            CCCryptorUpdate(self.ctr, plaintext_to_ciphertext.as_ptr().cast(), plaintext_to_ciphertext.len(), plaintext_to_ciphertext.as_mut_ptr().cast(), plaintext_to_ciphertext.len(), &mut data_out_written);
        }
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
        unsafe {
            if CCCryptorReset(self.ctr, self.tmp.as_ptr().cast()) != 0 {
                panic!("CCCryptorReset for CTR mode failed (old MacOS bug)");
            }
            let mut data_out_written: usize = 0;
            CCCryptorUpdate(self.ecb_dec, self.tag.as_ptr().cast(), 16, self.tag.as_mut_ptr().cast(), 16, &mut data_out_written);
            let tmp = self.tmp.as_mut_ptr().cast::<u64>();
            *tmp = *self.tag.as_mut_ptr().cast::<u64>();
            *tmp.offset(1) = 0;
            CCCryptorGCMSetIV(self.gmac, self.tmp.as_ptr().cast(), 16);
        }
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
        unsafe {
            let mut data_out_written: usize = 0;
            CCCryptorUpdate(self.ctr, ciphertext.as_ptr().cast(), ciphertext.len(), plaintext.as_mut_ptr().cast(), plaintext.len(), &mut data_out_written);
            CCCryptorGCMAddAAD(self.gmac, plaintext.as_ptr().cast(), plaintext.len());
        }
    }

    /// Decrypt ciphertext in place.
    /// This may be called more than once.
    #[inline(always)]
    pub fn decrypt_in_place(&mut self, ciphertext_to_plaintext: &mut [u8]) {
        unsafe {
            let mut data_out_written: usize = 0;
            CCCryptorUpdate(self.ctr, ciphertext_to_plaintext.as_ptr().cast(), ciphertext_to_plaintext.len(), ciphertext_to_plaintext.as_mut_ptr().cast(), ciphertext_to_plaintext.len(), &mut data_out_written);
            CCCryptorGCMAddAAD(self.gmac, ciphertext_to_plaintext.as_ptr().cast(), ciphertext_to_plaintext.len());
        }
    }

    /// Finish decryption and return true if authentication appears valid.
    /// If this returns false the message should be dropped.
    #[inline(always)]
    pub fn decrypt_finish(&mut self) -> bool {
        unsafe {
            CCCryptorGCMFinalize(self.gmac, self.tmp.as_mut_ptr().cast(), 16);
            let tmp = self.tmp.as_mut_ptr().cast::<u64>();
            *self.tag.as_mut_ptr().cast::<u64>().offset(1) == *tmp ^ *tmp.offset(1)
        }
    }
}
