#[cfg(target_os = "macos")]
mod mac_feth_tap;

pub trait VNIC {
    fn add_ip(&self, ip: &zerotier_core::InetAddress) -> bool;
    fn remove_ip(&self, ip: &zerotier_core::InetAddress) -> bool;
    fn ips(&self) -> Vec<zerotier_core::InetAddress>;
    fn device_name(&self) -> String;
    fn get_multicast_groups(&self) -> std::collections::BTreeSet<zerotier_core::MulticastGroup>;
    fn put(&self, source_mac: &zerotier_core::MAC, dest_mac: &zerotier_core::MAC, ethertype: u16, vlan_id: u16, data: *const u8, len: usize) -> bool;
}
