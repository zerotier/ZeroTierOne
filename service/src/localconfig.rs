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

use std::collections::BTreeMap;
use zerotier_core::{InetAddress, Address, NetworkId};
use serde::{Deserialize, Serialize};

pub const UNASSIGNED_PRIVILEGED_PORTS: [u16; 299] = [
    4,
    6,
    8,
    10,
    12,
    14,
    15,
    16,
    26,
    28,
    30,
    32,
    34,
    36,
    40,
    60,
    269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279,
    285,
    288, 289, 290, 291, 292, 293, 294, 295, 296, 297, 298, 299, 300, 301, 302, 303, 304, 305, 306, 307,
    323, 324, 325, 326, 327, 328, 329, 330, 331, 332,
    334, 335, 336, 337, 338, 339, 340, 341, 342, 343,
    703,
    708,
    713, 714, 715, 716, 717, 718, 719, 720, 721, 722, 723, 724, 725, 726, 727, 728,
    732, 733, 734, 735, 736, 737, 738, 739, 740,
    743,
    745, 746,
    755, 756,
    766,
    768,
    778, 779,
    781, 782, 783, 784, 785, 786, 787, 788, 789, 790, 791, 792, 793, 794, 795, 796, 797, 798, 799,
    802, 803, 804, 805, 806, 807, 808, 809,
    811, 812, 813, 814, 815, 816, 817, 818, 819, 820, 821, 822, 823, 824, 825, 826, 827,
    834, 835, 836, 837, 838, 839, 840, 841, 842, 843, 844, 845, 846,
    849, 850, 851, 852, 853, 854, 855, 856, 857, 858, 859,
    862, 863, 864, 865, 866, 867, 868, 869, 870, 871, 872,
    874, 875, 876, 877, 878, 879, 880, 881, 882, 883, 884, 885,
    889, 890, 891, 892, 893, 894, 895, 896, 897, 898, 899,
    904, 905, 906, 907, 908, 909, 910, 911,
    914, 915, 916, 917, 918, 919, 920, 921, 922, 923, 924, 925, 926, 927, 928, 929, 930, 931, 932, 933, 934, 935, 936, 937, 938, 939, 940, 941, 942, 943, 944, 945, 946, 947, 948, 949, 950, 951, 952, 953, 954, 955, 956, 957, 958, 959, 960, 961, 962, 963, 964, 965, 966, 967, 968, 969, 970, 971, 972, 973, 974, 975, 976, 977, 978, 979, 980, 981, 982, 983, 984, 985, 986, 987, 988,
    1001, 1002, 1003, 1004, 1005, 1006, 1007, 1008, 1009,
    1023,
];

#[derive(Serialize, Deserialize, Clone, PartialEq, Eq)]
#[serde(default)]
pub struct LocalConfigPhysicalPathConfig {
    pub blacklist: bool
}

#[derive(Serialize, Deserialize, Clone, PartialEq, Eq)]
#[serde(default)]
pub struct LocalConfigVirtualConfig {
    #[serde(rename = "try")]
    pub try_: Vec<InetAddress>
}

