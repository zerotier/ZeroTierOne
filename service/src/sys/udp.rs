// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::collections::{HashMap, HashSet};
#[allow(unused_imports)]
use std::mem::{size_of, transmute, MaybeUninit};
#[allow(unused_imports)]
use std::net::{IpAddr, Ipv4Addr, Ipv6Addr, SocketAddr};
#[allow(unused_imports)]
use std::ptr::{null, null_mut};
use std::sync::atomic::{AtomicBool, AtomicI64, Ordering};
use std::sync::{Arc, RwLock};

use crate::vl1::LocalInterface;

#[allow(unused_imports)]
use num_traits::AsPrimitive;

use zerotier_network_hypervisor::protocol::{PacketBufferPool, PooledPacketBuffer};
use zerotier_network_hypervisor::vl1::inetaddress::*;
use zerotier_utils::ms_monotonic;

use crate::sys::{getifaddrs, ipv6};

/// UDP socket receive timeout to allow sockets to close properly on some systems (seconds).
const SOCKET_RECV_TIMEOUT_SECONDS: i64 = 2;

fn socket_read_concurrency() -> usize {
    const MAX_PER_SOCKET_CONCURRENCY: usize = 8;

    static mut THREADS_PER_SOCKET: usize = 0;
    unsafe {
        let mut t = THREADS_PER_SOCKET;
        if t == 0 {
            t = std::thread::available_parallelism().unwrap().get().max(1).min(MAX_PER_SOCKET_CONCURRENCY);
            THREADS_PER_SOCKET = t;
        }
        t
    }
}

pub trait UdpPacketHandler: Send + Sync {
    fn incoming_udp_packet(self: &Arc<Self>, time_ticks: i64, socket: &Arc<BoundUdpSocket>, source_address: &InetAddress, packet: PooledPacketBuffer);
}

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
    pub bind_address: InetAddress,
    pub interface: LocalInterface,
    last_receive_time: AtomicI64,
    fd: i32,
    lock: RwLock<()>,
    open: AtomicBool,
}

impl Drop for BoundUdpSocket {
    fn drop(&mut self) {
        self.close();
        let _wait_for_close = self.lock.write();
    }
}

impl BoundUdpSocket {
    #[cfg(unix)]
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

    #[cfg(unix)]
    pub fn send(&self, dest: &InetAddress, data: &[u8], packet_ttl: u8) -> bool {
        if dest.family() == self.bind_address.family() {
            let (c_sockaddr, c_addrlen) = dest.c_sockaddr();
            if packet_ttl == 0 || !dest.is_ipv4() {
                unsafe {
                    return libc::sendto(
                        self.fd.as_(),
                        data.as_ptr().cast(),
                        data.len().as_(),
                        0,
                        c_sockaddr.cast(),
                        c_addrlen.as_(),
                    ) >= 0;
                }
            } else {
                self.set_ttl(packet_ttl);
                let ok = unsafe {
                    libc::sendto(
                        self.fd.as_(),
                        data.as_ptr().cast(),
                        data.len().as_(),
                        0,
                        c_sockaddr.cast(),
                        c_addrlen.as_(),
                    ) >= 0
                };
                self.set_ttl(0xff);
                return ok;
            }
        }
        return false;
    }

    #[cfg(unix)]
    fn close(&self) {
        unsafe {
            self.open.store(false, Ordering::SeqCst);
            let mut timeo: libc::timeval = std::mem::zeroed();
            timeo.tv_sec = 0;
            timeo.tv_usec = 1;
            libc::setsockopt(
                self.fd.as_(),
                libc::SOL_SOCKET.as_(),
                libc::SO_RCVTIMEO.as_(),
                (&mut timeo as *mut libc::timeval).cast(),
                std::mem::size_of::<libc::timeval>().as_(),
            );
            libc::shutdown(self.fd.as_(), libc::SHUT_RDWR);
            libc::close(self.fd.as_());
        }
    }

