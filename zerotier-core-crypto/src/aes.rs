// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

// MacOS implementation of AES primitives since CommonCrypto seems to be faster than OpenSSL, especially on ARM64.
#[cfg(target_os = "macos")]
mod fruit_flavored {
    use std::os::raw::{c_int, c_void};
    use std::ptr::{null, null_mut};

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
        fn CCCryptorUpdate(cryptor_ref: *mut c_void, data_in: *const c_void, data_in_len: usize, data_out: *mut c_void, data_out_len: usize, data_out_written: *mut usize) -> i32;
        fn CCCryptorReset(cryptor_ref: *mut c_void, iv: *const c_void) -> i32;
        fn CCCryptorRelease(cryptor_ref: *mut c_void) -> i32;
        fn CCCryptorGCMSetIV(cryptor_ref: *mut c_void, iv: *const c_void, iv_len: usize) -> i32;
        fn CCCryptorGCMAddAAD(cryptor_ref: *mut c_void, aad: *const c_void, len: usize) -> i32;
        fn CCCryptorGCMFinalize(cryptor_ref: *mut c_void, tag: *mut c_void, tag_len: usize) -> i32;
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
                let enc = CCCryptorCreateWithMode(kCCEncrypt, kCCModeECB, kCCAlgorithmAES, 0, null(), k.as_ptr().cast(), k.len(), null(), 0, 0, kCCOptionECBMode, &mut aes.0);
                if enc != 0 {
                    panic!("CCCryptorCreateWithMode for ECB encrypt mode returned {}", enc);
                }
                let dec = CCCryptorCreateWithMode(kCCDecrypt, kCCModeECB, kCCAlgorithmAES, 0, null(), k.as_ptr().cast(), k.len(), null(), 0, 0, kCCOptionECBMode, &mut aes.1);
                if dec != 0 {
                    panic!("CCCryptorCreateWithMode for ECB decrypt mode returned {}", dec);
                }
                aes
            }
        }

        #[inline(always)]
        pub fn encrypt_block(&self, plaintext: &[u8], ciphertext: &mut [u8]) {
            assert_eq!(plaintext.len(), 16);
            assert_eq!(ciphertext.len(), 16);
            unsafe {
                let mut data_out_written = 0;
                CCCryptorUpdate(self.0, plaintext.as_ptr().cast(), 16, ciphertext.as_mut_ptr().cast(), 16, &mut data_out_written);
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
                CCCryptorUpdate(self.1, ciphertext.as_ptr().cast(), 16, plaintext.as_mut_ptr().cast(), 16, &mut data_out_written);
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
                let result = CCCryptorCreateWithMode(kCCEncrypt, kCCModeCTR, kCCAlgorithmAES, 0, [0_u64; 2].as_ptr().cast(), k.as_ptr().cast(), k.len(), null(), 0, 0, 0, &mut ptr);
                if result != 0 {
                    panic!("CCCryptorCreateWithMode for CTR mode returned {}", result);
                }
                AesCtr(ptr)
            }
        }

        /// Initialize AES-CTR for encryption or decryption with the given IV.
        /// If it's already been used, this also resets the cipher. There is no separate reset.
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

    unsafe impl Send for AesCtr {}

    pub struct AesGcm(*mut c_void);

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
                let result = CCCryptorCreateWithMode(
                    if encrypt { kCCEncrypt } else { kCCDecrypt },
                    kCCModeGCM,
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
                    panic!("CCCryptorCreateWithMode for GCM mode returned {}", result);
                }
                AesGcm(ptr)
            }
        }

        #[inline(always)]
        pub fn init(&mut self, iv: &[u8]) {
            assert_eq!(iv.len(), 12);
            unsafe {
                assert_eq!(CCCryptorGCMReset(self.0), 0);
                CCCryptorGCMSetIV(self.0, iv.as_ptr().cast(), 12);
            }
        }

        #[inline(always)]
        pub fn aad(&mut self, aad: &[u8]) {
            unsafe {
                CCCryptorGCMAddAAD(self.0, aad.as_ptr().cast(), aad.len());
            }
        }

        #[inline(always)]
        pub fn crypt(&mut self, input: &[u8], output: &mut [u8]) {
            assert_eq!(input.len(), output.len());
            unsafe {
                let mut data_out_written: usize = 0;
                CCCryptorUpdate(self.0, input.as_ptr().cast(), input.len(), output.as_mut_ptr().cast(), output.len(), &mut data_out_written);
            }
        }

        #[inline(always)]
        pub fn finish(&mut self) -> [u8; 16] {
            let mut tag = [0_u8; 16];
            unsafe {
                if CCCryptorGCMFinalize(self.0, tag.as_mut_ptr().cast(), 16) != 0 {
                    tag.fill(0);
                }
            }
            tag
        }
    }

    unsafe impl Send for AesGcm {}
}

