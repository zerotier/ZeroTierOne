// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

// MacOS implementation of AES primitives since CommonCrypto seems to be faster than OpenSSL, especially on ARM64.
use std::os::raw::{c_int, c_void};
use std::ptr::{null, null_mut};

use crate::secret::Secret;
use crate::secure_eq;

#[allow(non_upper_case_globals, unused)]
const kCCModeECB: i32 = 1;
#[allow(non_upper_case_globals, unused)]
const kCCModeCTR: i32 = 4;
#[allow(non_upper_case_globals, unused)]
const kCCModeGCM: i32 = 11;
#[allow(non_upper_case_globals, unused)]
const kCCEncrypt: i32 = 0;
#[allow(non_upper_case_globals, unused)]
const kCCDecrypt: i32 = 1;
#[allow(non_upper_case_globals, unused)]
const kCCAlgorithmAES: i32 = 0;
#[allow(non_upper_case_globals, unused)]
const kCCOptionECBMode: i32 = 2;

extern "C" {
    fn CCCryptorCreateWithMode(
        op: i32,
        mode: i32,
        alg: i32,
        padding: i32,
        iv: *const c_void,
        key: *const c_void,
        key_len: usize,
        tweak: *const c_void,
        tweak_len: usize,
        num_rounds: c_int,
        options: i32,
        cryyptor_ref: *mut *mut c_void,
    ) -> i32;
    fn CCCryptorUpdate(
        cryptor_ref: *mut c_void,
        data_in: *const c_void,
        data_in_len: usize,
        data_out: *mut c_void,
        data_out_len: usize,
        data_out_written: *mut usize,
    ) -> i32;
    //fn CCCryptorReset(cryptor_ref: *mut c_void, iv: *const c_void) -> i32;
    fn CCCryptorRelease(cryptor_ref: *mut c_void) -> i32;
    fn CCCryptorGCMSetIV(cryptor_ref: *mut c_void, iv: *const c_void, iv_len: usize) -> i32;
    fn CCCryptorGCMAddAAD(cryptor_ref: *mut c_void, aad: *const c_void, len: usize) -> i32;
    fn CCCryptorGCMEncrypt(cryptor_ref: *mut c_void, data_in: *const c_void, data_in_len: usize, data_out: *mut c_void) -> i32;
    fn CCCryptorGCMDecrypt(cryptor_ref: *mut c_void, data_in: *const c_void, data_in_len: usize, data_out: *mut c_void) -> i32;
    fn CCCryptorGCMFinal(cryptor_ref: *mut c_void, tag: *mut c_void, tag_len: *mut usize) -> i32;
    fn CCCryptorGCMReset(cryptor_ref: *mut c_void) -> i32;
}



pub struct AesGcm<const ENCRYPT: bool> (*mut c_void);

impl<const ENCRYPT: bool> Drop for AesGcm<ENCRYPT> {
    #[inline(always)]
    fn drop(&mut self) {
        unsafe { CCCryptorRelease(self.0) };
    }
}

impl<const ENCRYPT: bool> AesGcm<ENCRYPT> {
    pub fn new<const KEY_SIZE: usize>(k: &Secret<KEY_SIZE>) -> Self {
        debug_assert!(KEY_SIZE == 32 || KEY_SIZE == 24 || KEY_SIZE == 16, "AES supports 128, 192, or 256 bits keys");
        unsafe {
            let mut ptr: *mut c_void = null_mut();
            assert_eq!(
                CCCryptorCreateWithMode(
                    if ENCRYPT {
                        kCCEncrypt
                    } else {
                        kCCDecrypt
                    },
                    kCCModeGCM,
                    kCCAlgorithmAES,
                    0,
                    null(),
                    k.as_ptr().cast(),
                    KEY_SIZE,
                    null(),
                    0,
                    0,
                    0,
                    &mut ptr,
                ),
                0
            );
            AesGcm(ptr)
        }
    }

    #[inline(always)]
    pub fn reset_init_gcm(&self, iv: &[u8]) {
        assert_eq!(iv.len(), 12);
        unsafe {
            assert_eq!(CCCryptorGCMReset(self.0), 0);
            assert_eq!(CCCryptorGCMSetIV(self.0, iv.as_ptr().cast(), 12), 0);
        }
    }

