// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::cmp::Ordering;
use std::hash::{Hash, Hasher};
use std::mem::{size_of, transmute_copy, zeroed, MaybeUninit};
use std::net::{IpAddr, Ipv4Addr, Ipv6Addr, SocketAddr, SocketAddrV4, SocketAddrV6, ToSocketAddrs};
use std::ptr::{copy_nonoverlapping, null, slice_from_raw_parts, write_bytes};
use std::str::FromStr;

use serde::{Deserialize, Deserializer, Serialize, Serializer};

use zerotier_utils::buffer::Buffer;
use zerotier_utils::error::{InvalidFormatError, InvalidParameterError};
use zerotier_utils::marshalable::{Marshalable, UnmarshalError};

#[cfg(windows)]
use winapi::um::winsock2;

#[allow(non_camel_case_types)]
#[cfg(not(windows))]
type sockaddr = libc::sockaddr;

#[allow(non_camel_case_types)]
#[cfg(not(windows))]
type sockaddr_in = libc::sockaddr_in;

#[allow(non_camel_case_types)]
#[cfg(not(windows))]
type sockaddr_in6 = libc::sockaddr_in6;

#[allow(non_camel_case_types)]
#[cfg(not(windows))]
type sockaddr_storage = libc::sockaddr_storage;

#[allow(non_camel_case_types)]
#[cfg(not(windows))]
type in6_addr = libc::in6_addr;

#[cfg(all(not(target_os = "windows"), not(target_os = "linux")))]
pub type AddressFamilyType = u8;

#[cfg(target_os = "linux")]
pub type AddressFamilyType = u16;

pub const AF_INET: AddressFamilyType = libc::AF_INET as AddressFamilyType;
pub const AF_INET6: AddressFamilyType = libc::AF_INET6 as AddressFamilyType;

#[repr(u8)]
pub enum IpScope {
    None = 0,
    Multicast = 1,
    Loopback = 2,
    PseudoPrivate = 3,
    Global = 4,
    LinkLocal = 5,
    Shared = 6,
    Private = 7,
}

/// An IPv4 or IPv6 socket address that directly encapsulates C sockaddr types.
///
/// The ZeroTier core uses this in preference to std::net stuff so this can be
/// directly used via the C API or with C socket I/O functions.
///
/// This supports into() and from() the std::net types and also has a custom
/// serde serializer that serializes it in ZT protocol binary form for binary
/// formats and canonical ZT string form for string formats.
///
/// Unfortunately this is full of unsafe because it's a union, but the code is
/// not complex and doesn't allocate anything.
#[repr(C)]
pub union InetAddress {
    sa: sockaddr,
    sin: sockaddr_in,
    sin6: sockaddr_in6,
    ss: sockaddr_storage, // some external code may expect the struct to be this full length
}

impl ToSocketAddrs for InetAddress {
    type Iter = std::iter::Once<SocketAddr>;

    #[inline(always)]
    fn to_socket_addrs(&self) -> std::io::Result<Self::Iter> {
        self.try_into().map_or_else(
            |_| Err(std::io::Error::new(std::io::ErrorKind::Other, "not an IP address")),
            |sa| Ok(std::iter::once(sa)),
        )
    }
}

impl TryInto<IpAddr> for InetAddress {
    type Error = InvalidParameterError;

    #[inline(always)]
    fn try_into(self) -> Result<IpAddr, Self::Error> {
        (&self).try_into()
    }
}

impl TryInto<IpAddr> for &InetAddress {
    type Error = InvalidParameterError;

    #[inline(always)]
    fn try_into(self) -> Result<IpAddr, Self::Error> {
        match unsafe { self.sa.sa_family } {
            AF_INET => Ok(IpAddr::V4(Ipv4Addr::from(unsafe { self.sin.sin_addr.s_addr.to_ne_bytes() }))),
            AF_INET6 => Ok(IpAddr::V6(Ipv6Addr::from(unsafe { self.sin6.sin6_addr.s6_addr }))),
            _ => Err(InvalidParameterError("not an IP address")),
        }
    }
}

impl TryInto<Ipv4Addr> for InetAddress {
    type Error = InvalidParameterError;

    #[inline(always)]
    fn try_into(self) -> Result<Ipv4Addr, Self::Error> {
        (&self).try_into()
    }
}

impl TryInto<Ipv4Addr> for &InetAddress {
    type Error = InvalidParameterError;

    #[inline(always)]
    fn try_into(self) -> Result<Ipv4Addr, Self::Error> {
        match unsafe { self.sa.sa_family } {
            AF_INET => Ok(Ipv4Addr::from(unsafe { self.sin.sin_addr.s_addr.to_ne_bytes() })),
            _ => Err(InvalidParameterError("not an IPv4 address")),
        }
    }
}

