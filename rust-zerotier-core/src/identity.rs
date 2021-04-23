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

use std::cmp::Ordering;
use std::ffi::CString;
use std::mem::{MaybeUninit, transmute};
use std::os::raw::*;
use std::hash::{Hash, Hasher};

use num_derive::{FromPrimitive, ToPrimitive};
use num_traits::{FromPrimitive, ToPrimitive};

use crate::*;
use crate::capi as ztcore;

#[derive(FromPrimitive, ToPrimitive, PartialEq, Eq, Clone, Copy)]
pub enum IdentityType {
    Curve25519 = ztcore::ZT_IdentityType_ZT_IDENTITY_TYPE_C25519 as isize,
    NistP384 = ztcore::ZT_IdentityType_ZT_IDENTITY_TYPE_P384 as isize,
}

impl IdentityType {
    fn to_str(&self) -> &'static str {
        if *self == IdentityType::Curve25519 { "c25519" } else { "p384" }
    }
}

impl ToString for IdentityType {
    fn to_string(&self) -> String {
        String::from(self.to_str())
    }
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
            Identity {
                type_: FromPrimitive::from_i32(ztcore::ZT_Identity_type(id) as i32).unwrap(),
                address: Address(ztcore::ZT_Identity_address(id)),
                capi: id,
                requires_delete,
            }
        }
    }

    /// Sync type and address fields with C API.
    /// This isn't truly unsafe because these are primitive types, but it's marked as such
    /// because it breaks the rules slightly. It's used in Node to make sure its identity
    /// wrapper matches the one in C++-land even if the latter changes.
    pub(crate) unsafe fn sync_type_and_address_with_capi(&self) {
        let s: *mut Identity = transmute(self as *const Identity);
        (*s).type_ = FromPrimitive::from_i32(ztcore::ZT_Identity_type(self.capi) as i32).unwrap();
        (*s).address.0 = ztcore::ZT_Identity_address(self.capi);
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
            let cs = CString::new(s);
            if cs.is_err() {
                return Err(ResultCode::ErrorBadParameter);
            }
            let cs = cs.unwrap();
            let id = ztcore::ZT_Identity_fromString(cs.as_ptr());
            if id.is_null() {
                return Err(ResultCode::ErrorBadParameter);
            }
            return Ok(Identity::new_from_capi(id, true));
        }
    }

    fn intl_to_string(&self, include_private: bool) -> String {
        let mut buf: MaybeUninit<[c_char; 4096]> = MaybeUninit::uninit();
        unsafe {
            let bufptr = (*buf.as_mut_ptr()).as_mut_ptr();
            if ztcore::ZT_Identity_toString(self.capi, bufptr, 4096, if include_private { 1 } else { 0 }).is_null() {
                return String::from("(invalid)");
            }
            return cstr_to_string(bufptr, 4096);
        }
    }

    /// Convert to a string and include the private key if present.
    /// If the private key is not present this is the same as to_string().
    #[inline(always)]
    pub fn to_secret_string(&self) -> String {
        self.intl_to_string(true)
    }

    /// Validate this identity, which can be slightly time consuming in some cases (20-40ms).
    #[inline(always)]
    pub fn validate(&self) -> bool {
        unsafe { ztcore::ZT_Identity_validate(self.capi) != 0 }
    }

    /// Returns true if this Identity includes its corresponding private key.
    #[inline(always)]
    pub fn has_private(&self) -> bool {
        unsafe { ztcore::ZT_Identity_hasPrivate(self.capi) != 0 }
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
    #[inline(always)]
    pub fn verify(&self, data: &[u8], signature: &[u8]) -> bool {
        unsafe { signature.len() > 0 && ztcore::ZT_Identity_verify(self.capi, data.as_ptr() as *const c_void, data.len() as c_uint, signature.as_ptr() as *const c_void, signature.len() as c_uint) != 0 }
    }
}

impl Hash for Identity {
    #[inline(always)]
    fn hash<H: Hasher>(&self, state: &mut H) {
        (self.address.0 | (self.type_.to_u64().unwrap_or(0) << 40)).hash(state);
    }
}

impl PartialEq for Identity {
    #[inline(always)]
    fn eq(&self, other: &Self) -> bool {
        unsafe { ztcore::ZT_Identity_compare(self.capi, other.capi) == 0 }
    }
}

impl Eq for Identity {}

impl Ord for Identity {
    fn cmp(&self, b: &Self) -> Ordering {
        let c = unsafe { ztcore::ZT_Identity_compare(self.capi, b.capi) };
        if c < 0 {
            Ordering::Less
        } else if c > 0 {
            Ordering::Greater
        } else {
            Ordering::Equal
        }
    }
}

impl PartialOrd for Identity {
    #[inline(always)]
    fn partial_cmp(&self, b: &Self) -> Option<Ordering> {
        Some(self.cmp(b))
    }
}

impl Clone for Identity {
    #[inline(always)]
    fn clone(&self) -> Identity {
        unsafe {
            return Identity::new_from_capi(ztcore::ZT_Identity_clone(self.capi), true);
        }
    }
}

impl Drop for Identity {
    #[inline(always)]
    fn drop(&mut self) {
        if self.requires_delete {
            unsafe {
                ztcore::ZT_Identity_delete(self.capi);
            }
        }
    }
}

impl ToString for Identity {
    #[inline(always)]
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
    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result { formatter.write_str("ZeroTier Identity in string format") }
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

#[cfg(test)]
mod tests {
    use crate::*;

    #[test]
    fn identity() {
        let test1 = Identity::new_generate(IdentityType::Curve25519);
        assert!(test1.is_ok());
        let test1 = test1.ok().unwrap();
        assert!(test1.has_private());

        let test2 = Identity::new_generate(IdentityType::NistP384);
        assert!(test2.is_ok());
        let test2 = test2.ok().unwrap();

        println!("test type 0: {}", test1.to_secret_string());
        println!("test type 1: {}", test2.to_secret_string());

        assert!(test1.clone() == test1);

        let test12 = Identity::new_from_string(test1.to_string().as_str());
        assert!(test12.is_ok());
        let test12 = test12.ok().unwrap();
        assert!(!test12.has_private());
        let test22 = Identity::new_from_string(test2.to_string().as_str());
        assert!(test22.is_ok());
        let test22 = test22.ok().unwrap();
        assert!(test1 == test12);
        assert!(test2 == test22);

        println!("test type 0, from string: {}", test12.to_string());
        println!("test type 1, from string: {}", test22.to_string());

        let from_str_fail = Identity::new_from_string("asdf:foo:invalid");
        assert!(from_str_fail.is_err());

        let mut to_sign: [u8; 4] = [1, 2, 3, 4];

        let signed = test1.sign(&to_sign);
        assert!(signed.is_ok());
        let signed = signed.ok().unwrap();
        assert!(test1.verify(&to_sign, signed.as_ref()));
        to_sign[0] = 2;
        assert!(!test1.verify(&to_sign, signed.as_ref()));
        to_sign[0] = 1;

        let signed = test2.sign(&to_sign);
        assert!(signed.is_ok());
        let signed = signed.ok().unwrap();
        assert!(test2.verify(&to_sign, signed.as_ref()));
        to_sign[0] = 2;
        assert!(!test2.verify(&to_sign, signed.as_ref()));
    }
}
