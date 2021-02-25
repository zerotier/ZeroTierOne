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
use std::os::raw::{c_uint, c_void};

use num_derive::{FromPrimitive, ToPrimitive};
use num_traits::FromPrimitive;

use crate::*;
use crate::capi as ztcore;
use std::net::{SocketAddr, IpAddr, Ipv4Addr};

// WARNING: here be dragons! This defines an opaque blob in Rust that shadows
// and is of the exact size as an opaque blob in C that shadows and is the
// exact size of struct sockaddr_storage. This Rust code makes use of a good
// deal of transmute() magic to save copying and allow these identically sized
// blobs to be freely cast to one another. That the sizes are correct is
// checked statically in the C++ code and in the tests in the Rust code.

#[derive(FromPrimitive, ToPrimitive, PartialEq, Eq)]
pub enum IpScope {
    None = ztcore::ZT_InetAddress_IpScope_ZT_IP_SCOPE_NONE as isize,
    Multicast = ztcore::ZT_InetAddress_IpScope_ZT_IP_SCOPE_MULTICAST as isize,
    Loopback = ztcore::ZT_InetAddress_IpScope_ZT_IP_SCOPE_LOOPBACK as isize,
    PseudoPrivate = ztcore::ZT_InetAddress_IpScope_ZT_IP_SCOPE_PSEUDOPRIVATE as isize,
    Global = ztcore::ZT_InetAddress_IpScope_ZT_IP_SCOPE_GLOBAL as isize,
    LinkLocal = ztcore::ZT_InetAddress_IpScope_ZT_IP_SCOPE_LINK_LOCAL as isize,
    Shared = ztcore::ZT_InetAddress_IpScope_ZT_IP_SCOPE_SHARED as isize,
    Private = ztcore::ZT_InetAddress_IpScope_ZT_IP_SCOPE_PRIVATE as isize
}

impl IpScope {
    pub fn to_str(&self) -> &'static str {
        match *self {
            IpScope::None => "None",
            IpScope::Multicast => "Multicast",
            IpScope::Loopback => "Loopback",
            IpScope::PseudoPrivate => "PseudoPrivate",
            IpScope::Global => "Global",
            IpScope::LinkLocal => "LinkLocal",
            IpScope::Shared => "Shared",
            IpScope::Private => "Private",
        }
    }
}

#[derive(PartialEq, Eq)]
pub enum InetAddressFamily {
    Nil,
    IPv4,
    IPv6
}

impl InetAddressFamily {
    pub fn to_str(&self) -> &'static str {
        match *self {
            InetAddressFamily::Nil => "Nil",
            InetAddressFamily::IPv4 => "IPv4",
            InetAddressFamily::IPv6 => "IPv6",
        }
    }
}

pub const IPV4_INADDR_ANY: [u8; 4] = [0; 4];
pub const IPV6_INADDR_ANY: [u8; 16] = [0; 16];

/// Opaque structure that can hold an IPv4 or IPv6 address.
pub struct InetAddress {
    // This must be the same size as ZT_InetAddress in zerotier.h. This is
    // checked in tests.
    bits: [u64; (ztcore::ZT_SOCKADDR_STORAGE_SIZE / 8) as usize]
}

impl InetAddress {
    /// Create a new empty and "nil" InetAddress.
    pub fn new() -> InetAddress {
        InetAddress {
            bits: [0; (ztcore::ZT_SOCKADDR_STORAGE_SIZE / 8) as usize]
        }
    }

    /// Create an IPv4 0.0.0.0 InetAddress
    pub fn new_ipv4_any(port: u16) -> InetAddress {
        let mut ia = InetAddress::new();
        unsafe {
            ztcore::ZT_InetAddress_setIpBytes(ia.as_capi_mut_ptr(), IPV4_INADDR_ANY.as_ptr().cast(), 4, port as c_uint);
        }
        ia
    }

    /// Create an IPv6 ::0 InetAddress
    pub fn new_ipv6_any(port: u16) -> InetAddress {
        let mut ia = InetAddress::new();
        unsafe {
            ztcore::ZT_InetAddress_setIpBytes(ia.as_capi_mut_ptr(), IPV6_INADDR_ANY.as_ptr().cast(), 16, port as c_uint);
        }
        ia
    }

    /// Create from a 4-byte IPv4 IP or a 16-byte IPv6 IP.
    /// Returns None if ip is not 4 or 16 bytes.
    pub fn new_from_ip_bytes(ip: &[u8], port: u16) -> Option<InetAddress> {
        if ip.len() != 4 && ip.len() != 16 {
            return None;
        }
        let mut a = InetAddress::new();
        unsafe {
            ztcore::ZT_InetAddress_setIpBytes(a.as_capi_mut_ptr(), ip.as_ptr() as *const c_void, ip.len() as c_uint, port as c_uint);
        }
        Some(a)
    }

    /// Create from an InetAddress in string form.
    /// Returns None if the string is not valid.
    pub fn new_from_string(s: &str) -> Option<InetAddress> {
        let mut a = InetAddress::new();
        let cs = CString::new(s);
        if cs.is_ok() {
            let cs = cs.unwrap();
            unsafe {
                if ztcore::ZT_InetAddress_fromString(a.as_capi_mut_ptr(), cs.as_ptr()) == 0 {
                    return None
                }
            }
        }
        Some(a)
    }

    /// Unsafely transmute a raw sockaddr_storage structure into an InetAddress.
    /// The type S MUST have a size equal to the size of this type and the
    /// OS's sockaddr_storage. If not, this may crash.
    #[inline(always)]
    pub unsafe fn transmute_raw_sockaddr_storage<S>(ss: &S) -> &InetAddress {
        transmute(ss)
    }

