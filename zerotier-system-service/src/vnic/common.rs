/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::collections::HashSet;
#[allow(unused_imports)]
use std::os::raw::c_int;

#[allow(unused_imports)]
use num_traits::AsPrimitive;
#[allow(unused_imports)]
use zerotier_network_hypervisor::vl1::MAC;

#[cfg(any(target_os = "macos", target_os = "ios", target_os = "netbsd", target_os = "openbsd", target_os = "dragonfly", target_os = "freebsd", target_os = "darwin"))]
#[allow(non_camel_case_types)]
#[repr(C)]
struct ifmaddrs {
    ifma_next: *mut ifmaddrs,
    ifma_name: *mut libc::sockaddr,
    ifma_addr: *mut libc::sockaddr,
    ifma_lladdr: *mut libc::sockaddr,
}

#[cfg(any(target_os = "macos", target_os = "ios", target_os = "netbsd", target_os = "openbsd", target_os = "dragonfly", target_os = "freebsd", target_os = "darwin"))]
extern "C" {
    fn getifmaddrs(ifmap: *mut *mut ifmaddrs) -> c_int;
    fn freeifmaddrs(ifmp: *mut ifmaddrs);
}

#[cfg(any(target_os = "macos", target_os = "ios", target_os = "netbsd", target_os = "openbsd", target_os = "dragonfly", target_os = "freebsd", target_os = "darwin"))]
pub fn get_l2_multicast_subscriptions(dev: &str) -> HashSet<MAC> {
    let mut groups: HashSet<MAC> = HashSet::new();
    let dev = dev.as_bytes();
    unsafe {
        let mut maddrs: *mut ifmaddrs = std::ptr::null_mut();
        if getifmaddrs(&mut maddrs as *mut *mut ifmaddrs) == 0 {
            let mut i = maddrs;
            while !i.is_null() {
                if !(*i).ifma_name.is_null() && !(*i).ifma_addr.is_null() && (*(*i).ifma_addr).sa_family as i32 == libc::AF_LINK as i32 {
                    let in_: &libc::sockaddr_dl = &*((*i).ifma_name.cast());
                    let la: &libc::sockaddr_dl = &*((*i).ifma_addr.cast());
                    if la.sdl_alen == 6 && in_.sdl_nlen <= dev.len().as_() && crate::libc::memcmp(dev.as_ptr().cast(), in_.sdl_data.as_ptr().cast(), in_.sdl_nlen.as_()) == 0 {
                        let mi = la.sdl_nlen as usize;
                        MAC::from_u64((la.sdl_data[mi] as u64) << 40 | (la.sdl_data[mi + 1] as u64) << 32 | (la.sdl_data[mi + 2] as u64) << 24 | (la.sdl_data[mi + 3] as u64) << 16 | (la.sdl_data[mi + 4] as u64) << 8 | la.sdl_data[mi + 5] as u64).map(|mac| groups.insert(mac));
                    }
                }
                i = (*i).ifma_next;
            }
            freeifmaddrs(maddrs);
        }
    }
    groups
}

/// Linux stores this stuff in /proc and it needs to be fetched from there.
#[cfg(target_os = "linux")]
pub fn get_l2_multicast_subscriptions(dev: &str) -> HashSet<MAC> {
    let mut groups: HashSet<MulticastGroup> = HashSet::new();
    groups
}
