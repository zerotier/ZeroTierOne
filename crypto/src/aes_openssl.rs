// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::{ptr, mem::MaybeUninit};

use foreign_types::ForeignType;

use crate::{secret::Secret, cipher_ctx::CipherCtx};

/// An OpenSSL AES_GCM context. Automatically frees itself on drop.
/// The current interface is custom made for ZeroTier, but could easily be adapted for other uses.
/// Whether `ENCRYPT` is true or false decides respectively whether this context encrypts or decrypts.
/// Even though OpenSSL lets you set this dynamically almost no operations work when you do this without resetting the context.
pub struct AesGcm<const ENCRYPT: bool> (CipherCtx);

impl<const ENCRYPT: bool> AesGcm<ENCRYPT> {
    /// Create an AesGcm context with the given key, key must be 16, 24 or 32 bytes long.
    /// OpenSSL internally processes and caches this key, so it is recommended to reuse this context whenever encrypting under the same key. Call `reset_init_gcm` to change the IV for each reuse.
    pub fn new<const KEY_SIZE: usize>(key: &Secret<KEY_SIZE>) -> Self {
        let ctx = CipherCtx::new().unwrap();
        unsafe {
            let t = match KEY_SIZE {
                16 => ffi::EVP_aes_128_gcm(),
                24 => ffi::EVP_aes_192_gcm(),
                32 => ffi::EVP_aes_256_gcm(),
                _ => panic!("Aes KEY_SIZE must be 16, 24 or 32")
            };
            ctx.cipher_init::<ENCRYPT>(t, key.as_ptr(), ptr::null()).unwrap();
            ffi::EVP_CIPHER_CTX_set_padding(ctx.as_ptr(), 0);
        }
        let ret = AesGcm(ctx);
        ret
    }

    /// Set the IV of this AesGcm context. This call resets the IV but leaves the key and encryption algorithm alone.
    /// This method must be called before any other method on AesGcm.
    /// `iv` must be exactly 12 bytes in length, because that is what Aes supports.
    pub fn reset_init_gcm(&mut self, iv: &[u8]) {
        debug_assert_eq!(iv.len(), 12, "Aes IV must be 12 bytes long");
        unsafe {
            self.0.cipher_init::<ENCRYPT>(ptr::null(), ptr::null(), iv.as_ptr()).unwrap();
        }
    }

    /// Add additional authentication data to AesGcm (same operation with CTR mode).
    #[inline(always)]
    pub fn aad(&mut self, aad: &[u8]) {
        unsafe { self.0.update::<ENCRYPT>(aad, ptr::null_mut()).unwrap() };
    }

    /// Encrypt or decrypt (same operation with CTR mode)
    #[inline(always)]
    pub fn crypt(&mut self, input: &[u8], output: &mut [u8]) {
        debug_assert!(output.len() >= input.len(), "output buffer must fit the size of the input buffer");
        unsafe { self.0.update::<ENCRYPT>(input, output.as_mut_ptr()).unwrap() };
    }

    /// Encrypt or decrypt in place (same operation with CTR mode).
    #[inline(always)]
    pub fn crypt_in_place(&mut self, data: &mut [u8]) {
        let ptr = data.as_mut_ptr();
        unsafe { self.0.update::<ENCRYPT>(data, ptr).unwrap() }
    }
}
impl AesGcm<true> {
    /// Produce the gcm authentication tag.
    #[inline(always)]
    pub fn finish_encrypt(&mut self) -> [u8; 16] {
        unsafe {
            let mut tag = MaybeUninit::<[u8; 16]>::uninit();
            self.0.finalize::<true>(tag.as_mut_ptr().cast()).unwrap();
            self.0.tag(&mut *tag.as_mut_ptr()).unwrap();
            tag.assume_init()
        }
    }
}
impl AesGcm<false> {
    /// Check the gcm authentication tag. Outputs true if it matches the just decrypted message, outputs false otherwise.
    #[inline(always)]
    pub fn finish_decrypt(&mut self, expected_tag: &[u8]) -> bool {
        debug_assert_eq!(expected_tag.len(), 16);
        if self.0.set_tag(expected_tag).is_ok() {
            unsafe { self.0.finalize::<false>(ptr::null_mut()).is_ok() }
        } else {
            false
        }
    }
}

const AES_BLOCK_SIZE: usize = 16;

/// An OpenSSL AES_ECB context. Automatically frees itself on drop.
/// AES_ECB is very insecure if used incorrectly so its public interface supports only exactly what ZeroTier uses it for.
pub struct Aes(CipherCtx, CipherCtx);

impl Aes {
    /// Create an AesEcb context with the given key, key must be 16, 24 or 32 bytes long.
    /// OpenSSL internally processes and caches this key, so it is recommended to reuse this context whenever encrypting under the same key.
    pub fn new<const KEY_SIZE: usize>(key: &Secret<KEY_SIZE>) -> Self {
        let ctx0 = CipherCtx::new().unwrap();
        let ctx1 = CipherCtx::new().unwrap();
        unsafe {
            let t = match KEY_SIZE {
                16 => ffi::EVP_aes_128_ecb(),
                24 => ffi::EVP_aes_192_ecb(),
                32 => ffi::EVP_aes_256_ecb(),
                _ => panic!("Aes KEY_SIZE must be 16, 24 or 32")
            };
            ctx0.cipher_init::<true>(t, key.as_ptr(), ptr::null()).unwrap();
            ffi::EVP_CIPHER_CTX_set_padding(ctx0.as_ptr(), 0);
            ctx1.cipher_init::<false>(t, key.as_ptr(), ptr::null()).unwrap();
            ffi::EVP_CIPHER_CTX_set_padding(ctx1.as_ptr(), 0);
        }
        let ret = Aes(ctx0, ctx1);
        ret
    }

    /// Do not ever encrypt the same plaintext twice. Make sure data is always different between calls.
    #[inline(always)]
    pub fn encrypt_block_in_place(&self, data: &mut [u8]) {
        debug_assert_eq!(data.len(), AES_BLOCK_SIZE, "AesEcb should not be used to encrypt more than one block at a time unless you really know what you are doing.");
        let ptr = data.as_mut_ptr();
        unsafe { self.0.update::<true>(data, ptr).unwrap() }
    }
    /// Do not ever encrypt the same plaintext twice. Make sure data is always different between calls.
    #[inline(always)]
    pub fn decrypt_block_in_place(&self, data: &mut [u8]) {
        debug_assert_eq!(data.len(), AES_BLOCK_SIZE, "AesEcb should not be used to encrypt more than one block at a time unless you really know what you are doing.");
        let ptr = data.as_mut_ptr();
        unsafe { self.1.update::<false>(data, ptr).unwrap() }
    }
}
