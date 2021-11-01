/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::cmp::Ordering;
use std::hash::{Hash, Hasher};
use std::mem::{MaybeUninit, size_of, transmute_copy, zeroed};
use std::net::{IpAddr, Ipv6Addr};
use std::ptr::{copy_nonoverlapping, null, slice_from_raw_parts, write_bytes};
use std::str::FromStr;

#[cfg(windows)]
use winapi::um::winsock2 as winsock2;

use crate::error::InvalidFormatError;
use crate::util::equal_ptr;
use crate::vl1::buffer::Buffer;

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

#[cfg(not(windows))]
pub const AF_INET: u8 = libc::AF_INET as u8;

#[cfg(not(windows))]
pub const AF_INET6: u8 = libc::AF_INET6 as u8;

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
/// Unfortunately this is full of unsafe because it's a union, but the code is
/// not complex and doesn't allocate anything.
#[repr(C)]
pub union InetAddress {
    sa: sockaddr,
    sin: sockaddr_in,
    sin6: sockaddr_in6,
    ss: sockaddr_storage, // some external code may expect the struct to be this full length
}

impl Clone for InetAddress {
    #[inline(always)]
    fn clone(&self) -> Self { unsafe { transmute_copy(self) } }
}

impl Default for InetAddress {
    #[inline(always)]
    fn default() -> InetAddress { unsafe { zeroed() } }
}

impl InetAddress {
    /// Get a new zero/nil InetAddress.
    #[inline(always)]
    pub fn new() -> InetAddress { unsafe { zeroed() } }

    /// Construct from IP and port.
    /// If the IP is not either 4 or 16 bytes in length, a nil/0 InetAddress is returned.
    #[inline(always)]
    pub fn from_ip_port(ip: &[u8], port: u16) -> InetAddress {
        unsafe {
            let mut c = MaybeUninit::<InetAddress>::uninit().assume_init();
            c.set(ip, port);
            c
        }
    }

    /// Zero the contents of this InetAddress.
    #[inline(always)]
    pub fn zero(&mut self) { unsafe { write_bytes((self as *mut Self).cast::<u8>(), 0, size_of::<Self>()) }; }

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
    pub fn is_nil(&self) -> bool { unsafe { self.sa.sa_family == 0 } }

    /// Check if this is an IPv4 address.
    #[inline(always)]
    pub fn is_ipv4(&self) -> bool { unsafe { self.sa.sa_family as u8 == AF_INET } }

    /// Check if this is an IPv6 address.
    #[inline(always)]
    pub fn is_ipv6(&self) -> bool { unsafe { self.sa.sa_family as u8 == AF_INET6 } }

    /// Get the address family of this InetAddress: AF_INET, AF_INET6, or 0 if uninitialized.
    #[inline(always)]
    pub fn family(&self) -> u8 { unsafe { self.sa.sa_family } }

    /// Get a pointer to the C "sockaddr" structure and the size of the returned structure in bytes.
    /// This is useful for interacting with C-level socket APIs. This returns a null pointer if
    /// the address is not initialized.
    #[inline(always)]
    pub fn c_sockaddr(&self) -> (*const (), usize) {
        unsafe {
            match self.sa.sa_family as u8 {
                AF_INET => ((&self.sin as *const sockaddr_in).cast(), size_of::<sockaddr_in>()),
                AF_INET6 => ((&self.sin6 as *const sockaddr_in6).cast(), size_of::<sockaddr_in6>()),
                _ => (null(), 0)
            }
        }
    }

    /// Set the IP and port of this InetAddress.
    /// Whether this is IPv4 or IPv6 is inferred from the size of ip[], which must be
    /// either 4 or 16 bytes. The family (AF_INET or AF_INET6) is returned, or zero on
    /// success.
    pub fn set(&mut self, ip: &[u8], port: u16) -> u8 {
        self.zero();
        let port = port.to_be();
        unsafe {
            if ip.len() == 4 {
                self.sin.sin_family = AF_INET.into();
                self.sin.sin_port = port.into();
                copy_nonoverlapping(ip.as_ptr(), (&mut self.sin.sin_addr.s_addr as *mut u32).cast::<u8>(), 4);
                AF_INET
            } else if ip.len() == 16 {
                self.sin6.sin6_family = AF_INET6.into();
                self.sin6.sin6_port = port.into();
                copy_nonoverlapping(ip.as_ptr(), (&mut self.sin6.sin6_addr as *mut in6_addr).cast::<u8>(), 16);
                AF_INET6
            } else {
                0
            }
        }
    }