#[derive(Serialize, Deserialize, Clone, PartialEq, Eq)]
#[serde(default)]
pub struct LocalConfigNetworkSettings {
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

#[derive(Serialize, Deserialize, Clone, PartialEq, Eq)]
#[serde(default)]
pub struct LocalConfigLogSettings {
    pub path: Option<String>,
    #[serde(rename = "maxSize")]
    pub max_size: usize,
    pub vl1: bool,
    pub vl2: bool,
    #[serde(rename = "vl2TraceRules")]
    pub vl2_trace_rules: bool,
    #[serde(rename = "vl2TraceMulticast")]
    pub vl2_trace_multicast: bool,
    pub debug: bool,
    pub stderr: bool,
}

#[derive(Serialize, Deserialize, Clone, PartialEq, Eq)]
#[serde(default)]
pub struct LocalConfigSettings {
    #[serde(rename = "primaryPort")]
    pub primary_port: u16,
    #[serde(rename = "secondaryPort")]
    pub secondary_port: Option<u16>,
    #[serde(rename = "autoPortSearch")]
    pub auto_port_search: bool,
    #[serde(rename = "portMapping")]
    pub port_mapping: bool,
    #[serde(rename = "log")]
    pub log: LocalConfigLogSettings,
    #[serde(rename = "interfacePrefixBlacklist")]
    pub interface_prefix_blacklist: Vec<String>,
    #[serde(rename = "explicitAddresses")]
    pub explicit_addresses: Vec<InetAddress>,
}

#[derive(Serialize, Deserialize, Clone, PartialEq, Eq)]
#[serde(default)]
pub struct LocalConfig {
    pub physical: BTreeMap<InetAddress, LocalConfigPhysicalPathConfig>,
    #[serde(rename = "virtual")]
    pub virtual_: BTreeMap<Address, LocalConfigVirtualConfig>,
    pub network: BTreeMap<NetworkId, LocalConfigNetworkSettings>,
    pub settings: LocalConfigSettings,
}

impl Default for LocalConfigPhysicalPathConfig {
    fn default() -> Self {
        LocalConfigPhysicalPathConfig {
            blacklist: false
        }
    }
}

impl Default for LocalConfigVirtualConfig {
    fn default() -> Self {
        LocalConfigVirtualConfig {
            try_: Vec::new()
        }
    }
}

impl Default for LocalConfigNetworkSettings {
    fn default() -> Self {
        LocalConfigNetworkSettings {
            allow_managed_ips: true,
            allow_global_ips: false,
            allow_managed_routes: true,
            allow_global_routes: false,
            allow_default_route_override: false
        }
    }
}

impl Default for LocalConfigLogSettings {
    fn default() -> Self {
        // TODO: change before release to saner defaults
        LocalConfigLogSettings {
            path: None,
            max_size: 131072,
            vl1: true,
            vl2: true,
            vl2_trace_rules: true,
            vl2_trace_multicast: true,
            debug: true,
            stderr: true,
        }
    }
}

impl LocalConfigSettings {
    #[cfg(target_os = "macos")]
    const DEFAULT_PREFIX_BLACKLIST: [&'static str; 8] = ["lo", "utun", "gif", "stf", "iptap", "pktap", "feth", "zt"];

    #[cfg(target_os = "linux")]
    const DEFAULT_PREFIX_BLACKLIST: [&'static str; 5] = ["lo", "tun", "tap", "ipsec", "zt"];

    #[cfg(windows)]
    const DEFAULT_PREFIX_BLACKLIST: [&'static str; 0] = [];

    pub fn is_interface_blacklisted(&self, ifname: &str) -> bool {
        for p in self.interface_prefix_blacklist.iter() {
            if ifname.starts_with(p.as_str()) {
                return true;
            }
        }
        false
    }
}

impl Default for LocalConfigSettings {
    fn default() -> Self {
        let mut bl: Vec<String> = Vec::new();
        bl.reserve(LocalConfigSettings::DEFAULT_PREFIX_BLACKLIST.len());
        for n in LocalConfigSettings::DEFAULT_PREFIX_BLACKLIST.iter() {
            bl.push(String::from(*n));
        }

        LocalConfigSettings {
            primary_port: zerotier_core::DEFAULT_PORT,
            secondary_port: Some(zerotier_core::DEFAULT_SECONDARY_PORT),
            auto_port_search: true,
            port_mapping: true,
            log: LocalConfigLogSettings::default(),
            interface_prefix_blacklist: bl,
            explicit_addresses: Vec::new()
        }
    }
}

impl Default for LocalConfig {
    fn default() -> Self {
        LocalConfig {
            physical: BTreeMap::new(),
            virtual_: BTreeMap::new(),
            network: BTreeMap::new(),
            settings: LocalConfigSettings::default()
        }
    }
}
