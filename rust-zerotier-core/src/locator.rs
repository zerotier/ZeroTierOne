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

use std::ffi::CString;
use std::os::raw::{c_int, c_uint};
use std::mem::MaybeUninit;
use std::ptr::null;

use crate::*;
use crate::capi as ztcore;

pub struct Locator {
    pub(crate) capi: *const ztcore::ZT_Locator,
    requires_delete: bool
}

impl Locator {
    /// Create and sign a new locator.
    /// The signer must include its secret key.
    pub fn new(signer: &Identity, revision: i64, endpoints: &Vec<Endpoint>) -> Result<Locator, ResultCode> {
        let mut capi_endpoints: Vec<ztcore::ZT_Endpoint> = Vec::new();
        capi_endpoints.reserve(endpoints.len());
        for ep in endpoints.iter() {
            capi_endpoints.push(ep.capi);
        }
        let loc = unsafe { ztcore::ZT_Locator_create(revision, capi_endpoints.as_ptr(), null(), capi_endpoints.len() as c_uint, signer.capi) };
        if loc.is_null() {
            Err(ResultCode::ErrorBadParameter)
        } else {
            Ok(Locator::new_from_capi(loc, true))
        }
    }

    #[inline(always)]
    pub(crate) fn new_from_capi(l: *const ztcore::ZT_Locator, requires_delete: bool) -> Locator {
        Locator{
            capi: l,
            requires_delete
        }
    }

    pub fn new_from_string(s: &str) -> Result<Locator, ResultCode> {
        unsafe {
            let cs = CString::new(s);
            if cs.is_err() {
                return Err(ResultCode::ErrorBadParameter);
            }
            let cs = cs.unwrap();
            let l = ztcore::ZT_Locator_fromString(cs.as_ptr());
            if l.is_null() {
                return Err(ResultCode::ErrorBadParameter);
            }
            return Ok(Locator::new_from_capi(l, true));
        }
    }

    #[inline(always)]
    pub fn revision(&self) -> i64 {
        unsafe { ztcore::ZT_Locator_revision(self.capi) as i64 }
    }

    #[inline(always)]
    pub fn signer(&self) -> Address {
        unsafe { Address(ztcore::ZT_Locator_signer(self.capi)) }
    }

    pub fn endpoints(&self) -> Vec<Endpoint> {
        let mut eps: Vec<Endpoint> = Vec::new();
        unsafe {
            let ep_count = ztcore::ZT_Locator_endpointCount(self.capi) as usize;
            eps.reserve(ep_count as usize);
            for i in 0..ep_count {
                let ep = ztcore::ZT_Locator_endpoint(self.capi, i as c_uint);
                if !ep.is_null() {
                    eps.push(Endpoint::new_from_capi(&(*ep)));
                }
            }
        }
        eps
    }

    #[inline(always)]
    pub fn verify(&self, id: &Identity) -> bool {
        unsafe { ztcore::ZT_Locator_verify(self.capi, id.capi) != 0 }
    }
}

impl Drop for Locator {
    #[inline(always)]
    fn drop(&mut self) {
        if self.requires_delete {
            unsafe { ztcore::ZT_Locator_delete(self.capi); }
        }
    }
}

impl Clone for Locator {
    #[inline(always)]
    fn clone(&self) -> Locator {
        Locator::new_from_string(self.to_string().as_str()).ok().unwrap()
    }
}

impl ToString for Locator {
    fn to_string(&self) -> String {
        const LOCATOR_STRING_BUF_LEN: usize = 16384;
        let mut buf: MaybeUninit<[u8; LOCATOR_STRING_BUF_LEN]> = MaybeUninit::uninit();
        if unsafe { ztcore::ZT_Locator_toString(self.capi, buf.as_mut_ptr().cast(), LOCATOR_STRING_BUF_LEN as c_int).is_null() }{
            "(invalid)".to_owned()
        } else {
            unsafe { cstr_to_string(buf.as_ptr().cast(), LOCATOR_STRING_BUF_LEN as isize) }
        }
    }
}

impl PartialEq for Locator {
    #[inline(always)]
    fn eq(&self, other: &Locator) -> bool {
        unsafe { ztcore::ZT_Locator_equals(self.capi, other.capi) != 0 }
    }
}

impl Eq for Locator {}

impl serde::Serialize for Locator {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: serde::Serializer { serializer.serialize_str(self.to_string().as_str()) }
}
struct LocatorVisitor;
impl<'de> serde::de::Visitor<'de> for LocatorVisitor {
    type Value = Locator;
    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result { formatter.write_str("Locator value in string form") }
    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E> where E: serde::de::Error {
        let id = Locator::new_from_string(s);
        if id.is_err() {
            return Err(serde::de::Error::invalid_value(serde::de::Unexpected::Str(s), &self));
        }
        return Ok(id.ok().unwrap() as Self::Value);
    }
}
impl<'de> serde::Deserialize<'de> for Locator {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: serde::Deserializer<'de> { deserializer.deserialize_str(LocatorVisitor) }
}