    /// Get raw IP bytes, with length dependent on address family (4 or 16).
    #[inline(always)]
    pub fn ip_bytes(&self) -> &[u8] {
        unsafe {
            match self.sa.sa_family as u8 {
                AF_INET => &*(&self.sin.sin_addr.s_addr as *const u32).cast::<[u8; 4]>(),
                AF_INET6 => &*(&(self.sin6.sin6_addr) as *const in6_addr).cast::<[u8; 16]>(),
                _ => &[],
            }
        }
    }

    /// Get the IP port for this InetAddress.
    #[inline(always)]
    pub fn port(&self) -> u16 {
        unsafe {
            u16::from_be(match self.sa.sa_family as u8 {
                AF_INET => self.sin.sin_port as u16,
                AF_INET6 => self.sin6.sin6_port as u16,
                _ => 0
            })
        }
    }

    /// Set the IP port.
    ///
    /// This does nothing on uninitialized InetAddress objects. An address must first
    /// be initialized with an IP to select the correct address type.
    #[inline(always)]
    pub fn set_port(&mut self, port: u16) {
        let port = port.to_be();
        unsafe {
            match self.sa.sa_family as u8 {
                AF_INET => self.sin.sin_port = port,
                AF_INET6 => self.sin6.sin6_port = port,
                _ => {}
            }
        }
    }