impl TryInto<Ipv6Addr> for InetAddress {
    type Error = InvalidParameterError;

    #[inline(always)]
    fn try_into(self) -> Result<Ipv6Addr, Self::Error> {
        (&self).try_into()
    }
}

impl TryInto<Ipv6Addr> for &InetAddress {
    type Error = InvalidParameterError;

    #[inline(always)]
    fn try_into(self) -> Result<Ipv6Addr, Self::Error> {
        match unsafe { self.sa.sa_family } {
            AF_INET6 => Ok(Ipv6Addr::from(unsafe { self.sin6.sin6_addr.s6_addr })),
            _ => Err(InvalidParameterError("not an IPv6 address")),
        }
    }
}

impl TryInto<SocketAddr> for InetAddress {
    type Error = InvalidParameterError;

    #[inline(always)]
    fn try_into(self) -> Result<SocketAddr, Self::Error> {
        (&self).try_into()
    }
}

impl TryInto<SocketAddr> for &InetAddress {
    type Error = InvalidParameterError;

    #[inline(always)]
    fn try_into(self) -> Result<SocketAddr, Self::Error> {
        unsafe {
            match self.sa.sa_family {
                AF_INET => Ok(SocketAddr::V4(SocketAddrV4::new(
                    Ipv4Addr::from(self.sin.sin_addr.s_addr.to_ne_bytes()),
                    u16::from_be(self.sin.sin_port as u16),
                ))),
                AF_INET6 => Ok(SocketAddr::V6(SocketAddrV6::new(
                    Ipv6Addr::from(self.sin6.sin6_addr.s6_addr),
                    u16::from_be(self.sin6.sin6_port as u16),
                    0,
                    0,
                ))),
                _ => Err(InvalidParameterError("not an IP address")),
            }
        }
    }
}

impl TryInto<SocketAddrV4> for InetAddress {
    type Error = InvalidParameterError;

    #[inline(always)]
    fn try_into(self) -> Result<SocketAddrV4, Self::Error> {
        (&self).try_into()
    }
}

impl TryInto<SocketAddrV4> for &InetAddress {
    type Error = InvalidParameterError;

    #[inline(always)]
    fn try_into(self) -> Result<SocketAddrV4, Self::Error> {
        unsafe {
            match self.sa.sa_family {
                AF_INET => Ok(SocketAddrV4::new(
                    Ipv4Addr::from(self.sin.sin_addr.s_addr.to_ne_bytes()),
                    u16::from_be(self.sin.sin_port as u16),
                )),
                _ => Err(InvalidParameterError("not an IPv4 address")),
            }
        }
    }
}

impl TryInto<SocketAddrV6> for InetAddress {
    type Error = InvalidParameterError;

    #[inline(always)]
    fn try_into(self) -> Result<SocketAddrV6, Self::Error> {
        (&self).try_into()
    }
}

impl TryInto<SocketAddrV6> for &InetAddress {
    type Error = InvalidParameterError;

    #[inline(always)]
    fn try_into(self) -> Result<SocketAddrV6, Self::Error> {
        unsafe {
            match self.sa.sa_family {
                AF_INET6 => Ok(SocketAddrV6::new(
                    Ipv6Addr::from(self.sin6.sin6_addr.s6_addr),
                    u16::from_be(self.sin6.sin6_port as u16),
                    0,
                    0,
                )),
                _ => Err(InvalidParameterError("not an IPv6 address")),
            }
        }
    }
}

impl From<&IpAddr> for InetAddress {
    #[inline(always)]
    fn from(ip: &IpAddr) -> Self {
        match ip {
            IpAddr::V4(ip4) => Self::from(ip4),
            IpAddr::V6(ip6) => Self::from(ip6),
        }
    }
}

impl From<IpAddr> for InetAddress {
    #[inline(always)]
    fn from(ip: IpAddr) -> Self {
        Self::from(&ip)
    }
}

impl From<&Ipv4Addr> for InetAddress {
    #[inline(always)]
    fn from(ip4: &Ipv4Addr) -> Self {
        Self::from_ip_port(&ip4.octets(), 0)
    }
}

impl From<Ipv4Addr> for InetAddress {
    #[inline(always)]
    fn from(ip4: Ipv4Addr) -> Self {
        Self::from_ip_port(&ip4.octets(), 0)
    }
}

impl From<&Ipv6Addr> for InetAddress {
    #[inline(always)]
    fn from(ip6: &Ipv6Addr) -> Self {
        Self::from_ip_port(&ip6.octets(), 0)
    }
}

