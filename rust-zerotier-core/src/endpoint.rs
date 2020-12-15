use crate::*;
use crate::bindings::capi as ztcore;
use num_traits::FromPrimitive;
use std::os::raw::{c_char, c_int};
use std::ffi::CStr;
use std::mem::MaybeUninit;

pub struct Endpoint {
    pub ep_type: EndpointType,
    intl: ztcore::ZT_Endpoint
}

impl Endpoint {
    pub fn new_from_string(s: &str) -> Result<Endpoint, ResultCode> {
        unsafe {
            let mut cep: MaybeUninit<ztcore::ZT_Endpoint> = MaybeUninit::uninit();
            let ec = ztcore::ZT_Endpoint_fromString(cep.as_mut_ptr(), s.as_ptr() as *const c_char) as i32;
            if ec == 0 {
                let epi = cep.assume_init();
                return Ok(Endpoint{
                    ep_type: EndpointType::from_u32(epi.type_ as u32).unwrap(),
                    intl: epi
                });
            }
            return Err(ResultCode::from_i32(ec).unwrap());
        }
    }

    pub(crate) fn new_from_capi(ep: *const ztcore::ZT_Endpoint) -> Endpoint {
        unsafe {
            return Endpoint{
                ep_type: EndpointType::from_u32((*ep).type_ as u32).unwrap(),
                intl: *ep
            };
        }
    }
}

impl ToString for Endpoint {
    fn to_string(&self) -> String {
        let mut buf: [u8; 1024] = [0; 1024];
        unsafe {
            if ztcore::ZT_Endpoint_toString(&(self.intl) as *const ztcore::ZT_Endpoint,buf.as_mut_ptr() as *mut c_char, buf.len() as c_int).is_null() {
                return String::from("(invalid)");
            }
            return String::from(CStr::from_bytes_with_nul(buf.as_ref()).unwrap().to_str().unwrap());
        }
    }
}
