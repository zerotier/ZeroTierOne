// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::collections::HashMap;
#[allow(unused_imports)]
use std::mem::{size_of, transmute, MaybeUninit};
#[allow(unused_imports)]
use std::net::SocketAddr;
#[allow(unused_imports)]
use std::ptr::{null, null_mut};
use std::sync::Arc;

#[cfg(unix)]
use std::os::unix::io::{FromRawFd, RawFd};

use crate::getifaddrs;
use crate::ipv6;
use crate::localinterface::LocalInterface;

#[allow(unused_imports)]
use num_traits::AsPrimitive;

use zerotier_network_hypervisor::vl1::{InetAddress, IpScope, AF_INET, AF_INET6};

/// A local port to which one or more UDP sockets is bound.
///
/// To bind a port we must bind sockets to each interface/IP pair directly. Sockets must
/// be "hard" bound to the interface so default route override can work.
pub struct BoundUdpPort {
    pub sockets: Vec<Arc<BoundUdpSocket>>,
    pub port: u16,
}

/// A socket bound to a specific interface and IP.
pub struct BoundUdpSocket {
    pub address: InetAddress,
    pub socket: Arc<tokio::net::UdpSocket>,
    pub interface: LocalInterface,
    pub socket_associated_tasks: parking_lot::Mutex<Vec<tokio::task::JoinHandle<()>>>,
    fd: RawFd,
}

impl Drop for BoundUdpSocket {
    fn drop(&mut self) {
        for t in self.socket_associated_tasks.lock().drain(..) {
            t.abort();
        }
    }
}

impl BoundUdpSocket {
    #[cfg(unix)]
    #[inline(always)]
    fn set_ttl(&self, packet_ttl: u8) {
        let ttl = packet_ttl as libc::c_int;
        unsafe {
            libc::setsockopt(
                self.fd.as_(),
                libc::IPPROTO_IP.as_(),
                libc::IP_TOS.as_(),
                (&ttl as *const libc::c_int).cast(),
                std::mem::size_of::<libc::c_int>().as_(),
            )
        };
    }

    #[cfg(any(target_os = "macos", target_os = "freebsd"))]
    pub fn send_sync_nonblock(&self, dest: &InetAddress, b: &[&[u8]], packet_ttl: u8) -> bool {
        let mut ok = false;
        if dest.family() == self.address.family() {
            if packet_ttl > 0 && dest.is_ipv4() {
                self.set_ttl(packet_ttl);
            }
            unsafe {
                if b.len() == 1 {
                    let bb = *b.get_unchecked(0);
                    ok = libc::sendto(
                        self.fd.as_(),
                        bb.as_ptr().cast(),
                        bb.len().as_(),
                        0,
                        (dest as *const InetAddress).cast(),
                        size_of::<InetAddress>().as_(),
                    ) > 0;
                } else {
                    let mut iov: [libc::iovec; 16] = MaybeUninit::uninit().assume_init();
                    assert!(b.len() <= iov.len());
                    for i in 0..b.len() {
                        let bb = *b.get_unchecked(i);
                        let ii = iov.get_unchecked_mut(i);
                        ii.iov_base = transmute(bb.as_ptr());
                        ii.iov_len = bb.len().as_();
                    }
                    let msghdr = libc::msghdr {
                        msg_name: transmute(dest as *const InetAddress),
                        msg_namelen: size_of::<InetAddress>().as_(),
                        msg_iov: iov.as_mut_ptr(),
                        msg_iovlen: b.len().as_(),
                        msg_control: null_mut(),
                        msg_controllen: 0,
                        msg_flags: 0,
                    };
                    ok = libc::sendmsg(self.fd.as_(), &msghdr, 0) > 0;
                }
            }
            if packet_ttl > 0 && dest.is_ipv4() {
                self.set_ttl(0xff);
            }
        }
        ok
    }

