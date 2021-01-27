/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use zerotier_core::{Buffer, InetAddress, InetAddressFamily};
use num_traits::cast::AsPrimitive;
use std::os::raw::c_int;
use crate::osdep as osdep;

/*
 * This is a threaded UDP socket listener for high performance. The fastest way to receive UDP
 * (without heroic efforts like kernel bypass) on most platforms is to create a separate socket
 * for each thread using options like SO_REUSEPORT and concurrent packet listening.
 */

#[cfg(windows)]
use winapi::um::winsock2 as winsock2;

#[cfg(windows)]
pub type FastUDPRawOsSocket = winsock2::SOCKET;

#[cfg(unix)]
pub type FastUDPRawOsSocket = c_int;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// bind_udp_socket() implementations for each platform

#[cfg(target_os = "macos")]
fn bind_udp_socket(_: &str, address: &InetAddress) -> Result<FastUDPRawOsSocket, &'static str> {
    unsafe {
        let af;
        let sa_len;
        match address.family() {
            InetAddressFamily::IPv4 => {
                af = osdep::AF_INET;
                sa_len = std::mem::size_of::<osdep::sockaddr_in>() as osdep::socklen_t;
            },
            InetAddressFamily::IPv6 => {
                af = osdep::AF_INET6;
                sa_len = std::mem::size_of::<osdep::sockaddr_in6>() as osdep::socklen_t;
            },
            _ => {
                return Err("unrecognized address family");
            }
        };

        let s = osdep::socket(af.as_(), osdep::SOCK_DGRAM.as_(), 0);
        if s < 0 {
            return Err("unable to create socket");
        }

        let mut fl: c_int;
        let fl_size = std::mem::size_of::<c_int>() as osdep::socklen_t;
        let mut setsockopt_results: c_int = 0;

        // Set options that must succeed: reuse port for multithreading, enable broadcast, disable SIGPIPE, and
        // for IPv6 sockets disable receipt of IPv4 packets.
        fl = 1;
        setsockopt_results |= osdep::setsockopt(s, osdep::SOL_SOCKET.as_(), osdep::SO_REUSEPORT.as_(), (&mut fl as *mut c_int).cast(), fl_size);
        //fl = 1;
        //setsockopt_results |= osdep::setsockopt(s, osdep::SOL_SOCKET, osdep::SO_REUSEADDR, (&mut fl as *mut c_int).cast(), fl_size);
        fl = 1;
        setsockopt_results |= osdep::setsockopt(s, osdep::SOL_SOCKET.as_(), osdep::SO_BROADCAST.as_(), (&mut fl as *mut c_int).cast(), fl_size);
        fl = 1;
        setsockopt_results |= osdep::setsockopt(s, osdep::SOL_SOCKET.as_(), osdep::SO_NOSIGPIPE.as_(), (&mut fl as *mut c_int).cast(), fl_size);
        if af == osdep::AF_INET6 {
            fl = 1;
            setsockopt_results |= osdep::setsockopt(s, osdep::IPPROTO_IPV6.as_(), osdep::IPV6_V6ONLY.as_(), (&mut fl as *mut c_int).cast(), fl_size);
        }
        if setsockopt_results != 0 {
            osdep::close(s);
            return Err("setsockopt() failed");
        }

        // Enable UDP fragmentation, which should never really be needed but might make this work if
        // somebody finds themselves on a weird network. These are okay if they fail.
        if af == osdep::AF_INET {
            fl = 0;
            osdep::setsockopt(s, osdep::IPPROTO_IP.as_(), 0x4000 /* IP_DF */, (&mut fl as *mut c_int).cast(), fl_size);
        }
        if af == osdep::AF_INET6 {
            fl = 0;
            osdep::setsockopt(s, osdep::IPPROTO_IPV6.as_(), 62 /* IPV6_DONTFRAG */, (&mut fl as *mut c_int).cast(), fl_size);
        }

        // Set send and receive buffers to the largest acceptable value up to desired 1MiB.
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
            //osdep::perror(std::ptr::null());
            osdep::close(s);
            return Err("bind to address failed");
        }

        Ok(s)
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// A multi-threaded (or otherwise fast) UDP socket that binds to both IPv4 and IPv6 addresses.
pub struct FastUDPSocket {
    threads: Vec<std::thread::JoinHandle<()>>,
    thread_run: Arc<AtomicBool>,
    sockets: Vec<FastUDPRawOsSocket>,
    pub bind_address: InetAddress,
}

