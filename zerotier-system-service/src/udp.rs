// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::collections::{HashMap, HashSet};
use std::hash::Hash;
use std::num::NonZeroI64;
use std::sync::Arc;

#[cfg(unix)]
use std::os::unix::io::{FromRawFd, RawFd};

use lazy_static::lazy_static;

#[allow(unused_imports)]
use num_traits::AsPrimitive;

use crate::getifaddrs;

use zerotier_network_hypervisor::vl1::inetaddress::{InetAddress, IpScope};

/// A locally bound UDP socket.
pub struct BoundUdpSocket {
    /// Locally bound address.
    pub address: InetAddress,
    /// Locally bound (to device) socket.
    pub socket: tokio::net::UdpSocket,
    /// Local interface device name or other unique identifier (OS-specific).
    pub interface: String,
    /// Raw socket FD, which only remains valid as long as 'socket' exists.
    pub fd: RawFd,
    /// Monotonic time of last activity.
    pub last_activity_time_ticks: i64,
}

impl BoundUdpSocket {
    /// Update 'sockets' by adding any missing local bindings and removing any that are no longer valid.
    ///
    /// Any device or local IP within any of the supplied blacklists is ignored. Multicast or loopback addresses are
    /// also ignored. All errors encountered are returned.
    ///
    /// This should always be called on the same port for the same socket collection. Calling on the same 'sockets'
    /// with different ports will lead to redundant or missed bindings.
    ///
    /// We must bind directly to each device/address pair for each port so default route override can work.
    fn update_bindings_for_port(sockets: &mut Vec<Arc<BoundUdpSocket>>, port: u16, device_prefix_blacklist: &Vec<String>, cidr_blacklist: &Vec<InetAddress>) -> Vec<std::io::Error> {
        let mut errors = Vec::new();
        let mut existing_bind_points: HashMap<String, Vec<InetAddress>> = HashMap::with_capacity(id_assignment_state.devices.len() + 1);
        let now = crate::utils::ms_monotonic();
        getifaddrs::for_each_address(|address, device| {
            if address.is_ip()
                && matches!(address.scope(), IpScope::Global | IpScope::PseudoPrivate | IpScope::Private | IpScope::Shared)
                && !device_prefix_blacklist.iter().any(|pfx| device.starts_with(pfx.as_str()))
                && !cidr_blacklist.iter().any(|r| address.is_within(r))
            {
                existing_bind_points.entry(device.to_string()).or_default().push(address.clone());
                if !sockets.iter().any(|_, s| s.address == address || s.local_device_id == did) {
                    let s = unsafe { bind_udp_to_device(device, address) };
                    if s.is_ok() {
                        let fd = s.unwrap();
                        let s = tokio::net::UdpSocket::from_std(unsafe { std::net::UdpSocket::from_raw_fd(fd) });
                        if s.is_ok() {
                            id_assignment_state.socket_id_counter += 1;
                            let lsid = NonZeroI64::new(id_assignment_state.socket_id_counter).unwrap();
                            sockets.push(Arc::new(BoundUdpSocket {
                                address: address.clone(),
                                socket: s.unwrap(),
                                interface: device.to_string(),
                                fd,
                                last_activity_time_ticks: now,
                            }));
                        } else {
                            errors.push(s.err().unwrap());
                        }
                    } else {
                        errors.push(std::io::Error::new(std::io::ErrorKind::AddrInUse, s.err().unwrap()));
                    }
                }
            }
        });
        sockets.retain(|s| existing_bind_points.get(&s.local_interface).map_or(false, |addr_list| addr_list.contains(&s.address)));
        errors
    }
}

#[allow(unused_variables)]
#[cfg(unix)]
unsafe fn bind_udp_to_device(device_name: &str, address: &InetAddress) -> Result<RawFd, &'static str> {
    let (af, sa_len) = match address.family() {
        InetAddressFamily::IPv4 => (libc::AF_INET, std::mem::size_of::<libc::sockaddr_in>().as_()),
        InetAddressFamily::IPv6 => (libc::AF_INET6, std::mem::size_of::<libc::sockaddr_in6>().as_()),
        _ => {
            return Err("unrecognized address family");
        }
    };

    let s = libc::socket(af.as_(), libc::SOCK_DGRAM, 0);
    if s <= 0 {
        return Err("unable to create socket");
    }

    let mut setsockopt_results: c_int = 0;

    let mut fl: c_int = 0;
    setsockopt_results |= libc::setsockopt(s, libc::SOL_SOCKET.as_(), libc::SO_LINGER.as_(), (&mut fl as *mut c_int).cast(), std::mem::size_of::<c_int>().as_());

    fl = 1;
    setsockopt_results |= libc::setsockopt(s, libc::SOL_SOCKET.as_(), libc::SO_BROADCAST.as_(), (&mut fl as *mut c_int).cast(), std::mem::size_of::<c_int>().as_());
    if af == libc::AF_INET6 {
        fl = 1;
        setsockopt_results |= libc::setsockopt(s, libc::IPPROTO_IPV6.as_(), libc::IPV6_V6ONLY.as_(), (&mut fl as *mut c_int).cast(), std::mem::size_of::<c_int>().as_());
    }

    #[cfg(target_os = "linux")]
    {
        if !device_name.is_empty() {
            let _ = std::ffi::CString::new(device_name).map(|dn| {
                let dnb = dn.as_bytes_with_nul();
                let _ = libc::setsockopt(s.as_(), libc::SOL_SOCKET.as_(), libc::SO_BINDTODEVICE.as_(), dnb.as_ptr().cast(), (dnb.len() - 1).as_());
            });
        }
    }

    if setsockopt_results != 0 {
        libc::close(s);
        return Err("setsockopt() failed");
    }

    if af == libc::AF_INET {
        #[cfg(not(target_os = "linux"))]
        {
            fl = 0;
            libc::setsockopt(s, libc::IPPROTO_IP.as_(), libc::IP_DF.as_(), (&mut fl as *mut c_int).cast(), std::mem::size_of::<c_int>().as_());
        }
        #[cfg(target_os = "linux")]
        {
            fl = libc::IP_PMTUDISC_DONT as c_int;
            libc::setsockopt(s, libc::IPPROTO_IP.as_(), libc::IP_MTU_DISCOVER.as_(), (&mut fl as *mut c_int).cast(), std::mem::size_of::<c_int>().as_());
        }
    }

    if af == libc::AF_INET6 {
        fl = 0;
        libc::setsockopt(s, libc::IPPROTO_IPV6.as_(), libc::IPV6_DONTFRAG.as_(), (&mut fl as *mut c_int).cast(), std::mem::size_of::<c_int>().as_());
    }

    fl = 1048576;
    while fl >= 131072 {
        if libc::setsockopt(s, libc::SOL_SOCKET.as_(), libc::SO_RCVBUF.as_(), (&mut fl as *mut c_int).cast(), std::mem::size_of::<c_int>().as_()) == 0 {
            break;
        }
        fl -= 65536;
    }
    fl = 1048576;
    while fl >= 131072 {
        if libc::setsockopt(s, libc::SOL_SOCKET.as_(), libc::SO_SNDBUF.as_(), (&mut fl as *mut c_int).cast(), std::mem::size_of::<c_int>().as_()) == 0 {
            break;
        }
        fl -= 65536;
    }

    if libc::bind(s, (address as *const InetAddress).cast(), sa_len) != 0 {
        libc::close(s);
        return Err("bind to address failed");
    }

    Ok(s as RawFd)
}