impl From<Ipv6Addr> for InetAddress {
    #[inline(always)]
    fn from(ip6: Ipv6Addr) -> Self {
        Self::from_ip_port(&ip6.octets(), 0)
    }
}

impl From<&SocketAddr> for InetAddress {
    #[inline(always)]
    fn from(sa: &SocketAddr) -> Self {
        match sa {
            SocketAddr::V4(sa4) => Self::from(sa4),
            SocketAddr::V6(sa6) => Self::from(sa6),
        }
    }
}

impl From<SocketAddr> for InetAddress {
    #[inline(always)]
    fn from(sa: SocketAddr) -> Self {
        Self::from(&sa)
    }
}

impl From<&SocketAddrV4> for InetAddress {
    #[inline(always)]
    fn from(sa: &SocketAddrV4) -> Self {
        Self::from_ip_port(&sa.ip().octets(), sa.port())
    }
}

impl From<SocketAddrV4> for InetAddress {
    #[inline(always)]
    fn from(sa: SocketAddrV4) -> Self {
        Self::from_ip_port(&sa.ip().octets(), sa.port())
    }
}

impl From<&SocketAddrV6> for InetAddress {
    #[inline(always)]
    fn from(sa: &SocketAddrV6) -> Self {
        Self::from_ip_port(&sa.ip().octets(), sa.port())
    }
}

impl From<SocketAddrV6> for InetAddress {
    #[inline(always)]
    fn from(sa: SocketAddrV6) -> Self {
        Self::from_ip_port(&sa.ip().octets(), sa.port())
    }
}

impl Clone for InetAddress {
    #[inline(always)]
    fn clone(&self) -> Self {
        unsafe { transmute_copy(self) }
    }
}

impl Default for InetAddress {
    #[inline(always)]
    fn default() -> Self {
        Self::new()
    }
}

impl std::fmt::Debug for InetAddress {
    #[inline(always)]
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(self.to_string().as_str())
    }
}

// Just has to be large enough to store any binary marshalled InetAddress
const TEMP_SERIALIZE_BUFFER_SIZE: usize = 24;

impl Serialize for InetAddress {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        if serializer.is_human_readable() {
            serializer.serialize_str(self.to_string().as_str())
        } else {
            let mut tmp: Buffer<TEMP_SERIALIZE_BUFFER_SIZE> = Buffer::new();
            assert!(self.marshal(&mut tmp).is_ok());
            serializer.serialize_bytes(tmp.as_bytes())
        }
    }
}

struct InetAddressVisitor;

impl<'de> serde::de::Visitor<'de> for InetAddressVisitor {
    type Value = InetAddress;

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("an InetAddress")
    }

    fn visit_bytes<E>(self, v: &[u8]) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        if v.len() <= TEMP_SERIALIZE_BUFFER_SIZE {
            let mut tmp: Buffer<TEMP_SERIALIZE_BUFFER_SIZE> = Buffer::new();
            let _ = tmp.append_bytes(v);
            let mut cursor = 0;
            InetAddress::unmarshal(&tmp, &mut cursor).map_err(|e| E::custom(e.to_string()))
        } else {
            Err(E::custom("object too large"))
        }
    }

    fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        InetAddress::from_str(v).map_err(|e| E::custom(e.to_string()))
    }
}

impl<'de> Deserialize<'de> for InetAddress {
    fn deserialize<D>(deserializer: D) -> Result<InetAddress, D::Error>
    where
        D: Deserializer<'de>,
    {
        if deserializer.is_human_readable() {
            deserializer.deserialize_str(InetAddressVisitor)
        } else {
            deserializer.deserialize_bytes(InetAddressVisitor)
        }
    }
}

impl InetAddress {
    /// Get a new zero/nil InetAddress.
    #[inline(always)]
    pub fn new() -> InetAddress {
        unsafe { zeroed() }
    }

    /// Construct from IP and port.
    /// If the IP is not either 4 or 16 bytes in length, a nil/0 InetAddress is returned.
    #[inline(always)]
    pub fn from_ip_port(ip: &[u8], port: u16) -> InetAddress {
        unsafe {
            let mut c = MaybeUninit::<InetAddress>::uninit().assume_init(); // gets zeroed in set()
            c.set(ip, port);
            c
        }
    }

    /// Zero the contents of this InetAddress.
    #[inline(always)]
    pub fn zero(&mut self) {
        unsafe { write_bytes((self as *mut Self).cast::<u8>(), 0, size_of::<Self>()) };
    }

    /// Get an instance of 127.0.0.1/port
    pub fn ipv4_loopback(port: u16) -> InetAddress {
        let mut addr = Self::new();
        addr.sin.sin_family = AF_INET.into();
        addr.sin.sin_port = port.to_be().into();
        addr.sin.sin_addr.s_addr = (0x7f000001 as u32).to_be();
        addr
    }

