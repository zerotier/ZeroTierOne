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

use std::os::raw::c_int;
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};

use num_traits::cast::AsPrimitive;

use zerotier_core::{Buffer, InetAddress, InetAddressFamily};

use crate::osdep as osdep;

/*
 * This is a threaded UDP socket listener for high performance. The fastest way to receive UDP
 * (without heroic efforts like kernel bypass) on most platforms is to create a separate socket
 * for each thread using options like SO_REUSEPORT and concurrent packet listening.
 */

#[cfg(windows)] use winapi::um::winsock2 as winsock2;

#[cfg(windows)] pub(crate) type FastUDPRawOsSocket = winsock2::SOCKET;
#[cfg(unix)] pub(crate) type FastUDPRawOsSocket = c_int;

#[cfg(unix)]
fn bind_udp_socket(_device_name: &str, address: &InetAddress) -> Result<FastUDPRawOsSocket, &'static str> {
    unsafe {
        let (af, sa_len) = match address.family() {
            InetAddressFamily::IPv4 => (osdep::AF_INET, std::mem::size_of::<osdep::sockaddr_in>() as osdep::socklen_t),
            InetAddressFamily::IPv6 => (osdep::AF_INET6, std::mem::size_of::<osdep::sockaddr_in6>() as osdep::socklen_t),
            _ => {
                return Err("unrecognized address family");
            }
        };

        #[cfg(not(target_os = "linux"))]
        let s = osdep::socket(af.as_(), osdep::SOCK_DGRAM.as_(), 0);
        #[cfg(target_os = "linux")]
        let s = osdep::socket(af.as_(), 2, 0);

        if s < 0 {
            return Err("unable to create socket");
        }

        let mut fl: c_int;
        let fl_size = std::mem::size_of::<c_int>() as osdep::socklen_t;
        let mut setsockopt_results: c_int = 0;

        fl = 1;
        setsockopt_results |= osdep::setsockopt(s, osdep::SOL_SOCKET.as_(), osdep::SO_REUSEPORT.as_(), (&mut fl as *mut c_int).cast(), fl_size);
        //fl = 1;
        //setsockopt_results |= osdep::setsockopt(s, osdep::SOL_SOCKET, osdep::SO_REUSEADDR, (&mut fl as *mut c_int).cast(), fl_size);
        fl = 1;
        setsockopt_results |= osdep::setsockopt(s, osdep::SOL_SOCKET.as_(), osdep::SO_BROADCAST.as_(), (&mut fl as *mut c_int).cast(), fl_size);
        if af == osdep::AF_INET6 {
            fl = 1;
            setsockopt_results |= osdep::setsockopt(s, osdep::IPPROTO_IPV6.as_(), osdep::IPV6_V6ONLY.as_(), (&mut fl as *mut c_int).cast(), fl_size);
        }

        #[cfg(any(target_os = "macos", target_os = "ios"))] {
            fl = 1;
            setsockopt_results |= osdep::setsockopt(s, osdep::SOL_SOCKET.as_(), osdep::SO_NOSIGPIPE.as_(), (&mut fl as *mut c_int).cast(), fl_size)
        }

        #[cfg(target_os = "linux")] {
            if !_device_name.is_empty() {
                let _ = std::ffi::CString::new(_device_name).map(|dn| {
                    let dnb = dn.as_bytes_with_nul();
                    let _ = osdep::setsockopt(s.as_(), osdep::SOL_SOCKET.as_(), osdep::SO_BINDTODEVICE.as_(), dnb.as_ptr().cast(), (dnb.len() - 1).as_());
                });
            }
        }

        if setsockopt_results != 0 {
            osdep::close(s);
            return Err("setsockopt() failed");
        }

        if af == osdep::AF_INET {
            #[cfg(not(target_os = "linux"))] {
                fl = 0;
                osdep::setsockopt(s, osdep::IPPROTO_IP.as_(), osdep::IP_DF.as_(), (&mut fl as *mut c_int).cast(), fl_size);
            }
            #[cfg(target_os = "linux")] {
                fl = osdep::IP_PMTUDISC_DONT as c_int;
                osdep::setsockopt(s, osdep::IPPROTO_IP.as_(), osdep::IP_MTU_DISCOVER.as_(), (&mut fl as *mut c_int).cast(), fl_size);
            }
        }

        if af == osdep::AF_INET6 {
            fl = 0;
            osdep::setsockopt(s, osdep::IPPROTO_IPV6.as_(), osdep::IPV6_DONTFRAG.as_(), (&mut fl as *mut c_int).cast(), fl_size);
        }

        fl = 1048576;
        while fl >= 131072 {
            if osdep::setsockopt(s, osdep::SOL_SOCKET.as_(), osdep::SO_RCVBUF.as_(), (&mut fl as *mut c_int).cast(), fl_size) == 0 {
                break;
            }
            fl -= 65536;
        }
        fl = 1048576;
        while fl >= 131072 {
            if osdep::setsockopt(s, osdep::SOL_SOCKET.as_(), osdep::SO_SNDBUF.as_(), (&mut fl as *mut c_int).cast(), fl_size) == 0 {
                break;
            }
            fl -= 65536;
        }

        if osdep::bind(s, (address as *const InetAddress).cast(), sa_len) != 0 {
            osdep::close(s);
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
    unsafe {
        osdep::close(*socket);
    }
}