    /// Transmute a ZT_InetAddress from the core into a reference to a Rust
    /// InetAddress containing exactly the same data. The returned reference
    /// of course only remains valid so long as the ZT_InetAddress remains
    /// valid.
    #[inline(always)]
    pub(crate) fn transmute_capi(a: &ztcore::ZT_InetAddress) -> &InetAddress {
        unsafe {
            transmute(a)
        }
    }

    /// Create an InetAddress by copying a ZT_InetAddress instead of transmuting.
    /// Returns None if the input is a nil address.
    pub(crate) fn new_from_capi(a: &ztcore::ZT_InetAddress) -> Option<InetAddress> {
        if a.bits[0] != 0 {
            Some(InetAddress {
                bits: a.bits
            })
        } else {
            None
        }
    }

    /// Clear and set to the "nil" value.
    pub fn clear(&mut self) {
        for i in self.bits.iter_mut() {
            *i = 0;
        }
    }

    /// Returns true if this InetAddress holds nothing.
    #[inline(always)]
    pub fn is_nil(&self) -> bool {
        self.bits[0] == 0 // if ss_family != 0, this will not be zero
    }

    #[inline(always)]
    pub(crate) fn as_capi_ptr(&self) -> *const ztcore::ZT_InetAddress {
        unsafe {
            transmute(self as *const InetAddress)
        }
    }

    #[inline(always)]
    pub(crate) fn as_capi_mut_ptr(&mut self) -> *mut ztcore::ZT_InetAddress {
        unsafe {
            transmute(self as *mut InetAddress)
        }
    }

    #[inline(always)]
    pub fn port(&self) -> u16 {
        unsafe {
            ztcore::ZT_InetAddress_port(self.as_capi_ptr()) as u16
        }
    }

    #[inline(always)]
    pub fn set_port(&mut self, port: u16) {
        unsafe {
            ztcore::ZT_InetAddress_setPort(self.as_capi_mut_ptr(), port as c_uint);
        }
    }

    /// Get the network scope of the IP in this object.
    #[inline(always)]
    pub fn ip_scope(&self) -> IpScope {
        unsafe {
            IpScope::from_i32(ztcore::ZT_InetAddress_ipScope(self.as_capi_ptr()) as i32).unwrap_or(IpScope::None)
        }
    }

    #[inline(always)]
    pub fn is_v4(&self) -> bool {
        unsafe {
            ztcore::ZT_InetAddress_isV4(self.as_capi_ptr()) != 0
        }
    }

    #[inline(always)]
    pub fn is_v6(&self) -> bool {
        unsafe {
            ztcore::ZT_InetAddress_isV6(self.as_capi_ptr()) != 0
        }
    }

    /// Get the address family of this InetAddress.
    pub fn family(&self) -> InetAddressFamily {
        if !self.is_nil() {
            unsafe {
                if ztcore::ZT_InetAddress_isV4(self.as_capi_ptr()) != 0 {
                    return InetAddressFamily::IPv4;
                }
                if ztcore::ZT_InetAddress_isV6(self.as_capi_ptr()) != 0 {
                    return InetAddressFamily::IPv6;
                }
            }
        }
        InetAddressFamily::Nil
    }

    /// Convert to std::net::SocketAddr for use with std::net APIs
    pub fn to_socketaddr(&self) -> Option<SocketAddr> {
        let mut buf: MaybeUninit<[u8; 16]> = MaybeUninit::uninit();
        let len;
        let buf = unsafe {
            len = ztcore::ZT_InetAddress_ipBytes(self.as_capi_ptr(), buf.as_mut_ptr().cast());
            buf.assume_init()
        };
        if len == 4 {
            Some(SocketAddr::new(IpAddr::from(Ipv4Addr::new(buf[0], buf[1], buf[2], buf[3])), self.port()))
        } else if len == 16 {
            Some(SocketAddr::new(IpAddr::from(buf), self.port()))
        } else {
            None
        }
    }
}

impl ToString for InetAddress {
    fn to_string(&self) -> String {
        let mut buf: MaybeUninit<[c_char; 128]> = MaybeUninit::uninit();
        unsafe {
            return cstr_to_string(ztcore::ZT_InetAddress_toString(self.as_capi_ptr(), (*buf.as_mut_ptr()).as_mut_ptr(), 128), 128);
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

impl Clone for InetAddress {
    #[inline(always)]
    fn clone(&self) -> Self {
        InetAddress{
            bits: self.bits
        }
    }
}

impl Ord for InetAddress {
    fn cmp(&self, other: &Self) -> Ordering {
        unsafe {
            if ztcore::ZT_InetAddress_lessThan(self.as_capi_ptr(), other.as_capi_ptr()) != 0 {
                return Ordering::Less;
            } else if ztcore::ZT_InetAddress_lessThan(other.as_capi_ptr(), self.as_capi_ptr()) != 0 {
                return Ordering::Greater;
            }
            return Ordering::Equal;
        }
    }
}

impl PartialOrd for InetAddress {
    #[inline(always)]
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl PartialEq for InetAddress {
    #[inline(always)]
    fn eq(&self, other: &Self) -> bool {
        self.to_string() == other.to_string()
    }
}

impl Eq for InetAddress {}

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
    use std::mem::size_of;

    use crate::*;

    #[test]
    fn type_sizes() {
        assert_eq!(size_of::<ztcore::ZT_InetAddress>(), size_of::<InetAddress>());
    }
}
