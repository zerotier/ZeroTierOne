// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

#[allow(unused_imports)]
use zerotier_network_hypervisor::vl1::InetAddress;

#[cfg(any(target_os = "macos", target_os = "ios", target_os = "freebsd", target_os = "darwin"))]
mod freebsd_like {
    use num_traits::AsPrimitive;
    use std::mem::size_of;
    use std::sync::Mutex;
    use zerotier_network_hypervisor::vl1::InetAddress;

    static INFO_SOCKET: Mutex<i32> = Mutex::new(-1);

    #[allow(unused)]
    const SIZE_OF_IN6_IFREQ: usize = 288;

    #[allow(unused)]
    const SIOCGIFAFLAG_IN6: libc::c_ulong = 3240126793;

    #[allow(unused)]
    const IN6_IFF_TEMPORARY: libc::c_int = 128;

    #[allow(unused)]
    #[repr(C)]
    union in6_ifreq_inner {
        ifru_addr: libc::sockaddr_in6,
        ifru_flags: libc::c_short,
        ifru_flags6: libc::c_int,
        _padding: [u8; SIZE_OF_IN6_IFREQ - 16],
    }

    #[allow(non_camel_case_types)]
    #[allow(unused)]
    #[repr(C)]
    struct in6_ifreq {
        ifr_name: [u8; 16],
        ifr_ifru: in6_ifreq_inner,
    }

    pub fn is_ipv6_temporary(device_name: &str, address: &InetAddress) -> bool {
        if address.is_ipv6() {
            unsafe {
                let mut info_socket = INFO_SOCKET.lock().unwrap();
                if *info_socket < 0 {
                    *info_socket = libc::socket(libc::AF_INET6.as_(), libc::SOCK_DGRAM.as_(), 0) as i32;
                    if *info_socket < 0 {
                        return true; // probably means IPv6 is not enabled!
                    }
                }

                let mut ifr6: in6_ifreq = std::mem::zeroed();
                let device_name_bytes = device_name.as_bytes();
                assert!(device_name_bytes.len() <= 15);
                ifr6.ifr_name[..device_name_bytes.len()].copy_from_slice(device_name_bytes);
                std::ptr::copy_nonoverlapping(
                    (address as *const InetAddress).cast(),
                    &mut ifr6.ifr_ifru.ifru_addr,
                    size_of::<libc::sockaddr_in6>(),
                );
                if libc::ioctl((*info_socket).as_(), SIOCGIFAFLAG_IN6, &mut ifr6 as *mut in6_ifreq) != -1 {
                    if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_TEMPORARY) != 0 {
                        return true;
                    }
                }
            }
        }

        return false;
    }
}

#[cfg(any(target_os = "macos", target_os = "ios", target_os = "freebsd", target_os = "darwin"))]
pub use freebsd_like::is_ipv6_temporary;

#[cfg(not(any(target_os = "macos", target_os = "ios", target_os = "freebsd", target_os = "darwin")))]
pub fn is_ipv6_temporary(_device_name: &str, _address: &InetAddress) -> bool {
    false
}
