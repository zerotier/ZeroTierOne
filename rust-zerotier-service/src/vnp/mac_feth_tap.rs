/*
 * This creates a pair of feth devices with the lower numbered device
 * being the ZeroTier virtual interface and the other being the device
 * used to actually read and write packets. The latter gets no IP config
 * and is only used for I/O. The behavior of feth is similar to the
 * veth pairs that exist on Linux.
 *
 * The feth device has only existed since MacOS Sierra, but that's fairly
 * long ago in Mac terms.
 *
 * I/O with feth must be done using two different sockets. The BPF socket
 * is used to receive packets, while an AF_NDRV (low-level network driver
 * access) socket must be used to inject. AF_NDRV can't read IP frames
 * since BSD doesn't forward packets out the NDRV tap if they've already
 * been handled, and while BPF can inject its MTU for injected packets
 * is limited to 2048. AF_NDRV packet injection is required to inject
 * ZeroTier's large MTU frames.
 *
 * All this stuff is completely undocumented. A lot of tracing through
 * the Darwin/XNU kernel source was required to find feth in the first place
 * and figure out how to make this work.
 */

use std::cell::Cell;
use std::error::Error;
use std::ffi::CString;
use std::os::raw::{c_int, c_ulong, c_void};
use std::process::Command;
use std::sync::Mutex;
use std::thread::JoinHandle;
use std::intrinsics::copy_nonoverlapping;

use lazy_static::lazy_static;

use zerotier_core::{NetworkId, MAC, InetAddress, MulticastGroup};

use crate::osdep as osdep;
use crate::physicallink::PhysicalLink;
use crate::vnp::Port;
use std::collections::BTreeSet;

const BPF_BUFFER_SIZE: usize = 131072;
const IFCONFIG: &str = "/sbin/ifconfig";

// Holds names of feth devices and destroys them on Drop.
struct MacFethDevice {
    pub name: String,
    pub peer_name: String
}

impl Drop for MacFethDevice {
    fn drop(&mut self) {
        if self.name.len() > 0 && self.peer_name.len() > 0 {
            let destroy_peer = Command::new(IFCONFIG).arg(self.peer_name.as_str()).arg("destroy").spawn();
            if destroy_peer.is_ok() {
                let _ = destroy_peer.unwrap().wait();
            }
            let destroy = Command::new(IFCONFIG).arg(self.name.as_str()).arg("destroy").spawn();
            if destroy.is_ok() {
                let _ = destroy.unwrap().wait();
            }
        }
    }
}

pub struct MacFethTap {
    network_id: u64,
    device: MacFethDevice,
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
    /// given will not remain valid after it returns. Also note that F will be called
    /// from another thread that is spawned here, so all its bound references must
    /// be "Send" and "Sync" e.g. Arc<>.
    pub fn new<F: Fn(&[u8]) + Send + Sync + 'static>(nwid: &NetworkId, mac: &MAC, mtu: i32, metric: i32, eth_frame_func: F) -> Result<MacFethTap, String> {
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

        let cmd = Command::new(IFCONFIG).arg(&device_name).arg("create").spawn();
        if cmd.is_err() {
            return Err(format!("unable to create device '{}': {}", device_name.as_str(), cmd.err().unwrap().to_string()));
        }
        let _ = cmd.unwrap().wait();
        let cmd = Command::new(IFCONFIG).arg(&peer_device_name).arg("create").spawn();
        if cmd.is_err() {
            return Err(format!("unable to create device '{}': {}", peer_device_name.as_str(), cmd.err().unwrap().to_string()));
        }
        let _ = cmd.unwrap().wait();

        let device = MacFethDevice{
            name: device_name,
            peer_name: peer_device_name,
        };

        let cmd = Command::new(IFCONFIG).arg(&device.name).arg("lladdr").arg(mac.to_string()).spawn();
        if cmd.is_err() {
            return Err(format!("unable to configure device '{}': {}", &device.name, cmd.err().unwrap().to_string()));
        }
        let _ = cmd.unwrap().wait();

        let cmd = Command::new(IFCONFIG).arg(&device.peer_name).arg("peer").arg(device.name.as_str()).spawn();
        if cmd.is_err() {
            return Err(format!("unable to configure device '{}': {}", &device.peer_name, cmd.err().unwrap().to_string()));
        }
        let _ = cmd.unwrap().wait();
        let cmd = Command::new(IFCONFIG).arg(&device.peer_name).arg("mtu").arg("16370").arg("up").spawn();
        if cmd.is_err() {
            return Err(format!("unable to configure device '{}': {}", &device.peer_name, cmd.err().unwrap().to_string()));
        }
        let _ = cmd.unwrap().wait();

