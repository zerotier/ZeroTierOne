use crate::*;
use crate::bindings::capi as ztcore;
use std::os::raw::*;
use num_traits::ToPrimitive;
use std::ffi::CStr;

pub struct Identity {
    pub identity_type: IdentityType,
    pub address: Address,
    capi: *mut ztcore::ZT_Identity,
}

impl Identity {
    pub fn generate(id_type: IdentityType) -> Result<Identity, ResultCode> {
        unsafe {
            let id = ztcore::ZT_Identity_new(id_type.to_u32().unwrap());
            if id.is_null() {
                return Err(ResultCode::ErrorBadParameter); // this only really happens if type is invalid
            }
            let a = ztcore::ZT_Identity_address(id);
            return Ok(Identity {
                identity_type: id_type,
                address: Address(a as u64),
                capi: id,
            });
        }
    }

    pub fn new_from_string(s: String) -> Result<Identity, ResultCode> {
        unsafe {
            let id = ztcore::ZT_Identity_fromString(s.as_ptr() as *const c_char);
            if id.is_null() {
                return Err(ResultCode::ErrorBadParameter);
            }
            let idt = ztcore::ZT_Identity_type(id);
            let a = ztcore::ZT_Identity_address(id);
            return Ok(Identity {
                identity_type: num_traits::FromPrimitive::from_u32(idt).unwrap(),
                address: Address(a as u64),
                capi: id
            });
        }
    }

    pub fn to_string(&self, include_private: bool) -> String {
        let mut buf: [u8; 1024] = [0; 1024];
        unsafe {
            if ztcore::ZT_Identity_toString(self.capi, buf.as_mut_ptr() as *mut c_char, 1024, if include_private { 1 } else { 0 }).is_null() {
                return String::from("(invalid)");
            }
            return String::from(CStr::from_bytes_with_nul(&buf).unwrap().to_str().unwrap());
        }
    }

    pub fn validate(&self) -> bool {
        unsafe {
            if ztcore::ZT_Identity_validate(self.capi) != 0 {
                return true;
            }
        }
        false
    }

    pub fn has_private(&self) -> bool {
        unsafe {
            if ztcore::ZT_Identity_hasPrivate(self.capi) != 0 {
                return true;
            }
        }
        false
    }
}

impl ToString for Identity {
    fn to_string(&self) -> String {
        self.to_string(false)
    }
}

impl From<Identity> for String {
    fn from(id: Identity) -> String {
        id.to_string(false)
    }
}
