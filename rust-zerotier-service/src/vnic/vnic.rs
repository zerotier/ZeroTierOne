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

/// Virtual network interface
pub(crate) trait VNIC {
    /// Add a new IPv4 or IPv6 address to this interface, returning true on success.
    fn add_ip(&self, ip: &zerotier_core::InetAddress) -> bool;

    /// Remove an IPv4 or IPv6 address, returning true on success.
    /// Nothing happens if the address is not found.
    fn remove_ip(&self, ip: &zerotier_core::InetAddress) -> bool;

    /// Enumerate all IPs on this interface including ones assigned outside ZeroTier.
    fn ips(&self) -> Vec<zerotier_core::InetAddress>;

    /// Get the OS-specific device name for this interface, e.g. zt## or tap##.
    fn device_name(&self) -> String;

    /// Get L2 multicast groups to which this interface is subscribed.
    /// This doesn't do any IGMP snooping. It just reports the groups the port
    /// knows about. On some OSes this may not be supported in which case it
    /// will return an empty set.
    fn get_multicast_groups(&self) -> std::collections::BTreeSet<zerotier_core::MulticastGroup>;

    /// Inject an Ethernet frame into this port.
    fn put(&self, source_mac: &zerotier_core::MAC, dest_mac: &zerotier_core::MAC, ethertype: u16, vlan_id: u16, data: *const u8, len: usize) -> bool;
}
