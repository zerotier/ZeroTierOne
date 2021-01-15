use std::sync::Arc;
use std::sync::atomic::{AtomicBool, AtomicU32, Ordering};
use zerotier_core::{Buffer, InetAddress, InetAddressFamily};
use std::ffi::CString;

#[cfg(windows)]
pub type RawOsSocket = winapi::um::winsock2::SOCKET;

#[cfg(windows)]
type AfInet = winapi::um::winsock2::AF_INET;

#[cfg(windows)]
type AfInet6 = winapi::um::winsock2::AF_INET6;

#[cfg(unix)]
pub type RawOsSocket = std::os::raw::c_int;

#[cfg(unix)]
type AfInet = libc::AF_INET;

#[cfg(unix)]
type AfInet6 = libc::AF_INET6;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#[cfg(target_os = "macos")]
unsafe fn bind_udp_socket(device_name: &CString, address: &InetAddress, af: libc::c_int) -> Option<RawOsSocket> {
    let s = libc::socket(af, libc::SOCK_DGRAM, 0);
    if s < 0 {
        return None;
    }

    let mut fl: libc::c_int;
    let fl_size = std::mem::size_of::<libc::c_int>() as libc::socklen_t;
    let mut setsockopt_results: libc::c_int = 0;

    fl = 1;
    setsockopt_results |= libc::setsockopt(s, libc::SOL_SOCKET, libc::SO_REUSEPORT, &mut fl, fl_size);
    fl = 1;
    setsockopt_results |= libc::setsockopt(s, libc::SOL_SOCKET, libc::SO_REUSEADDR, &mut fl, fl_size);
    fl = 1;
    setsockopt_results |= libc::setsockopt(s, libc::SOL_SOCKET, libc::SO_BROADCAST, &mut fl, fl_size);
    if setsockopt_results != 0 {
        libc::close(s);
        return None;
    }

    fl = 1;
    libc::setsockopt(s, libc::SOL_SOCKET, libc::SO_NOSIGPIPE, &mut fl, fl_size);

    if af == libc::AF_INET {
        fl = 1;
        libc::setsockopt(s, libc::IPPROTO_IP, 0x4000 /* IP_DF */, &mut fl, fl_size);
    }
    if af == libc::AF_INET6 {
        fl = 1;
        libc::setsockopt(s, libc::IPPROTO_IPV6, 62 /* IPV6_DONTFRAG */, &mut fl, fl_size);
        fl = 1;
        libc::setsockopt(s, libc::IPPROTO_IPV6, libc::IPV6_V6ONLY, &mut fl, fl_size);
    }

    fl = 1048576;
    while fl >= 131072 {
        if libc::setsockopt(s, libc::SOL_SOCKET, libc::SO_RCVBUF, &mut fl, fl_size) == 0 {
            break;
        }
        fl -= 65536;
    }
    fl = 1048576;
    while fl >= 131072 {
        if libc::setsockopt(s, libc::SOL_SOCKET, libc::SO_SNDBUF, &mut fl, fl_size) == 0 {
            break;
        }
        fl -= 65536;
    }

    let namidx = libc::if_nametoindex(device_name.as_ptr()) as libc::c_int;
    if namidx != 0 {
        libc::setsockopt(s, libc::IPPROTO_IP, 25 /* IP_BOUND_IF */, &namidx, fl_size);
    }

    if libc::bind(s, (address as *const InetAddress).cast::<libc::sockaddr>(), std::mem::size_of::<libc::sockaddr_in6>() as libc::socklen_t) != 0 {
        libc::close(s);
        return None;
    }

    Some(s)
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

pub trait FastUDPSocketPacketHandler {
    fn incoming_udp_packet(socket: &RawOsSocket, from_adddress: &InetAddress, mut data: Buffer);
}

/// A multi-threaded (or otherwise fast) UDP socket that binds to both IPv4 and IPv6 addresses.
pub struct FastUDPSocket<H: FastUDPSocketPacketHandler + 'static> {
    handler: Arc<H>,
    threads: Vec<std::thread::JoinHandle<()>>,
    thread_run: Arc<AtomicBool>,
    sockets: Vec<RawOsSocket>,
    bind_address: InetAddress,
}

