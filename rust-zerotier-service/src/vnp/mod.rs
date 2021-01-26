// VNP = virtual network port, a.k.a. tun/tap device driver interface.

#[cfg(target_os = "macos")]
mod mac_feth_tap;

pub trait Port {
    fn set_enabled(&self, enabled: bool);
    fn is_enabled(&self, ) -> bool;
    fn set_ips(&self, zerotier_core::InetAddress: &ip);
    fn ips(&self) -> Vec<zerotier_core::InetAddress>;
    fn device_name(&self) -> String;
    fn routing_device_name(&self) -> String;
    fn set_friendly_name(&self, friendly_name: &str);
    fn friendly_name(&self) -> String;
    fn get_multicast_groups(&self) -> std::collections::BTreeSet<zerotier_core::MulticastGroup>;
}

