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

use std::error::Error;
use std::fs::File;
use std::io::Read;
use std::mem::MaybeUninit;
use std::os::raw::c_uint;
use std::path::Path;

use zerotier_core::{Identity, Locator};

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

/// Convenience function to read up to limit bytes from a file.
/// If the file is larger than limit, the excess is not read.
pub(crate) fn read_limit<P: AsRef<Path>>(path: P, limit: usize) -> std::io::Result<Vec<u8>> {
    let mut v: Vec<u8> = Vec::new();
    let _ = File::open(path)?.take(limit as u64).read_to_end(&mut v)?;
    Ok(v)
}

/// Read an identity as either a literal or from a file.
pub(crate) fn read_identity(input: &str, validate: bool) -> Result<Identity, String> {
    let parse_func = |s: &str| {
        Identity::new_from_string(s).map_or_else(|e| {
            Err(format!("invalid identity: {}", e.to_str()))
        }, |id| {
            if !validate || id.validate() {
                Ok(id)
            } else {
                Err(String::from("invalid identity: local validation failed"))
            }
        })
    };
    if Path::new(input).exists() {
        read_limit(input, 16384).map_or_else(|e| {
            Err(e.to_string())
        }, |v| {
            String::from_utf8(v).map_or_else(|e| {
                Err(e.to_string())
            }, |s| {
                parse_func(s.as_str())
            })
        })
    } else {
        parse_func(input)
    }
}

/// Read a locator as either a literal or from a file.
pub(crate) fn read_locator(input: &str) -> Result<Locator, String> {
    let parse_func = |s: &str| {
        Locator::new_from_string(s).map_or_else(|e| {
            Err(format!("invalid locator: {}", e.to_str()))
        }, |loc| {
            Ok(loc)
        })
    };
    if Path::new(input).exists() {
        read_limit(input, 16384).map_or_else(|e| {
            Err(e.to_string())
        }, |v| {
            String::from_utf8(v).map_or_else(|e| {
                Err(e.to_string())
            }, |s| {
                parse_func(s.as_str())
            })
        })
    } else {
        parse_func(input)
    }
}
