use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use zerotier_core::{Buffer, InetAddress, InetAddressFamily};

//
// A very low-level fast UDP socket that uses thread-per-core semantics to
// achieve maximum possible throughput. This will spawn a lot of threads but
// these threads will be inactive unless packets are being received with them.
//
// On most OSes this is by far the fastest way to handle incoming UDP except
// for bypassing the kernel's TCP/IP stack entirely.
//

#[cfg(windows)]
use winapi::um::winsock2 as winsock2;

#[cfg(windows)]
pub type FastUDPRawOsSocket = winsock2::SOCKET;

#[cfg(unix)]
pub type FastUDPRawOsSocket = libc::c_int;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// bind_udp_socket() implementations for each platform

#[cfg(target_os = "macos")]
fn bind_udp_socket(_: &str, address: &InetAddress) -> Result<FastUDPRawOsSocket, &'static str> {
    unsafe {
        let af;
        let sa_len;
        match address.family() {
            InetAddressFamily::IPv4 => {
                af = libc::AF_INET;
                sa_len = std::mem::size_of::<libc::sockaddr_in>() as libc::socklen_t;
            },
            InetAddressFamily::IPv6 => {
                af = libc::AF_INET6;
                sa_len = std::mem::size_of::<libc::sockaddr_in6>() as libc::socklen_t;
            },
            _ => {
                return Err("unrecognized address family");
            }
        };

        let s = libc::socket(af, libc::SOCK_DGRAM, 0);
        if s < 0 {
            return Err("unable to create socket");
        }

        let mut fl: libc::c_int;
        let fl_size = std::mem::size_of::<libc::c_int>() as libc::socklen_t;
        let mut setsockopt_results: libc::c_int = 0;

        // Set options that must succeed: reuse port for multithreading, enable broadcast, disable SIGPIPE, and
        // for IPv6 sockets disable receipt of IPv4 packets.
        fl = 1;
        setsockopt_results |= libc::setsockopt(s, libc::SOL_SOCKET, libc::SO_REUSEPORT, (&mut fl as *mut libc::c_int).cast(), fl_size);
        //fl = 1;
        //setsockopt_results |= libc::setsockopt(s, libc::SOL_SOCKET, libc::SO_REUSEADDR, (&mut fl as *mut libc::c_int).cast(), fl_size);
        fl = 1;
        setsockopt_results |= libc::setsockopt(s, libc::SOL_SOCKET, libc::SO_BROADCAST, (&mut fl as *mut libc::c_int).cast(), fl_size);
        fl = 1;
        setsockopt_results |= libc::setsockopt(s, libc::SOL_SOCKET, libc::SO_NOSIGPIPE, (&mut fl as *mut libc::c_int).cast(), fl_size);
        if af == libc::AF_INET6 {
            fl = 1;
            setsockopt_results |= libc::setsockopt(s, libc::IPPROTO_IPV6, libc::IPV6_V6ONLY, (&mut fl as *mut libc::c_int).cast(), fl_size);
        }
        if setsockopt_results != 0 {
            libc::close(s);
            return Err("setsockopt() failed");
        }

        // Enable UDP fragmentation, which should never really be needed but might make this work if
        // somebody finds themselves on a weird network. These are okay if they fail.
        if af == libc::AF_INET {
            fl = 0;
            libc::setsockopt(s, libc::IPPROTO_IP, 0x4000 /* IP_DF */, (&mut fl as *mut libc::c_int).cast(), fl_size);
        }
        if af == libc::AF_INET6 {
            fl = 0;
            libc::setsockopt(s, libc::IPPROTO_IPV6, 62 /* IPV6_DONTFRAG */, (&mut fl as *mut libc::c_int).cast(), fl_size);
        }

        // Set send and receive buffers to the largest acceptable value up to desired 1MiB.
        fl = 1048576;
        while fl >= 131072 {
            if libc::setsockopt(s, libc::SOL_SOCKET, libc::SO_RCVBUF, (&mut fl as *mut libc::c_int).cast(), fl_size) == 0 {
                break;
            }
            fl -= 65536;
        }
        fl = 1048576;
        while fl >= 131072 {
            if libc::setsockopt(s, libc::SOL_SOCKET, libc::SO_SNDBUF, (&mut fl as *mut libc::c_int).cast(), fl_size) == 0 {
                break;
            }
            fl -= 65536;
        }

        if libc::bind(s, (address as *const InetAddress).cast(), sa_len) != 0 {
            //libc::perror(std::ptr::null());
            libc::close(s);
            return Err("bind to address failed");
        }

        Ok(s)
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Handler for incoming packets received by a FastUDPSocket.
/// Note that this may be called concurrently from any number of threads.
pub trait FastUDPSocketPacketHandler {
    fn incoming_udp_packet(&self, raw_socket: &FastUDPRawOsSocket, from_adddress: &InetAddress, data: Buffer);
}

/// A multi-threaded (or otherwise fast) UDP socket that binds to both IPv4 and IPv6 addresses.
pub struct FastUDPSocket<H: FastUDPSocketPacketHandler + Send + Sync + 'static> {
    handler: Arc<H>,
    threads: Vec<std::thread::JoinHandle<()>>,
    thread_run: Arc<AtomicBool>,
    sockets: Vec<FastUDPRawOsSocket>,
    bind_address: InetAddress,
}

