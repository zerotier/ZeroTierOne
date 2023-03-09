// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::ffi::c_void;
use std::io::Write;
use std::mem::MaybeUninit;
use std::os::raw::{c_int, c_uint};
use std::ptr::null;

use crate::secret::Secret;

pub const SHA512_HASH_SIZE: usize = 64;
pub const SHA384_HASH_SIZE: usize = 48;
pub const HMAC_SHA512_SIZE: usize = 64;
pub const HMAC_SHA384_SIZE: usize = 48;

pub struct SHA512(ffi::SHA512_CTX);

impl SHA512 {
    #[inline(always)]
    pub fn hash(data: &[u8]) -> [u8; SHA512_HASH_SIZE] {
        unsafe {
            let mut hash = MaybeUninit::<[u8; SHA512_HASH_SIZE]>::uninit();
            ffi::SHA512(data.as_ptr(), data.len(), hash.as_mut_ptr() as *mut _);
            hash.assume_init()
        }
    }

    /// Creates a new hasher.
    #[inline(always)]
    pub fn new() -> Self {
        unsafe {
            let mut ctx = MaybeUninit::uninit();
            ffi::SHA512_Init(ctx.as_mut_ptr());
            SHA512(ctx.assume_init())
        }
    }

    /// Feeds some data into the hasher.
    ///
    /// This can be called multiple times.
    #[inline(always)]
    pub fn reset(&mut self) {
        unsafe { ffi::SHA512_Init(&mut self.0) };
    }

    #[inline(always)]
    pub fn update(&mut self, buf: &[u8]) {
        unsafe {
            ffi::SHA512_Update(&mut self.0, buf.as_ptr() as *const c_void, buf.len());
        }
    }

    /// Returns the hash of the data.
    #[inline(always)]
    pub fn finish(&mut self) -> [u8; SHA512_HASH_SIZE] {
        unsafe {
            let mut hash = MaybeUninit::<[u8; SHA512_HASH_SIZE]>::uninit();
            ffi::SHA512_Final(hash.as_mut_ptr() as *mut _, &mut self.0);
            hash.assume_init()
        }
    }
}

impl Write for SHA512 {
    #[inline(always)]
    fn write(&mut self, b: &[u8]) -> std::io::Result<usize> {
        self.update(b);
        Ok(b.len())
    }

    #[inline(always)]
    fn flush(&mut self) -> std::io::Result<()> {
        Ok(())
    }
}

unsafe impl Send for SHA512 {}

pub struct SHA384(ffi::SHA512_CTX);

impl SHA384 {
    #[inline(always)]
    pub fn hash(data: &[u8]) -> [u8; SHA384_HASH_SIZE] {
        unsafe {
            let mut hash = MaybeUninit::<[u8; SHA384_HASH_SIZE]>::uninit();
            ffi::SHA384(data.as_ptr(), data.len(), hash.as_mut_ptr() as *mut _);
            hash.assume_init()
        }
    }

    #[inline(always)]
    pub fn new() -> Self {
        unsafe {
            let mut ctx = MaybeUninit::uninit();
            ffi::SHA384_Init(ctx.as_mut_ptr());
            SHA384(ctx.assume_init())
        }
    }

    #[inline(always)]
    pub fn reset(&mut self) {
        unsafe {
            ffi::SHA384_Init(&mut self.0);
        }
    }

    #[inline(always)]
    pub fn update(&mut self, buf: &[u8]) {
        unsafe {
            ffi::SHA384_Update(&mut self.0, buf.as_ptr() as *const c_void, buf.len());
        }
    }

    #[inline(always)]
    pub fn finish(&mut self) -> [u8; SHA384_HASH_SIZE] {
        unsafe {
            let mut hash = MaybeUninit::<[u8; SHA384_HASH_SIZE]>::uninit();
            ffi::SHA384_Final(hash.as_mut_ptr() as *mut _, &mut self.0);
            hash.assume_init()
        }
    }
}

impl Write for SHA384 {
    #[inline(always)]
    fn write(&mut self, b: &[u8]) -> std::io::Result<usize> {
        self.update(b);
        Ok(b.len())
    }

    #[inline(always)]
    fn flush(&mut self) -> std::io::Result<()> {
        Ok(())
    }
}

unsafe impl Send for SHA384 {}

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
    evp_md: *const c_void,
}

impl HMACSHA512 {
    #[inline(always)]
    pub fn new(key: &[u8]) -> Self {
        unsafe {
            let hm = Self { ctx: HMAC_CTX_new(), evp_md: EVP_sha512() };
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
            debug_assert_eq!(md.len(), HMAC_SHA512_SIZE);
            let mut mdlen = HMAC_SHA512_SIZE as c_uint;
            assert_ne!(HMAC_Final(self.ctx, md.as_mut_ptr().cast(), &mut mdlen), 0);
            debug_assert_eq!(mdlen, HMAC_SHA512_SIZE as c_uint);
        }
    }

    #[inline(always)]
    pub fn finish(&mut self) -> [u8; HMAC_SHA512_SIZE] {
        let mut tmp = [0u8; HMAC_SHA512_SIZE];
        self.finish_into(&mut tmp);
        tmp
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
    evp_md: *const c_void,
}

impl HMACSHA384 {
    #[inline(always)]
    pub fn new(key: &[u8]) -> Self {
        unsafe {
            let hm = Self { ctx: HMAC_CTX_new(), evp_md: EVP_sha384() };
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
            assert_eq!(md.len(), HMAC_SHA384_SIZE);
            let mut mdlen = HMAC_SHA384_SIZE as c_uint;
            assert_ne!(HMAC_Final(self.ctx, md.as_mut_ptr().cast(), &mut mdlen), 0);
            assert_eq!(mdlen, HMAC_SHA384_SIZE as c_uint);
        }
    }

    #[inline(always)]
    pub fn finish(&mut self) -> [u8; HMAC_SHA384_SIZE] {
        let mut tmp = [0u8; HMAC_SHA384_SIZE];
        self.finish_into(&mut tmp);
        tmp
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
pub fn hmac_sha512(key: &[u8], msg: &[u8]) -> [u8; HMAC_SHA512_SIZE] {
    let mut hm = HMACSHA512::new(key);
    hm.update(msg);
    hm.finish()
}

#[inline(always)]
pub fn hmac_sha512_into(key: &[u8], msg: &[u8], md: &mut [u8]) {
    let mut hm = HMACSHA512::new(key);
    hm.update(msg);
    hm.finish_into(md);
}

pub fn hmac_sha512_secret<const C: usize>(key: &[u8], msg: &[u8]) -> Secret<C> {
    debug_assert!(C <= HMAC_SHA512_SIZE);
    let mut hm = HMACSHA512::new(key);
    hm.update(msg);
    let buff = hm.finish();
    unsafe { Secret::from_bytes(&buff[..C]) }
}

#[inline(always)]
pub fn hmac_sha384(key: &[u8], msg: &[u8]) -> [u8; HMAC_SHA384_SIZE] {
    let mut hm = HMACSHA384::new(key);
    hm.update(msg);
    hm.finish()
}

#[inline(always)]
pub fn hmac_sha384_into(key: &[u8], msg: &[u8], md: &mut [u8]) {
    let mut hm = HMACSHA384::new(key);
    hm.update(msg);
    hm.finish_into(md);
}