#[cfg(windows)]
#[inline(always)]
fn fast_udp_socket_close(socket: &FastUDPRawOsSocket) {
    unsafe {
        osdep::close(*socket);
    }
}

#[inline(always)]
pub(crate) fn fast_udp_socket_to_i64(socket: &FastUDPRawOsSocket) -> i64 {
    (*socket) as i64
}

#[inline(always)]
pub(crate) fn fast_udp_socket_from_i64(socket: i64) -> Option<FastUDPRawOsSocket> {
    if socket >= 0 {
        return Some(socket as FastUDPRawOsSocket);
    }
    None
}

/// Send to a raw UDP socket with optional packet TTL.
/// If the packet_ttl option is <=0, packet is sent with the default TTL. TTL setting is only used
/// in ZeroTier right now to do escalating TTL probes for IPv4 NAT traversal.
#[cfg(unix)]
#[inline(always)]
pub(crate) fn fast_udp_socket_sendto(socket: &FastUDPRawOsSocket, to_address: &InetAddress, data: *const u8, len: usize, packet_ttl: i32) {
    unsafe {
        if packet_ttl <= 0 {
            osdep::sendto(*socket, data.cast(), len.as_(), 0, (to_address as *const InetAddress).cast(), std::mem::size_of::<InetAddress>().as_());
        } else {
            let mut ttl = packet_ttl as c_int;
            osdep::setsockopt(*socket, osdep::IPPROTO_IP.as_(), osdep::IP_TTL.as_(), (&mut ttl as *mut c_int).cast(), std::mem::size_of::<c_int>().as_());
            osdep::sendto(*socket, data.cast(), len.as_(), 0, (to_address as *const InetAddress).cast(), std::mem::size_of::<InetAddress>().as_());
            ttl = 255;
            osdep::setsockopt(*socket, osdep::IPPROTO_IP.as_(), osdep::IP_TTL.as_(), (&mut ttl as *mut c_int).cast(), std::mem::size_of::<c_int>().as_());
        }
    }
}

#[cfg(windows)]
#[inline(always)]
pub(crate) fn fast_udp_socket_sendto(socket: &FastUDPRawOsSocket, to_address: &InetAddress, data: &[u8], packet_ttl: i32) {}

#[cfg(unix)]
#[inline(always)]
fn fast_udp_socket_recvfrom(socket: &FastUDPRawOsSocket, buf: &mut Buffer, from_address: &mut InetAddress) -> i32 {
    unsafe {
        let mut addrlen = std::mem::size_of::<InetAddress>() as osdep::socklen_t;
        osdep::recvfrom(*socket, buf.as_mut_ptr().cast(), Buffer::CAPACITY.as_(), 0, (from_address as *mut InetAddress).cast(), &mut addrlen) as i32
    }
}

impl FastUDPSocket {
    pub fn new<F: Fn(&FastUDPRawOsSocket, &InetAddress, Buffer) + Send + Sync + Clone + 'static>(device_name: &str, address: &InetAddress, handler: F) -> Result<FastUDPSocket, String> {
        let thread_count = num_cpus::get_physical().min(num_cpus::get());

