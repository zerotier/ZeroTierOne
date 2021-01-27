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

use zerotier_core::InetAddress;
use std::ffi::CStr;
use std::ptr::{null_mut, copy_nonoverlapping};
use std::mem::size_of;
use num_traits::cast::AsPrimitive;
use crate::osdep as osdep;

pub struct PhysicalLink {
    pub address: InetAddress,
    pub device: String
}

#[inline(always)]
fn s6_addr_as_ptr<A>(a: &A) -> *const A {
    a as *const A
}

impl PhysicalLink {
    #[cfg(unix)]
    pub fn map<F: FnMut(PhysicalLink)>(mut f: F) {
        unsafe {
            let mut ifap: *mut osdep::ifaddrs = null_mut();
            if osdep::getifaddrs((&mut ifap as *mut *mut osdep::ifaddrs).cast()) == 0 {
                let mut i = ifap;
                while !i.is_null() {
                    if !(*i).ifa_addr.is_null() {
                        let mut a = InetAddress::new();

                        let sa_family = (*(*i).ifa_addr).sa_family as u8;
                        if sa_family == osdep::AF_INET as u8 {
                            copy_nonoverlapping((*i).ifa_addr.cast::<u8>(), (&mut a as *mut InetAddress).cast::<u8>(), size_of::<osdep::sockaddr_in>());
                        } else if sa_family == osdep::AF_INET6 as u8 {
                            copy_nonoverlapping((*i).ifa_addr.cast::<u8>(), (&mut a as *mut InetAddress).cast::<u8>(), size_of::<osdep::sockaddr_in6>());
                        } else {
                            continue;
                        }

                        let mut netmask_bits: u16 = 0;
                        if !(*i).ifa_netmask.is_null() {
                            if sa_family == osdep::AF_INET as u8 {
                                let mut a = (*(*i).ifa_netmask.cast::<osdep::sockaddr_in>()).sin_addr.s_addr as u32;
                                netmask_bits = a.leading_ones() as u16;
                            } else if sa_family == osdep::AF_INET6 as u8 {
                                let a = s6_addr_as_ptr(&((*(*i).ifa_netmask.cast::<osdep::sockaddr_in6>()).sin6_addr)).cast::<u8>();
                                for i in 0..16 as isize {
                                    let mut b = *a.offset(i);
                                    if b == 0xff {
                                        netmask_bits += 8;
                                    } else {
                                        netmask_bits += b.leading_ones() as u16;
                                        break;
                                    }
                                }
                            }
                        }
                        a.set_port(netmask_bits);

                        f(PhysicalLink{
                            address: a,
                            device: if (*i).ifa_name.is_null() { String::new() } else { String::from(CStr::from_ptr((*i).ifa_name).to_str().unwrap()) }
                        });
                    }
                    i = (*i).ifa_next;
                }
                osdep::freeifaddrs(ifap.cast());
            }
        }
    }
}
