/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::os::raw::c_int;
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};

use num_traits::cast::AsPrimitive;

use zerotier_network_hypervisor::vl1::InetAddress;
use zerotier_network_hypervisor::{PacketBuffer, PacketBufferPool};

/*
 * This is a threaded UDP socket listener for high performance. The fastest way to receive UDP
 * (without heroic efforts like kernel bypass) on most platforms is to create a separate socket
 * for each thread using options like SO_REUSEPORT and concurrent packet listening.
 */

#[cfg(windows)]
pub(crate) type FastUDPRawOsSocket = winsock2::SOCKET;

#[cfg(unix)]
pub(crate) type FastUDPRawOsSocket = c_int;

#[cfg(unix)]
fn bind_udp_socket(_device_name: &str, address: &InetAddress) -> Result<FastUDPRawOsSocket, &'static str> {
    unsafe {
        let (af, sa_len) = match address.family() {
            InetAddressFamily::IPv4 => (libc::AF_INET, std::mem::size_of::<libc::sockaddr_in>() as libc::socklen_t),
            InetAddressFamily::IPv6 => (libc::AF_INET6, std::mem::size_of::<libc::sockaddr_in6>() as libc::socklen_t),
            _ => {
                return Err("unrecognized address family");
            }
        };

        let s = libc::socket(af.as_(), libc::SOCK_DGRAM, 0);
        if s < 0 {
            return Err("unable to create socket");
        }

        let mut fl: c_int;
        let fl_size = std::mem::size_of::<c_int>() as libc::socklen_t;
        let mut setsockopt_results: c_int = 0;

        fl = 1;
        setsockopt_results |= libc::setsockopt(s, libc::SOL_SOCKET.as_(), libc::SO_REUSEPORT.as_(), (&mut fl as *mut c_int).cast(), fl_size);

        fl = 0;
        setsockopt_results |= libc::setsockopt(s, libc::SOL_SOCKET.as_(), libc::SO_LINGER.as_(), (&mut fl as *mut c_int).cast(), fl_size);

        //fl = 1;
        //setsockopt_results |= libc::setsockopt(s, libc::SOL_SOCKET, libc::SO_REUSEADDR, (&mut fl as *mut c_int).cast(), fl_size);

        fl = 1;
        setsockopt_results |= libc::setsockopt(s, libc::SOL_SOCKET.as_(), libc::SO_BROADCAST.as_(), (&mut fl as *mut c_int).cast(), fl_size);
        if af == libc::AF_INET6 {
            fl = 1;
            setsockopt_results |= libc::setsockopt(s, libc::IPPROTO_IPV6.as_(), libc::IPV6_V6ONLY.as_(), (&mut fl as *mut c_int).cast(), fl_size);
        }

        #[cfg(any(target_os = "macos", target_os = "ios"))] {
            fl = 1;
            setsockopt_results |= libc::setsockopt(s, libc::SOL_SOCKET.as_(), libc::SO_NOSIGPIPE.as_(), (&mut fl as *mut c_int).cast(), fl_size)
        }

        #[cfg(target_os = "linux")] {
            if !_device_name.is_empty() {
                let _ = std::ffi::CString::new(_device_name).map(|dn| {
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
            #[cfg(not(target_os = "linux"))] {
                fl = 0;
                libc::setsockopt(s, libc::IPPROTO_IP.as_(), libc::IP_DF.as_(), (&mut fl as *mut c_int).cast(), fl_size);
            }
            #[cfg(target_os = "linux")] {
                fl = libc::IP_PMTUDISC_DONT as c_int;
                libc::setsockopt(s, libc::IPPROTO_IP.as_(), libc::IP_MTU_DISCOVER.as_(), (&mut fl as *mut c_int).cast(), fl_size);
            }
        }

        if af == libc::AF_INET6 {
            fl = 0;
            libc::setsockopt(s, libc::IPPROTO_IPV6.as_(), libc::IPV6_DONTFRAG.as_(), (&mut fl as *mut c_int).cast(), fl_size);
        }

        fl = 1048576;
        while fl >= 131072 {
            if libc::setsockopt(s, libc::SOL_SOCKET.as_(), libc::SO_RCVBUF.as_(), (&mut fl as *mut c_int).cast(), fl_size) == 0 {
                break;
            }
            fl -= 65536;
        }
        fl = 1048576;
        while fl >= 131072 {
            if libc::setsockopt(s, libc::SOL_SOCKET.as_(), libc::SO_SNDBUF.as_(), (&mut fl as *mut c_int).cast(), fl_size) == 0 {
                break;
            }
            fl -= 65536;
        }

        if libc::bind(s, (address as *const InetAddress).cast(), sa_len) != 0 {
            libc::close(s);
            return Err("bind to address failed");
        }

        Ok(s)
    }
}