    /// Get an instance of 0.0.0.0/0
    pub fn ipv4_any() -> InetAddress {
        let mut addr = Self::new();
        addr.sin.sin_family = AF_INET.into();
        addr
    }

    /// Get an instance of ::1/port
    pub fn ipv6_loopback(port: u16) -> InetAddress {
        let mut addr = Self::new();
        addr.sin6.sin6_family = AF_INET6.into();
        addr.sin6.sin6_port = port.to_be().into();
        unsafe {
            *((&mut (addr.sin6.sin6_addr) as *mut in6_addr).cast::<u8>().offset(15)) = 1;
        }
        addr
    }

    /// Get an instance of ::0/0
    pub fn ipv6_any() -> InetAddress {
        let mut addr = Self::new();
        addr.sin6.sin6_family = AF_INET6.into();
        addr
    }

    /// Returns true if this InetAddress is the nil value (zero).
    #[inline(always)]
    pub fn is_nil(&self) -> bool {
        unsafe { self.sa.sa_family == 0 }
    }

    /// Check if this is an IPv4 address.
    #[inline(always)]
    pub fn is_ipv4(&self) -> bool {
        unsafe { self.sa.sa_family as AddressFamilyType == AF_INET }
    }

    /// Check if this is an IPv6 address.
    #[inline(always)]
    pub fn is_ipv6(&self) -> bool {
        unsafe { self.sa.sa_family as AddressFamilyType == AF_INET6 }
    }

    /// Check if this is either an IPv4 or an IPv6 address.
    #[inline(always)]
    pub fn is_ip(&self) -> bool {
        let family = unsafe { self.sa.sa_family } as AddressFamilyType;
        family == AF_INET || family == AF_INET6
    }

    /// Get the address family of this InetAddress: AF_INET, AF_INET6, or 0 if uninitialized.
    #[inline(always)]
    pub fn family(&self) -> AddressFamilyType {
        unsafe { self.sa.sa_family as AddressFamilyType }
    }

    /// Get a pointer to the C "sockaddr" structure and the size of the returned structure in bytes.
    /// This is useful for interacting with C-level socket APIs. This returns a null pointer if
    /// the address is not initialized.
    #[inline(always)]
    pub fn c_sockaddr(&self) -> (*const (), usize) {
        unsafe {
            match self.sa.sa_family as AddressFamilyType {
                AF_INET => ((&self.sin as *const sockaddr_in).cast(), size_of::<sockaddr_in>()),
                AF_INET6 => ((&self.sin6 as *const sockaddr_in6).cast(), size_of::<sockaddr_in6>()),
                _ => (null(), 0),
            }
        }
    }

    /// Set the IP and port of this InetAddress.
    /// Whether this is IPv4 or IPv6 is inferred from the size of ip[], which must be
    /// either 4 or 16 bytes. The family (AF_INET or AF_INET6) is returned, or zero on
    /// failure.
    pub fn set<T: AsRef<[u8]>>(&mut self, ip: T, port: u16) -> AddressFamilyType {
        self.zero();
        let port = port.to_be();
        let ip2 = ip.as_ref();
        unsafe {
            if ip2.len() == 4 {
                self.sin.sin_family = AF_INET.into();
                self.sin.sin_port = port.into();
                copy_nonoverlapping(ip2.as_ptr(), (&mut self.sin.sin_addr.s_addr as *mut u32).cast::<u8>(), 4);
                AF_INET
            } else if ip2.len() == 16 {
                self.sin6.sin6_family = AF_INET6.into();
                self.sin6.sin6_port = port.into();
                copy_nonoverlapping(ip2.as_ptr(), (&mut self.sin6.sin6_addr as *mut in6_addr).cast::<u8>(), 16);
                AF_INET6
            } else {
                0
            }
        }
    }

    /// Get raw IP bytes, with length dependent on address family (4 or 16).
    pub fn ip_bytes(&self) -> &[u8] {
        unsafe {
            match self.sa.sa_family as AddressFamilyType {
                AF_INET => &*(&self.sin.sin_addr.s_addr as *const u32).cast::<[u8; 4]>(),
                AF_INET6 => &*(&self.sin6.sin6_addr as *const in6_addr).cast::<[u8; 16]>(),
                _ => &[],
            }
        }
    }

