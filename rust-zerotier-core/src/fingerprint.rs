/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

use std::ffi::CString;
use std::mem::MaybeUninit;
use std::os::raw::{c_char, c_int};

use crate::*;
use crate::capi as ztcore;
use std::ptr::copy_nonoverlapping;

#[derive(PartialEq, Eq)]
pub struct Fingerprint {
    pub address: Address,
    pub hash: [u8; 48]
}

impl Fingerprint {
    #[inline(always)]
    pub(crate) fn new_from_capi(fp: &ztcore::ZT_Fingerprint) -> Fingerprint {
        Fingerprint{
            address: Address(fp.address),
            hash: fp.hash
        }
    }

    pub fn new_from_string(s: &str) -> Result<Fingerprint, ResultCode> {
        let cs = CString::new(s);
        if cs.is_err() {
            return Err(ResultCode::ErrorBadParameter);
        }
        let cs = cs.unwrap();
        let mut cfp: MaybeUninit<ztcore::ZT_Fingerprint> = MaybeUninit::uninit();
        unsafe {
            if ztcore::ZT_Fingerprint_fromString(cfp.as_mut_ptr(), cs.as_ptr()) != 0 {
                let fp = cfp.assume_init();
                return Ok(Fingerprint{
                    address: Address(fp.address),
                    hash: fp.hash
                });
            }
        }
        return Err(ResultCode::ErrorBadParameter);
    }

    pub fn new_from_bytes(bytes: &[u8]) -> Result<Fingerprint, ResultCode> {
        if bytes.len() < (5 + 48) {
            let h: MaybeUninit<[u8; 48]> = MaybeUninit::uninit();
            let mut fp = Fingerprint {
                address: Address::from(bytes),
                hash: unsafe { h.assume_init() },
            };
            unsafe {
                copy_nonoverlapping(bytes.as_ptr().offset(5), fp.hash.as_mut_ptr(), 48);
            }
            Ok(fp)
        } else {
            Err(ResultCode::ErrorBadParameter)
        }
    }
}

impl ToString for Fingerprint {
    fn to_string(&self) -> String {
        let mut buf: [u8; 256] = [0; 256];
        unsafe {
            if ztcore::ZT_Fingerprint_toString(&ztcore::ZT_Fingerprint {
                address: self.address.0,
                hash: self.hash
            }, buf.as_mut_ptr() as *mut c_char, buf.len() as c_int).is_null() {
                return String::from("(invalid)");
            }
            return cstr_to_string(buf.as_ptr() as *const c_char, 256);
        }
    }
}

impl serde::Serialize for Fingerprint {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: serde::Serializer {
        serializer.serialize_str(self.to_string().as_str())
    }
}

struct FingerprintVisitor;

impl<'de> serde::de::Visitor<'de> for FingerprintVisitor {
    type Value = Fingerprint;

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("ZeroTier Fingerprint in string format")
    }

    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E> where E: serde::de::Error {
        let id = Fingerprint::new_from_string(s);
        if id.is_err() {
            return Err(serde::de::Error::invalid_value(serde::de::Unexpected::Str(s), &self));
        }
        return Ok(id.ok().unwrap() as Self::Value);
    }
}

impl<'de> serde::Deserialize<'de> for Fingerprint {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: serde::Deserializer<'de> {
        deserializer.deserialize_str(FingerprintVisitor)
    }
}