/// A multi-threaded (or otherwise fast) UDP socket that binds to both IPv4 and IPv6 addresses.
pub(crate) struct FastUDPSocket {
    threads: Vec<std::thread::JoinHandle<()>>,
    thread_run: Arc<AtomicBool>,
    sockets: Vec<FastUDPRawOsSocket>,
    pub bind_address: InetAddress,
}

#[cfg(unix)]
#[inline(always)]
fn fast_udp_socket_close(socket: &FastUDPRawOsSocket) {
    unsafe { libc::close(*socket); }
}

#[inline(always)]
pub(crate) fn fast_udp_socket_to_i64(socket: &FastUDPRawOsSocket) -> i64 { (*socket) as i64 }

#[inline(always)]
pub(crate) fn fast_udp_socket_from_i64(socket: i64) -> Option<FastUDPRawOsSocket> {
    if socket >= 0 {
        Some(socket as FastUDPRawOsSocket)
    } else {
        None
    }
}

/// Send to a raw UDP socket with optional packet TTL.
/// If the packet_ttl option is <=0, packet is sent with the default TTL. TTL setting is only used
/// in ZeroTier right now to do escalating TTL probes for IPv4 NAT traversal.
#[cfg(unix)]
#[inline(always)]
pub(crate) fn fast_udp_socket_sendto(socket: &FastUDPRawOsSocket, to_address: &InetAddress, data: &[u8], packet_ttl: i32) {
    unsafe {
        if packet_ttl <= 0 {
            libc::sendto(*socket, data.as_ptr().cast(), data.len().as_(), 0, (to_address as *const InetAddress).cast(), std::mem::size_of::<InetAddress>().as_());
        } else {
            let mut ttl = packet_ttl as c_int;
            libc::setsockopt(*socket, libc::IPPROTO_IP.as_(), libc::IP_TTL.as_(), (&mut ttl as *mut c_int).cast(), std::mem::size_of::<c_int>().as_());
            libc::sendto(*socket, data.as_ptr().cast(), data.len().as_(), 0, (to_address as *const InetAddress).cast(), std::mem::size_of::<InetAddress>().as_());
            ttl = 255;
            libc::setsockopt(*socket, libc::IPPROTO_IP.as_(), libc::IP_TTL.as_(), (&mut ttl as *mut c_int).cast(), std::mem::size_of::<c_int>().as_());
        }
    }
}

#[cfg(unix)]
#[inline(always)]
fn fast_udp_socket_recvfrom(socket: &FastUDPRawOsSocket, buf: &mut PacketBuffer, from_address: &mut InetAddress) -> isize {
    unsafe {
        let mut addrlen = std::mem::size_of::<InetAddress>() as libc::socklen_t;
        let s = libc::recvfrom(*socket, buf.as_mut_ptr().cast(), buf.capacity().as_(), 0, (from_address as *mut InetAddress).cast(), &mut addrlen) as isize;
        if s > 0 {
            buf.set_size_unchecked(s as usize);
        }
        s
    }
}

impl FastUDPSocket {
    pub fn new<F: Fn(&FastUDPRawOsSocket, &InetAddress, PacketBuffer) + Send + Sync + Clone + 'static>(device_name: &str, address: &InetAddress, packet_buffer_pool: &Arc<PacketBufferPool>, handler: F) -> Result<Self, String> {
        let thread_count = num_cpus::get_physical().clamp(1, 4);

        let mut s = Self {
            thread_run: Arc::new(AtomicBool::new(true)),
            threads: Vec::new(),
            sockets: Vec::new(),
            bind_address: address.clone(),
        };
        s.threads.reserve(thread_count);
        s.sockets.reserve(thread_count);

