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
use std::collections::BTreeSet;
use std::error::Error;
use std::ffi::CString;
use std::ptr::{null_mut, copy_nonoverlapping};
use std::mem::transmute;
use std::os::raw::{c_int, c_uchar, c_ulong, c_void};
use std::process::Command;
use std::sync::Mutex;
use std::thread::JoinHandle;

use lazy_static::lazy_static;
use num_traits::cast::AsPrimitive;

use zerotier_core::{InetAddress, MAC, MulticastGroup, NetworkId};

use crate::osdep as osdep;
use crate::physicallink::PhysicalLink;
use crate::vnic::VNIC;

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
    ndrv_fd: c_int,
    bpf_fd: c_int,
    bpf_no: u32,
    bpf_read_thread: Cell<Option<JoinHandle<()>>>,
}

// Rust implementation of the following macro from Darwin sys/bpf.h:
// #define BPF_WORDALIGN(x) (((x)+(BPF_ALIGNMENT-1))&~(BPF_ALIGNMENT-1))
// ... and also ...
// #define BPF_ALIGNMENT sizeof(int32_t)
#[allow(non_snake_case)]
#[inline(always)]
fn BPF_WORDALIGN(x: isize) -> isize {
    (((x + 3) as usize) & (!(3 as usize))) as isize
}

lazy_static! {
    static ref MAC_FETH_BPF_DEVICES_USED: Mutex<BTreeSet<u32>> = Mutex::new(BTreeSet::new());
}

