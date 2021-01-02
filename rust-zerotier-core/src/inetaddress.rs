use std::ffi::CString;
use std::mem::{MaybeUninit, transmute, size_of};

use serde::{Deserialize, Serialize};

use crate::*;
use crate::bindings::capi as ztcore;

/// Opaque structure that can hold an IPv4 or IPv6 address.
pub struct InetAddress {
    // This must be the same size as ZT_InetAddress in zerotier.h. This is
    // checked in tests.
    bits: [u64; (ztcore::ZT_SOCKADDR_STORAGE_SIZE / 8) as usize]
}

impl InetAddress {
    #[inline(always)]
    pub fn new() -> InetAddress {
        InetAddress {
            bits: [0; (ztcore::ZT_SOCKADDR_STORAGE_SIZE / 8) as usize]
        }
    }

    pub fn new_from_string(s: &str) -> Option<InetAddress> {
        let mut a = InetAddress::new();
        let cs = CString::new(s);
        if cs.is_ok() {
            let cs = cs.unwrap();
            unsafe {
                if ztcore::ZT_InetAddress_fromString(a.as_mut_ptr(), cs.as_ptr()) == 0 {
                    return None
                }
            }
        }
        Some(a)
    }

    #[inline(always)]
    pub(crate) unsafe fn transmute_capi(a: &ztcore::ZT_InetAddress) -> &InetAddress {
        unsafe {
            transmute(a)
        }
    }

    pub(crate) fn new_from_capi(a: ztcore::ZT_InetAddress) -> Option<InetAddress> {
        if a.bits[0] != 0 {
            Some(InetAddress {
                bits: a.bits
            })
        } else {
            None
        }
    }

    pub fn clear(&mut self) {
        for i in self.bits.iter_mut() {
            *i = 0;
        }
    }

    #[inline(always)]
    pub fn is_nil(&self) -> bool {
        self.bits[0] == 0 // if ss_family != 0, this will not be zero
    }

    #[inline(always)]
    pub(crate) fn as_ptr(&self) -> *const ztcore::ZT_InetAddress {
        unsafe {
            transmute(self as *const InetAddress)
        }
    }

    #[inline(always)]
    pub(crate) fn as_mut_ptr(&mut self) -> *mut ztcore::ZT_InetAddress {
        unsafe {
            transmute(self as *mut InetAddress)
        }
    }
}

impl ToString for InetAddress {
    fn to_string(&self) -> String {
        let mut buf: MaybeUninit<[c_char; 128]> = MaybeUninit::uninit();
        unsafe {
            return cstr_to_string(ztcore::ZT_InetAddress_toString(self.as_ptr(), (*buf.as_mut_ptr()).as_mut_ptr(), 128), 128);
        }
    }
}

impl From<&str> for InetAddress {
    fn from(s: &str) -> InetAddress {
        let a = InetAddress::new_from_string(s);
        if a.is_none() {
            return InetAddress::new();
        }
        a.unwrap()
    }
}

impl serde::Serialize for InetAddress {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: serde::Serializer {
        serializer.serialize_str(self.to_string().as_str())
    }
}

struct InetAddressVisitor;

impl<'de> serde::de::Visitor<'de> for InetAddressVisitor {
    type Value = InetAddress;

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("InetAddress value in string form")
    }

    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E> where E: serde::de::Error {
        let id = InetAddress::new_from_string(s);
        if id.is_none() {
            return Err(serde::de::Error::invalid_value(serde::de::Unexpected::Str(s), &self));
        }
        return Ok(id.unwrap() as Self::Value);
    }
}

impl<'de> serde::Deserialize<'de> for InetAddress {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: serde::Deserializer<'de> {
        deserializer.deserialize_str(InetAddressVisitor)
    }
}

#[cfg(test)]
mod tests {
    use std::mem::{size_of, zeroed};

    use crate::*;

    #[test]
    fn type_sizes() {
        assert_eq!(size_of::<ztcore::ZT_InetAddress>(), size_of::<InetAddress>());
    }
}
