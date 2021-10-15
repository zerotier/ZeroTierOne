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

/*
 * This creates a pair of feth devices with the lower numbered device
 * being the ZeroTier virtual interface and the higher being the device
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
 * This is all completely undocumented. Finding it and learning how to
 * use it required sifting through XNU/Darwin kernel source code on
 * opensource.apple.com. Needless to say we are exploring other options
 * for future releases, but this works for now.
 */

use std::cell::Cell;
use std::collections::HashSet;
use std::error::Error;
use std::ffi::CString;
use std::mem::{transmute, zeroed};
use std::os::raw::{c_char, c_int, c_short, c_uchar, c_uint, c_void};
use std::process::Command;
use std::ptr::{copy_nonoverlapping, null_mut};
use std::sync::Mutex;
use std::thread::JoinHandle;

use lazy_static::lazy_static;
use num_traits::cast::AsPrimitive;

use zerotier_network_hypervisor::vl1::{InetAddress, MAC};

use crate::getifaddrs;
use crate::vnic::vnic::VNIC;
use zerotier_network_hypervisor::vl2::MulticastGroup;

const BPF_BUFFER_SIZE: usize = 131072;
const IFCONFIG: &'static str = "/sbin/ifconfig";
const SYSCTL: &'static str = "/usr/sbin/sysctl";

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

/*
struct nd_ifinfo {
	u_int32_t linkmtu;		/* LinkMTU */
	u_int32_t maxmtu;		/* Upper bound of LinkMTU */
	u_int32_t basereachable;	/* BaseReachableTime */
	u_int32_t reachable;		/* Reachable Time */
	u_int32_t retrans;		/* Retrans Timer */
	u_int32_t flags;		/* Flags */
	int recalctm;			/* BaseReacable re-calculation timer */
	u_int8_t chlim;			/* CurHopLimit */
	u_int8_t receivedra;
};
struct	in6_ndireq {
    char ifname[IFNAMSIZ];
    struct nd_ifinfo ndi;
};
struct in6_addrlifetime {
	time_t ia6t_expire;     /* valid lifetime expiration time */
	time_t ia6t_preferred;  /* preferred lifetime expiration time */
	u_int32_t ia6t_vltime;  /* valid lifetime */
	u_int32_t ia6t_pltime;  /* prefix lifetime */
};
struct in6_ifstat {
	ifs6_in_receive;       /* # of total input datagram */
	ifs6_in_hdrerr;        /* # of datagrams with invalid hdr */
	ifs6_in_toobig;        /* # of datagrams exceeded MTU */
	ifs6_in_noroute;       /* # of datagrams with no route */
	ifs6_in_addrerr;       /* # of datagrams with invalid dst */
	ifs6_in_protounknown;  /* # of datagrams with unknown proto */
	                                /* NOTE: increment on final dst if */
	ifs6_in_truncated;     /* # of truncated datagrams */
	ifs6_in_discard;       /* # of discarded datagrams */
	                                /* NOTE: fragment timeout is not here */
	ifs6_in_deliver;       /* # of datagrams delivered to ULP */
	                                /* NOTE: increment on final dst if */
	ifs6_out_forward;      /* # of datagrams forwarded */
	                                /* NOTE: increment on outgoing if */
	ifs6_out_request;      /* # of outgoing datagrams from ULP */
	                                /* NOTE: does not include forwrads */
	ifs6_out_discard;      /* # of discarded datagrams */
	ifs6_out_fragok;       /* # of datagrams fragmented */
	ifs6_out_fragfail;     /* # of datagrams failed on fragment */
	ifs6_out_fragcreat;    /* # of fragment datagrams */
	                                /* NOTE: this is # after fragment */
	ifs6_reass_reqd;       /* # of incoming fragmented packets */
	                                /* NOTE: increment on final dst if */
	ifs6_reass_ok;         /* # of reassembled packets */
	                                /* NOTE: this is # after reass */
	                                /* NOTE: increment on final dst if */
	ifs6_atmfrag_rcvd;     /* # of atomic fragments received */
	ifs6_reass_fail;       /* # of reass failures */
	                                /* NOTE: may not be packet count */
	                                /* NOTE: increment on final dst if */
	ifs6_in_mcast;         /* # of inbound multicast datagrams */
	ifs6_out_mcast;        /* # of outbound multicast datagrams */