impl MacFethTap {
    /// Create a new MacFethTap with a function to call for Ethernet frames.
    /// The function F should return as quickly as possible. It should pass copies
    /// of frames elsewhere if anything needs to be done with them. The slice it's
    /// given will not remain valid after it returns. Also note that F will be called
    /// from another thread that is spawned here, so all its bound references must
    /// be "Send" and "Sync" e.g. Arc<>.
    pub fn new<F: Fn(&[u8]) + Send + Sync + 'static>(nwid: &NetworkId, mac: &MAC, mtu: i32, metric: i32, eth_frame_func: F) -> Result<MacFethTap, String> {
        // This tracks BPF devices we are using so we don't try to reopen them, and also
        // doubles as a global lock to ensure that only one feth tap is created at once per
        // ZeroTier process per system.
        let mut bpf_devices_used = MAC_FETH_BPF_DEVICES_USED.lock().unwrap();

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
            if bpf_devices_used.contains(&bpf_no) {
                bpf_no += 1;
            } else {
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
        if unsafe { osdep::ioctl(bpf_fd as c_int, osdep::c_BIOCSETIF, (&mut bpf_ifr as *mut osdep::ifreq).cast::<c_void>()) } != 0 {
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

        // Create BPF listener thread, which calls the supplied function on each incoming packet.
        let t = std::thread::Builder::new().stack_size(zerotier_core::RECOMMENDED_THREAD_STACK_SIZE).spawn(move || {
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

        // Create AF_NDRV socket used to inject packets. We could inject with BPF but that has
        // a hard MTU limit of 2048 so we have to use AF_NDRV instead. Performance is probably
        // the same, but it means another socket.
        let ndrv_fd = unsafe { osdep::socket(osdep::AF_NDRV as c_int, osdep::SOCK_RAW as c_int, 0) };
        if ndrv_fd < 0 {
            unsafe { osdep::close(bpf_fd); }
            return Err(String::from("unable to create AF_NDRV socket"));
        }
        let mut ndrv_sa: osdep::sockaddr_ndrv = unsafe { std::mem::zeroed() };
        ndrv_sa.snd_len = std::mem::size_of::<osdep::sockaddr_ndrv>() as c_uchar;
        ndrv_sa.snd_family = osdep::AF_NDRV as c_uchar;
        unsafe { copy_nonoverlapping(peer_dev_name_bytes.as_ptr(), ndrv_sa.snd_name.as_mut_ptr().cast::<u8>(), if peer_dev_name_bytes.len() > (bpf_ifr.ifr_name.len() - 1) { bpf_ifr.ifr_name.len() - 1 } else { peer_dev_name_bytes.len() }); }
        if unsafe { osdep::bind(ndrv_fd, (&ndrv_sa as *const osdep::sockaddr_ndrv).cast(), std::mem::size_of::<osdep::sockaddr_ndrv>() as osdep::socklen_t) } != 0 {
            unsafe { osdep::close(bpf_fd); }
            unsafe { osdep::close(ndrv_fd); }
            return Err(String::from("unable to bind AF_NDRV socket"));
        }
        if unsafe { osdep::connect(ndrv_fd, (&ndrv_sa as *const osdep::sockaddr_ndrv).cast(), std::mem::size_of::<osdep::sockaddr_ndrv>() as osdep::socklen_t) } != 0 {
            unsafe { osdep::close(bpf_fd); }
            unsafe { osdep::close(ndrv_fd); }
            return Err(String::from("unable to connect AF_NDRV socket"));
        }

        bpf_devices_used.insert(bpf_no);

        Ok(MacFethTap {
            network_id: nwid.0,
            device: device,
            ndrv_fd: ndrv_fd,
            bpf_fd: bpf_fd,
            bpf_no: bpf_no,
            bpf_read_thread: Cell::new(Some(t.unwrap()))
        })
    }

    fn have_ip(&self, ip: &InetAddress) -> bool {
        let mut have_ip = false;
        PhysicalLink::map(|link: PhysicalLink| {
            if link.device.eq(&self.device.name) && link.address.eq(ip) {
                have_ip = true;
            }
        });
        have_ip
    }
}

impl VNIC for MacFethTap {
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
        let dev = self.device.name.as_bytes();
        let mut groups: BTreeSet<MulticastGroup> = BTreeSet::new();
        unsafe {
            let mut maddrs: *mut osdep::ifmaddrs = null_mut();
            if osdep::getifmaddrs(&mut maddrs as *mut *mut osdep::ifmaddrs) == 0 {
                let mut i = maddrs;
                while !i.is_null() {
                    if !(*i).ifma_name.is_null() && !(*i).ifma_addr.is_null() && (*(*i).ifma_addr).sa_family == osdep::AF_LINK as osdep::sa_family_t {
                        let in_: &osdep::sockaddr_dl = &*((*i).ifma_name.cast());
                        let la: &osdep::sockaddr_dl = &*((*i).ifma_addr.cast());
                        if la.sdl_alen == 6 && in_.sdl_nlen <= dev.len() as osdep::u_char && osdep::memcmp(dev.as_ptr().cast(), in_.sdl_data.as_ptr().cast(), in_.sdl_nlen as c_ulong) == 0 {
                            let mi = la.sdl_nlen as usize;
                            groups.insert(MulticastGroup{
                                mac: MAC(
                                    (la.sdl_data[mi] as u64) << 40 |
                                    (la.sdl_data[mi+1] as u64) << 32 |
                                    (la.sdl_data[mi+2] as u64) << 24 |
                                    (la.sdl_data[mi+3] as u64) << 16 |
                                    (la.sdl_data[mi+4] as u64) << 8 |
                                    la.sdl_data[mi+5] as u64
                                ),
                                adi: 0,
                            });
                        }
                    }
                    i = (*i).ifma_next;
                }
                osdep::freeifmaddrs(maddrs);
            }
        }
        groups
    }

    #[inline(always)]
    fn put(&self, source_mac: &zerotier_core::MAC, dest_mac: &zerotier_core::MAC, ethertype: u16, vlan_id: u16, data: *const u8, len: usize) -> bool {
        let dm = dest_mac.0;
        let sm = source_mac.0;
        let mut hdr: [u8; 14] = [
            (dm >> 40) as u8,
            (dm >> 32) as u8,
            (dm >> 24) as u8,
            (dm >> 16) as u8,
            (dm >> 8) as u8,
            dm as u8,
            (sm >> 40) as u8,
            (sm >> 32) as u8,
            (sm >> 24) as u8,
            (sm >> 16) as u8,
            (sm >> 8) as u8,
            sm as u8,
            (ethertype >> 8) as u8,
            ethertype as u8
        ];
        unsafe {
            let iov: [osdep::iovec; 2] = [
                osdep::iovec {
                    iov_base: hdr.as_mut_ptr().cast(),
                    iov_len: 14,
                },
                osdep::iovec {
                    iov_base: transmute(data), // have to "cast away const" even though data is not modified by writev()
                    iov_len: len as osdep::size_t,
                },
            ];
            osdep::writev(self.ndrv_fd, iov.as_ptr(), 2) == (len + 14) as osdep::ssize_t
        }
    }
}

impl Drop for MacFethTap {
    fn drop(&mut self) {
        if self.bpf_fd >= 0 {
            unsafe {
                osdep::shutdown(self.bpf_fd, osdep::SHUT_RDWR as c_int);
                osdep::close(self.bpf_fd);
                MAC_FETH_BPF_DEVICES_USED.lock().unwrap().remove(&self.bpf_no);
            }
        }
        if self.ndrv_fd >= 0 {
            unsafe {
                osdep::close(self.ndrv_fd);
            }
        }
        let t = self.bpf_read_thread.replace(None);
        if t.is_some() {
            let _ = t.unwrap().join();
        }
    }
}
