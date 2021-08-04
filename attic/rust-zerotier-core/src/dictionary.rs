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

use std::collections::HashMap;
use std::ffi::c_void;
use std::os::raw::{c_char, c_uint};

use crate::{cstr_to_string, ResultCode};

/// Rust interface to the Dictionary data structure.
#[derive(Clone)]
pub struct Dictionary {
    data: HashMap<String, Vec<u8>>,
}

// Callback called by ZeroTier core to parse a Dictionary, populates a Rust Dictionary object.
extern "C" fn populate_dict_callback(arg: *mut c_void, c_key: *const c_char, key_len: c_uint, c_value: *const c_void, value_len: c_uint) {
    let d = unsafe { &mut *(arg.cast::<Dictionary>()) };
    let k = unsafe { cstr_to_string(c_key, key_len as isize) };
    if !k.is_empty() {
        let mut v: Vec<u8> = Vec::new();
        if value_len > 0 {
            let vp = c_value.cast::<u8>();
            v.reserve(value_len as usize);
            for i in 0..(value_len as isize) {
                unsafe { v.push(*(vp.offset(i))) };
            }
        }
        let _ = d.data.insert(k, v);
    }
}

impl Dictionary {
    #[inline(always)]
    pub fn new() -> Dictionary {
        Dictionary { data: HashMap::new() }
    }

    pub fn new_from_bytes(dict: &[u8]) -> Result<Dictionary, ResultCode> {
        let mut d = Dictionary{ data: HashMap::new() };
        if unsafe { crate::capi::ZT_Dictionary_parse(dict.as_ptr().cast(), dict.len() as c_uint, (&mut d as *mut Dictionary).cast(), Some(populate_dict_callback)) != 0 } {
            Ok(d)
        } else {
            Err(ResultCode::ErrorBadParameter)
        }
    }

    pub fn get<K: AsRef<[u8]>>(&self, k: K) -> Option<&Vec<u8>> {
        let ks = String::from(String::from_utf8_lossy(k.as_ref()));
        self.data.get(&ks)
    }

    pub fn get_or_empty<K: AsRef<[u8]>>(&self, k: K) -> Vec<u8> {
        let ks = String::from(String::from_utf8_lossy(k.as_ref()));
        self.data.get(&ks).map_or_else(|| -> Vec<u8> { Vec::new() }, |d| -> Vec<u8> { d.clone() })
    }

    pub fn get_str<K: AsRef<[u8]>>(&self, k: K) -> Option<&str> {
        let ks = String::from(String::from_utf8_lossy(k.as_ref()));
        let v = self.data.get(&ks);
        v.map_or(None, |v: &Vec<u8>| {
            let vs = std::str::from_utf8(v.as_slice());
            vs.map_or(None, |v: &str| {
                Some(v)
            })
        })
    }

    pub fn get_string_or_empty<K: AsRef<[u8]>>(&self, k: K) -> String {
        self.get_str(k).map_or_else(|| { String::new() },|s| { String::from(s) })
    }

    pub fn get_ui<K: AsRef<[u8]>>(&self, k: K) -> Option<u64> {
        let v = self.get_str(k);
        v.map_or(None, |v: &str| {
            let vi = u64::from_str_radix(v, 16);
            vi.map_or(None, |i: u64| {
                Some(i)
            })
        })
    }

    #[inline(always)]
    pub fn len(&self) -> usize {
        self.data.len()
    }
}
