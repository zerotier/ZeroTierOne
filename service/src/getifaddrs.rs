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

use std::mem::size_of;
use std::ptr::{copy_nonoverlapping, null_mut};

use zerotier_core::InetAddress;

use crate::osdep as osdep;

#[inline(always)]
fn s6_addr_as_ptr<A>(a: &A) -> *const A {
    a as *const A
}

/// Call supplied function or closure for each physical IP address in the system.
#[cfg(unix)]
pub(crate) fn for_each_address<F: FnMut(&InetAddress, &str)>(mut f: F) {
    unsafe {
        let mut ifa_name = [0_u8; osdep::IFNAMSIZ as usize];
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
                        i = (*i).ifa_next;
                        continue;
                    }

                    let mut netmask_bits: u16 = 0;
                    if !(*i).ifa_netmask.is_null() {
                        if sa_family == osdep::AF_INET as u8 {
                            let a = (*(*i).ifa_netmask.cast::<osdep::sockaddr_in>()).sin_addr.s_addr as u32;
                            netmask_bits = a.leading_ones() as u16;
                        } else if sa_family == osdep::AF_INET6 as u8 {
                            let a = s6_addr_as_ptr(&((*(*i).ifa_netmask.cast::<osdep::sockaddr_in6>()).sin6_addr)).cast::<u8>();
                            for i in 0..16 as isize {
                                let b = *a.offset(i);
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

                    let mut namlen: usize = 0;
                    while namlen < (osdep::IFNAMSIZ as usize) {
                        let c = *(*i).ifa_name.offset(namlen as isize);
                        if c != 0 {
                            ifa_name[namlen] = c as u8;
                            namlen += 1;
                        } else {
                            break;
                        }
                    }
                    if namlen > 0 {
                        let dev = String::from_utf8_lossy(&ifa_name[0..namlen]);
                        if dev.len() > 0 {
                            f(&a, dev.as_ref());
                        }
                    }
                }
                i = (*i).ifa_next;
            }
            osdep::freeifaddrs(ifap.cast());
        }
    }
}

#[cfg(test)]
mod tests {
    use zerotier_core::InetAddress;

    #[test]
    fn test_getifaddrs() {
        println!("starting getifaddrs...");
        crate::getifaddrs::for_each_address(|a: &InetAddress, dev: &str| {
            println!("  device: {} ip: {}", dev, a.to_string())
        });
        println!("done.")
    }
}
