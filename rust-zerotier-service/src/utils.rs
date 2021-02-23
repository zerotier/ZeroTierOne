/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

use std::mem::MaybeUninit;
use std::os::raw::c_uint;

use crate::osdep;

#[inline(always)]
pub(crate) fn sha512<T: AsRef<[u8]>>(data: T) -> [u8; 64] {
    let mut r: MaybeUninit<[u8; 64]> = MaybeUninit::uninit();
    let d = data.as_ref();
    unsafe {
        osdep::sha512(d.as_ptr().cast(), d.len() as c_uint, r.as_mut_ptr().cast());
        r.assume_init()
    }
}

#[inline(always)]
pub(crate) fn sha384<T: AsRef<[u8]>>(data: T) -> [u8; 48] {
    let mut r: MaybeUninit<[u8; 48]> = MaybeUninit::uninit();
    let d = data.as_ref();
    unsafe {
        osdep::sha384(d.as_ptr().cast(), d.len() as c_uint, r.as_mut_ptr().cast());
        r.assume_init()
    }
}

#[inline(always)]
pub(crate) fn ms_since_epoch() -> i64 {
    // This is easy to do in the Rust stdlib, but the version in OSUtils is probably faster.
    unsafe { osdep::msSinceEpoch() }
}