    /// Get a Rust stdlib SocketAddr structure from this InetAddress.
    pub fn to_socketaddr(&self) -> Option<SocketAddr> {
        unsafe {
            match self.sa.sa_family as AddressFamilyType {
                AF_INET => Some(SocketAddr::V4(SocketAddrV4::new(
                    Ipv4Addr::from(self.sin.sin_addr.s_addr.to_ne_bytes()),
                    u16::from_be(self.sin.sin_port as u16),
                ))),
                AF_INET6 => Some(SocketAddr::V6(SocketAddrV6::new(
                    Ipv6Addr::from(self.sin6.sin6_addr.s6_addr),
                    u16::from_be(self.sin6.sin6_port as u16),
                    0,
                    0,
                ))),
                _ => None,
            }
        }
    }

    /// Get the IP port for this InetAddress.
    pub fn port(&self) -> u16 {
        unsafe {
            u16::from_be(match self.sa.sa_family as AddressFamilyType {
                AF_INET => self.sin.sin_port as u16,
                AF_INET6 => self.sin6.sin6_port as u16,
                _ => 0,
            })
        }
    }

    /// Set the IP port.
    ///
    /// This does nothing on uninitialized InetAddress objects. An address must first
    /// be initialized with an IP to select the correct address type.
    pub fn set_port(&mut self, port: u16) {
        let port = port.to_be();
        unsafe {
            match self.sa.sa_family as AddressFamilyType {
                AF_INET => self.sin.sin_port = port,
                AF_INET6 => self.sin6.sin6_port = port,
                _ => {}
            }
        }
    }

    /// Check whether this IP address is within a CIDR range
    ///
    /// The argument is a CIDR range in which the port is interpreted as the number of bits, e.g. 10.0.0.0/24.
    pub fn is_within(&self, cidr: &InetAddress) -> bool {
        unsafe {
            if self.sa.sa_family == cidr.sa.sa_family {
                let mut cidr_bits = cidr.port() as u32;
                match self.sa.sa_family as AddressFamilyType {
                    AF_INET => {
                        if cidr_bits <= 32 {
                            let discard_bits = 32 - cidr_bits;
                            if u32::from_be(self.sin.sin_addr.s_addr as u32).wrapping_shr(discard_bits)
                                == u32::from_be(cidr.sin.sin_addr.s_addr as u32).wrapping_shr(discard_bits)
                            {
                                return true;
                            }
                        }
                    }
                    AF_INET6 => {
                        if cidr_bits <= 128 {
                            let a = &self.sin6.sin6_addr.s6_addr;
                            let b = &cidr.sin6.sin6_addr.s6_addr;
                            let mut p = 0;
                            while cidr_bits >= 8 {
                                cidr_bits -= 8;
                                if a[p] != b[p] {
                                    return false;
                                }
                                p += 1;
                            }
                            let discard_bits = 8 - cidr_bits;
                            if a[p].wrapping_shr(discard_bits) == b[p].wrapping_shr(discard_bits) {
                                return true;
                            }
                        }
                    }
                    _ => {}
                }
            }
        }
        return false;
    }