        let cmd = Command::new(IFCONFIG).arg(&device.name).arg("mtu").arg(mtu.to_string()).arg("metric").arg(metric.to_string()).arg("up").spawn();
        if cmd.is_err() {
            return Err(format!("unable to configure device '{}': {}", &device.name.as_str(), cmd.err().unwrap().to_string()));
        }
        let _ = cmd.unwrap().wait();

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
                break;
            }
        }
        if bpf_fd < 0 {
            return Err(String::from("unable to open /dev/bpf## where attempted ## from 1 to 1000"));
        }

        // Set/get buffer length.
        let mut fl: c_int = BPF_BUFFER_SIZE as c_int;
        if unsafe { osdep::ioctl(bpf_fd as c_int, osdep::c_BIOCSBLEN, (&mut fl as *mut c_int).cast::<c_void>()) } != 0 {
            unsafe { osdep::close(bpf_fd); }
            return Err(String::from("unable to configure BPF device"));
        }
        let bpf_read_size = fl as osdep::size_t;

        // Set immediate mode.
        fl = 1;
        if unsafe { osdep::ioctl(bpf_fd as c_int, osdep::c_BIOCIMMEDIATE, (&mut fl as *mut c_int).cast::<c_void>()) } != 0 {
            unsafe { osdep::close(bpf_fd); }
            return Err(String::from("unable to configure BPF device"));
        }

        // We don't want to see packets we inject.
        fl = 0;
        if unsafe { osdep::ioctl(bpf_fd as c_int, osdep::c_BIOCSSEESENT, (&mut fl as *mut c_int).cast::<c_void>()) } != 0 {
            unsafe { osdep::close(bpf_fd); }
            return Err(String::from("unable to configure BPF device"));
        }

        // Set device name that we're monitoring.
        let mut bpf_ifr: osdep::ifreq = unsafe { std::mem::zeroed() };
        let peer_dev_name_bytes = device.peer_name.as_bytes();
        unsafe { copy_nonoverlapping(peer_dev_name_bytes.as_ptr(), bpf_ifr.ifr_name.as_mut_ptr().cast::<u8>(), if peer_dev_name_bytes.len() > (bpf_ifr.ifr_name.len() - 1) { bpf_ifr.ifr_name.len() - 1 } else { peer_dev_name_bytes.len() }); }
        if unsafe { osdep::ioctl(bpf_fd as c_int, BIOCSETIF, (&mut bpf_ifr as *mut osdep::ifreq).cast::<c_void>()) } != 0 {
            unsafe { osdep::close(bpf_fd); }
            return Err(String::from("unable to configure BPF device"));
        }

        // Include Ethernet header.
        fl = 1;
        if unsafe { osdep::ioctl(bpf_fd as c_int, osdep::c_BIOCSHDRCMPLT, (&mut fl as *mut c_int).cast::<c_void>()) } != 0 {
            unsafe { osdep::close(bpf_fd); }
            return Err(String::from("unable to configure BPF device"));
        }

        // Set promiscuous mode so bridging could work, etc.
        fl = 1;
        if unsafe { osdep::ioctl(bpf_fd as c_int, osdep::c_BIOCPROMISC, (&mut fl as *mut c_int).cast::<c_void>()) } != 0 {
            unsafe { osdep::close(bpf_fd); }
            return Err(String::from("unable to configure BPF device"));
        }

        let t = std::thread::Builder::new().stack_size(zerotier_core::RECOMMENDED_THREAD_STACK_SIZE).spawn(|| {
            let mut buf: [u8; BPF_BUFFER_SIZE] = [0_u8; BPF_BUFFER_SIZE];
            let hdr_struct_size = std::mem::size_of::<osdep::bpf_hdr>() as isize;
            loop {
                let n = unsafe { osdep::read(bpf_fd, buf.as_mut_ptr().cast(), bpf_read_size) } as isize;
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
            device: device,
            bpf_fd: bpf_fd,
            bpf_read_thread: Cell::new(Some(t.unwrap()))
        })
    }

    fn have_ip(&self, ip: &InetAddress) -> bool {
        let mut have_ip = false;
        PhysicalLink::map(|link: PhysicalLink| {
            if link.device.eq(dev) && link.address.eq(ip) {
                have_ip = true;
            }
        });
        have_ip
    }
}

impl Port for MacFethTap {
    fn add_ip(&self, ip: &InetAddress) -> bool {
        if !self.have_ip(ip) {
            let cmd = Command::new(IFCONFIG).arg(&self.device.name).arg(if ip.is_v6() { "inet6" } else { "inet" }).arg(ip.to_string()).arg("alias").spawn();
            if cmd.is_ok() {
                let _ = cmd.unwrap().wait();
            }
            return self.have_ip(ip);
        }
        true
    }

    fn remove_ip(&self, ip: &InetAddress) -> bool {
        if self.have_ip(ip) {
            let cmd = Command::new(IFCONFIG).arg(&self.device.name).arg(if ip.is_v6() { "inet6" } else { "inet" }).arg(ip.to_string()).arg("-alias").spawn();
            if cmd.is_ok() {
                let _ = cmd.unwrap().wait();
            }
            return !self.have_ip(ip);
        }
        true // if we don't have it it's successfully removed
    }

    fn ips(&self) -> Vec<InetAddress> {
        let mut ipv: Vec<InetAddress> = Vec::new();
        ipv.reserve(8);
        let dev = self.device.name.as_str();
        PhysicalLink::map(|link: PhysicalLink| {
            if link.device.eq(dev) {
                ipv.push(link.address.clone());
            }
        });
        ipv.sort();
        ipv
    }

    #[inline(always)]
    fn device_name(&self) -> String {
        self.device.name.clone()
    }

    fn get_multicast_groups(&self) -> BTreeSet<MulticastGroup> {
        let groups: BTreeSet<MulticastGroup> = BTreeSet::new();
        groups
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
