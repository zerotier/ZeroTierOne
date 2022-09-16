// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::collections::BTreeMap;

use serde::{Deserialize, Serialize};

use zerotier_network_hypervisor::vl1::{Address, Endpoint, InetAddress};
use zerotier_network_hypervisor::vl2::NetworkId;

/// Default primary ZeroTier port.
pub const DEFAULT_PORT: u16 = 9993;

#[derive(Serialize, Deserialize, Clone, PartialEq, Eq)]
#[serde(default)]
pub struct PhysicalPathSettings {
    pub blacklist: bool,
}

impl Default for PhysicalPathSettings {
    fn default() -> Self {
        Self { blacklist: false }
    }
}

#[derive(Serialize, Deserialize, Clone, PartialEq, Eq)]
#[serde(default)]
pub struct VirtualPathSettings {
    #[serde(rename = "try")]
    pub try_: Vec<Endpoint>,
}

impl Default for VirtualPathSettings {
    fn default() -> Self {
        Self { try_: Vec::new() }
    }
}

#[derive(Serialize, Deserialize, Clone, PartialEq, Eq)]
#[serde(default)]
pub struct NetworkSettings {
    #[serde(rename = "allowManagedIPs")]
    pub allow_managed_ips: bool,
    #[serde(rename = "allowGlobalIPs")]
    pub allow_global_ips: bool,
    #[serde(rename = "allowManagedRoutes")]
    pub allow_managed_routes: bool,
    #[serde(rename = "allowGlobalRoutes")]
    pub allow_global_routes: bool,
    #[serde(rename = "allowDefaultRouteOverride")]
    pub allow_default_route_override: bool,
}

impl Default for NetworkSettings {
    fn default() -> Self {
        Self {
            allow_managed_ips: true,
            allow_global_ips: false,
            allow_managed_routes: true,
            allow_global_routes: false,
            allow_default_route_override: false,
        }
    }
}

#[derive(Serialize, Deserialize, Clone, PartialEq, Eq)]
#[serde(default)]
pub struct GlobalSettings {
    /// Primary ZeroTier port that is always bound, default is 9993.
    #[serde(rename = "primaryPort")]
    pub primary_port: u16,

    /// Enable uPnP, NAT-PMP, and other router port mapping technologies?
    #[serde(rename = "portMapping")]
    pub port_mapping: bool,

    /// Interface name prefix blacklist for local bindings (not remote IPs).
    #[serde(rename = "interfacePrefixBlacklist")]
    pub interface_prefix_blacklist: Vec<String>,

    /// IP/bits CIDR blacklist for local bindings (not remote IPs).
    #[serde(rename = "cidrBlacklist")]
    pub cidr_blacklist: Vec<InetAddress>,
}

impl Default for GlobalSettings {
    fn default() -> Self {
        let mut bl: Vec<String> = Vec::new();
        bl.reserve(Self::DEFAULT_PREFIX_BLACKLIST.len());
        for n in Self::DEFAULT_PREFIX_BLACKLIST.iter() {
            bl.push(String::from(*n));
        }

        Self {
            primary_port: DEFAULT_PORT,
            port_mapping: true,
            interface_prefix_blacklist: bl,
            cidr_blacklist: Vec::new(),
        }
    }
}

impl GlobalSettings {
    #[cfg(target_os = "macos")]
    pub const DEFAULT_PREFIX_BLACKLIST: [&'static str; 10] = ["lo", "utun", "gif", "stf", "iptap", "pktap", "feth", "zt", "llw", "anpi"];

    #[cfg(target_os = "linux")]
    pub const DEFAULT_PREFIX_BLACKLIST: [&'static str; 5] = ["lo", "tun", "tap", "ipsec", "zt"];

    #[cfg(windows)]
    pub const DEFAULT_PREFIX_BLACKLIST: [&'static str; 0] = [];

    pub fn is_interface_blacklisted(&self, ifname: &str) -> bool {
        for p in self.interface_prefix_blacklist.iter() {
            if ifname.starts_with(p.as_str()) {
                return true;
            }
        }
        false
    }
}

#[derive(Serialize, Deserialize, Clone, PartialEq, Eq)]
#[serde(default)]
pub struct Config {
    pub physical: BTreeMap<Endpoint, PhysicalPathSettings>,
    #[serde(rename = "virtual")]
    pub virtual_: BTreeMap<Address, VirtualPathSettings>,
    pub network: BTreeMap<NetworkId, NetworkSettings>,
    pub settings: GlobalSettings,
}

impl Default for Config {
    fn default() -> Self {
        Self {
            physical: BTreeMap::new(),
            virtual_: BTreeMap::new(),
            network: BTreeMap::new(),
            settings: GlobalSettings::default(),
        }
    }
}
