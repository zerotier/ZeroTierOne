// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::collections::{BTreeMap, BTreeSet};
use std::hash::Hash;

use serde::{Deserialize, Serialize};

use zerotier_crypto::typestate::Valid;
use zerotier_network_hypervisor::vl1::identity::Identity;
use zerotier_network_hypervisor::vl1::{InetAddress, PartialAddress};
use zerotier_network_hypervisor::vl2::NetworkId;

#[derive(Clone, PartialEq, Eq, Serialize, Deserialize, Debug)]
pub struct Member {
    /// Member node ID
    ///
    /// This can be a partial address if it was manually added as such by a user. As soon as a node matching
    /// this partial is seen, this will be replaced by a full specificity PartialAddress from the querying
    /// node's full identity. The 'identity' field will also be populated in this case.
    #[serde(rename = "address")]
    pub node_id: PartialAddress,

    #[serde(rename = "networkId")]
    pub network_id: NetworkId,

    /// Full identity of this node, if known.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub identity: Option<Identity>,

    /// A short name that can also be used for DNS, etc.
    #[serde(skip_serializing_if = "String::is_empty")]
    #[serde(default)]
    pub name: String,

    /// Time member was most recently authorized, None for 'never'.
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(rename = "lastAuthorizedTime")]
    pub last_authorized_time: Option<i64>,

    /// Time member was most recently deauthorized, None for 'never'.
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(rename = "lastDeauthorizedTime")]
    pub last_deauthorized_time: Option<i64>,

    /// ZeroTier-managed IP assignments.
    #[serde(skip_serializing_if = "BTreeSet::is_empty")]
    #[serde(rename = "ipAssignments")]
    #[serde(default)]
    pub ip_assignments: BTreeSet<InetAddress>,

    /// If true, do not auto-assign IPs in the controller.
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(rename = "noAutoAssignIps")]
    #[serde(default)]
    pub no_auto_assign_ips: Option<bool>,

    /// If true this member is a full Ethernet bridge.
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(rename = "activeBridge")]
    #[serde(default)]
    pub bridge: Option<bool>,

    /// Tags that can be used in rule evaluation for ACL-like behavior.
    #[serde(skip_serializing_if = "BTreeMap::is_empty")]
    #[serde(default)]
    pub tags: BTreeMap<u32, u32>,

    /// Member is exempt from SSO, authorization managed conventionally.
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(rename = "ssoExempt")]
    #[serde(default)]
    pub sso_exempt: Option<bool>,

    /// If true this node is explicitly listed in every member's network configuration.
    /// This is only supported for V2 nodes.
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(default)]
    pub advertised: Option<bool>,
}

impl Member {
    pub fn new(node_identity: Valid<Identity>, network_id: NetworkId) -> Self {
        Self {
            node_id: node_identity.address.to_partial(),
            network_id,
            identity: Some(node_identity.remove_typestate()),
            name: String::new(),
            last_authorized_time: None,
            last_deauthorized_time: None,
            ip_assignments: BTreeSet::new(),
            no_auto_assign_ips: None,
            bridge: None,
            tags: BTreeMap::new(),
            sso_exempt: None,
            advertised: None,
        }
    }

    /// Check whether this member is authorized.
    /// This is true if the last authorized time is after last deauthorized time.
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
