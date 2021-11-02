mod ext {

    use std::ffi::{CStr, CString};
    use std::os::raw::c_char;

    use crate::{AuthInfo, ZeroIDC};

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

        let iss = unsafe { CStr::from_ptr(issuer) };
        let c_id = unsafe { CStr::from_ptr(client_id) };
        let auth_endpoint = unsafe { CStr::from_ptr(auth_endpoint) };
        match ZeroIDC::new(
            iss.to_str().unwrap(),
            c_id.to_str().unwrap(),
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
        let ai = unsafe {
            assert!(!ptr.is_null());
            &mut *ptr
        };
        let s = CString::new(ai.url.to_string()).unwrap();
        return s.as_ptr();
    }
}