	ifs6_cantfoward_icmp6; /* # of ICMPv6 packets received for unreachable dest */
	ifs6_addr_expiry_cnt;  /* # of address expiry events (excluding privacy addresses) */
	ifs6_pfx_expiry_cnt;   /* # of prefix expiry events */
	ifs6_defrtr_expiry_cnt;        /* # of default router expiry events */
};
struct in6_ifreq {
	char    ifr_name[IFNAMSIZ];
	union {
		struct  sockaddr_in6 ifru_addr;
		struct  sockaddr_in6 ifru_dstaddr;
		int     ifru_flags;
		int     ifru_flags6;
		int     ifru_metric;
		int     ifru_intval;
		caddr_t ifru_data;
		struct in6_addrlifetime ifru_lifetime;
		struct in6_ifstat ifru_stat;
		struct icmp6_ifstat ifru_icmp6stat;
		u_int32_t ifru_scope_id[SCOPE6_ID_MAX];
	} ifr_ifru;
};
*/

#[allow(non_camel_case_types)]
#[repr(C)]
struct icmp6_ifstat {
    ifs6_in_msg: u64,
    ifs6_in_error: u64,
    ifs6_in_dstunreach: u64,
    ifs6_in_adminprohib: u64,
    ifs6_in_timeexceed: u64,
    ifs6_in_paramprob: u64,
    ifs6_in_pkttoobig: u64,
    ifs6_in_echo: u64,
    ifs6_in_echoreply: u64,
    ifs6_in_routersolicit: u64,
    ifs6_in_routeradvert: u64,
    ifs6_in_neighborsolicit: u64,
    ifs6_in_neighboradvert: u64,
    ifs6_in_redirect: u64,
    ifs6_in_mldquery: u64,
    ifs6_in_mldreport: u64,
    ifs6_in_mlddone: u64,
    ifs6_out_msg: u64,
    ifs6_out_error: u64,
    ifs6_out_dstunreach: u64,
    ifs6_out_adminprohib: u64,
    ifs6_out_timeexceed: u64,
    ifs6_out_paramprob: u64,
    ifs6_out_pkttoobig: u64,
    ifs6_out_echo: u64,
    ifs6_out_echoreply: u64,
    ifs6_out_routersolicit: u64,
    ifs6_out_routeradvert: u64,
    ifs6_out_neighborsolicit: u64,
    ifs6_out_neighboradvert: u64,
    ifs6_out_redirect: u64,
    ifs6_out_mldquery: u64,
    ifs6_out_mldreport: u64,
    ifs6_out_mlddone: u64,
}

#[allow(non_camel_case_types)]
#[repr(C)]
struct in6_ifstat {
    ifs6_in_receive: u64,
    ifs6_in_hdrerr: u64,
    ifs6_in_toobig: u64,
    ifs6_in_noroute: u64,
    ifs6_in_addrerr: u64,
    ifs6_in_protounknown: u64,
    ifs6_in_truncated: u64,
    ifs6_in_discard: u64,
    ifs6_in_deliver: u64,
    ifs6_out_forward: u64,
    ifs6_out_request: u64,
    ifs6_out_discard: u64,
    ifs6_out_fragok: u64,
    ifs6_out_fragfail: u64,
    ifs6_out_fragcreat: u64,
    ifs6_reass_reqd: u64,
    ifs6_reass_ok: u64,
    ifs6_atmfrag_rcvd: u64,
    ifs6_reass_fail: u64,
    ifs6_in_mcast: u64,
    ifs6_out_mcast: u64,
    ifs6_cantfoward_icmp6: u64,
    ifs6_addr_expiry_cnt: u64,
    ifs6_pfx_expiry_cnt: u64,
    ifs6_defrtr_expiry_cnt: u64,
}

#[allow(non_camel_case_types)]
#[repr(C)]
struct in6_addrlifetime {
    ia6t_expire: libc::time_t,
    ia6t_preferred: libc::time_t,
    ia6t_vltime: u32,
    ia6t_pltime: u32,
}

