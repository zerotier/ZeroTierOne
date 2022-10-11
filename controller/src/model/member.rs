// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::collections::{HashMap, HashSet};
use std::hash::Hash;

use serde::{Deserialize, Serialize};

use zerotier_network_hypervisor::vl1::{Address, Identity, InetAddress};
use zerotier_network_hypervisor::vl2::NetworkId;

use crate::model::ObjectType;

#[derive(Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct Member {
    #[serde(rename = "address")]
    pub node_id: Address,
    #[serde(rename = "networkId")]
    pub network_id: NetworkId,

    /// Pinned full member identity, if known.
    pub identity: Option<Identity>,

    /// A short name that can also be used for DNS, etc.
    #[serde(default)]
    pub name: String,

    /// Time member was most recently authorized, None for 'never'.
    #[serde(rename = "lastAuthorizedTime")]
    pub last_authorized_time: Option<i64>,

    /// Time member was most recently deauthorized, None for 'never'.
    #[serde(rename = "lastDeauthorizedTime")]
    pub last_deauthorized_time: Option<i64>,

    /// ZeroTier-managed IP assignments.
    #[serde(rename = "ipAssignments")]
    #[serde(default)]
    pub ip_assignments: HashSet<InetAddress>,

    /// If true, do not auto-assign IPs in the controller.
    #[serde(rename = "noAutoAssignIps")]
    #[serde(default)]
    pub no_auto_assign_ips: bool,

    /// If true this member is a full Ethernet bridge.
    #[serde(rename = "activeBridge")]
    #[serde(default)]
    pub bridge: bool,

    /// Tags that can be used in rule evaluation for ACL-like behavior.
    #[serde(default)]
    pub tags: HashMap<u32, u32>,

    /// Member is exempt from SSO, authorization managed conventionally.
    #[serde(rename = "ssoExempt")]
    #[serde(default)]
    pub sso_exempt: bool,

    /// If true this node is explicitly listed in every member's network configuration.
    /// This is only supported for V2 nodes.
    #[serde(rename = "advertised")]
    #[serde(default)]
    pub advertised: bool,

    /// API object type documentation field, not actually edited/used.
    #[serde(default = "ObjectType::member")]
    pub objtype: ObjectType,
}

impl Member {
    pub fn new_without_identity(node_id: Address, network_id: NetworkId) -> Self {
        Self {
            node_id,
            network_id,
            identity: None,
            name: String::new(),
            last_authorized_time: None,
            last_deauthorized_time: None,
            ip_assignments: HashSet::new(),
            no_auto_assign_ips: false,
            bridge: false,
            tags: HashMap::new(),
            sso_exempt: false,
            advertised: false,
            objtype: ObjectType::Member,
        }
    }

    pub fn new_with_identity(identity: Identity, network_id: NetworkId) -> Self {
        let mut tmp = Self::new_without_identity(identity.address, network_id);
        tmp.identity = Some(identity);
        tmp
    }

    /// Check whether this member is authorized, which is true if the last authorized time is after last deauthorized time.
    pub fn authorized(&self) -> bool {
        self.last_authorized_time
            .map_or(false, |la| self.last_deauthorized_time.map_or(true, |ld| la > ld))
    }
}

impl Hash for Member {
    #[inline]
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.node_id.hash(state);
        self.network_id.hash(state);
    }
}

impl ToString for Member {
    #[inline(always)]
    fn to_string(&self) -> String {
        zerotier_utils::json::to_json_pretty(self)
    }
}

#[derive(Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct Tag {
    pub id: u32,
    pub value: u32,
}
