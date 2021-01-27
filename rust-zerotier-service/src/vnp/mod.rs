// VNP = virtual network port, a.k.a. tun/tap device driver interface.

#[cfg(target_os = "macos")]
mod mac_feth_tap;

pub trait Port {
    fn add_ip(&self, ip: &zerotier_core::InetAddress) -> bool;
    fn remove_ip(&self, ip: &zerotier_core::InetAddress) -> bool;
    fn ips(&self) -> Vec<zerotier_core::InetAddress>;
    fn device_name(&self) -> String;
    fn get_multicast_groups(&self) -> std::collections::BTreeSet<zerotier_core::MulticastGroup>;
}
