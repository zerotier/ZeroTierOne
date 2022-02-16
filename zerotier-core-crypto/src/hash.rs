/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::ffi::c_void;
use std::io::Write;
use std::mem::MaybeUninit;
use std::os::raw::{c_int, c_uint};
use std::ptr::null;

pub const SHA512_HASH_SIZE: usize = 64;
pub const SHA384_HASH_SIZE: usize = 48;

pub struct SHA512(Option<openssl::sha::Sha512>);

impl SHA512 {
    #[inline(always)]
    pub fn hash(b: &[u8]) -> [u8; SHA512_HASH_SIZE] {
        openssl::sha::sha512(b)
    }

    #[inline(always)]
    pub fn new() -> Self {
        Self(Some(openssl::sha::Sha512::new()))
    }

    #[inline(always)]
    pub fn reset(&mut self) {
        let _ = self.0.replace(openssl::sha::Sha512::new());
    }

    #[inline(always)]
    pub fn update(&mut self, b: &[u8]) {
        self.0.as_mut().unwrap().update(b);
    }

    #[inline(always)]
    pub fn finish(&mut self) -> [u8; SHA512_HASH_SIZE] {
        self.0.take().unwrap().finish()
    }
}

impl Write for SHA512 {
    #[inline(always)]
    fn write(&mut self, b: &[u8]) -> std::io::Result<usize> {
        self.0.as_mut().unwrap().update(b);
        Ok(b.len())
    }

    #[inline(always)]
    fn flush(&mut self) -> std::io::Result<()> { Ok(()) }
}

pub struct SHA384(Option<openssl::sha::Sha384>);

impl SHA384 {
    #[inline(always)]
    pub fn hash(b: &[u8]) -> [u8; SHA384_HASH_SIZE] {
        openssl::sha::sha384(b)
    }

    #[inline(always)]
    pub fn new() -> Self {
        Self(Some(openssl::sha::Sha384::new()))
    }

    #[inline(always)]
    pub fn reset(&mut self) {
        let _ = self.0.replace(openssl::sha::Sha384::new());
    }

    #[inline(always)]
    pub fn update(&mut self, b: &[u8]) {
        self.0.as_mut().unwrap().update(b);
    }

    #[inline(always)]
    pub fn finish(&mut self) -> [u8; SHA384_HASH_SIZE] {
        self.0.take().unwrap().finish()
    }
}

impl Write for SHA384 {
    #[inline(always)]
    fn write(&mut self, b: &[u8]) -> std::io::Result<usize> {
        self.0.as_mut().unwrap().update(b);
        Ok(b.len())
    }

    #[inline(always)]
    fn flush(&mut self) -> std::io::Result<()> { Ok(()) }
}

//#[link(name="crypto")]
extern "C" {
    fn HMAC_CTX_new() -> *mut c_void;
    fn HMAC_CTX_reset(ctx: *mut c_void) -> c_int;
    fn HMAC_Init_ex(ctx: *mut c_void, key: *const c_void, key_len: c_int, evp_md: *const c_void, _impl: *const c_void) -> c_int;
    fn HMAC_Update(ctx: *mut c_void, data: *const c_void, len: usize) -> c_int;
    fn HMAC_Final(ctx: *mut c_void, output: *mut c_void, output_len: *mut c_uint) -> c_int;
    fn HMAC_CTX_free(ctx: *mut c_void);
    fn EVP_sha384() -> *const c_void;
    fn EVP_sha512() -> *const c_void;
}

pub struct HMACSHA512 {
    ctx: *mut c_void,
    evp_md: *const c_void
}

impl HMACSHA512 {
    #[inline(always)]
    pub fn new(key: &[u8]) -> Self {
        unsafe {
            let hm = Self {
                ctx: HMAC_CTX_new(),
                evp_md: EVP_sha512()
            };
            assert!(!hm.ctx.is_null());
            assert_ne!(HMAC_Init_ex(hm.ctx, key.as_ptr().cast(), key.len() as c_int, hm.evp_md, null()), 0);
            hm
        }

    }

    #[inline(always)]
    pub fn reset(&mut self) {
        unsafe {
            assert_ne!(HMAC_CTX_reset(self.ctx), 0);
        }
    }

    #[inline(always)]
    pub fn update(&mut self, b: &[u8]) {
        unsafe {
            assert_ne!(HMAC_Update(self.ctx, b.as_ptr().cast(), b.len()), 0);
        }
    }

    #[inline(always)]
    pub fn finish_into(&mut self, md: &mut [u8]) {
        unsafe {
            assert_eq!(md.len(), 64);
            let mut mdlen: c_uint = 64;
            assert_ne!(HMAC_Final(self.ctx, md.as_mut_ptr().cast(), &mut mdlen), 0);
            assert_eq!(mdlen, 64);
        }
    }

    #[inline(always)]
    pub fn finish(&mut self) -> [u8; 64] {
        unsafe {
            let mut tmp: [u8; 64] = MaybeUninit::uninit().assume_init();
            self.finish_into(&mut tmp);
            tmp
        }
    }
}

impl Drop for HMACSHA512 {
    #[inline(always)]
    fn drop(&mut self) {
        unsafe { HMAC_CTX_free(self.ctx) };
    }
}

unsafe impl Send for HMACSHA512 {}

pub struct HMACSHA384 {
    ctx: *mut c_void,
    evp_md: *const c_void
}

impl HMACSHA384 {
    #[inline(always)]
    pub fn new(key: &[u8]) -> Self {
        unsafe {
            let hm = Self {
                ctx: HMAC_CTX_new(),
                evp_md: EVP_sha384()
            };
            assert!(!hm.ctx.is_null());
            assert_ne!(HMAC_Init_ex(hm.ctx, key.as_ptr().cast(), key.len() as c_int, hm.evp_md, null()), 0);
            hm
        }

    }

    #[inline(always)]
    pub fn reset(&mut self) {
        unsafe {
            assert_ne!(HMAC_CTX_reset(self.ctx), 0);
        }
    }

    #[inline(always)]
    pub fn update(&mut self, b: &[u8]) {
        unsafe {
            assert_ne!(HMAC_Update(self.ctx, b.as_ptr().cast(), b.len()), 0);
        }
    }

    #[inline(always)]
    pub fn finish_into(&mut self, md: &mut [u8]) {
        unsafe {
            assert_eq!(md.len(), 48);
            let mut mdlen: c_uint = 48;
            assert_ne!(HMAC_Final(self.ctx, md.as_mut_ptr().cast(), &mut mdlen), 0);
            assert_eq!(mdlen, 48);
        }
    }

    #[inline(always)]
    pub fn finish(&mut self) -> [u8; 48] {
        unsafe {
            let mut tmp: [u8; 48] = MaybeUninit::uninit().assume_init();
            self.finish_into(&mut tmp);
            tmp
        }
    }
}

impl Drop for HMACSHA384 {
    #[inline(always)]
    fn drop(&mut self) {
        unsafe { HMAC_CTX_free(self.ctx) };
    }
}

unsafe impl Send for HMACSHA384 {}

#[inline(always)]
pub fn hmac_sha512(key: &[u8], msg: &[u8]) -> [u8; 64] {
    let mut hm = HMACSHA512::new(key);
    hm.update(msg);
    hm.finish()
}

#[inline(always)]
pub fn hmac_sha384(key: &[u8], msg: &[u8]) -> [u8; 48] {
    let mut hm = HMACSHA384::new(key);
    hm.update(msg);
    hm.finish()
}
