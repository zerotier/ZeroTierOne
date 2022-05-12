/*
 * Copyright (c)2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use url::Url;

use crate::ZeroIDC;

#[cfg(any(
    all(target_os = "linux", target_arch = "x86"),
    all(target_os = "linux", target_arch = "x86_64"),
    all(target_os = "linux", target_arch = "aarch64"),
    target_os = "windows",
    target_os = "macos",
))]
#[no_mangle]
pub extern "C" fn zeroidc_new(
    issuer: *const c_char,
    client_id: *const c_char,
    auth_endpoint: *const c_char,
    web_listen_port: u16,
) -> *mut ZeroIDC {
    if issuer.is_null() {
        println!("issuer is null");
        return std::ptr::null_mut();
    }

    if client_id.is_null() {
        println!("client_id is null");
        return std::ptr::null_mut();
    }

    if auth_endpoint.is_null() {
        println!("auth_endpoint is null");
        return std::ptr::null_mut();
    }

    let issuer = unsafe { CStr::from_ptr(issuer) };
    let client_id = unsafe { CStr::from_ptr(client_id) };
    let auth_endpoint = unsafe { CStr::from_ptr(auth_endpoint) };
    match ZeroIDC::new(
        issuer.to_str().unwrap(),
        client_id.to_str().unwrap(),
        auth_endpoint.to_str().unwrap(),
        web_listen_port,
    ) {
        Ok(idc) => Box::into_raw(Box::new(idc)),
        Err(s) => {
            println!("Error creating ZeroIDC instance: {}", s);
            std::ptr::null_mut()
        }
    }
}

#[cfg(any(
    all(target_os = "linux", target_arch = "x86"),
    all(target_os = "linux", target_arch = "x86_64"),
    all(target_os = "linux", target_arch = "aarch64"),
    target_os = "windows",
    target_os = "macos",
))]
#[no_mangle]
pub extern "C" fn zeroidc_delete(ptr: *mut ZeroIDC) {
    if ptr.is_null() {
        return;
    }
    let idc = unsafe {
        assert!(!ptr.is_null());
        &mut *ptr
    };
    idc.stop();

    unsafe {
        Box::from_raw(ptr);
    }
}

#[cfg(any(
    all(target_os = "linux", target_arch = "x86"),
    all(target_os = "linux", target_arch = "x86_64"),
    all(target_os = "linux", target_arch = "aarch64"),
    target_os = "windows",
    target_os = "macos",
))]
#[no_mangle]
pub extern "C" fn zeroidc_start(ptr: *mut ZeroIDC) {
    let idc = unsafe {
        assert!(!ptr.is_null());
        &mut *ptr
    };
    idc.start();
}

#[cfg(any(
    all(target_os = "linux", target_arch = "x86"),
    all(target_os = "linux", target_arch = "x86_64"),
    all(target_os = "linux", target_arch = "aarch64"),
    target_os = "windows",
    target_os = "macos",
))]
#[no_mangle]
pub extern "C" fn zeroidc_stop(ptr: *mut ZeroIDC) {
    let idc = unsafe {
        assert!(!ptr.is_null());
        &mut *ptr
    };
    idc.stop();
}

#[cfg(any(
    all(target_os = "linux", target_arch = "x86"),
    all(target_os = "linux", target_arch = "x86_64"),
    all(target_os = "linux", target_arch = "aarch64"),
    target_os = "windows",
    target_os = "macos",
))]
#[no_mangle]
pub extern "C" fn zeroidc_is_running(ptr: *mut ZeroIDC) -> bool {
    let idc = unsafe {
        assert!(!ptr.is_null());
        &mut *ptr
    };

    idc.is_running()
}

#[no_mangle]
pub extern "C" fn zeroidc_get_exp_time(ptr: *mut ZeroIDC) -> u64 {
    let id = unsafe {
        assert!(!ptr.is_null());
        &mut *ptr
    };

    id.get_exp_time()
}

#[cfg(any(
    all(target_os = "linux", target_arch = "x86"),
    all(target_os = "linux", target_arch = "x86_64"),
    all(target_os = "linux", target_arch = "aarch64"),
    target_os = "windows",
    target_os = "macos",
))]
#[no_mangle]
pub extern "C" fn zeroidc_set_nonce_and_csrf(
    ptr: *mut ZeroIDC,
    csrf_token: *const c_char,
    nonce: *const c_char,
) {
    let idc = unsafe {
        assert!(!ptr.is_null());
        &mut *ptr
    };

    if csrf_token.is_null() {
        println!("csrf_token is null");
        return;
    }

    if nonce.is_null() {
        println!("nonce is null");
        return;
    }

    let csrf_token = unsafe { CStr::from_ptr(csrf_token) }
        .to_str()
        .unwrap()
        .to_string();
    let nonce = unsafe { CStr::from_ptr(nonce) }
        .to_str()
        .unwrap()
        .to_string();

    idc.set_nonce_and_csrf(csrf_token, nonce);
}

#[cfg(any(
    all(target_os = "linux", target_arch = "x86"),
    all(target_os = "linux", target_arch = "x86_64"),
    all(target_os = "linux", target_arch = "aarch64"),
    target_os = "windows",
    target_os = "macos",
))]
#[no_mangle]
pub extern "C" fn free_cstr(s: *mut c_char) {
    if s.is_null() {
        println!("passed a null object");
        return;
    }

    unsafe {
        let _ = CString::from_raw(s);
    }
}

#[cfg(any(
    all(target_os = "linux", target_arch = "x86"),
    all(target_os = "linux", target_arch = "x86_64"),
    all(target_os = "linux", target_arch = "aarch64"),
    target_os = "windows",
    target_os = "macos",
))]
#[no_mangle]
pub extern "C" fn zeroidc_get_auth_url(ptr: *mut ZeroIDC) -> *mut c_char {
    if ptr.is_null() {
        println!("passed a null object");
        return std::ptr::null_mut();
    }
    let idc = unsafe { &mut *ptr };

    let s = CString::new(idc.auth_url()).unwrap();
    return s.into_raw();
}

#[cfg(any(
    all(target_os = "linux", target_arch = "x86"),
    all(target_os = "linux", target_arch = "x86_64"),
    all(target_os = "linux", target_arch = "aarch64"),
    target_os = "windows",
    target_os = "macos",
))]
#[no_mangle]
pub extern "C" fn zeroidc_token_exchange(idc: *mut ZeroIDC, code: *const c_char) -> *mut c_char {
    if idc.is_null() {
        println!("idc is null");
        return std::ptr::null_mut();
    }

    if code.is_null() {
        println!("code is null");
        return std::ptr::null_mut();
    }
    let idc = unsafe { &mut *idc };

    let code = unsafe { CStr::from_ptr(code) }.to_str().unwrap();

    let ret = idc.do_token_exchange(code);
    match ret {
        Ok(ret) => {
            let ret = CString::new(ret).unwrap();
            ret.into_raw()
        }
        Err(e) => {
            let errstr = format!("{{\"errorMessage\":\"{}\"\"}}", e);
            let ret = CString::new(errstr).unwrap();
            ret.into_raw()
        }
    }
}

#[no_mangle]
pub extern "C" fn zeroidc_get_url_param_value(
    param: *const c_char,
    path: *const c_char,
) -> *mut c_char {
    if param.is_null() {
        println!("param is null");
        return std::ptr::null_mut();
    }
    if path.is_null() {
        println!("path is null");
        return std::ptr::null_mut();
    }
    let param = unsafe { CStr::from_ptr(param) }.to_str().unwrap();
    let path = unsafe { CStr::from_ptr(path) }.to_str().unwrap();

    let url = "http://localhost:9993".to_string() + path;
    let url = Url::parse(&url).unwrap();

    let pairs = url.query_pairs();
    for p in pairs {
        if p.0 == param {
            let s = CString::new(p.1.into_owned()).unwrap();
            return s.into_raw();
        }
    }

    std::ptr::null_mut()
}

#[no_mangle]
pub extern "C" fn zeroidc_network_id_from_state(state: *const c_char) -> *mut c_char {
    if state.is_null() {
        println!("state is null");
        return std::ptr::null_mut();
    }

    let state = unsafe { CStr::from_ptr(state) }.to_str().unwrap();

    let split = state.split('_');
    let split = split.collect::<Vec<&str>>();
    if split.len() != 2 {
        return std::ptr::null_mut();
    }

    let s = CString::new(split[1]).unwrap();
    s.into_raw()
}

#[cfg(any(
    all(target_os = "linux", target_arch = "x86"),
    all(target_os = "linux", target_arch = "x86_64"),
    all(target_os = "linux", target_arch = "aarch64"),
    target_os = "windows",
    target_os = "macos",
))]
#[no_mangle]
pub extern "C" fn zeroidc_kick_refresh_thread(idc: *mut ZeroIDC) {
    if idc.is_null() {
        println!("idc is null");
        return;
    }
    let idc = unsafe { &mut *idc };

    idc.kick_refresh_thread();
}