#[allow(non_camel_case_types)]
#[repr(C)]
union u_ifr_ifru {
    ifru_addr: libc::sockaddr_in6,
    ifru_dstaddr: libc::sockaddr_in6,
    ifru_flags: c_int,
    ifru_flags6: c_int,
    ifru_metric: c_int,
    ifru_intval: c_int,
    ifru_data: *mut c_void,
    ifru_lifetime: in6_addrlifetime,
    ifru_stat: in6_ifstat,
    ifru_icmp6stat: icmp6_ifstat,
    ifru_scope_id: [u32; 16 /* SCOPE6_ID_MAX */],
}

#[allow(non_camel_case_types)]
#[repr(C)]
struct in6_ifreq {
    ifr_name: [c_char; libc::IFNAMSIZ],
    ifr_ifru: u_ifr_ifru,
}

#[allow(non_camel_case_types)]
#[repr(C)]
struct nd_ifinfo {
    linkmtu: u32,
    maxmtu: u32,
    basereachable: u32,
    reachable: u32,
    retrans: u32,
    flags: u32,
    recalctm: c_int,
    chlim: u8,
    receivedra: u8,
}

#[allow(non_camel_case_types)]
#[repr(C)]
struct in6_ndireq {
    ifname: [c_char; libc::IFNAMSIZ],
    ndi: nd_ifinfo,
}

fn device_ipv6_set_params(device: &String, perform_nud: bool, accept_ra: bool) -> bool {
    let dev = device.as_bytes();
    let mut ok = true;
    unsafe {
        let s = libc::socket(libc::AF_INET6 as c_int, libc::SOCK_DGRAM as c_int, 0);
        if s < 0 {
            return false;
        }

        let mut nd: in6_ndireq = zeroed();
        copy_nonoverlapping(dev.as_ptr(), nd.ifname.as_mut_ptr().cast::<u8>(), if dev.len() > (nd.ifname.len() - 1) { nd.ifname.len() - 1 } else { dev.len() });
        if libc::ioctl(s, 76 /* SIOCGIFINFO_IN6 */, (&mut nd as *mut in6_ndireq).cast::<c_void>()) == 0 {
            let oldflags = nd.ndi.flags;
            if perform_nud {
                nd.ndi.flags |= 0x1 /* ND6_IFF_PERFORMNUD */;
            } else {
                nd.ndi.flags &= !0x1 /* !ND6_IFF_PERFORMNUD */;
            }
            if nd.ndi.flags != oldflags {
                if libc::ioctl(s, 87 /* SIOCSIFINFO_FLAGS */, (&mut nd as *mut in6_ndireq).cast::<c_void>()) != 0 {
                    ok = false;
                }
            }
        } else {
            ok = false;
        }

        let mut ifr: in6_ifreq = zeroed();
        copy_nonoverlapping(dev.as_ptr(), ifr.ifr_name.as_mut_ptr().cast::<u8>(), if dev.len() > (ifr.ifr_name.len() - 1) { ifr.ifr_name.len() - 1 } else { dev.len() });
        if libc::ioctl(s, if accept_ra { 132 /* SIOCAUTOCONF_START */ } else { 133 /* SIOCAUTOCONF_STOP */ }, (&mut ifr as *mut in6_ifreq).cast::<c_void>()) != 0 {
            ok = false;
        }

        libc::close(s);
    }
    ok
}

