use crate::osdep as osdep;
use crate::vnp::Port;
use std::error::Error;
use std::os::raw::c_int;
use std::ffi::CString;
use std::thread::JoinHandle;
use std::sync::Mutex;
use std::cell::Cell;
use zerotier_core::NetworkId;
use lazy_static::lazy_static;
use crate::physicallink::PhysicalLink;

const BPF_BUFFER_SIZE: usize = 131072;

pub struct MacFethTap {
    network_id: u64,
    device_name: String,
    peer_device_name: String,
    bpf_fd: c_int,
    bpf_read_thread: Cell<Option<JoinHandle<()>>>,
}

// Rust implementation of the following macro from Darwin sys/bpf.h:
// #define BPF_WORDALIGN(x) (((x)+(BPF_ALIGNMENT-1))&~(BPF_ALIGNMENT-1))
// ... and also ...
// #define BPF_ALIGNMENT sizeof(int32_t)
#[inline(always)]
#[allow(non_snake_case)]
pub fn BPF_WORDALIGN(x: isize) -> isize {
    (((x + 3) as usize) & (!(3 as usize))) as isize
}

lazy_static! {
    static ref MAC_FETH_GLOBAL_LOCK: Mutex<i32> = Mutex::new(0_i32);
}

impl MacFethTap {
    /// Create a new MacFethTap with a function to call for Ethernet frames.
    /// The function F should return as quickly as possible. It should pass copies
    /// of frames elsewhere if anything needs to be done with them. The slice it's
    /// given will not remain valid after it returns.
    pub fn new<F: Fn(&[u8]) + Send + Sync + 'static>(nwid: &NetworkId, eth_frame_func: F) -> Result<MacFethTap, String> {
        let _one_at_a_time = unsafe { MAC_FETH_GLOBAL_LOCK.lock().unwrap() };

        if unsafe { osdep::getuid() } != 0 {
            return Err(String::from("ZeroTier MacFethTap must run as root"));
        }

        let mut device_name: String;
        let mut peer_device_name: String;
        let mut device_feth_ctr = nwid.0 ^ (nwid.0 >> 32) ^ (nwid.0 >> 48);
        let mut device_alloc_tries = 0;
        loop {
            let device_feth_no = 100 + (device_feth_ctr % 4900);
            device_name = format!("feth{}", device_feth_no);
            peer_device_name = format!("feth{}", device_feth_no + 5000);
            let mut already_allocated = false;
            PhysicalLink::map(|link: PhysicalLink| {
                if link.device.eq(&device_name) || link.device.eq(&peer_device_name) {
                    already_allocated = true;
                }
            });
            if !already_allocated {
                break;
            }

            device_alloc_tries += 1;
            if device_alloc_tries >= 4899 {
                return Err(String::from("unable to find unallocated 'feth' device"));
            }
            device_feth_ctr += 1;
        }

        let mut bpf_no: u32 = 1; // start at 1 since some software hard-codes /dev/bpf0
        let mut bpf_fd: c_int = -1;
        loop {
            let bpf_dev = CString::new(format!("/dev/bpf{}", bpf_no)).unwrap();
            let bpf_dev = bpf_dev.as_bytes_with_nul();
            bpf_fd = unsafe { osdep::open(bpf_dev.as_ptr().cast(), osdep::O_RDWR as c_int) };
            if bpf_fd >= 0 {
                break;
            }
            bpf_no += 1;
            if bpf_no > 1000 {
                return Err(String::from("unable to open /dev/bpf## where attempted ## from 1 to 1000"));
            }
        }

        let bpf_fd_copy = bpf_fd;
        let t = std::thread::Builder::new().stack_size(zerotier_core::RECOMMENDED_THREAD_STACK_SIZE).spawn(move || {
            let mut buf: [u8; BPF_BUFFER_SIZE] = [0_u8; BPF_BUFFER_SIZE];
            let hdr_struct_size = std::mem::size_of::<osdep::bpf_hdr>() as isize;
            loop {
                let n = unsafe { osdep::read(bpf_fd_copy, buf.as_mut_ptr().cast(), BPF_BUFFER_SIZE as osdep::size_t) } as isize;
                if n >= 0 {
                    let mut p: isize = 0;
                    while (p + hdr_struct_size) < n {
                        unsafe {
                            let h = buf.as_ptr().offset(p).cast::<osdep::bpf_hdr>();
                            let hdrlen = (*h).bh_hdrlen as isize;
                            let caplen = (*h).bh_caplen as isize;
                            let pktlen = hdrlen + caplen;
                            if caplen > 0 && (p + pktlen) <= n {
                                eth_frame_func(std::slice::from_raw_parts(buf.as_ptr().offset(p + hdrlen), caplen as usize));
                            }
                            p += BPF_WORDALIGN(pktlen);
                        }
                    }
                } else {
                    break;
                }
            }
        });
        if t.is_err() {
            unsafe { osdep::close(bpf_fd); }
            return Err(String::from("unable to start thread"));
        }

        Ok(MacFethTap {
            network_id: nwid.0,
            device_name: device_name,
            peer_device_name: peer_device_name,
            bpf_fd: bpf_fd,
            bpf_read_thread: Cell::new(Some(t.unwrap()))
        })
    }
}

impl Drop for MacFethTap {
    fn drop(&mut self) {
        if self.bpf_fd >= 0 {
            unsafe {
                osdep::shutdown(self.bpf_fd, osdep::SHUT_RDWR as c_int);
                osdep::close(self.bpf_fd);
            }
        }
        let t = self.bpf_read_thread.replace(None);
        if t.is_some() {
            let _ = t.unwrap().join();
        }
    }
}