    #[cfg(not(any(target_os = "macos", target_os = "freebsd")))]
    pub fn send_sync_nonblock(&self, dest: &InetAddress, b: &[&[u8]], packet_ttl: u8) -> bool {
        let mut ok = false;
        if dest.family() == self.address.family() {
            let mut tmp: [u8; 16384] = unsafe { std::mem::MaybeUninit::uninit().assume_init() };
            let data = if b.len() == 1 {
                *unsafe { b.get_unchecked(0) }
            } else {
                let mut p = 0;
                for bb in b.iter() {
                    let pp = p + bb.len();
                    if pp < 16384 {
                        tmp[p..pp].copy_from_slice(*bb);
                        p = pp;
                    } else {
                        return false;
                    }
                }
                &tmp[..p]
            };

            if packet_ttl > 0 && dest.is_ipv4() {
                self.set_ttl(packet_ttl);
                ok = self.socket.try_send_to(data, dest.try_into().unwrap()).is_ok();
                self.set_ttl(0xff);
            } else {
                ok = self.socket.try_send_to(data, dest.try_into().unwrap()).is_ok();
            }
        }
        ok
    }
}

impl BoundUdpPort {
    /// Create a new port binding.
    ///
    /// You must call update_bindings() after this to actually bind to system interfaces.
    pub fn new(port: u16) -> Self {
        Self { sockets: Vec::new(), port }
    }

    /// Synchronize bindings with devices and IPs in system.
    ///
    /// Any device or local IP within any of the supplied blacklists is ignored. Multicast or loopback addresses are
    /// also ignored.
    ///
    /// The caller can check the 'sockets' member variable after calling to determine which if any bindings were
    /// successful. Any errors that occurred are returned as tuples of (interface, address, error). The second vector
    /// returned contains newly bound sockets.
    pub fn update_bindings(
        &mut self,
        interface_prefix_blacklist: &Vec<String>,
        cidr_blacklist: &Vec<InetAddress>,
    ) -> (Vec<(LocalInterface, InetAddress, std::io::Error)>, Vec<Arc<BoundUdpSocket>>) {
        let mut existing_bindings: HashMap<LocalInterface, HashMap<InetAddress, Arc<BoundUdpSocket>>> = HashMap::with_capacity(4);
        for s in self.sockets.drain(..) {
            existing_bindings
                .entry(s.interface)
                .or_insert_with(|| HashMap::with_capacity(4))
                .insert(s.address.clone(), s);
        }

        let mut errors = Vec::new();
        let mut new_sockets = Vec::new();
        getifaddrs::for_each_address(|address, interface| {
            let interface_str = interface.to_string();
            let mut addr_with_port = address.clone();
            addr_with_port.set_port(self.port);
            if address.is_ip()
                && matches!(
                    address.scope(),
                    IpScope::Global | IpScope::PseudoPrivate | IpScope::Private | IpScope::Shared
                )
                && !interface_prefix_blacklist.iter().any(|pfx| interface_str.starts_with(pfx.as_str()))
                && !cidr_blacklist.iter().any(|r| address.is_within(r))
                && !ipv6::is_ipv6_temporary(interface_str.as_str(), address)
            {
                let mut found = false;
                if let Some(byaddr) = existing_bindings.get(interface) {
                    if let Some(socket) = byaddr.get(&addr_with_port) {
                        found = true;
                        self.sockets.push(socket.clone());
                    }
                }
                if !found {
                    let s = unsafe { bind_udp_to_device(interface_str.as_str(), &addr_with_port) };
                    if s.is_ok() {
                        let fd = s.unwrap();
                        let s = tokio::net::UdpSocket::from_std(unsafe { std::net::UdpSocket::from_raw_fd(fd) });
                        if s.is_ok() {
                            let s = Arc::new(BoundUdpSocket {
                                address: addr_with_port,
                                socket: Arc::new(s.unwrap()),
                                socket_associated_tasks: parking_lot::Mutex::new(Vec::new()),
                                interface: interface.clone(),
                                fd,
                            });
                            self.sockets.push(s.clone());
                            new_sockets.push(s);
                        } else {
                            errors.push((interface.clone(), addr_with_port, s.err().unwrap()));
                        }
                    } else {
                        errors.push((
                            interface.clone(),
                            addr_with_port,
                            std::io::Error::new(std::io::ErrorKind::Other, s.err().unwrap()),
                        ));
                    }
                }
            }
        });

        (errors, new_sockets)
    }
}

