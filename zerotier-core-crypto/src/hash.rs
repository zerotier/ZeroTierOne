/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::mem::MaybeUninit;
use std::convert::TryInto;
use std::io::Write;

pub const SHA512_HASH_SIZE: usize = 64;
pub const SHA384_HASH_SIZE: usize = 48;

pub struct SHA512(gcrypt::digest::MessageDigest);

impl SHA512 {
    #[inline(always)]
    pub fn hash(b: &[u8]) -> [u8; SHA512_HASH_SIZE] {
        let mut h = unsafe { MaybeUninit::<[u8; SHA512_HASH_SIZE]>::uninit().assume_init() };
        gcrypt::digest::hash(gcrypt::digest::Algorithm::Sha512, b, &mut h);
        h
    }

    pub fn hmac(key: &[u8], msg: &[u8]) -> [u8; SHA512_HASH_SIZE] {
        let mut m = gcrypt::mac::Mac::new(gcrypt::mac::Algorithm::HmacSha512).unwrap();
        m.set_key(key).expect("FATAL: invalid HMAC-SHA512 key");
        m.update(msg).expect("FATAL: HMAC-SHA512 failed");
        let mut h = [0_u8; SHA512_HASH_SIZE];
        m.get_mac(&mut h).expect("FATAL: HMAC-SHA512 failed");
        h
    }

    #[inline(always)]
    pub fn new() -> Self { Self(gcrypt::digest::MessageDigest::new(gcrypt::digest::Algorithm::Sha512).unwrap()) }

    #[inline(always)]
    pub fn reset(&mut self) { self.0.reset(); }

    #[inline(always)]
    pub fn update(&mut self, b: &[u8]) { self.0.update(b); }

    #[inline(always)]
    pub fn finish(&mut self) -> [u8; SHA512_HASH_SIZE] {
        self.0.finish();
        self.0.get_only_digest().unwrap().try_into().unwrap()
    }

    #[inline(always)]
    pub fn finish_get_ref(&mut self) -> &[u8] {
        self.0.finish();
        self.0.get_only_digest().unwrap()
    }
}

impl Write for SHA512 {
    #[inline(always)]
    fn write(&mut self, b: &[u8]) -> std::io::Result<usize> {
        self.0.update(b);
        Ok(b.len())
    }

    #[inline(always)]
    fn flush(&mut self) -> std::io::Result<()> { self.0.flush() }
}

pub struct SHA384(gcrypt::digest::MessageDigest);

impl SHA384 {
    #[inline(always)]
    pub fn hash(b: &[u8]) -> [u8; SHA384_HASH_SIZE] {
        let mut h = unsafe { MaybeUninit::<[u8; SHA384_HASH_SIZE]>::uninit().assume_init() };
        gcrypt::digest::hash(gcrypt::digest::Algorithm::Sha384, b, &mut h);
        h
    }

    pub fn hmac(key: &[u8], msg: &[u8]) -> [u8; SHA384_HASH_SIZE] {
        let mut m = gcrypt::mac::Mac::new(gcrypt::mac::Algorithm::HmacSha384).unwrap();
        m.set_key(key).expect("FATAL: invalid HMAC-SHA384 key");
        m.update(msg).expect("FATAL: HMAC-SHA384 failed");
        let mut h = [0_u8; SHA384_HASH_SIZE];
        m.get_mac(&mut h).expect("FATAL: HMAC-SHA384 failed");
        h
    }

    #[inline(always)]
    pub fn new() -> Self { Self(gcrypt::digest::MessageDigest::new(gcrypt::digest::Algorithm::Sha384).unwrap()) }

    #[inline(always)]
    pub fn reset(&mut self) { self.0.reset(); }

    #[inline(always)]
    pub fn update(&mut self, b: &[u8]) { self.0.update(b); }

    #[inline(always)]
    pub fn finish(&mut self) -> [u8; SHA384_HASH_SIZE] {
        self.0.finish();
        self.0.get_only_digest().unwrap().try_into().unwrap()
    }

    #[inline(always)]
    pub fn finish_get_ref(&mut self) -> &[u8] {
        self.0.finish();
        self.0.get_only_digest().unwrap()
    }
}

impl Write for SHA384 {
    #[inline(always)]
    fn write(&mut self, b: &[u8]) -> std::io::Result<usize> {
        self.0.update(b);
        Ok(b.len())
    }

    #[inline(always)]
    fn flush(&mut self) -> std::io::Result<()> { self.0.flush() }
}
