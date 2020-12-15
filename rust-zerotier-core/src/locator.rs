use crate::*;
use crate::bindings::capi as ztcore;
use std::os::raw::{c_char, c_int, c_uint};
use std::ffi::CStr;

pub struct Locator {
    capi: *const ztcore::ZT_Locator,
    requires_delete: bool
}

impl Locator {
    pub(crate) fn new_from_capi(l: *const ztcore::ZT_Locator, requires_delete: bool) -> Locator {
        Locator{
            capi: l,
            requires_delete: requires_delete
        }
    }

    pub fn new_from_string(s: &str) -> Result<Locator, ResultCode> {
        unsafe {
            let l = ztcore::ZT_Locator_fromString(s.as_ptr() as *const c_char);
            if l.is_null() {
                return Err(ResultCode::ErrorBadParameter);
            }
            return Ok(Locator::new_from_capi(l, true));
        }
    }

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
                eps.push(Endpoint::new_from_capi(ztcore::ZT_Locator_endpoint(self.capi, i as c_uint)));
            }
        }
        eps.into_boxed_slice()
    }
}

impl Drop for Locator {
    fn drop(&mut self) {
        if self.requires_delete && !self.capi.is_null() {
            unsafe {
                ztcore::ZT_Locator_delete(self.capi);
            }
        }
    }
}

impl ToString for Locator {
    fn to_string(&self) -> String {
        let mut buf: [u8; 4096] = [0; 4096];
        unsafe {
            if ztcore::ZT_Locator_toString(self.capi, buf.as_mut_ptr() as *mut c_char, buf.len() as c_int).is_null() {
                return String::from("(invalid)");
            }
            return String::from(CStr::from_bytes_with_nul(buf.as_ref()).unwrap().to_str().unwrap());
        }
    }
}
