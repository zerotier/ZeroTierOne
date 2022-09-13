// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::hash::Hash;

use serde::{Deserialize, Serialize};

use zerotier_network_hypervisor::vl1::Address;
use zerotier_network_hypervisor::vl1::InetAddress;
use zerotier_network_hypervisor::vl2::NetworkId;

#[derive(Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum ObjectType {
    #[serde(rename = "network")]
    Network,
    #[serde(rename = "member")]
    Member,
}

impl ObjectType {
    fn network() -> ObjectType {
        Self::Network
    }
    fn member() -> ObjectType {
        Self::Member
    }
}

#[derive(Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct Ipv4AssignMode {
    pub zt: bool,
}

#[derive(Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct Ipv6AssignMode {
    pub zt: bool,
    pub rfc4193: bool,
    #[serde(rename = "6plane")]
    pub _6plane: bool,
}

#[derive(Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct IpAssignmentPool {
    #[serde(rename = "ipRangeStart")]
    ip_range_start: InetAddress,
    #[serde(rename = "ipRangeEnd")]
    ip_range_end: InetAddress,
}

#[derive(Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct Network {
    pub id: NetworkId,
    pub name: String,

    #[serde(rename = "creationTime")]
    pub creation_time: i64,

    #[serde(rename = "multicastLimit")]
    pub multicast_limit: u64,
    #[serde(rename = "enableBroadcast")]
    pub enable_broadcast: bool,

    #[serde(rename = "v4AssignMode")]
    pub v4_assign_mode: Ipv4AssignMode,
    #[serde(rename = "v6AssignMode")]
    pub v6_assign_mode: Ipv6AssignMode,
    #[serde(rename = "ipAssignmentPools")]
    pub ip_assignment_pools: Vec<IpAssignmentPool>,

    #[serde(rename = "rulesSource")]
    pub rules_source: String,

    pub mtu: u16,
    pub private: bool,

    #[serde(default = "ObjectType::network")]
    pub objtype: ObjectType,
}

impl Hash for Network {
    #[inline(always)]
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.id.hash(state)
    }
}

#[derive(Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct Member {
    #[serde(rename = "networkId")]
    pub network_id: NetworkId,
    pub address: Address,
    pub name: String,

    #[serde(rename = "creationTime")]
    pub creation_time: i64,

    #[serde(rename = "ipAssignments")]
    pub ip_assignments: Vec<InetAddress>,
    #[serde(rename = "noAutoAssignIps")]
    pub no_auto_assign_ips: bool,

    #[serde(rename = "vMajor")]
    pub version_major: u16,
    #[serde(rename = "vMinor")]
    pub version_minor: u16,
    #[serde(rename = "vRev")]
    pub version_revision: u16,
    #[serde(rename = "vProto")]
    pub version_protocol: u16,

    pub authorized: bool,
    #[serde(rename = "activeBridge")]
    pub bridge: bool,

    #[serde(rename = "ssoExempt")]
    pub sso_exempt: bool,

    #[serde(default = "ObjectType::member")]
    pub objtype: ObjectType,
}

impl Hash for Member {
    #[inline(always)]
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.network_id.hash(state);
        self.address.hash(state);
    }
}