        let mut s = FastUDPSocket {
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
                s.threads.push(std::thread::Builder::new().stack_size(zerotier_core::RECOMMENDED_THREAD_STACK_SIZE).spawn(move || {
                    let mut from_address = InetAddress::new();
                    while thread_run.load(Ordering::Relaxed) {
                        let mut buf = Buffer::new();
                        let read_length = fast_udp_socket_recvfrom(&thread_socket, &mut buf, &mut from_address);
                        if read_length > 0 {
                            buf.set_len(read_length as usize);
                            handler_copy(&thread_socket, &from_address, buf);
                        } else if read_length < 0 {
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

    /// Get a slice of all raw sockets used.
    #[inline(always)]
    pub fn all_sockets(&self) -> &[FastUDPRawOsSocket] {
        self.sockets.as_slice()
    }

    /// Send from this socket.
    /// This actually picks a thread's socket and sends from it. Since all
    /// are bound to the same IP:port which one is chosen doesn't matter.
    /// Sockets are thread safe.
    #[inline(always)]
    pub fn send(&self, to_address: &InetAddress, data: *const u8, len: usize, packet_ttl: i32) {
        fast_udp_socket_sendto(self.sockets.get(0).unwrap(), to_address, data, len, packet_ttl);
    }

    /// Get a raw socket that can be used to send UDP packets.
    #[inline(always)]
    pub fn raw_socket(&self) -> FastUDPRawOsSocket {
        *self.sockets.get(0).unwrap()
    }
}

impl Drop for FastUDPSocket {
    #[cfg(windows)]
    fn drop(&mut self) {
        // TODO
    }

    #[cfg(unix)]
    fn drop(&mut self) {
        let tmp: [u8; 1] = [0];
        self.thread_run.store(false, Ordering::Relaxed);
        for s in self.sockets.iter() {
            unsafe {
                osdep::sendto(*s, tmp.as_ptr().cast(), 0, 0, (&self.bind_address as *const InetAddress).cast(), std::mem::size_of::<InetAddress>() as osdep::socklen_t);
            }
        }
        for s in self.sockets.iter() {
            unsafe {
                osdep::shutdown(*s, osdep::SHUT_RDWR.as_());
            }
        }
        for s in self.sockets.iter() {
            unsafe {
                osdep::close(*s);
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

    use zerotier_core::{Buffer, InetAddress};

    use crate::fastudpsocket::*;

    #[test]
    fn test_udp_bind_and_transfer() {
        {
            let ba0 = InetAddress::new_from_string("127.0.0.1/23333");
            assert!(ba0.is_some());
            let ba0 = ba0.unwrap();
            let cnt0 = Arc::new(AtomicU32::new(0));
            let cnt0c = cnt0.clone();
            let s0 = FastUDPSocket::new("", &ba0, move |sock: &FastUDPRawOsSocket, _: &InetAddress, data: Buffer| {
                cnt0c.fetch_add(1, Ordering::Relaxed);
            });
            assert!(s0.is_ok());
            let s0 = s0.unwrap();

            let ba1 = InetAddress::new_from_string("127.0.0.1/23334");
            assert!(ba1.is_some());
            let ba1 = ba1.unwrap();
            let cnt1 = Arc::new(AtomicU32::new(0));
            let cnt1c = cnt1.clone();
            let s1 = FastUDPSocket::new("", &ba1, move |sock: &FastUDPRawOsSocket, _: &InetAddress, data: Buffer| {
                cnt1c.fetch_add(1, Ordering::Relaxed);
            });
            assert!(s1.is_ok());
            let s1 = s1.unwrap();

            let data_bytes = [0_u8; 1024];
            loop {
                s0.send(&ba1, data_bytes.as_ptr(), data_bytes.len(), 0);
                s1.send(&ba0, data_bytes.as_ptr(), data_bytes.len(), 0);
                if cnt0.load(Ordering::Relaxed) > 10000 && cnt1.load(Ordering::Relaxed) > 10000 {
                    break;
                }
            }
        }
        //println!("FastUDPSocket shutdown successful");
    }
}