/*
struct ifkpi {
	unsigned int    ifk_module_id;
	unsigned int    ifk_type;
	union {
		void            *ifk_ptr;
		int             ifk_value;
	} ifk_data;
};
struct ifdevmtu {
	int     ifdm_current;
	int     ifdm_min;
	int     ifdm_max;
};
struct  ifreq {
#ifndef IFNAMSIZ
#define IFNAMSIZ        IF_NAMESIZE
#endif
	char    ifr_name[IFNAMSIZ];             /* if name, e.g. "en0" */
	union {
		struct  sockaddr ifru_addr;
		struct  sockaddr ifru_dstaddr;
		struct  sockaddr ifru_broadaddr;
		short   ifru_flags;
		int     ifru_metric;
		int     ifru_mtu;
		int     ifru_phys;
		int     ifru_media;
		int     ifru_intval;
		caddr_t ifru_data;
		struct  ifdevmtu ifru_devmtu;
		struct  ifkpi   ifru_kpi;
		u_int32_t ifru_wake_flags;
		u_int32_t ifru_route_refcnt;
		int     ifru_cap[2];
		u_int32_t ifru_functional_type;
#define IFRTYPE_FUNCTIONAL_UNKNOWN              0
#define IFRTYPE_FUNCTIONAL_LOOPBACK             1
#define IFRTYPE_FUNCTIONAL_WIRED                2
#define IFRTYPE_FUNCTIONAL_WIFI_INFRA           3
#define IFRTYPE_FUNCTIONAL_WIFI_AWDL            4
#define IFRTYPE_FUNCTIONAL_CELLULAR             5
#define IFRTYPE_FUNCTIONAL_INTCOPROC            6
#define IFRTYPE_FUNCTIONAL_COMPANIONLINK        7
#define IFRTYPE_FUNCTIONAL_LAST                 7
	} ifr_ifru;
#define ifr_addr        ifr_ifru.ifru_addr      /* address */
#define ifr_dstaddr     ifr_ifru.ifru_dstaddr   /* other end of p-to-p link */
#define ifr_broadaddr   ifr_ifru.ifru_broadaddr /* broadcast address */
#ifdef __APPLE__
#define ifr_flags       ifr_ifru.ifru_flags     /* flags */
#else
#define ifr_flags       ifr_ifru.ifru_flags[0]  /* flags */
#define ifr_prevflags   ifr_ifru.ifru_flags[1]  /* flags */
#endif /* __APPLE__ */
#define ifr_metric      ifr_ifru.ifru_metric    /* metric */
#define ifr_mtu         ifr_ifru.ifru_mtu       /* mtu */
#define ifr_phys        ifr_ifru.ifru_phys      /* physical wire */
#define ifr_media       ifr_ifru.ifru_media     /* physical media */
#define ifr_data        ifr_ifru.ifru_data      /* for use by interface */
#define ifr_devmtu      ifr_ifru.ifru_devmtu
#define ifr_intval      ifr_ifru.ifru_intval    /* integer value */
#define ifr_kpi         ifr_ifru.ifru_kpi
#define ifr_wake_flags  ifr_ifru.ifru_wake_flags /* wake capabilities */
#define ifr_route_refcnt ifr_ifru.ifru_route_refcnt /* route references count */
#define ifr_reqcap      ifr_ifru.ifru_cap[0]    /* requested capabilities */
#define ifr_curcap      ifr_ifru.ifru_cap[1]    /* current capabilities */
};
struct sockaddr_ndrv {
	unsigned char snd_len;
	unsigned char snd_family;
	unsigned char snd_name[IFNAMSIZ]; /* from if.h */
};
*/

#[allow(non_camel_case_types)]
#[repr(C)]
struct ifkpi {
    ifk_module_id: c_uint,
    ifk_type: c_uint,
    ifk_data: *mut c_void,
}

#[allow(non_camel_case_types)]
#[repr(C)]
struct ifdevmtu {
    ifdm_current: c_int,
    ifdm_min: c_int,
    ifdm_max: c_int,
}

#[allow(non_camel_case_types)]
#[repr(C)]
union ifr_ifru {
    ifru_addr: libc::sockaddr,
    ifru_dstaddr: libc::sockaddr,
    ifru_broadaddr: libc::sockaddr,
    ifru_flags: c_short,
    ifru_metric: c_int,
    ifru_mtu: c_int,
    ifru_phys: c_int,
    ifru_media: c_int,
    ifru_intval: c_int,
    ifru_data: u64,
    ifru_devmtu: ifdevmtu,
    ifru_kpi: ifkpi,
    ifru_wake_flags: u32,
    ifru_route_refcnt: u32,
    ifru_cap: [c_int; 2],
    ifru_functional_type: u32,
}

#[allow(non_camel_case_types)]
#[repr(C)]
struct ifreq {
    ifr_name: [c_char; libc::IFNAMSIZ],
    ifr_ifru: ifr_ifru,
}

