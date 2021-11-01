/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use zerotier_network_hypervisor::vl2::MulticastGroup;

/// Virtual network interface
pub trait VNIC {
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
    fn get_multicast_groups(&self) -> std::collections::BTreeSet<MulticastGroup>;

    /// Inject an Ethernet frame into this port.
    fn put(&self, source_mac: &zerotier_core::MAC, dest_mac: &zerotier_core::MAC, ethertype: u16, vlan_id: u16, data: *const u8, len: usize) -> bool;
}