    /// Get this IP address's scope as per RFC documents and what is advertised via BGP.
    pub fn scope(&self) -> IpScope {
        unsafe {
            match self.sa.sa_family as u8 {
                AF_INET => {
                    let ip = self.sin.sin_addr.s_addr as u32;
                    let class_a = (ip >> 24) as u8;
                    match class_a {
                        0x00 | 0xff => IpScope::None, // 0.0.0.0/8 and 255.0.0.0/8 are not usable
                        0x0a => IpScope::Private,     // 10.0.0.0/8
                        0x7f => IpScope::Loopback,    // 127.0.0.0/8
                        0x64 => {
                            if (ip & 0xffc00000) == 0x64400000 { // 100.64.0.0/10
                                IpScope::Private
                            } else {
                                IpScope::Global
                            }
                        }
                        0xa9 => {
                            if (ip & 0xffff0000) == 0xa9fe0000 { // 169.254.0.0/16
                                IpScope::LinkLocal
                            } else {
                                IpScope::Global
                            }
                        }
                        0xac => {
                            if (ip & 0xfff00000) == 0xac100000 { // 172.16.0.0/12
                                IpScope::Private
                            } else {
                                IpScope::Global
                            }
                        }
                        0xc0 => {
                            if (ip & 0xffff0000) == 0xc0a80000 || (ip & 0xffffff00) == 0xc0000200 { // 192.168.0.0/16 and 192.0.2.0/24
                                IpScope::Private
                            } else {
                                IpScope::Global
                            }
                        }
                        0xc6 => {
                            if (ip & 0xfffe0000) == 0xc6120000 || (ip & 0xffffff00) == 0xc6336400 { // 198.18.0.0/15 and 198.51.100.0/24
                                IpScope::Private
                            } else {
                                IpScope::Global
                            }
                        }
                        0xcb => {
                            if (ip & 0xffffff00) == 0xcb007100 { // 203.0.113.0/24
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
                            ].contains(&class_a) {
                                IpScope::PseudoPrivate
                            } else {
                                match ip >> 28 {
                                    0xe => IpScope::Multicast, // 224.0.0.0/4
                                    0xf => IpScope::Private,   // 240.0.0.0/4 ("reserved," usually unusable)
                                    _ => IpScope::Global
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
                _ => IpScope::None
            }
        }
    }

    /// Get only the IP portion of this address as a string.
    pub fn to_ip_string(&self) -> String {
        unsafe {
            match self.sa.sa_family as u8 {
                AF_INET => {
                    let ip = &*(&self.sin.sin_addr.s_addr as *const u32).cast::<[u8; 4]>();
                    format!("{}.{}.{}.{}", ip[0], ip[1], ip[2], ip[3])
                }
                AF_INET6 => Ipv6Addr::from(*(&(self.sin6.sin6_addr) as *const in6_addr).cast::<[u8; 16]>()).to_string(),
                _ => String::from("(null)")
            }
        }
    }

    pub fn marshal<const BL: usize>(&self, buf: &mut Buffer<BL>) -> std::io::Result<()> {
        unsafe {
            match self.sa.sa_family as u8 {
                AF_INET => {
                    buf.append_and_init_bytes_fixed(|b: &mut [u8; 7]| {
                        b[0] = 4;
                        copy_nonoverlapping((&self.sin.sin_addr.s_addr as *const u32).cast::<u8>(), b.as_mut_ptr().offset(1), 4);
                        b[5] = *(&self.sin.sin_port as *const u16).cast::<u8>();
                        b[6] = *(&self.sin.sin_port as *const u16).cast::<u8>().offset(1);
                    })
                }
                AF_INET6 => {
                    buf.append_and_init_bytes_fixed(|b: &mut [u8; 19]| {
                        b[0] = 6;
                        copy_nonoverlapping((&(self.sin6.sin6_addr) as *const in6_addr).cast::<u8>(), b.as_mut_ptr().offset(1), 16);
                        b[17] = *(&self.sin6.sin6_port as *const u16).cast::<u8>();
                        b[18] = *(&self.sin6.sin6_port as *const u16).cast::<u8>().offset(1);
                    })
                }
                _ => buf.append_u8(0)
            }
        }
    }

    pub fn unmarshal<const BL: usize>(buf: &Buffer<BL>, cursor: &mut usize) -> std::io::Result<InetAddress> {
        let t = buf.read_u8(cursor)?;
        if t == 4 {
            let b: &[u8; 6] = buf.read_bytes_fixed(cursor)?;
            Ok(InetAddress::from_ip_port(&b[0..4], crate::util::load_u16_be(&b[4..6])))
        } else if t == 6 {
            let b: &[u8; 18] = buf.read_bytes_fixed(cursor)?;
            Ok(InetAddress::from_ip_port(&b[0..16], crate::util::load_u16_be(&b[16..18])))
        } else {
            Ok(InetAddress::new())
        }
    }
}

impl ToString for InetAddress {
    fn to_string(&self) -> String {
        unsafe {
            let mut s = self.to_ip_string();
            match self.sa.sa_family as u8 {
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

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let mut addr = InetAddress::new();
        let (ip_str, port) = s.find('/').map_or_else(|| {
            (s, 0)
        }, |pos| {
            let ss = s.split_at(pos);
            let mut port_str = ss.1;
            if port_str.starts_with('/') {
                port_str = &port_str[1..];
            }
            (ss.0, u16::from_str_radix(port_str, 10).unwrap_or(0).to_be())
        });
        IpAddr::from_str(ip_str).map_or_else(|_| Err(InvalidFormatError), |ip| {
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
        })
    }
}

impl PartialEq for InetAddress {
    fn eq(&self, other: &Self) -> bool {
        unsafe {
            if self.sa.sa_family == other.sa.sa_family {
                match self.sa.sa_family as u8 {
                    AF_INET => { self.sin.sin_port == other.sin.sin_port && self.sin.sin_addr.s_addr == other.sin.sin_addr.s_addr }
                    AF_INET6 => {
                        if self.sin6.sin6_port == other.sin6.sin6_port {
                            equal_ptr((&(self.sin6.sin6_addr) as *const in6_addr).cast(), (&(other.sin6.sin6_addr) as *const in6_addr).cast(), 16)
                        } else {
                            false
                        }
                    }
                    _ => true
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
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> { Some(self.cmp(other)) }
}

// Manually implement Ord to ensure consistent sort order across platforms, since we don't know exactly
// how sockaddr structs will be laid out.
impl Ord for InetAddress {
    fn cmp(&self, other: &Self) -> Ordering {
        unsafe {
            if self.sa.sa_family == other.sa.sa_family {
                match self.sa.sa_family as u8 {
                    0 => {
                        Ordering::Equal
                    }
                    AF_INET => {
                        let ip_ordering = u32::from_be(self.sin.sin_addr.s_addr as u32).cmp(&u32::from_be(other.sin.sin_addr.s_addr as u32));
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
                        (*slice_from_raw_parts((self as *const Self).cast::<u8>(), size_of::<Self>())).cmp(&*slice_from_raw_parts((other as *const Self).cast::<u8>(), size_of::<Self>()))
                    }
                }
            } else {
                match self.sa.sa_family as u8 {
                    0 => {
                        Ordering::Less
                    }
                    AF_INET => {
                        if other.sa.sa_family as u8 == AF_INET6 {
                            Ordering::Less
                        } else {
                            self.sa.sa_family.cmp(&other.sa.sa_family)
                        }
                    }
                    AF_INET6 => {
                        if other.sa.sa_family as u8 == AF_INET {
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
            match self.sa.sa_family as u8 {
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