    /// Get this IP address's scope as per RFC documents and what is advertised via BGP.
    pub fn scope(&self) -> IpScope {
        unsafe {
            match self.sa.sa_family as AddressFamilyType {
                AF_INET => {
                    let ip = self.sin.sin_addr.s_addr as u32;
                    let class_a = (ip >> 24) as u8;
                    match class_a {
                        0x00 | 0xff => IpScope::None, // 0.0.0.0/8 and 255.0.0.0/8 are not usable
                        0x0a => IpScope::Private,     // 10.0.0.0/8
                        0x7f => IpScope::Loopback,    // 127.0.0.0/8
                        0x64 => {
                            if (ip & 0xffc00000) == 0x64400000 {
                                // 100.64.0.0/10
                                IpScope::Private
                            } else {
                                IpScope::Global
                            }
                        }
                        0xa9 => {
                            if (ip & 0xffff0000) == 0xa9fe0000 {
                                // 169.254.0.0/16
                                IpScope::LinkLocal
                            } else {
                                IpScope::Global
                            }
                        }
                        0xac => {
                            if (ip & 0xfff00000) == 0xac100000 {
                                // 172.16.0.0/12
                                IpScope::Private
                            } else {
                                IpScope::Global
                            }
                        }
                        0xc0 => {
                            if (ip & 0xffff0000) == 0xc0a80000 || (ip & 0xffffff00) == 0xc0000200 {
                                // 192.168.0.0/16 and 192.0.2.0/24
                                IpScope::Private
                            } else {
                                IpScope::Global
                            }
                        }
                        0xc6 => {
                            if (ip & 0xfffe0000) == 0xc6120000 || (ip & 0xffffff00) == 0xc6336400 {
                                // 198.18.0.0/15 and 198.51.100.0/24
                                IpScope::Private
                            } else {
                                IpScope::Global
                            }
                        }
                        0xcb => {
                            if (ip & 0xffffff00) == 0xcb007100 {
                                // 203.0.113.0/24
                                IpScope::Private
                            } else {
                                IpScope::Global
                            }
                        }
                        _ => {
                            if [
                                0x06_u8, // 6.0.0.0/8 (US Army)
                                0x15_u8, // 21.0.0.0/8 (US DDN-RVN)
                                0x16_u8, // 22.0.0.0/8 (US DISA)
                                0x19_u8, // 25.0.0.0/8 (UK Ministry of Defense)
                                0x1a_u8, // 26.0.0.0/8 (US DISA)
                                0x1c_u8, // 28.0.0.0/8 (US DSI-North)
                                0x1d_u8, // 29.0.0.0/8 (US DISA)
                                0x1e_u8, // 30.0.0.0/8 (US DISA)
                                0x33_u8, // 51.0.0.0/8 (UK Department of Social Security)
                                0x37_u8, // 55.0.0.0/8 (US DoD)
                                0x38_u8, // 56.0.0.0/8 (US Postal Service)
                            ]
                            .contains(&class_a)
                            {
                                IpScope::PseudoPrivate
                            } else {
                                match ip >> 28 {
                                    0xe => IpScope::Multicast, // 224.0.0.0/4
                                    0xf => IpScope::Private,   // 240.0.0.0/4 ("reserved," usually unusable)
                                    _ => IpScope::Global,
                                }
                            }
                        }
                    }
                }
                AF_INET6 => {
                    let ip = &*(&(self.sin6.sin6_addr) as *const in6_addr).cast::<[u8; 16]>();
                    if (ip[0] & 0xf0) == 0xf0 {
                        if ip[0] == 0xff {
                            return IpScope::Multicast; // ff00::/8
                        }
                        if ip[0] == 0xfe && (ip[1] & 0xc0) == 0x80 {
                            let mut k: usize = 2;
                            while ip[k] == 0 && k < 15 {
                                k += 1;
                            }
                            return if k == 15 && ip[15] == 0x01 {
                                IpScope::Loopback // fe80::1/128
                            } else {
                                IpScope::LinkLocal // fe80::/10
                            };
                        }
                        if (ip[0] & 0xfe) == 0xfc {
                            return IpScope::Private; // fc00::/7
                        }
                    }
                    let mut k: usize = 0;
                    while ip[k] == 0 && k < 15 {
                        k += 1;
                    }
                    if k == 15 {
                        if ip[15] == 0x01 {
                            return IpScope::Loopback; // ::1/128
                        } else if ip[15] == 0x00 {
                            return IpScope::None; // ::/128
                        }
                    }
                    IpScope::Global
                }
                _ => IpScope::None,
            }
        }
    }

    /// Get only the IP portion of this address as a string.
    pub fn to_ip_string(&self) -> String {
        unsafe {
            match self.sa.sa_family as AddressFamilyType {
                AF_INET => {
                    let ip = &*(&self.sin.sin_addr.s_addr as *const u32).cast::<[u8; 4]>();
                    format!("{}.{}.{}.{}", ip[0], ip[1], ip[2], ip[3])
                }
                AF_INET6 => Ipv6Addr::from(*(&(self.sin6.sin6_addr) as *const in6_addr).cast::<[u8; 16]>()).to_string(),
                _ => String::from("(null)"),
            }
        }
    }
}

impl Marshalable for InetAddress {
    const MAX_MARSHAL_SIZE: usize = 19;

    fn marshal<const BL: usize>(&self, buf: &mut Buffer<BL>) -> Result<(), UnmarshalError> {
        unsafe {
            match self.sa.sa_family as AddressFamilyType {
                AF_INET => {
                    let b = buf.append_bytes_fixed_get_mut::<7>()?;
                    b[0] = 4;
                    copy_nonoverlapping((&self.sin.sin_addr.s_addr as *const u32).cast::<u8>(), b.as_mut_ptr().offset(1), 4);
                    b[5] = *(&self.sin.sin_port as *const u16).cast::<u8>();
                    b[6] = *(&self.sin.sin_port as *const u16).cast::<u8>().offset(1);
                }
                AF_INET6 => {
                    let b = buf.append_bytes_fixed_get_mut::<19>()?;
                    b[0] = 6;
                    copy_nonoverlapping(
                        (&(self.sin6.sin6_addr) as *const in6_addr).cast::<u8>(),
                        b.as_mut_ptr().offset(1),
                        16,
                    );
                    b[17] = *(&self.sin6.sin6_port as *const u16).cast::<u8>();
                    b[18] = *(&self.sin6.sin6_port as *const u16).cast::<u8>().offset(1);
                }
                _ => buf.append_u8(0)?,
            }
            Ok(())
        }
    }