    #[cfg(windows)]
    fn set_ttl(&self, packet_ttl: u8) {
        todo!()
    }

    #[cfg(windows)]
    pub fn send(&self, dest: &InetAddress, data: &[u8], packet_ttl: u8) -> bool {
        todo!()
    }

    #[cfg(windows)]
    fn close(&self) {
        todo!()
    }
}

impl BoundUdpPort {
    /// Create a new port binding.
    ///
    /// You must call update_bindings() after this to actually bind to system interfaces.
    pub fn new(port: u16) -> Self {
        Self { sockets: Vec::new(), port }
    }

    /// Return a tuple of: total number of Arc<>+Weak<> references to sockets, and most recent receive time on any socket.
    pub fn liveness(&self) -> (usize, i64) {
        let mut rt_latest = i64::MIN;
        let mut total_handles = 0;
        for s in self.sockets.iter() {
            rt_latest = rt_latest.max(s.last_receive_time.load(Ordering::Relaxed));
            total_handles += Arc::strong_count(s) + Arc::weak_count(s);
        }
        (total_handles, rt_latest)
    }

    /// Synchronize bindings with devices and IPs in system.
    ///
    /// Any device or local IP within any of the supplied blacklists is ignored. Multicast or loopback addresses are
    /// also ignored.
    ///
    /// The caller can check the 'sockets' member variable after calling to determine which if any bindings were
    /// successful. Any errors that occurred are returned as tuples of (interface, address, error). The second vector
    /// returned contains newly bound sockets.
    pub fn update_bindings<UdpPacketHandlerImpl: UdpPacketHandler + ?Sized + 'static>(
        &mut self,
        interface_prefix_blacklist: &HashSet<String>,
        cidr_blacklist: &HashSet<InetAddress>,
        buffer_pool: &Arc<PacketBufferPool>,
        handler: &Arc<UdpPacketHandlerImpl>,
    ) -> Vec<(LocalInterface, InetAddress, std::io::Error)> {
        let mut existing_bindings: HashMap<LocalInterface, HashMap<InetAddress, Arc<BoundUdpSocket>>> = HashMap::with_capacity(4);
        for s in self.sockets.drain(..) {
            existing_bindings
                .entry(s.interface)
                .or_insert_with(|| HashMap::with_capacity(4))
                .insert(s.bind_address.clone(), s);
        }

        let mut errors: Vec<(LocalInterface, InetAddress, std::io::Error)> = Vec::new();
        getifaddrs::for_each_address(|address, interface| {
            let interface_str = interface.to_string();
            let mut addr_with_port = address.clone();
            addr_with_port.set_port(self.port);

            if address.is_ip()
                && matches!(
                    address.scope(),
                    IpScope::Global | IpScope::PseudoPrivate | IpScope::Private | IpScope::Shared
                )
                && !interface_prefix_blacklist.iter().any(|pfx| interface_str.starts_with(pfx))
                && !cidr_blacklist.iter().any(|r| address.is_within(r))
                && !ipv6::is_ipv6_temporary(interface_str.as_str(), address)
            {
                let mut found = false;
                if let Some(byaddr) = existing_bindings.get_mut(interface) {
                    if let Some(socket) = byaddr.remove(&addr_with_port) {
                        found = true;
                        self.sockets.push(socket);
                    }
                }

                if !found {
                    let s = unsafe { bind_udp_to_device(interface_str.as_str(), &addr_with_port) };
                    if s.is_ok() {
                        let fd = s.unwrap();
                        if s.is_ok() {
                            let s = Arc::new(BoundUdpSocket {
                                bind_address: addr_with_port,
                                interface: interface.clone(),
                                last_receive_time: AtomicI64::new(i64::MIN),
                                fd,
                                lock: RwLock::new(()),
                                open: AtomicBool::new(true),
                            });

                            for _ in 0..socket_read_concurrency() {
                                let ss = s.clone();
                                let bp = buffer_pool.clone();
                                let h = handler.clone();
                                std::thread::spawn(move || unsafe {
                                    let _hold = ss.lock.read();
                                    let mut from = InetAddress::new();
                                    while ss.open.load(Ordering::Relaxed) {
                                        let mut b = bp.get();
                                        let s;
                                        #[cfg(windows)]
                                        {
                                            let mut addrlen: usize = std::mem::size_of::<InetAddress>().as_();
                                            s = 0;
                                            todo!();
                                        }
                                        #[cfg(not(windows))]
                                        {
                                            let mut addrlen = std::mem::size_of::<InetAddress>().as_();
                                            s = libc::recvfrom(
                                                ss.fd.as_(),
                                                b.entire_buffer_mut().as_mut_ptr().cast(),
                                                b.capacity().as_(),
                                                0,
                                                (&mut from as *mut InetAddress).cast(),
                                                &mut addrlen,
                                            );
                                        }
                                        if s > 0 {
                                            b.set_size_unchecked(s as usize);
                                            let time_ticks = ms_monotonic();
                                            ss.last_receive_time.store(time_ticks, Ordering::Relaxed);
                                            h.incoming_udp_packet(time_ticks, &ss, &from, b);
                                        }
                                    }
                                });
                            }

                            self.sockets.push(s);
                        } else {
                            errors.push((
                                interface.clone(),
                                addr_with_port,
                                std::io::Error::new(std::io::ErrorKind::Other, s.err().unwrap()),
                            ));
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

        for (_, byaddr) in existing_bindings.iter() {
            for (_, s) in byaddr.iter() {
                s.close();
            }
        }

        errors
    }
}

impl Drop for BoundUdpPort {
    fn drop(&mut self) {
        for s in self.sockets.iter() {
            s.close();
        }
    }
}

/// Attempt to bind universally to a given UDP port and then close to determine if we can use it.
///
/// This succeeds if either IPv4 or IPv6 global can be bound.
pub fn udp_test_bind(port: u16) -> bool {
    std::net::UdpSocket::bind(
        &[
            SocketAddr::new(IpAddr::V4(Ipv4Addr::UNSPECIFIED), port),
            SocketAddr::new(IpAddr::V6(Ipv6Addr::UNSPECIFIED), port),
        ][..],
    )
    .is_ok()
}

#[cfg(windows)]
unsafe fn bind_udp_to_device(device_name: &str, address: &InetAddress) -> Result<i32, &'static str> {
    todo!()
}

#[allow(unused_variables)]
#[cfg(unix)]
unsafe fn bind_udp_to_device(device_name: &str, address: &InetAddress) -> Result<i32, &'static str> {
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

    #[allow(unused_variables)]
    let mut setsockopt_results: libc::c_int = 0;
    let mut fl;

    //assert_ne!(libc::fcntl(s, libc::F_SETFL, libc::O_NONBLOCK), -1);

    let mut timeo: libc::timeval = std::mem::zeroed();
    timeo.tv_sec = SOCKET_RECV_TIMEOUT_SECONDS.as_();
    timeo.tv_usec = 0;
    setsockopt_results |= libc::setsockopt(
        s,
        libc::SOL_SOCKET.as_(),
        libc::SO_RCVTIMEO.as_(),
        (&mut timeo as *mut libc::timeval).cast(),
        std::mem::size_of::<libc::timeval>().as_(),
    );
    debug_assert!(setsockopt_results == 0);

    /*
    fl = 1;
    setsockopt_results |= libc::setsockopt(
        s,
        libc::SOL_SOCKET.as_(),
        libc::SO_REUSEPORT.as_(),
        (&mut fl as *mut libc::c_int).cast(),
        std::mem::size_of::<libc::c_int>().as_(),
    );
    debug_assert!(setsockopt_results == 0);
    */

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

    Ok(s as i32)
}
