use crate::*;
use crate::bindings::capi as ztcore;
use std::os::raw::{c_char, c_int};
use std::ffi::CStr;
use std::mem::MaybeUninit;

pub struct Fingerprint {
    pub address: Address,
    pub hash: [u8; 48]
}

impl Fingerprint {
    pub fn new_from_string(s: &str) -> Result<Fingerprint, ResultCode> {
        unsafe {
            let mut cfp: MaybeUninit<ztcore::ZT_Fingerprint> = MaybeUninit::uninit();
            if ztcore::ZT_Fingerprint_fromString(cfp.as_mut_ptr(), s.as_ptr() as *const c_char) != 0 {
                let fp = cfp.assume_init();
                return Ok(Fingerprint{
                    address: fp.address as Address,
                    hash: fp.hash
                });
            }
        }
        return Err(ResultCode::ErrorBadParameter);
    }
}

impl ToString for Fingerprint {
    fn to_string(&self) -> String {
        let mut buf: [u8; 256] = [0; 256];
        unsafe {
            if ztcore::ZT_Fingerprint_toString(&ztcore::ZT_Fingerprint {
                address: self.address,
                hash: self.hash
            }, buf.as_mut_ptr() as *mut c_char, buf.len() as c_int).is_null() {
                return String::from("(invalid)");
            }
            return String::from(CStr::from_bytes_with_nul(buf.as_ref()).unwrap().to_str().unwrap());
        }
    }
}