    fn unmarshal<const BL: usize>(buf: &Buffer<BL>, cursor: &mut usize) -> Result<InetAddress, UnmarshalError> {
        let t = buf.read_u8(cursor)?;
        if t == 4 {
            let b: &[u8; 6] = buf.read_bytes_fixed(cursor)?;
            Ok(InetAddress::from_ip_port(&b[0..4], u16::from_be_bytes(b[4..6].try_into().unwrap())))
        } else if t == 6 {
            let b: &[u8; 18] = buf.read_bytes_fixed(cursor)?;
            Ok(InetAddress::from_ip_port(
                &b[0..16],
                u16::from_be_bytes(b[16..18].try_into().unwrap()),
            ))
        } else {
            Ok(InetAddress::new())
        }
    }
}

impl ToString for InetAddress {
    fn to_string(&self) -> String {
        unsafe {
            let mut s = self.to_ip_string();
            match self.sa.sa_family as AddressFamilyType {
                AF_INET => {
                    s.push('/');
                    s.push_str(u16::from_be(self.sin.sin_port as u16).to_string().as_str())
                }
                AF_INET6 => {
                    s.push('/');
                    s.push_str(u16::from_be(self.sin6.sin6_port as u16).to_string().as_str())
                }
                _ => {}
            }
            s
        }
    }
}

impl FromStr for InetAddress {
    type Err = InvalidFormatError;

    fn from_str(ip_string: &str) -> Result<Self, Self::Err> {
        let mut addr = InetAddress::new();
        let s = ip_string.trim();
        if !s.is_empty() {
            let (ip_str, port) = s.find('/').map_or_else(
                || (s, 0),
                |pos| {
                    let ss = s.split_at(pos);
                    let mut port_str = ss.1;
                    if port_str.starts_with('/') {
                        port_str = &port_str[1..];
                    }
                    (ss.0, u16::from_str_radix(port_str, 10).unwrap_or(0).to_be())
                },
            );
            IpAddr::from_str(ip_str).map_or_else(
                |_| Err(InvalidFormatError),
                |ip| {
                    unsafe {
                        match ip {
                            IpAddr::V4(v4) => {
                                addr.sin.sin_family = AF_INET.into();
                                addr.sin.sin_port = port.into();
                                copy_nonoverlapping(v4.octets().as_ptr(), (&mut (addr.sin.sin_addr.s_addr) as *mut u32).cast(), 4);
                            }
                            IpAddr::V6(v6) => {
                                addr.sin6.sin6_family = AF_INET6.into();
                                addr.sin6.sin6_port = port.into();
                                copy_nonoverlapping(v6.octets().as_ptr(), (&mut (addr.sin6.sin6_addr) as *mut in6_addr).cast(), 16);
                            }
                        }
                    }
                    Ok(addr)
                },
            )
        } else {
            Ok(addr)
        }
    }
}

impl PartialEq for InetAddress {
    fn eq(&self, other: &Self) -> bool {
        unsafe {
            if self.sa.sa_family == other.sa.sa_family {
                match self.sa.sa_family as AddressFamilyType {
                    AF_INET => self.sin.sin_port == other.sin.sin_port && self.sin.sin_addr.s_addr == other.sin.sin_addr.s_addr,
                    AF_INET6 => {
                        if self.sin6.sin6_port == other.sin6.sin6_port {
                            (*(&(self.sin6.sin6_addr) as *const in6_addr).cast::<[u8; 16]>())
                                .eq(&*(&(other.sin6.sin6_addr) as *const in6_addr).cast::<[u8; 16]>())
                        } else {
                            false
                        }
                    }
                    _ => true,
                }
            } else {
                false
            }
        }
    }
}

impl Eq for InetAddress {}