#[allow(non_camel_case_types)]
#[repr(C)]
struct sockaddr_ndrv {
    snd_len: c_uchar,
    snd_family: c_uchar,
    snd_name: [c_uchar; libc::IFNAMSIZ],
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

        if unsafe { libc::getuid() } != 0 {
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
            getifaddrs::for_each_address(|_: &InetAddress, dn: &str| {
                if dn.eq(&device_name) || dn.eq(&peer_device_name) {
                    already_allocated = true;
                }
            });
            if !already_allocated {
                break;
            }

            device_alloc_tries += 1;
            if device_alloc_tries >= 1000 {
                return Err(String::from("unable to find unallocated 'feth' device"));
            }
            device_feth_ctr += 1;
        }
        device_ipv6_set_params(&device_name, true, false);

        // Set sysctl for max if_fake MTU. This is allowed to fail since this sysctl doesn't
        // exist on older versions of MacOS (and isn't required there). 16000 is larger than
        // anything ZeroTier supports. OS max is 16384 - some overhead.
        let _ = Command::new(SYSCTL).arg("net.link.fake.max_mtu").arg("10000").spawn().map(|mut c| { let _ = c.wait(); });

        // Create pair of feth interfaces and create MacFethDevice struct.
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
        let device = MacFethDevice {
            name: device_name,
            peer_name: peer_device_name,
        };

        // Set link-layer (MAC) address of primary interface.
        let cmd = Command::new(IFCONFIG).arg(&device.name).arg("lladdr").arg(mac.to_string()).spawn();
        if cmd.is_err() {
            return Err(format!("unable to configure device '{}': {}", &device.name, cmd.err().unwrap().to_string()));
        }
        let _ = cmd.unwrap().wait();

        // Bind peer interfaces together.
        let cmd = Command::new(IFCONFIG).arg(&device.peer_name).arg("peer").arg(device.name.as_str()).spawn();
        if cmd.is_err() {
            return Err(format!("unable to configure device '{}': {}", &device.peer_name, cmd.err().unwrap().to_string()));
        }
        let _ = cmd.unwrap().wait();

        // Set MTU of secondary peer interface, bring up.
        let cmd = Command::new(IFCONFIG).arg(&device.peer_name).arg("mtu").arg(mtu.to_string()).arg("up").spawn();
        if cmd.is_err() {
            return Err(format!("unable to configure device '{}': {}", &device.peer_name, cmd.err().unwrap().to_string()));
        }
        let _ = cmd.unwrap().wait();

        // Set MTU and metric of primary interface, bring up.
        let cmd = Command::new(IFCONFIG).arg(&device.name).arg("mtu").arg(mtu.to_string()).arg("metric").arg(metric.to_string()).arg("up").spawn();
        if cmd.is_err() {
            return Err(format!("unable to configure device '{}': {}", &device.name.as_str(), cmd.err().unwrap().to_string()));
        }
        let _ = cmd.unwrap().wait();