#[cfg(not(target_os = "macos"))]
mod openssl {
    use crate::secret::Secret;
    use openssl::symm::{Cipher, Crypter, Mode};
    use std::cell::UnsafeCell;

    #[inline(always)]
    fn aes_ctr_by_key_size(ks: usize) -> Cipher {
        match ks {
            16 => Cipher::aes_128_ctr(),
            24 => Cipher::aes_192_ctr(),
            32 => Cipher::aes_256_ctr(),
            _ => {
                panic!("AES supports 128, 192, or 256 bits keys");
            }
        }
    }

    #[inline(always)]
    fn aes_gcm_by_key_size(ks: usize) -> Cipher {
        match ks {
            16 => Cipher::aes_128_gcm(),
            24 => Cipher::aes_192_gcm(),
            32 => Cipher::aes_256_gcm(),
            _ => {
                panic!("AES supports 128, 192, or 256 bits keys");
            }
        }
    }

    #[inline(always)]
    fn aes_ecb_by_key_size(ks: usize) -> Cipher {
        match ks {
            16 => Cipher::aes_128_ecb(),
            24 => Cipher::aes_192_ecb(),
            32 => Cipher::aes_256_ecb(),
            _ => {
                panic!("AES supports 128, 192, or 256 bits keys");
            }
        }
    }

    pub struct Aes(UnsafeCell<Crypter>, UnsafeCell<Crypter>);

    impl Aes {
        #[inline(always)]
        pub fn new(k: &[u8]) -> Self {
            let (mut c, mut d) =
                (Crypter::new(aes_ecb_by_key_size(k.len()), Mode::Encrypt, k, None).unwrap(), Crypter::new(aes_ecb_by_key_size(k.len()), Mode::Decrypt, k, None).unwrap());
            c.pad(false);
            d.pad(false);
            Self(UnsafeCell::new(c), UnsafeCell::new(d))
        }

        #[inline(always)]
        pub fn encrypt_block(&self, plaintext: &[u8], ciphertext: &mut [u8]) {
            let mut tmp = [0_u8; 32];
            let c: &mut Crypter = unsafe { &mut *self.0.get() };
            if c.update(plaintext, &mut tmp).unwrap() != 16 {
                assert_eq!(c.finalize(&mut tmp).unwrap(), 16);
            }
            ciphertext[..16].copy_from_slice(&tmp[..16]);
        }

        #[inline(always)]
        pub fn encrypt_block_in_place(&self, data: &mut [u8]) {
            let mut tmp = [0_u8; 32];
            let c: &mut Crypter = unsafe { &mut *self.0.get() };
            if c.update(data, &mut tmp).unwrap() != 16 {
                assert_eq!(c.finalize(&mut tmp).unwrap(), 16);
            }
            data[..16].copy_from_slice(&tmp[..16]);
        }

        #[inline(always)]
        pub fn decrypt_block(&self, ciphertext: &[u8], plaintext: &mut [u8]) {
            let mut tmp = [0_u8; 32];
            let c: &mut Crypter = unsafe { &mut *self.1.get() };
            if c.update(plaintext, &mut tmp).unwrap() != 16 {
                assert_eq!(c.finalize(&mut tmp).unwrap(), 16);
            }
            plaintext[..16].copy_from_slice(&tmp[..16]);
        }

        #[inline(always)]
        pub fn decrypt_block_in_place(&self, data: &mut [u8]) {
            let mut tmp = [0_u8; 32];
            let c: &mut Crypter = unsafe { &mut *self.1.get() };
            if c.update(data, &mut tmp).unwrap() != 16 {
                assert_eq!(c.finalize(&mut tmp).unwrap(), 16);
            }
            data[..16].copy_from_slice(&tmp[..16]);
        }
    }

