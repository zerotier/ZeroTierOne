// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::collections::BTreeMap;

use serde::{Deserialize, Serialize};

use zerotier_network_hypervisor::vl1::{Address, Endpoint};
use zerotier_network_hypervisor::vl2::NetworkId;
use zerotier_vl1_service::VL1Settings;

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
pub struct Config {
    pub physical: BTreeMap<Endpoint, PhysicalPathSettings>,
    #[serde(rename = "virtual")]
    pub virtual_: BTreeMap<Address, VirtualPathSettings>,
    pub network: BTreeMap<NetworkId, NetworkSettings>,
    pub settings: VL1Settings,
}

impl Default for Config {
    fn default() -> Self {
        Self {
            physical: BTreeMap::new(),
            virtual_: BTreeMap::new(),
            network: BTreeMap::new(),
            settings: VL1Settings::default(),
        }
    }
}
