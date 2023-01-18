// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::collections::HashSet;

use serde::{Deserialize, Serialize};

use zerotier_network_hypervisor::vl1::InetAddress;

#[derive(Serialize, Deserialize, Clone, PartialEq, Eq)]
#[serde(default)]
pub struct VL1Settings {
    /// Primary ZeroTier port that is always bound, default is 9993.
    pub fixed_ports: HashSet<u16>,

    /// Number of additional random ports to bind.
    pub random_port_count: usize,

    /// Enable uPnP, NAT-PMP, and other router port mapping technologies?
    pub port_mapping: bool,

    /// Interface name prefix blacklist for local bindings (not remote IPs).
    pub interface_prefix_blacklist: HashSet<String>,

    /// IP/bits CIDR blacklist for local bindings (not remote IPs).
    pub cidr_blacklist: HashSet<InetAddress>,
}

impl VL1Settings {
    #[cfg(target_os = "macos")]
    pub const DEFAULT_PREFIX_BLACKLIST: [&'static str; 11] = ["lo", "utun", "gif", "stf", "iptap", "pktap", "feth", "zt", "llw", "anpi", "bridge"];

    #[cfg(target_os = "linux")]
    pub const DEFAULT_PREFIX_BLACKLIST: [&'static str; 5] = ["lo", "tun", "tap", "ipsec", "zt"];

    #[cfg(windows)]
    pub const DEFAULT_PREFIX_BLACKLIST: [&'static str; 0] = [];
}

impl Default for VL1Settings {
    fn default() -> Self {
        Self {
            fixed_ports: HashSet::from([9993u16]),
            random_port_count: 5,
            port_mapping: true,
            interface_prefix_blacklist: Self::DEFAULT_PREFIX_BLACKLIST.iter().map(|s| s.to_string()).collect(),
            cidr_blacklist: HashSet::new(),
        }
    }
}