    unsafe impl Send for Aes {}
    unsafe impl Sync for Aes {}

    pub struct AesCtr(Secret<32>, usize, Option<Crypter>);

    impl AesCtr {
        /// Construct a new AES-CTR cipher.
        /// Key must be 16, 24, or 32 bytes in length or a panic will occur.
        #[inline(always)]
        pub fn new(k: &[u8]) -> Self {
            let mut s: Secret<32> = Secret::default();
            match k.len() {
                16 | 24 | 32 => {
                    s.0[..k.len()].copy_from_slice(k);
                    Self(s, k.len(), None)
                }
                _ => {
                    panic!("AES supports 128, 192, or 256 bits keys");
                }
            }
        }

        /// Initialize AES-CTR for encryption or decryption with the given IV.
        /// If it's already been used, this also resets the cipher. There is no separate reset.
        #[inline(always)]
        pub fn init(&mut self, iv: &[u8]) {
            let mut c = Crypter::new(aes_ctr_by_key_size(self.1), Mode::Encrypt, &self.0 .0[..self.1], Some(iv)).unwrap();
            c.pad(false);
            let _ = self.2.replace(c);
        }

        /// Encrypt or decrypt (same operation with CTR mode)
        #[inline(always)]
        pub fn crypt(&mut self, input: &[u8], output: &mut [u8]) {
            let _ = self.2.as_mut().unwrap().update(input, output);
        }

        /// Encrypt or decrypt in place (same operation with CTR mode)
        #[inline(always)]
        pub fn crypt_in_place(&mut self, data: &mut [u8]) {
            let _ = self.2.as_mut().unwrap().update(unsafe { &*std::slice::from_raw_parts(data.as_ptr(), data.len()) }, data);
        }
    }

    unsafe impl Send for AesCtr {}

    pub struct AesGcm(Secret<32>, usize, Option<Crypter>, bool);

    impl AesGcm {
        /// Construct a new AES-GCM cipher.
        /// Key must be 16, 24, or 32 bytes in length or a panic will occur.
        #[inline(always)]
        pub fn new(k: &[u8], encrypt: bool) -> Self {
            let mut s: Secret<32> = Secret::default();
            match k.len() {
                16 | 24 | 32 => {
                    s.0[..k.len()].copy_from_slice(k);
                    Self(s, k.len(), None, encrypt)
                }
                _ => {
                    panic!("AES supports 128, 192, or 256 bits keys");
                }
            }
        }

        /// Initialize AES-CTR for encryption or decryption with the given IV.
        /// If it's already been used, this also resets the cipher. There is no separate reset.
        #[inline(always)]
        pub fn init(&mut self, iv: &[u8]) {
            assert_eq!(iv.len(), 12);
            let mut c = Crypter::new(aes_gcm_by_key_size(self.1), if self.3 { Mode::Encrypt } else { Mode::Decrypt }, &self.0 .0[..self.1], Some(iv)).unwrap();
            c.pad(false);
            let _ = self.2.replace(c);
        }

        #[inline(always)]
        pub fn aad(&mut self, aad: &[u8]) {
            let _ = self.2.as_mut().unwrap().aad_update(aad);
        }

        /// Encrypt or decrypt (same operation with CTR mode)
        #[inline(always)]
        pub fn crypt(&mut self, input: &[u8], output: &mut [u8]) {
            let _ = self.2.as_mut().unwrap().update(input, output);
        }

        /// Encrypt or decrypt in place (same operation with CTR mode)
        #[inline(always)]
        pub fn crypt_in_place(&mut self, data: &mut [u8]) {
            let _ = self.2.as_mut().unwrap().update(unsafe { &*std::slice::from_raw_parts(data.as_ptr(), data.len()) }, data);
        }

        #[inline(always)]
        pub fn finish(&mut self) -> [u8; 16] {
            let mut tag = [0_u8; 16];
            let c = self.2.as_mut().unwrap();
            if c.finalize(&mut []).is_ok() {
                if !c.get_tag(&mut tag).is_ok() {
                    tag.fill(0);
                }
            }
            tag
        }
    }

    unsafe impl Send for AesGcm {}
}

#[cfg(target_os = "macos")]
pub use fruit_flavored::{Aes, AesCtr, AesGcm};

#[cfg(not(target_os = "macos"))]
pub use openssl::{Aes, AesCtr, AesGcm};