impl PartialOrd for InetAddress {
    #[inline(always)]
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

// Manually implement Ord to ensure consistent sort order across platforms, since we don't know exactly
// how sockaddr structs will be laid out.
impl Ord for InetAddress {
    fn cmp(&self, other: &Self) -> Ordering {
        unsafe {
            if self.sa.sa_family == other.sa.sa_family {
                match self.sa.sa_family as AddressFamilyType {
                    0 => Ordering::Equal,
                    AF_INET => {
                        let ip_ordering =
                            u32::from_be(self.sin.sin_addr.s_addr as u32).cmp(&u32::from_be(other.sin.sin_addr.s_addr as u32));
                        if ip_ordering == Ordering::Equal {
                            u16::from_be(self.sin.sin_port as u16).cmp(&u16::from_be(other.sin.sin_port as u16))
                        } else {
                            ip_ordering
                        }
                    }
                    AF_INET6 => {
                        let a = &*(&(self.sin6.sin6_addr) as *const in6_addr).cast::<[u8; 16]>();
                        let b = &*(&(other.sin6.sin6_addr) as *const in6_addr).cast::<[u8; 16]>();
                        let ip_ordering = a.cmp(b);
                        if ip_ordering == Ordering::Equal {
                            u16::from_be(self.sin6.sin6_port as u16).cmp(&u16::from_be(other.sin6.sin6_port as u16))
                        } else {
                            ip_ordering
                        }
                    }
                    _ => {
                        // This shouldn't be possible, but handle it for correctness.
                        (*slice_from_raw_parts((self as *const Self).cast::<u8>(), size_of::<Self>()))
                            .cmp(&*slice_from_raw_parts((other as *const Self).cast::<u8>(), size_of::<Self>()))
                    }
                }
            } else {
                match self.sa.sa_family as AddressFamilyType {
                    0 => Ordering::Less,
                    AF_INET => {
                        if other.sa.sa_family as AddressFamilyType == AF_INET6 {
                            Ordering::Less
                        } else {
                            self.sa.sa_family.cmp(&other.sa.sa_family)
                        }
                    }
                    AF_INET6 => {
                        if other.sa.sa_family as AddressFamilyType == AF_INET {
                            Ordering::Greater
                        } else {
                            self.sa.sa_family.cmp(&other.sa.sa_family)
                        }
                    }
                    _ => {
                        // This likewise should not be possible.
                        self.sa.sa_family.cmp(&other.sa.sa_family)
                    }
                }
            }
        }
    }
}

impl Hash for InetAddress {
    fn hash<H: Hasher>(&self, state: &mut H) {
        unsafe {
            state.write_u8(self.sa.sa_family as u8);

            match self.sa.sa_family as AddressFamilyType {
                AF_INET => {
                    state.write_u16(self.sin.sin_port as u16);
                    state.write_u32(self.sin.sin_addr.s_addr as u32);
                }
                AF_INET6 => {
                    state.write_u16(self.sin6.sin6_port as u16);
                    state.write(&*(&(self.sin6.sin6_addr) as *const in6_addr).cast::<[u8; 16]>());
                }
                _ => {}
            }
        }
    }
}

unsafe impl Send for InetAddress {}

#[cfg(test)]
mod tests {
    use std::mem::size_of;
    use std::str::FromStr;

    use crate::vl1::inetaddress::*;

    #[test]
    fn values() {
        assert_ne!(AF_INET, 0);
        assert_ne!(AF_INET6, 0);
        assert_ne!(AF_INET, AF_INET6);
    }

    #[test]
    fn set_get() {
        // ipv4
        let mut v = [0_u8; 4];
        for port in 0..=65535 {
            v.fill_with(|| rand::random());
            let mut addr = InetAddress::new();
            assert_ne!(addr.set(&v, port), 0);
            assert_eq!(addr.ip_bytes(), &v);
            assert_eq!(addr.port(), port);
        }

        // ipv6
        let mut v = [0_u8; 16];
        for port in 0..=65535 {
            v.fill_with(|| rand::random());
            let mut addr = InetAddress::new();
            assert_ne!(addr.set(&v, port), 0);
            assert_eq!(addr.ip_bytes(), &v);
            assert_eq!(addr.port(), port);
        }
    }

    #[test]
    fn layout() {
        unsafe {
            assert_eq!(size_of::<sockaddr_storage>(), size_of::<InetAddress>());

            let mut tmp = InetAddress::new();
            tmp.sa.sa_family = 0xab;
            if tmp.sin.sin_family != 0xab {
                panic!("sin_family misaligned in union");
            }
            if tmp.sin6.sin6_family != 0xab {
                panic!("sin6_family misaligned in union");
            }
            if tmp.ss.ss_family != 0xab {
                panic!("ss_family misaligned in union");
            }
        }
    }

    #[test]
    fn ipv6_string() {
        let ip = InetAddress::from_str("2603:6010:6e00:1118:d92a:ab88:4dfb:670a/1234").unwrap();
        assert_eq!("2603:6010:6e00:1118:d92a:ab88:4dfb:670a/1234", ip.to_string());
        let ip = InetAddress::from_str("fd80::1/1234").unwrap();
        assert_eq!("fd80::1/1234", ip.to_string());
    }

    #[test]
    fn ipv4_string() {
        let ip = InetAddress::from_str("1.2.3.4/1234").unwrap();
        assert_eq!("1.2.3.4/1234", ip.to_string());
    }
}