#[cfg(unix)]
#[inline(always)]
pub fn fast_udp_socket_send_buffer(socket: &RawOsSocket, to_address: &InetAddress, data: &[u8], packet_ttl: i32) {
    unsafe {
        if packet_ttl <= 0 {
            libc::sendto(*socket, data.as_ptr(), data.len() as libc::size_t, 0, (to_address as *const InetAddress).cast::<libc::sockaddr>(), std::mem::size_of::<InetAddress>() as libc::socklen_t);
        } else {
            let mut ttl = packet_ttl as libc::c_int;
            libc::setsockopt(*socket, libc::IPPROTO_IP, libc::IP_TTL, &mut ttl, std::mem::size_of::<libc::c_int>() as libc::socklen_t);
            libc::sendto(*socket, data.as_ptr(), data.len() as libc::size_t, 0, (to_address as *const InetAddress).cast::<libc::sockaddr>(), std::mem::size_of::<InetAddress>() as libc::socklen_t);
            ttl = 255;
            libc::setsockopt(*socket, libc::IPPROTO_IP, libc::IP_TTL, &mut ttl, std::mem::size_of::<libc::c_int>() as libc::socklen_t);
        }
    }
}

#[cfg(windows)]
#[inline(always)]
pub fn fast_udp_socket_send_buffer(socket: &RawOsSocket, to_address: &InetAddress, data: &[u8], packet_ttl: i32) {
}

impl<H: FastUDPSocketPacketHandler + 'static> FastUDPSocket<H> {
    pub fn new(device_name: &str, address: &InetAddress, handler: &Arc<H>) -> Result<FastUDPSocket<H>, String> {
        let thread_count = num_cpus::get();

        let mut s = FastUDPSocket{
            handler: handler.clone(),
            threads: Vec::new(),
            thread_run: Arc::new(AtomicBool::new(true)),
            sockets: Vec::new(),
            bind_address: address.clone()
        };

        let device_name_c = CString::from(device_name);
        let af = match address.family() {
            InetAddressFamily::IPv4 => AfInet,
            InetAddressFamily::IPv6 => AfInet6,
            _ => { return Err(String::from("unrecognized address family")); }
        };

        for _ in 0..thread_count {
            let thread_socket = unsafe { bind_udp_socket(&device_name_c, address, af) };
            if thread_socket.is_some() {
                let thread_socket = thread_socket.unwrap();
                s.sockets.push(thread_socket);

                let thread_run = s.thread_run.clone();
                let handler_weak = Arc::downgrade(handler);
                s.threads.push(std::thread::spawn(move || {
                    let mut from_address = InetAddress::new();
                    while thread_run.load(Ordering::Relaxed) {
                        let mut buf = Buffer::new();
                        let mut addrlen = std::mem::size_of::<InetAddress>() as libc::socklen_t;
                        let read_length = unsafe { libc::recvfrom(thread_socket, buf.as_mut_ptr(), Buffer::CAPACITY as libc::size_t, 0, (&mut from_address as *mut InetAddress).cast::<libc::sockaddr>(), &mut addrlen) };
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
            }
        }

        if s.threads.is_empty() {
            return Err(String::from("unable to bind to address for IPv4 or IPv6"));
        }

        Ok(s)
    }

    /// Get a socket suitable for sending.
    #[inline(always)]
    pub fn socket(&self) -> RawOsSocket {
        return *self.sockets.get(0).unwrap();
    }
}

impl<H: FastUDPSocketPacketHandler + 'static> Drop for FastUDPSocket<H> {
    fn drop(&mut self) {
        let tmp: [u8; 1] = [0];
        self.thread_run.store(false, Ordering::Relaxed);
        for s in self.sockets.iter() {
            unsafe {
                libc::sendto(*s as libc::c_int, tmp.as_ptr(), 0, 0, (&self.bind_address as *const InetAddress).cast::<libc::sockaddr>(), std::mem::size_of::<InetAddress>() as libc::socklen_t);
            }
        }
        for s in self.sockets.iter() {
            unsafe {
                libc::shutdown(*s as libc::c_int, libc::SHUT_RDWR);
                libc::close(*s as libc::c_int);
            }
        }
        for t in self.threads.iter() {
            t.join()
        }
    }
}