#[cfg(unix)]
#[inline(always)]
pub fn fast_udp_socket_sendto(socket: &FastUDPRawOsSocket, to_address: &InetAddress, data: *const u8, len: usize, packet_ttl: i32) {
    unsafe {
        if packet_ttl <= 0 {
            libc::sendto(*socket, data.cast(), len as libc::size_t, 0, (to_address as *const InetAddress).cast(), std::mem::size_of::<InetAddress>() as libc::socklen_t);
        } else {
            let mut ttl = packet_ttl as libc::c_int;
            libc::setsockopt(*socket, libc::IPPROTO_IP, libc::IP_TTL, (&mut ttl as *mut libc::c_int).cast(), std::mem::size_of::<libc::c_int>() as libc::socklen_t);
            libc::sendto(*socket, data.cast(), len as libc::size_t, 0, (to_address as *const InetAddress).cast(), std::mem::size_of::<InetAddress>() as libc::socklen_t);
            ttl = 255;
            libc::setsockopt(*socket, libc::IPPROTO_IP, libc::IP_TTL, (&mut ttl as *mut libc::c_int).cast(), std::mem::size_of::<libc::c_int>() as libc::socklen_t);
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
        let mut addrlen = std::mem::size_of::<InetAddress>() as libc::socklen_t;
        libc::recvfrom(*socket, buf.as_mut_ptr().cast(), Buffer::CAPACITY as libc::size_t, 0, (from_address as *mut InetAddress).cast(), &mut addrlen) as i32
    }
}

// Integer incremented to select sockets on a mostly round robin basis. This
// isn't synchronized since if all cores don't see it the same there is no
// significant impact. It's just a faster way to pick a socket for sending
// than a random number generator.
static mut SOCKET_SPIN_INT: usize = 0;

impl<H: FastUDPSocketPacketHandler + Send + Sync + 'static> FastUDPSocket<H> {
    pub fn new(device_name: &str, address: &InetAddress, handler: &Arc<H>) -> Result<FastUDPSocket<H>, String> {
        let thread_count = num_cpus::get();

        let mut s = FastUDPSocket{
            handler: handler.clone(),
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
                let handler_weak = Arc::downgrade(&s.handler);
                s.threads.push(std::thread::spawn(move || {
                    let mut from_address = InetAddress::new();
                    while thread_run.load(Ordering::Relaxed) {
                        let mut buf = Buffer::new();
                        let read_length = fast_udp_socket_recvfrom(&thread_socket, &mut buf, &mut from_address);
                        if read_length > 0 {
                            let handler = handler_weak.upgrade();
                            if handler.is_some() {
                                unsafe { buf.set_len(read_length as u32); }
                                handler.unwrap().incoming_udp_packet(&thread_socket, &from_address, buf);
                            } else {
                                break;
                            }
                        } else if read_length < 0 {
                            break;
                        }
                    }
                }));
            } else {
                bind_failed_reason = thread_socket.err().unwrap();
            }
        }

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
        let mut i;
        unsafe {
            i = SOCKET_SPIN_INT;
            SOCKET_SPIN_INT = i + 1;
        }
        i %= self.sockets.len();
        fast_udp_socket_sendto(self.sockets.get(i).unwrap(), to_address, data, len, packet_ttl);
    }

    /// Get the number of threads this socket is currently running.
    #[inline(always)]
    pub fn thread_count(&self) -> usize {
        self.threads.len()
    }
}

impl<H: FastUDPSocketPacketHandler + Send + Sync + 'static> Drop for FastUDPSocket<H> {
    #[cfg(windows)]
    fn drop(&mut self) {
        self.thread_run.store(false, Ordering::Relaxed);
        // TODO
        for t in self.threads.iter() {
            t.join()
        }
    }

    #[cfg(unix)]
    fn drop(&mut self) {
        let tmp: [u8; 1] = [0];
        self.thread_run.store(false, Ordering::Relaxed);
        for s in self.sockets.iter() {
            unsafe {
                libc::sendto(*s, tmp.as_ptr().cast(), 0, 0, (&self.bind_address as *const InetAddress).cast(), std::mem::size_of::<InetAddress>() as libc::socklen_t);
            }
        }
        for s in self.sockets.iter() {
            unsafe {
                libc::shutdown(*s, libc::SHUT_RDWR);
            }
        }
        for s in self.sockets.iter() {
            unsafe {
                libc::close(*s);
            }
        }
        while !self.threads.is_empty() {
            self.threads.pop().unwrap().join().expect("unable to join to thread");
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::fastudp::*;
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
