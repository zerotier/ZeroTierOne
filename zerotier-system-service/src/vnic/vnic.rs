// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use async_trait::async_trait;

use zerotier_network_hypervisor::vl1::{InetAddress, MAC};
use zerotier_network_hypervisor::vl2::MulticastGroup;

/// Virtual network interface
#[async_trait]
pub trait VNIC {
    /// Add a new IPv4 or IPv6 address to this interface, returning true on success.
    async fn add_ip(&self, ip: &InetAddress) -> bool;

    /// Remove an IPv4 or IPv6 address, returning true on success.
    /// Nothing happens if the address is not found.
    async fn remove_ip(&self, ip: &InetAddress) -> bool;

    /// Enumerate all IPs on this interface including ones assigned outside ZeroTier.
    async fn ips(&self) -> Vec<InetAddress>;

    /// Get the OS-specific device name for this interface, e.g. zt## or tap##.
    fn device_name(&self) -> String;

    /// Get L2 multicast groups to which this interface is subscribed.
    /// This doesn't do any IGMP snooping. It just reports the groups the port
    /// knows about. On some OSes this may not be supported in which case it
    /// will return an empty set.
    async fn get_multicast_groups(&self) -> std::collections::BTreeSet<MulticastGroup>;

    /// Inject an Ethernet frame into this port.
    async fn put(&self, source_mac: &MAC, dest_mac: &MAC, ethertype: u16, vlan_id: u16, data: &[u8]) -> bool;
}
