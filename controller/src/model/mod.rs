// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

mod member;
mod network;

pub use member::*;
pub use network::*;

use std::collections::HashMap;

use serde::{Deserialize, Serialize};

use zerotier_network_hypervisor::vl1::{Address, Endpoint};
use zerotier_network_hypervisor::vl2::NetworkId;
use zerotier_utils::blob::Blob;

/// A complete network with all member configuration information for import/export or blob storage.
#[derive(Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct NetworkExport {
    pub network: Network,
    pub members: HashMap<Address, Member>,
}

/// Static string included in JSON-serializable objects to indicate their object type through the API.
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
    #[serde(rename = "re")]
    RejectedDueToError = 4,
    #[serde(rename = "rm")]
    RejectedIdentityMismatch = 5,
    #[serde(rename = "a")]
    Approved = 128,
    #[serde(rename = "as")]
    ApprovedViaSSO = 129,
    #[serde(rename = "at")]
    ApprovedViaToken = 130,
    #[serde(rename = "ap")]
    ApprovedOnPublicNetwork = 131,
}

impl AuthorizationResult {
    pub fn as_str(&self) -> &'static str {
        // These short codes should match the serde enum names above.
        match self {
            Self::Rejected => "r",
            Self::RejectedViaSSO => "rs",
            Self::RejectedViaToken => "rt",
            Self::RejectedTooOld => "ro",
            Self::RejectedDueToError => "re",
            Self::RejectedIdentityMismatch => "rm",
            Self::Approved => "a",
            Self::ApprovedViaSSO => "as",
            Self::ApprovedViaToken => "at",
            Self::ApprovedOnPublicNetwork => "ap",
        }
    }
}

impl ToString for AuthorizationResult {
    #[inline(always)]
    fn to_string(&self) -> String {
        self.as_str().to_string()
    }
}

#[derive(Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct RequestLogItem {
    #[serde(rename = "nwid")]
    pub network_id: NetworkId,
    #[serde(rename = "nid")]
    pub node_id: Address,
    #[serde(rename = "nf")]
    pub node_fingerprint: Blob<48>,
    #[serde(rename = "cid")]
    pub controller_node_id: Address,
    #[serde(skip_serializing_if = "Vec::is_empty")]
    #[serde(default)]
    #[serde(rename = "md")]
    pub metadata: Vec<u8>,
    #[serde(rename = "ts")]
    pub timestamp: i64,
    #[serde(rename = "s")]
    pub source_remote_endpoint: Endpoint,
    #[serde(rename = "sh")]
    pub source_hops: u8,
    #[serde(rename = "r")]
    pub result: AuthorizationResult,
}

impl ToString for RequestLogItem {
    fn to_string(&self) -> String {
        format!(
            "{} {} {} ts={} s={},{} {}",
            self.controller_node_id.to_string(),
            self.network_id.to_string(),
            self.node_id.to_string(),
            self.timestamp,
            self.source_remote_endpoint.to_string(),
            self.source_hops,
            self.result.to_string()
        )
    }
}