        let mut bind_failed_reason: &'static str = "";
        for _ in 0..thread_count {
            let thread_socket = bind_udp_socket(device_name, address);
            if thread_socket.is_ok() {
                let thread_socket = thread_socket.unwrap();
                s.sockets.push(thread_socket);

                let thread_run = s.thread_run.clone();
                let handler_copy = handler.clone();
                let packet_buffer_pool_copy = packet_buffer_pool.clone();
                s.threads.push(std::thread::Builder::new().stack_size(zerotier_core::RECOMMENDED_THREAD_STACK_SIZE).spawn(move || {
                    let mut from_address = InetAddress::new();
                    while thread_run.load(Ordering::Relaxed) {
                        let mut buf = packet_buffer_pool_copy.get_packet_buffer();
                        let s = fast_udp_socket_recvfrom(&thread_socket, &mut buf, &mut from_address);
                        if s > 0 {
                            handler_copy(&thread_socket, &from_address, buf);
                        } else if s < 0 {
                            break;
                        }
                    }
                }).unwrap());
            } else {
                bind_failed_reason = thread_socket.err().unwrap();
            }
        }

        // This is successful if it is able to bind successfully once and launch at least one thread,
        // since in a few cases it may be impossible to do multithreaded binding such as old Linux
        // kernels or emulation layers.
        if s.sockets.is_empty() {
            return Err(format!("unable to bind to address for IPv4 or IPv6 ({})", bind_failed_reason));
        }

        Ok(s)
    }

    #[inline(always)]
    pub fn all_sockets(&self) -> &[FastUDPRawOsSocket] {
        self.sockets.as_slice()
    }

    #[inline(always)]
    pub fn send(&self, to_address: &InetAddress, data: &[u8], packet_ttl: i32) {
        fast_udp_socket_sendto(self.sockets.get(0).unwrap(), to_address, data, packet_ttl);
    }

    #[inline(always)]
    pub fn raw_socket(&self) -> FastUDPRawOsSocket {
        *self.sockets.get(0).unwrap()
    }
}

impl Drop for FastUDPSocket {
    #[cfg(windows)]
    fn drop(&mut self) {
        todo!()
    }

    #[cfg(unix)]
    fn drop(&mut self) {
        let tmp: [u8; 1] = [0];
        self.thread_run.store(false, Ordering::Relaxed);
        for s in self.sockets.iter() {
            unsafe {
                libc::sendto(*s, tmp.as_ptr().cast(), 0, 0, (&self.bind_address as *const InetAddress).cast(), std::mem::size_of::<InetAddress>() as osdep::socklen_t);
            }
        }
        for s in self.sockets.iter() {
            unsafe {
                libc::shutdown(*s, libc::SHUT_RDWR.as_());
            }
        }
        for s in self.sockets.iter() {
            unsafe {
                libc::close(*s);
            }
        }
        while !self.threads.is_empty() {
            let _ = self.threads.pop().unwrap().join();
        }
    }
}

#[cfg(test)]
mod tests {
    use std::sync::atomic::{AtomicU32, Ordering};

    use crate::fastudpsocket::*;
    use zerotier_network_hypervisor::{PacketBufferPool, PacketBufferFactory, PacketBuffer};

    #[test]
    fn test_udp_bind_and_transfer() {
        {
            let pool = Arc::new(PacketBufferPool::new(64, PacketBufferFactory));

            let ba0 = InetAddress::new_from_string("127.0.0.1/23333");
            assert!(ba0.is_some());
            let ba0 = ba0.unwrap();
            let cnt0 = Arc::new(AtomicU32::new(0));
            let cnt0c = cnt0.clone();
            let s0 = FastUDPSocket::new("", &ba0, &pool,  move |sock: &FastUDPRawOsSocket, _: &InetAddress, data: PacketBuffer| {
                cnt0c.fetch_add(1, Ordering::Relaxed);
            });
            assert!(s0.is_ok());
            let s0 = s0.unwrap();

            let ba1 = InetAddress::new_from_string("127.0.0.1/23334");
            assert!(ba1.is_some());
            let ba1 = ba1.unwrap();
            let cnt1 = Arc::new(AtomicU32::new(0));
            let cnt1c = cnt1.clone();
            let s1 = FastUDPSocket::new("", &ba1, &pool, move |sock: &FastUDPRawOsSocket, _: &InetAddress, data: PacketBuffer| {
                cnt1c.fetch_add(1, Ordering::Relaxed);
            });
            assert!(s1.is_ok());
            let s1 = s1.unwrap();

            let data_bytes = [0_u8; 1024];
            loop {
                s0.send(&ba1, &data_bytes, 0);
                s1.send(&ba0, &data_bytes, 0);
                if cnt0.load(Ordering::Relaxed) > 10000 && cnt1.load(Ordering::Relaxed) > 10000 {
                    break;
                }
            }
        }
        println!("FastUDPSocket shutdown successful");
    }
}
