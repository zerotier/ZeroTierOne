// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::collections::{HashMap, HashSet};
use std::hash::Hash;

use serde::{Deserialize, Serialize};

use zerotier_network_hypervisor::vl1::InetAddress;
use zerotier_network_hypervisor::vl2::networkconfig::IpRoute;
use zerotier_network_hypervisor::vl2::{NetworkId, Rule};

use crate::database::Database;
use crate::model::{Member, ObjectType};

pub const CREDENTIAL_WINDOW_SIZE_DEFAULT: i64 = 1000 * 60 * 60;

#[derive(Clone, PartialEq, Eq, Serialize, Deserialize, Default)]
pub struct Ipv4AssignMode {
    pub zt: bool,
}

#[derive(Clone, PartialEq, Eq, Serialize, Deserialize, Default)]
pub struct Ipv6AssignMode {
    pub zt: bool,
    pub rfc4193: bool,
    #[serde(rename = "6plane")]
    pub _6plane: bool,
}

#[derive(Clone, PartialEq, Eq, Serialize, Deserialize, Hash)]
pub struct IpAssignmentPool {
    #[serde(rename = "ipRangeStart")]
    ip_range_start: InetAddress,
    #[serde(rename = "ipRangeEnd")]
    ip_range_end: InetAddress,
}

/// Virtual network configuration.
///
/// This contains only fields of relevance to the controller. Other fields can be tracked by various
/// database implementations such as row last modified, creation time, ownership in an admin panel, etc.
#[derive(Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct Network {
    pub id: NetworkId,

    /// Network name that's sent to network members
    #[serde(default)]
    pub name: String,

    /// Guideline for the maximum number of multicast recipients on a network (not a hard limit).
    /// Setting to zero disables multicast entirely. The default is used if this is not set.
    #[serde(rename = "multicastLimit")]
    pub multicast_limit: Option<u32>,

    /// If true, this network supports ff:ff:ff:ff:ff:ff Ethernet broadcast.
    #[serde(rename = "enableBroadcast")]
    #[serde(default = "troo")]
    pub enable_broadcast: bool,

    /// Auto IP assignment mode(s) for IPv4 addresses.
    #[serde(rename = "v4AssignMode")]
    #[serde(default)]
    pub v4_assign_mode: Ipv4AssignMode,

    /// Auto IP assignment mode(s) for IPv6 addresses.
    #[serde(rename = "v6AssignMode")]
    #[serde(default)]
    pub v6_assign_mode: Ipv6AssignMode,

    /// IPv4 or IPv6 auto-assignment pools available, must be present to use 'zt' mode.
    #[serde(rename = "ipAssignmentPools")]
    #[serde(default)]
    pub ip_assignment_pools: HashSet<IpAssignmentPool>,

    /// IPv4 or IPv6 routes to advertise.
    #[serde(default)]
    pub ip_routes: HashSet<IpRoute>,

    /// DNS records to push to members.
    pub dns: HashMap<String, HashSet<InetAddress>>,

    /// Network rule set.
    #[serde(default)]
    pub rules: Vec<Rule>,

    /// If set this overrides the default "agreement" window for certificates and credentials.
    ///
    /// Making it smaller causes deauthorized nodes to fall out of the window more rapidly but can
    /// come at the expense of reliability if it's too short for everyone to update their certs
    /// on time from the controller. Note that revocations are also used to deauthorize nodes
    /// promptly, so nodes will still deauthorize quickly even if the window is long.
    ///
    /// Usually this does not need to be changed.
    #[serde(rename = "credentialWindowSize")]
    pub credential_window_size: Option<i64>,

    /// MTU inside the virtual network, default of 2800 is used if not set.
    pub mtu: Option<u16>,

    /// If true the network has access control, which is usually what you want.
    #[serde(default = "troo")]
    pub private: bool,

    /// If true this network will add not-authorized members for anyone who requests a config.
    #[serde(default = "troo")]
    pub learn_members: bool,

    /// Static object type field for use with API.
    #[serde(default = "ObjectType::network")]
    pub objtype: ObjectType,
}

impl Hash for Network {
    #[inline]
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.id.hash(state)
    }
}

#[inline(always)]
fn troo() -> bool {
    true
}

impl Network {
    /// Check member IP assignments and return 'true' if IP assignments were created or modified.
    pub async fn check_zt_ip_assignments<DatabaseImpl: Database>(&self, database: &DatabaseImpl, member: &mut Member) -> bool {
        let mut modified = false;

        if self.v4_assign_mode.zt {
            if !member.ip_assignments.iter().any(|ip| ip.is_ipv4()) {
                'ip_search: for pool in self.ip_assignment_pools.iter() {
                    if pool.ip_range_start.is_ipv4() && pool.ip_range_end.is_ipv4() {
                        let mut ip_ptr = u32::from_be_bytes(pool.ip_range_start.ip_bytes().try_into().unwrap());
                        let ip_end = u32::from_be_bytes(pool.ip_range_end.ip_bytes().try_into().unwrap());
                        while ip_ptr < ip_end {
                            for route in self.ip_routes.iter() {
                                let ip = InetAddress::from_ip_port(&ip_ptr.to_be_bytes(), route.target.port()); // IP/bits
                                if ip.is_within(&route.target) {
                                    if !database.is_ip_assigned(self.id, &ip).await.unwrap_or(true) {
                                        modified = true;
                                        let _ = member.ip_assignments.insert(ip);
                                        break 'ip_search;
                                    }
                                }
                            }
                            ip_ptr += 1;
                        }
                    }
                }
            }
        }

        if self.v6_assign_mode.zt {
            if !member.ip_assignments.iter().any(|ip| ip.is_ipv6()) {
                'ip_search: for pool in self.ip_assignment_pools.iter() {
                    if pool.ip_range_start.is_ipv6() && pool.ip_range_end.is_ipv6() {
                        let mut ip_ptr = u128::from_be_bytes(pool.ip_range_start.ip_bytes().try_into().unwrap());
                        let ip_end = u128::from_be_bytes(pool.ip_range_end.ip_bytes().try_into().unwrap());
                        while ip_ptr < ip_end {
                            for route in self.ip_routes.iter() {
                                let ip = InetAddress::from_ip_port(&ip_ptr.to_be_bytes(), route.target.port()); // IP/bits
                                if ip.is_within(&route.target) {
                                    if !database.is_ip_assigned(self.id, &ip).await.unwrap_or(true) {
                                        modified = true;
                                        let _ = member.ip_assignments.insert(ip);
                                        break 'ip_search;
                                    }
                                }
                            }
                            ip_ptr += 1;
                        }
                    }
                }
            }
        }

        modified
    }
}
