use crate::*;
use crate::bindings::capi as ztcore;
use std::os::raw::*;
use std::ffi::CStr;
use num_traits::{ToPrimitive, FromPrimitive};

pub struct Identity {
    pub id_type: IdentityType,
    pub address: Address,
    capi: *const ztcore::ZT_Identity,
    requires_delete: bool
}

impl Identity {
    pub(crate) fn new_from_capi(id: *const ztcore::ZT_Identity, requires_delete: bool) -> Identity {
        unsafe {
            let idt = ztcore::ZT_Identity_type(id);
            let a = ztcore::ZT_Identity_address(id);
            return Identity{
                id_type: FromPrimitive::from_u32(idt as u32).unwrap(),
                address: a as Address,
                capi: id,
                requires_delete: requires_delete
            };
        }
    }

    pub fn generate(id_type: IdentityType) -> Result<Identity, ResultCode> {
        unsafe {
            let id = ztcore::ZT_Identity_new(id_type.to_u32().unwrap());
            if id.is_null() {
                return Err(ResultCode::ErrorBadParameter); // this only really happens if type is invalid
            }
            return Ok(Identity::new_from_capi(id, true));
        }
    }

    pub fn new_from_string(s: String) -> Result<Identity, ResultCode> {
        unsafe {
            let id = ztcore::ZT_Identity_fromString(s.as_ptr() as *const c_char);
            if id.is_null() {
                return Err(ResultCode::ErrorBadParameter);
            }
            return Ok(Identity::new_from_capi(id, true));
        }
    }

    pub fn to_string(&self, include_private: bool) -> String {
        let mut buf: [u8; 2048] = [0; 2048];
        unsafe {
            if ztcore::ZT_Identity_toString(self.capi, buf.as_mut_ptr() as *mut c_char, buf.len() as c_int, if include_private { 1 } else { 0 }).is_null() {
                return String::from("(invalid)");
            }
            return String::from(CStr::from_bytes_with_nul(buf.as_ref()).unwrap().to_str().unwrap());
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

    pub fn fingerprint(&self) -> Fingerprint {
        unsafe {
            let cfp = ztcore::ZT_Identity_fingerprint(self.capi);
            return Fingerprint {
                address: (*cfp).address,
                hash: (*cfp).hash
            }
        }
    }

    pub fn sign(&self, data: &[u8]) -> Result<Box<[u8]>, ResultCode> {
        unsafe {
            let mut sig: Vec<u8> = vec!(0; 128);
            let siglen = ztcore::ZT_Identity_sign(self.capi, data.as_ptr() as *const c_void, data.len() as c_uint, sig.as_mut_ptr() as *mut c_void, sig.len() as u32);
            if siglen > 0 {
                sig.resize(siglen as usize, 0);
                return Ok(sig.into_boxed_slice());
            }
            return Err(ResultCode::ErrorBadParameter);
        }
    }

    pub fn verify(&self, data: &[u8], signature: &[u8]) -> bool {
        if signature.len() == 0 {
            return false;
        }
        unsafe {
            if ztcore::ZT_Identity_verify(self.capi, data.as_ptr() as *const c_void, data.len() as c_uint, signature.as_ptr() as *const c_void, signature.len() as c_uint) != 0 {
                return true;
            }
        }
        false
    }
}

impl Drop for Identity {
    fn drop(&mut self) {
        if self.requires_delete && !self.capi.is_null() {
            unsafe {
                ztcore::ZT_Identity_delete(self.capi);
            }
        }
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
