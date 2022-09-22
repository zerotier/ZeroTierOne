// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::collections::{HashMap, HashSet};
use std::hash::Hash;

use serde::{Deserialize, Serialize};

use zerotier_network_hypervisor::vl1::{Address, Endpoint, Identity, InetAddress};
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
pub struct Tag {
    pub id: u32,
    pub value: u32,
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
    #[serde(rename = "address")]
    pub node_id: Address,
    #[serde(rename = "networkId")]
    pub network_id: NetworkId,
    pub identity: Option<Identity>,
    pub name: String,
    pub description: String,

    #[serde(rename = "creationTime")]
    pub creation_time: i64,

    #[serde(rename = "revision")]
    pub last_modified_time: i64,

    pub authorized: bool,
    #[serde(rename = "lastAuthorizedTime")]
    pub last_authorized_time: Option<i64>,
    #[serde(rename = "lastDeauthorizedTime")]
    pub last_deauthorized_time: Option<i64>,

    #[serde(rename = "ipAssignments")]
    pub ip_assignments: HashSet<InetAddress>,
    #[serde(rename = "noAutoAssignIps")]
    pub no_auto_assign_ips: bool,

    /// If true this member is a full Ethernet bridge.
    #[serde(rename = "activeBridge")]
    pub bridge: bool,

    pub tags: Vec<Tag>,

    #[serde(rename = "ssoExempt")]
    pub sso_exempt: bool,

    /// If true this node is explicitly listed in every member's network configuration.
    #[serde(rename = "advertised")]
    pub advertised: bool,

    /// Most recently generated and signed network configuration for this member in binary format.
    #[serde(rename = "networkConfig")]
    pub network_config: Option<Vec<u8>>,

    /// API object type documentation field, not actually edited/used.
    #[serde(default = "ObjectType::member")]
    pub objtype: ObjectType,
}

/// A complete network with all member configuration information for import/export or blob storage.
#[derive(Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct NetworkExport {
    pub network: Network,
    pub members: HashMap<Address, Member>,
}

#[derive(Clone, PartialEq, Eq, Serialize, Deserialize)]
#[repr(u8)]
pub enum AuthorizationResult {
    #[serde(rename = "r")]
    Rejected = 0,
    #[serde(rename = "rs")]
    RejectedViaSSO = 1,
    #[serde(rename = "rt")]
    RejectedViaToken = 2,
    #[serde(rename = "ro")]
    RejectedTooOld = 3,
    #[serde(rename = "a")]
    Approved = 16,
    #[serde(rename = "as")]
    ApprovedViaSSO = 17,
    #[serde(rename = "at")]
    ApprovedViaToken = 18,
}

#[derive(Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct QueryLogItem {
    #[serde(rename = "nwid")]
    pub network_id: NetworkId,
    #[serde(rename = "nid")]
    pub node_id: Address,
    #[serde(rename = "cid")]
    pub controller_node_id: Address,
    #[serde(rename = "md")]
    pub metadata: Vec<u8>,
    #[serde(rename = "ts")]
    pub timestamp: i64,
    #[serde(rename = "v")]
    pub version: (u16, u16, u16, u16),
    #[serde(rename = "s")]
    pub source_remote_endpoint: Endpoint,
    #[serde(rename = "sh")]
    pub source_hops: u8,
    #[serde(rename = "r")]
    pub result: AuthorizationResult,
}