/// Send to a raw UDP socket with optional packet TTL.
/// If the packet_ttl option is <=0, packet is sent with the default TTL. TTL setting is only used
/// in ZeroTier right now to do escalating TTL probes for IPv4 NAT traversal.
#[cfg(unix)]
#[inline(always)]
pub fn fast_udp_socket_sendto(socket: &FastUDPRawOsSocket, to_address: &InetAddress, data: *const u8, len: usize, packet_ttl: i32) {
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
pub fn fast_udp_socket_sendto(socket: &FastUDPRawOsSocket, to_address: &InetAddress, data: &[u8], packet_ttl: i32) {
}

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

        let mut s = FastUDPSocket{
            thread_run: Arc::new(AtomicBool::new(true)),
            threads: Vec::new(),
            sockets: Vec::new(),
            bind_address: address.clone()
        };

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
                            unsafe { buf.set_len(read_length as usize); }
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

    /// Get the number of threads this socket is currently running.
    #[inline(always)]
    pub fn thread_count(&self) -> usize {
        self.threads.len()
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

/*
#[cfg(test)]
mod tests {
    use crate::fastudpsocket::*;
    use zerotier_core::{InetAddress, Buffer};
    use std::sync::Arc;
    use std::sync::atomic::{AtomicU32, Ordering};

    #[allow(dead_code)]
    struct TestPacketHandler {
        cnt: AtomicU32,
        side: &'static str
    }

    impl FastUDPSocketPacketHandler for TestPacketHandler {
        #[allow(unused)]
        fn incoming_udp_packet(&self, raw_socket: &FastUDPRawOsSocket, from_adddress: &InetAddress, data: Buffer) {
            self.cnt.fetch_add(1, Ordering::Relaxed);
            //println!("{}: {} bytes from {} (socket: {})", self.side, data.len(), from_adddress.to_string().as_str(), *raw_socket);
        }
    }

    #[test]
    fn test_udp_bind_and_transfer() {
        {
            let ba1 = InetAddress::new_from_string("127.0.0.1/23333");
            assert!(ba1.is_some());
            let ba1 = ba1.unwrap();
            let h1: Arc<TestPacketHandler> = Arc::new(TestPacketHandler {
                cnt: AtomicU32::new(0),
                side: "Alice",
            });
            let s1 = FastUDPSocket::new("lo0", &ba1, &h1);
            assert!(s1.is_ok());
            let s1 = s1.ok().unwrap();

            let ba2 = InetAddress::new_from_string("127.0.0.1/23334");
            assert!(ba2.is_some());
            let ba2 = ba2.unwrap();
            let h2: Arc<TestPacketHandler> = Arc::new(TestPacketHandler {
                cnt: AtomicU32::new(0),
                side: "Bob",
            });
            let s2 = FastUDPSocket::new("lo0", &ba2, &h2);
            assert!(s2.is_ok());
            let s2 = s2.ok().unwrap();

            let data_bytes = [0_u8; 1024];
            loop {
                s1.send(&ba2, data_bytes.as_ptr(), data_bytes.len(), 0);
                s2.send(&ba1, data_bytes.as_ptr(), data_bytes.len(), 0);
                if h1.cnt.load(Ordering::Relaxed) > 10000 && h2.cnt.load(Ordering::Relaxed) > 10000 {
                    break;
                }
            }
        }
        //println!("FastUDPSocket shutdown successful");
    }
}
*/
