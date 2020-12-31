use crate::*;
use crate::bindings::capi as ztcore;
use std::os::raw::*;
use std::ffi::CStr;
use num_traits::{ToPrimitive, FromPrimitive};

#[derive(FromPrimitive,ToPrimitive)]
pub enum IdentityType {
    Curve25519 = ztcore::ZT_IdentityType_ZT_IDENTITY_TYPE_C25519 as isize,
    NistP384 = ztcore::ZT_IdentityType_ZT_IDENTITY_TYPE_P384 as isize,
}

pub struct Identity {
    pub type_: IdentityType,
    pub address: Address,
    pub(crate) capi: *const ztcore::ZT_Identity,
    requires_delete: bool,
}

impl Identity {
    pub(crate) fn new_from_capi(id: *const ztcore::ZT_Identity, requires_delete: bool) -> Identity {
        unsafe {
            let idt = ztcore::ZT_Identity_type(id);
            let a = ztcore::ZT_Identity_address(id);
            return Identity {
                type_: FromPrimitive::from_u32(idt as u32).unwrap(),
                address: Address(a),
                capi: id,
                requires_delete: requires_delete,
            };
        }
    }

    /// Generate a new identity.
    /// This is time consuming due to one time proof of work. It can take several seconds.
    pub fn new_generate(id_type: IdentityType) -> Result<Identity, ResultCode> {
        unsafe {
            let id = ztcore::ZT_Identity_new(id_type.to_u32().unwrap());
            if id.is_null() {
                return Err(ResultCode::ErrorBadParameter); // this only really happens if type is invalid
            }
            return Ok(Identity::new_from_capi(id, true));
        }
    }

    /// Construct from a string representation of this identity.
    pub fn new_from_string(s: &str) -> Result<Identity, ResultCode> {
        unsafe {
            let id = ztcore::ZT_Identity_fromString(s.as_ptr() as *const c_char);
            if id.is_null() {
                return Err(ResultCode::ErrorBadParameter);
            }
            return Ok(Identity::new_from_capi(id, true));
        }
    }

    fn intl_to_string(&self, include_private: bool) -> String {
        let mut buf: [u8; 2048] = [0; 2048];
        unsafe {
            if ztcore::ZT_Identity_toString(self.capi, buf.as_mut_ptr() as *mut c_char, buf.len() as c_int, if include_private { 1 } else { 0 }).is_null() {
                return String::from("(invalid)");
            }
            return String::from(CStr::from_bytes_with_nul(buf.as_ref()).unwrap().to_str().unwrap());
        }
    }

    /// Convert to a string and include the private key if present.
    /// If the private key is not present this is the same as to_string().
    #[inline]
    pub fn to_secret_string(&self) -> String {
        self.intl_to_string(true)
    }

    /// Validate this identity, which can be slightly time consuming in some cases (20-40ms).
    pub fn validate(&self) -> bool {
        unsafe {
            if ztcore::ZT_Identity_validate(self.capi) != 0 {
                return true;
            }
        }
        false
    }

    /// Returns true if this Identity includes its corresponding private key.
    pub fn has_private(&self) -> bool {
        unsafe {
            if ztcore::ZT_Identity_hasPrivate(self.capi) != 0 {
                return true;
            }
        }
        false
    }

    /// Obtain the full fingerprint of this identity, which includes a SHA384 hash of the public key.
    pub fn fingerprint(&self) -> Fingerprint {
        unsafe {
            let cfp = ztcore::ZT_Identity_fingerprint(self.capi);
            return Fingerprint {
                address: Address((*cfp).address),
                hash: (*cfp).hash,
            };
        }
    }

    /// Sign some data with this identity.
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

    /// Verify a signature by this identity.
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

impl Clone for Identity {
    fn clone(&self) -> Identity {
        unsafe {
            return Identity::new_from_capi(ztcore::ZT_Identity_clone(self.capi), true);
        }
    }
}

impl Drop for Identity {
    fn drop(&mut self) {
        if self.requires_delete {
            unsafe {
                ztcore::ZT_Identity_delete(self.capi);
            }
        }
    }
}

impl ToString for Identity {
    #[inline]
    fn to_string(&self) -> String {
        self.intl_to_string(false)
    }
}

impl serde::Serialize for Identity {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: serde::Serializer {
        serializer.serialize_str(self.intl_to_string(false).as_str())
    }
}

struct IdentityVisitor;

impl<'de> serde::de::Visitor<'de> for IdentityVisitor {
    type Value = Identity;

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("ZeroTier Identity in string format")
    }

    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E> where E: serde::de::Error {
        let id = Identity::new_from_string(s);
        if id.is_err() {
            return Err(serde::de::Error::invalid_value(serde::de::Unexpected::Str(s), &self));
        }
        return Ok(id.ok().unwrap() as Self::Value);
    }
}

impl<'de> serde::Deserialize<'de> for Identity {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: serde::Deserializer<'de> {
        deserializer.deserialize_str(IdentityVisitor)
    }
}
