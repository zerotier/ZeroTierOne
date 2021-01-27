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
use std::os::raw::{c_char, c_int, c_uint};

use crate::*;
use crate::capi as ztcore;

pub struct Locator {
    pub(crate) capi: *const ztcore::ZT_Locator,
    requires_delete: bool
}

impl Locator {
    #[inline(always)]
    pub(crate) fn new_from_capi(l: *const ztcore::ZT_Locator, requires_delete: bool) -> Locator {
        Locator{
            capi: l,
            requires_delete: requires_delete
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
    pub fn timestamp(&self) -> i64 {
        unsafe {
            return ztcore::ZT_Locator_timestamp(self.capi) as i64;
        }
    }

    pub fn endpoints(&self) -> Box<[Endpoint]> {
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
        eps.into_boxed_slice()
    }
}

impl Drop for Locator {
    #[inline(always)]
    fn drop(&mut self) {
        if self.requires_delete {
            unsafe {
                ztcore::ZT_Locator_delete(self.capi);
            }
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
        let mut buf: [u8; 4096] = [0; 4096];
        unsafe {
            if ztcore::ZT_Locator_toString(self.capi, buf.as_mut_ptr() as *mut c_char, buf.len() as c_int).is_null() {
                return String::from("(invalid)");
            }
            return cstr_to_string(buf.as_ptr() as *const c_char, 4096);
        }
    }
}

impl PartialEq for Locator {
    fn eq(&self, other: &Locator) -> bool {
        self.to_string() == other.to_string()
    }
}

impl Eq for Locator {}

impl serde::Serialize for Locator {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: serde::Serializer {
        serializer.serialize_str(self.to_string().as_str())
    }
}

struct LocatorVisitor;

impl<'de> serde::de::Visitor<'de> for LocatorVisitor {
    type Value = Locator;

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("Locator value in string form")
    }

    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E> where E: serde::de::Error {
        let id = Locator::new_from_string(s);
        if id.is_err() {
            return Err(serde::de::Error::invalid_value(serde::de::Unexpected::Str(s), &self));
        }
        return Ok(id.ok().unwrap() as Self::Value);
    }
}

impl<'de> serde::Deserialize<'de> for Locator {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: serde::Deserializer<'de> {
        deserializer.deserialize_str(LocatorVisitor)
    }
}
