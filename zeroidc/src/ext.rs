use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use url::{Url, ParseError};

use crate::{AuthInfo, ZeroIDC};

#[no_mangle]
pub extern "C" fn zeroidc_new(
    network_id: *const c_char,
    issuer: *const c_char,
    client_id: *const c_char,
    auth_endpoint: *const c_char,
    web_listen_port: u16,
) -> *mut ZeroIDC {
    if network_id.is_null() {
        println!("network_id is null");
        return std::ptr::null_mut();

    }
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

    let network_id = unsafe {CStr::from_ptr(network_id) };
    let issuer = unsafe { CStr::from_ptr(issuer) };
    let client_id = unsafe { CStr::from_ptr(client_id) };
    let auth_endpoint = unsafe { CStr::from_ptr(auth_endpoint) };
    match ZeroIDC::new(
        network_id.to_str().unwrap(),
        issuer.to_str().unwrap(),
        client_id.to_str().unwrap(),
        auth_endpoint.to_str().unwrap(),
        web_listen_port,
    ) {
        Ok(idc) => {
            return Box::into_raw(Box::new(idc));
        }
        Err(s) => {
            println!("Error creating ZeroIDC instance: {}", s);
            return std::ptr::null_mut();
        }
    }
}

#[no_mangle]
pub extern "C" fn zeroidc_delete(ptr: *mut ZeroIDC) {
    if ptr.is_null() {
        return;
    }
    unsafe {
        Box::from_raw(ptr);
    }
}

#[no_mangle]
pub extern "C" fn zeroidc_start(ptr: *mut ZeroIDC) {
    let idc = unsafe {
        assert!(!ptr.is_null());
        &mut *ptr
    };
    idc.start();
}

#[no_mangle]
pub extern "C" fn zeroidc_stop(ptr: *mut ZeroIDC) {
    let idc = unsafe {
        assert!(!ptr.is_null());
        &mut *ptr
    };
    idc.stop();
}

#[no_mangle]
pub extern "C" fn zeroidc_is_running(ptr: *mut ZeroIDC) -> bool {
    let idc = unsafe {
        assert!(!ptr.is_null());
        &mut *ptr
    };

    idc.is_running()
}

#[no_mangle]
pub extern "C" fn zeroidc_process_form_post(ptr: *mut ZeroIDC, body: *const c_char) -> bool {
    let idc = unsafe {
        assert!(!ptr.is_null());
        &mut *ptr
    };

    if body.is_null() {
        println!("body is null");
        return false
    }

    let body = unsafe { CStr::from_ptr(body) }
        .to_str().unwrap().to_string();

    false
}

#[no_mangle]
pub extern "C" fn zeroidc_get_auth_info(
    ptr: *mut ZeroIDC,
    csrf_token: *const c_char,
    nonce: *const c_char,
) -> *mut AuthInfo {
    let idc = unsafe {
        assert!(!ptr.is_null());
        &mut *ptr
    };

    if csrf_token.is_null() {
        println!("csrf_token is null");
        return std::ptr::null_mut();
    }

    if nonce.is_null() {
        println!("nonce is null");
        return std::ptr::null_mut();
    }

    let csrf_token = unsafe { CStr::from_ptr(csrf_token) }
        .to_str()
        .unwrap()
        .to_string();
    let nonce = unsafe { CStr::from_ptr(nonce) }
        .to_str()
        .unwrap()
        .to_string();

    match idc.get_auth_info(csrf_token, nonce) {
        Some(a) => Box::into_raw(Box::new(a)),
        None => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn zeroidc_auth_info_delete(ptr: *mut AuthInfo) {
    if ptr.is_null() {
        return;
    }
    unsafe {
        Box::from_raw(ptr);
    }
}

#[no_mangle]
pub extern "C" fn zeroidc_get_auth_url(ptr: *mut AuthInfo) -> *const c_char {
    if ptr.is_null() {
        println!("passed a null object");
        return std::ptr::null_mut();
    }
    let ai = unsafe {
        &mut *ptr
    };
    
    let s = CString::new(ai.url.to_string()).unwrap();
    return s.into_raw();
}

#[no_mangle]
pub extern "C" fn zeroidc_token_exchange(idc: *mut ZeroIDC, ai: *mut AuthInfo, code: *const c_char ) {
    if idc.is_null() {
        println!("idc is null");
        return
    }
    if ai.is_null() {
        println!("ai is null");
        return
    }
    let idc = unsafe {
        &mut *idc
    };
    let ai = unsafe {
        &mut *ai
    };


}

#[no_mangle]
pub extern "C" fn zeroidc_get_state_param_value(path: *const c_char) -> *const c_char {
    if path.is_null() {
        println!("path is null");
        return std::ptr::null();
    }

    let path =  unsafe {CStr::from_ptr(path)}.to_str().unwrap();

    let url = "http://localhost:9993".to_string() + path;
    let url = Url::parse(&url).unwrap();

    let mut pairs = url.query_pairs();  
    for p in pairs {
        if p.0 == "state" {
            let s = CString::new(p.1.into_owned()).unwrap();
            return s.into_raw()
        }
    }

    return std::ptr::null();
}

#[no_mangle]
pub extern "C" fn zeroidc_network_id_from_state(state: *const c_char) -> *const c_char {
    if state.is_null() {
        println!("state is null");
        return std::ptr::null();
    }

    let state = unsafe{CStr::from_ptr(state)}.to_str().unwrap();

    let split = state.split("_");
    let split = split.collect::<Vec<&str>>();
    if split.len() != 2 {
        return std::ptr::null();
    }

    let s = CString::new(split[1]).unwrap();
    return s.into_raw();
}