#[allow(unused_variables)]
#[cfg(unix)]
unsafe fn bind_udp_to_device(device_name: &str, address: &InetAddress) -> Result<RawFd, &'static str> {
    let (af, sa_len) = match address.family() {
        AF_INET => (AF_INET, std::mem::size_of::<libc::sockaddr_in>().as_()),
        AF_INET6 => (AF_INET6, std::mem::size_of::<libc::sockaddr_in6>().as_()),
        _ => {
            return Err("unrecognized address family");
        }
    };

    let s = libc::socket(af.as_(), libc::SOCK_DGRAM, 0);
    if s <= 0 {
        return Err("unable to create new UDP socket");
    }

    assert_ne!(libc::fcntl(s, libc::F_SETFL, libc::O_NONBLOCK), -1);

    #[allow(unused_variables)]
    let mut setsockopt_results: libc::c_int = 0;
    let mut fl: libc::c_int;

    fl = 1;
    setsockopt_results |= libc::setsockopt(
        s,
        libc::SOL_SOCKET.as_(),
        libc::SO_REUSEPORT.as_(),
        (&mut fl as *mut libc::c_int).cast(),
        std::mem::size_of::<libc::c_int>().as_(),
    );
    debug_assert!(setsockopt_results == 0);

    fl = 1;
    setsockopt_results |= libc::setsockopt(
        s,
        libc::SOL_SOCKET.as_(),
        libc::SO_BROADCAST.as_(),
        (&mut fl as *mut libc::c_int).cast(),
        std::mem::size_of::<libc::c_int>().as_(),
    );
    debug_assert!(setsockopt_results == 0);
    if af == AF_INET6 {
        fl = 1;
        setsockopt_results |= libc::setsockopt(
            s,
            libc::IPPROTO_IPV6.as_(),
            libc::IPV6_V6ONLY.as_(),
            (&mut fl as *mut libc::c_int).cast(),
            std::mem::size_of::<libc::c_int>().as_(),
        );
        debug_assert!(setsockopt_results == 0);
    }

    #[cfg(target_os = "linux")]
    {
        if !device_name.is_empty() {
            let _ = std::ffi::CString::new(device_name).map(|dn| {
                let dnb = dn.as_bytes_with_nul();
                let _ = libc::setsockopt(
                    s.as_(),
                    libc::SOL_SOCKET.as_(),
                    libc::SO_BINDTODEVICE.as_(),
                    dnb.as_ptr().cast(),
                    (dnb.len() - 1).as_(),
                );
            });
        }
    }

    if setsockopt_results != 0 {
        libc::close(s);
        return Err("setsockopt() failed");
    }

    if af == AF_INET {
        #[cfg(not(target_os = "linux"))]
        {
            fl = 0;
            libc::setsockopt(
                s,
                libc::IPPROTO_IP.as_(),
                libc::IP_DONTFRAG.as_(),
                (&mut fl as *mut libc::c_int).cast(),
                std::mem::size_of::<libc::c_int>().as_(),
            );
        }
        #[cfg(target_os = "linux")]
        {
            fl = libc::IP_PMTUDISC_DONT as libc::c_int;
            libc::setsockopt(
                s,
                libc::IPPROTO_IP.as_(),
                libc::IP_MTU_DISCOVER.as_(),
                (&mut fl as *mut libc::c_int).cast(),
                std::mem::size_of::<libc::c_int>().as_(),
            );
        }
    }

    if af == AF_INET6 {
        fl = 0;
        libc::setsockopt(
            s,
            libc::IPPROTO_IPV6.as_(),
            libc::IPV6_DONTFRAG.as_(),
            (&mut fl as *mut libc::c_int).cast(),
            std::mem::size_of::<libc::c_int>().as_(),
        );
    }

    fl = 1048576;
    while fl >= 65536 {
        if libc::setsockopt(
            s,
            libc::SOL_SOCKET.as_(),
            libc::SO_RCVBUF.as_(),
            (&mut fl as *mut libc::c_int).cast(),
            std::mem::size_of::<libc::c_int>().as_(),
        ) == 0
        {
            break;
        }
        fl -= 65536;
    }
    fl = 1048576;
    while fl >= 65536 {
        if libc::setsockopt(
            s,
            libc::SOL_SOCKET.as_(),
            libc::SO_SNDBUF.as_(),
            (&mut fl as *mut libc::c_int).cast(),
            std::mem::size_of::<libc::c_int>().as_(),
        ) == 0
        {
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