    #[inline(always)]
    pub fn aad(&self, aad: &[u8]) {
        unsafe {
            assert_eq!(CCCryptorGCMAddAAD(self.0, aad.as_ptr().cast(), aad.len()), 0);
        }
    }

    #[inline(always)]
    pub fn crypt(&self, input: &[u8], output: &mut [u8]) {
        unsafe {
            assert_eq!(input.len(), output.len());
            if ENCRYPT {
                assert_eq!(
                    CCCryptorGCMEncrypt(self.0, input.as_ptr().cast(), input.len(), output.as_mut_ptr().cast()),
                    0
                );
            } else {
                assert_eq!(
                    CCCryptorGCMDecrypt(self.0, input.as_ptr().cast(), input.len(), output.as_mut_ptr().cast()),
                    0
                );
            }
        }
    }

    #[inline(always)]
    pub fn crypt_in_place(&self, data: &mut [u8]) {
        unsafe {
            if ENCRYPT {
                assert_eq!(CCCryptorGCMEncrypt(self.0, data.as_ptr().cast(), data.len(), data.as_mut_ptr().cast()), 0);
            } else {
                assert_eq!(CCCryptorGCMDecrypt(self.0, data.as_ptr().cast(), data.len(), data.as_mut_ptr().cast()), 0);
            }
        }
    }

    #[inline(always)]
    fn finish(&self) -> [u8; 16] {
        let mut tag = 0_u128.to_ne_bytes();
        unsafe {
            let mut tag_len = 16;
            if CCCryptorGCMFinal(self.0, tag.as_mut_ptr().cast(), &mut tag_len) != 0 {
                debug_assert!(false);
                tag.fill(0);
            }
        }
        tag
    }

}

impl AesGcm<true> {
    /// Produce the gcm authentication tag.
    #[inline(always)]
    pub fn finish_encrypt(&self) -> [u8; 16] {
        self.finish()
    }
}
impl AesGcm<false> {
    /// Check the gcm authentication tag. Outputs true if it matches the just decrypted message, outputs false otherwise.
    #[inline(always)]
    pub fn finish_decrypt(&self, expected_tag: &[u8]) -> bool {
        secure_eq(&self.finish(), expected_tag)
    }
}



pub struct Aes(*mut c_void, *mut c_void);

impl Drop for Aes {
    #[inline(always)]
    fn drop(&mut self) {
        unsafe {
            CCCryptorRelease(self.0);
            CCCryptorRelease(self.1);
        }
    }
}

impl Aes {
    pub fn new<const KEY_SIZE: usize>(k: &Secret<KEY_SIZE>) -> Self {
        unsafe {
            debug_assert!(KEY_SIZE == 32 || KEY_SIZE == 24 || KEY_SIZE == 16, "AES supports 128, 192, or 256 bits keys");
            let mut aes: Self = std::mem::zeroed();
            assert_eq!(
                CCCryptorCreateWithMode(
                    kCCEncrypt,
                    kCCModeECB,
                    kCCAlgorithmAES,
                    0,
                    null(),
                    k.as_ptr().cast(),
                    KEY_SIZE,
                    null(),
                    0,
                    0,
                    kCCOptionECBMode,
                    &mut aes.0
                ),
                0
            );
            assert_eq!(
                CCCryptorCreateWithMode(
                    kCCDecrypt,
                    kCCModeECB,
                    kCCAlgorithmAES,
                    0,
                    null(),
                    k.as_ptr().cast(),
                    KEY_SIZE,
                    null(),
                    0,
                    0,
                    kCCOptionECBMode,
                    &mut aes.1
                ),
                0
            );
            aes
        }
    }

    #[inline(always)]
    pub fn encrypt_block_in_place(&self, data: &mut [u8]) {
        assert_eq!(data.len(), 16);
        unsafe {
            let mut data_out_written = 0;
            CCCryptorUpdate(self.0, data.as_ptr().cast(), 16, data.as_mut_ptr().cast(), 16, &mut data_out_written);
        }
    }

    #[inline(always)]
    pub fn decrypt_block_in_place(&self, data: &mut [u8]) {
        assert_eq!(data.len(), 16);
        unsafe {
            let mut data_out_written = 0;
            CCCryptorUpdate(self.1, data.as_ptr().cast(), 16, data.as_mut_ptr().cast(), 16, &mut data_out_written);
        }
    }
}
