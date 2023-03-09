// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

// MacOS implementation of AES primitives since CommonCrypto seems to be faster than OpenSSL, especially on ARM64.
#[cfg(target_os = "macos")]
mod fruit_flavored {
    use std::os::raw::{c_int, c_void};
    use std::ptr::{null, null_mut};

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
        fn CCCryptorReset(cryptor_ref: *mut c_void, iv: *const c_void) -> i32;
        fn CCCryptorRelease(cryptor_ref: *mut c_void) -> i32;
        fn CCCryptorGCMSetIV(cryptor_ref: *mut c_void, iv: *const c_void, iv_len: usize) -> i32;
        fn CCCryptorGCMAddAAD(cryptor_ref: *mut c_void, aad: *const c_void, len: usize) -> i32;
        fn CCCryptorGCMEncrypt(cryptor_ref: *mut c_void, data_in: *const c_void, data_in_len: usize, data_out: *mut c_void) -> i32;
        fn CCCryptorGCMDecrypt(cryptor_ref: *mut c_void, data_in: *const c_void, data_in_len: usize, data_out: *mut c_void) -> i32;
        fn CCCryptorGCMFinal(cryptor_ref: *mut c_void, tag: *mut c_void, tag_len: *mut usize) -> i32;
        fn CCCryptorGCMReset(cryptor_ref: *mut c_void) -> i32;
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
        pub fn new(k: &[u8]) -> Self {
            unsafe {
                if k.len() != 32 && k.len() != 24 && k.len() != 16 {
                    panic!("AES supports 128, 192, or 256 bits keys");
                }
                let mut aes: Self = std::mem::zeroed();
                assert_eq!(
                    CCCryptorCreateWithMode(
                        kCCEncrypt,
                        kCCModeECB,
                        kCCAlgorithmAES,
                        0,
                        null(),
                        k.as_ptr().cast(),
                        k.len(),
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
                        k.len(),
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
        pub fn encrypt_block(&self, plaintext: &[u8], ciphertext: &mut [u8]) {
            assert_eq!(plaintext.len(), 16);
            assert_eq!(ciphertext.len(), 16);
            unsafe {
                let mut data_out_written = 0;
                CCCryptorUpdate(
                    self.0,
                    plaintext.as_ptr().cast(),
                    16,
                    ciphertext.as_mut_ptr().cast(),
                    16,
                    &mut data_out_written,
                );
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
        pub fn decrypt_block(&self, ciphertext: &[u8], plaintext: &mut [u8]) {
            assert_eq!(plaintext.len(), 16);
            assert_eq!(ciphertext.len(), 16);
            unsafe {
                let mut data_out_written = 0;
                CCCryptorUpdate(
                    self.1,
                    ciphertext.as_ptr().cast(),
                    16,
                    plaintext.as_mut_ptr().cast(),
                    16,
                    &mut data_out_written,
                );
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

    unsafe impl Send for Aes {}
    unsafe impl Sync for Aes {}

    pub struct AesCtr(*mut c_void);

    impl Drop for AesCtr {
        #[inline(always)]
        fn drop(&mut self) {
            unsafe { CCCryptorRelease(self.0) };
        }
    }

    impl AesCtr {
        /// Construct a new AES-CTR cipher.
        /// Key must be 16, 24, or 32 bytes in length or a panic will occur.
        pub fn new(k: &[u8]) -> Self {
            if k.len() != 32 && k.len() != 24 && k.len() != 16 {
                panic!("AES supports 128, 192, or 256 bits keys");
            }
            unsafe {
                let mut ptr: *mut c_void = null_mut();
                let result = CCCryptorCreateWithMode(
                    kCCEncrypt,
                    kCCModeCTR,
                    kCCAlgorithmAES,
                    0,
                    [0_u64; 2].as_ptr().cast(),
                    k.as_ptr().cast(),
                    k.len(),
                    null(),
                    0,
                    0,
                    0,
                    &mut ptr,
                );
                if result != 0 {
                    panic!("CCCryptorCreateWithMode for CTR mode returned {}", result);
                }
                AesCtr(ptr)
            }
        }

        /// Initialize AES-CTR for encryption or decryption with the given IV.
        /// If it's already been used, this also resets the cipher. There is no separate reset.
        pub fn reset_set_iv(&mut self, iv: &[u8]) {
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
                CCCryptorUpdate(
                    self.0,
                    input.as_ptr().cast(),
                    input.len(),
                    output.as_mut_ptr().cast(),
                    output.len(),
                    &mut data_out_written,
                );
            }
        }

        /// Encrypt or decrypt in place (same operation with CTR mode)
        #[inline(always)]
        pub fn crypt_in_place(&mut self, data: &mut [u8]) {
            unsafe {
                let mut data_out_written: usize = 0;
                CCCryptorUpdate(
                    self.0,
                    data.as_ptr().cast(),
                    data.len(),
                    data.as_mut_ptr().cast(),
                    data.len(),
                    &mut data_out_written,
                );
            }
        }
    }

    unsafe impl Send for AesCtr {}

    pub struct AesGcm(*mut c_void, bool);

    impl Drop for AesGcm {
        #[inline(always)]
        fn drop(&mut self) {
            unsafe { CCCryptorRelease(self.0) };
        }
    }

    impl AesGcm {
        pub fn new(k: &[u8], encrypt: bool) -> Self {
            if k.len() != 32 && k.len() != 24 && k.len() != 16 {
                panic!("AES supports 128, 192, or 256 bits keys");
            }
            unsafe {
                let mut ptr: *mut c_void = null_mut();
                assert_eq!(
                    CCCryptorCreateWithMode(
                        if encrypt {
                            kCCEncrypt
                        } else {
                            kCCDecrypt
                        },
                        kCCModeGCM,
                        kCCAlgorithmAES,
                        0,
                        null(),
                        k.as_ptr().cast(),
                        k.len(),
                        null(),
                        0,
                        0,
                        0,
                        &mut ptr,
                    ),
                    0
                );
                AesGcm(ptr, encrypt)
            }
        }

        #[inline(always)]
        pub fn reset_init_gcm(&mut self, iv: &[u8]) {
            assert_eq!(iv.len(), 12);
            unsafe {
                assert_eq!(CCCryptorGCMReset(self.0), 0);
                assert_eq!(CCCryptorGCMSetIV(self.0, iv.as_ptr().cast(), 12), 0);
            }
        }

        #[inline(always)]
        pub fn aad(&mut self, aad: &[u8]) {
            unsafe {
                assert_eq!(CCCryptorGCMAddAAD(self.0, aad.as_ptr().cast(), aad.len()), 0);
            }
        }

        #[inline(always)]
        pub fn crypt(&mut self, input: &[u8], output: &mut [u8]) {
            unsafe {
                assert_eq!(input.len(), output.len());
                if self.1 {
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
        pub fn crypt_in_place(&mut self, data: &mut [u8]) {
            unsafe {
                if self.1 {
                    assert_eq!(CCCryptorGCMEncrypt(self.0, data.as_ptr().cast(), data.len(), data.as_mut_ptr().cast()), 0);
                } else {
                    assert_eq!(CCCryptorGCMDecrypt(self.0, data.as_ptr().cast(), data.len(), data.as_mut_ptr().cast()), 0);
                }
            }
        }

        #[inline(always)]
        pub fn finish_encrypt(&mut self) -> [u8; 16] {
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

        #[inline(always)]
        pub fn finish_decrypt(&mut self, expected_tag: &[u8]) -> bool {
            secure_eq(&self.finish_encrypt(), expected_tag)
        }
    }

    unsafe impl Send for AesGcm {}
}