        // Look for a /dev/bpf node to open. Start at 1 since some software
        // hard codes /dev/bpf0 and we don't want to break it. If all BPF nodes
        // are taken MacOS automatically adds more, so we shouldn't run out.
        let mut bpf_no: u32 = 1;
        let mut bpf_fd: c_int = -1;
        loop {
            if bpf_devices_used.contains(&bpf_no) {
                bpf_no += 1;
            } else {
                let bpf_dev = CString::new(format!("/dev/bpf{}", bpf_no)).unwrap();
                let bpf_dev = bpf_dev.as_bytes_with_nul();
                bpf_fd = unsafe { libc::open(bpf_dev.as_ptr().cast(), libc::O_RDWR as c_int) };
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

        // Set/get buffer length to use with reads from BPF device, trying to
        // use up to BPF_BUFFER_SIZE bytes.
        let mut fl: c_int = BPF_BUFFER_SIZE as c_int;
        if unsafe { libc::ioctl(bpf_fd as c_int, 102 /* BIOCSBLEN */, (&mut fl as *mut c_int).cast::<c_void>()) } != 0 {
            unsafe { libc::close(bpf_fd); }
            return Err(String::from("unable to configure BPF device"));
        }
        let bpf_read_size = fl as libc::size_t;

        // Set immediate mode for "live" capture.
        fl = 1;
        if unsafe { libc::ioctl(bpf_fd as c_int, 112 /* BIOCIMMEDIATE */, (&mut fl as *mut c_int).cast::<c_void>()) } != 0 {
            unsafe { libc::close(bpf_fd); }
            return Err(String::from("unable to configure BPF device"));
        }

        // Do not send us back packets we inject or send.
        fl = 0;
        if unsafe { libc::ioctl(bpf_fd as c_int, 119 /* BIOCSSEESENT */, (&mut fl as *mut c_int).cast::<c_void>()) } != 0 {
            unsafe { libc::close(bpf_fd); }
            return Err(String::from("unable to configure BPF device"));
        }

        // Bind BPF to secondary feth device.
        let mut bpf_ifr: ifreq = unsafe { std::mem::zeroed() };
        let peer_dev_name_bytes = device.peer_name.as_bytes();
        unsafe { copy_nonoverlapping(peer_dev_name_bytes.as_ptr(), bpf_ifr.ifr_name.as_mut_ptr().cast::<u8>(), if peer_dev_name_bytes.len() > (bpf_ifr.ifr_name.len() - 1) { bpf_ifr.ifr_name.len() - 1 } else { peer_dev_name_bytes.len() }); }
        if unsafe { libc::ioctl(bpf_fd as c_int, 108 /* BIOCSETIF */, (&mut bpf_ifr as *mut ifreq).cast::<c_void>()) } != 0 {
            unsafe { libc::close(bpf_fd); }
            return Err(String::from("unable to configure BPF device"));
        }

        // Include Ethernet header in BPF captures.
        fl = 1;
        if unsafe { libc::ioctl(bpf_fd as c_int, 117 /* BIOCSHDRCMPLT */, (&mut fl as *mut c_int).cast::<c_void>()) } != 0 {
            unsafe { libc::close(bpf_fd); }
            return Err(String::from("unable to configure BPF device"));
        }

        // Set promiscuous mode so bridging can work.
        fl = 1;
        if unsafe { libc::ioctl(bpf_fd as c_int, 105 /* BIOCPROMISC */, (&mut fl as *mut c_int).cast::<c_void>()) } != 0 {
            unsafe { libc::close(bpf_fd); }
            return Err(String::from("unable to configure BPF device"));
        }

        // Create BPF listener thread, which calls the supplied function on each incoming packet.
        let t = std::thread::Builder::new().stack_size(zerotier_core::RECOMMENDED_THREAD_STACK_SIZE).spawn(move || {
            let mut buf: [u8; BPF_BUFFER_SIZE] = [0_u8; BPF_BUFFER_SIZE];
            let hdr_struct_size = std::mem::size_of::<libc::bpf_hdr>() as isize;
            loop {
                let n = unsafe { libc::read(bpf_fd, buf.as_mut_ptr().cast(), bpf_read_size) } as isize;
                if n >= 0 {
                    let mut p: isize = 0;
                    while (p + hdr_struct_size) < n {
                        unsafe {
                            let h = buf.as_ptr().offset(p).cast::<libc::bpf_hdr>();
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
            unsafe { libc::close(bpf_fd); }
            return Err(String::from("unable to start thread"));
        }

        // Create AF_NDRV socket used to inject packets. We could inject with BPF but that has
        // a hard MTU limit of 2048 so we have to use AF_NDRV instead. Performance is probably
        // the same, but it means another socket.
        let ndrv_fd = unsafe { libc::socket(27 /* AF_NDRV */, libc::SOCK_RAW, 0) };
        if ndrv_fd < 0 {
            unsafe { libc::close(bpf_fd); }
            return Err(String::from("unable to create AF_NDRV socket"));
        }
        let mut ndrv_sa: sockaddr_ndrv = unsafe { std::mem::zeroed() };
        ndrv_sa.snd_len = std::mem::size_of::<sockaddr_ndrv>() as c_uchar;
        ndrv_sa.snd_family = 27 /* AF_NDRV */;
        unsafe { copy_nonoverlapping(peer_dev_name_bytes.as_ptr(), ndrv_sa.snd_name.as_mut_ptr().cast::<u8>(), if peer_dev_name_bytes.len() > (bpf_ifr.ifr_name.len() - 1) { bpf_ifr.ifr_name.len() - 1 } else { peer_dev_name_bytes.len() }); }
        if unsafe { libc::bind(ndrv_fd, (&ndrv_sa as *const sockaddr_ndrv).cast(), std::mem::size_of::<sockaddr_ndrv>() as libc::socklen_t) } != 0 {
            unsafe { libc::close(bpf_fd); }
            unsafe { libc::close(ndrv_fd); }
            return Err(String::from("unable to bind AF_NDRV socket"));
        }
        if unsafe { libc::connect(ndrv_fd, (&ndrv_sa as *const sockaddr_ndrv).cast(), std::mem::size_of::<sockaddr_ndrv>() as libc::socklen_t) } != 0 {
            unsafe { libc::close(bpf_fd); }
            unsafe { libc::close(ndrv_fd); }
            return Err(String::from("unable to connect AF_NDRV socket"));
        }

        bpf_devices_used.insert(bpf_no);

        Ok(MacFethTap {
            network_id: nwid.0,
            device,
            ndrv_fd,
            bpf_fd,
            bpf_no,
            bpf_read_thread: Cell::new(Some(t.unwrap()))
        })
    }

    fn have_ip(&self, ip: &InetAddress) -> bool {
        let mut have_ip = false;
        getifaddrs::for_each_address(|addr: &InetAddress, device_name: &str| {
            if device_name.eq(&self.device.name) && addr.eq(ip) {
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
        getifaddrs::for_each_address(|addr: &InetAddress, device_name: &str| {
            if device_name.eq(dev) {
                ipv.push(addr.clone());
            }
        });
        ipv.sort();
        ipv
    }

    fn device_name(&self) -> String {
        self.device.name.clone()
    }

    fn get_multicast_groups(&self) -> HashSet<MulticastGroup> {
        let mut all_groups: HashSet<MulticastGroup> = HashSet::new();
        crate::vnic::common::get_l2_multicast_subscriptions(self.device.name.as_str()).into_iter().for_each(|mac| {
            all_groups.insert(MulticastGroup::from(&mac));
        });
        all_groups
    }

    #[inline(always)]
    fn put(&self, source_mac: &zerotier_core::MAC, dest_mac: &zerotier_core::MAC, ethertype: u16, _vlan_id: u16, data: *const u8, len: usize) -> bool {
        let dm = dest_mac.0;
        let sm = source_mac.0;
        let mut hdr: [u8; 14] = [(dm >> 40) as u8, (dm >> 32) as u8, (dm >> 24) as u8, (dm >> 16) as u8, (dm >> 8) as u8, dm as u8, (sm >> 40) as u8, (sm >> 32) as u8, (sm >> 24) as u8, (sm >> 16) as u8, (sm >> 8) as u8, sm as u8, (ethertype >> 8) as u8, ethertype as u8];
        unsafe {
            let iov: [libc::iovec; 2] = [
                libc::iovec {
                    iov_base: hdr.as_mut_ptr().cast(),
                    iov_len: 14,
                },
                libc::iovec {
                    iov_base: transmute(data), // have to "cast away const" even though data is not modified by writev()
                    iov_len: len as libc::size_t,
                },
            ];
            libc::writev(self.ndrv_fd, iov.as_ptr(), 2) == (len + 14) as libc::ssize_t
        }
    }
}

impl Drop for MacFethTap {
    fn drop(&mut self) {
        if self.bpf_fd >= 0 {
            unsafe {
                libc::shutdown(self.bpf_fd, libc::SHUT_RDWR);
                libc::close(self.bpf_fd);
                MAC_FETH_BPF_DEVICES_USED.lock().unwrap().remove(&self.bpf_no);
            }
        }
        if self.ndrv_fd >= 0 {
            unsafe {
                libc::close(self.ndrv_fd);
            }
        }
        let t = self.bpf_read_thread.replace(None);
        if t.is_some() {
            let _ = t.unwrap().join();
        }
        // NOTE: the feth devices are destroyed by MacFethDevice's drop().
    }
}
