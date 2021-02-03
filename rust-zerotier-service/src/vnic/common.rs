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

use std::collections::BTreeSet;
use std::ptr::null_mut;
use std::os::raw::c_ulong;
use zerotier_core::{MAC, MulticastGroup};
use crate::osdep as osdep;

#[cfg(any(target_os = "macos", target_os = "freebsd", target_os = "netbsd", target_os = "openbsd", target_os = "dragonfly", target_os = "ios"))]
pub(crate) fn bsd_get_multicast_groups(dev: &str) -> BTreeSet<MulticastGroup> {
    let dev = dev.as_bytes();
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
