// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::collections::HashMap;

use serde::{Deserialize, Serialize};

use crate::vl1::{Address, InetAddress};
use crate::vl2::certificateofmembership::CertificateOfMembership;
use crate::vl2::certificateofownership::CertificateOfOwnership;
use crate::vl2::rule::Rule;
use crate::vl2::tag::Tag;

#[allow(unused)]
pub mod dictionary_fields {
    pub mod network_config {
        pub const VERSION: &'static str = "v";
        pub const NETWORK_ID: &'static str = "nwid";
        pub const TIMESTAMP: &'static str = "ts";
        pub const REVISION: &'static str = "r";
        pub const ISSUED_TO: &'static str = "id";
        pub const FLAGS: &'static str = "f";
        pub const MULTICAST_LIMIT: &'static str = "ml";
        pub const TYPE: &'static str = "t";
        pub const NAME: &'static str = "n";
        pub const MOTD: &'static str = "motd";
        pub const MTU: &'static str = "mtu";
        pub const MAX_DELTA: &'static str = "ctmd";
        pub const CERTIFICATE_OF_MEMBERSHIP: &'static str = "C";
        pub const ROUTES: &'static str = "RT";
        pub const STATIC_IPS: &'static str = "I";
        pub const RULES: &'static str = "R";
        pub const TAGS: &'static str = "TAG";
        pub const CERTIFICATES_OF_OWNERSHIP: &'static str = "COO";
        pub const DNS: &'static str = "DNS";
        pub const NODE_INFO: &'static str = "NI";
        pub const CENTRAL_URL: &'static str = "ssoce";
        pub const SSO_ENABLED: &'static str = "ssoe";
        pub const SSO_VERSION: &'static str = "ssov";
        pub const SSO_AUTHENTICATION_URL: &'static str = "aurl";
        pub const SSO_AUTHENTICATION_EXPIRY_TIME: &'static str = "aexpt";
        pub const SSO_ISSUER_URL: &'static str = "iurl";
        pub const SSO_NONCE: &'static str = "sson";
        pub const SSO_STATE: &'static str = "ssos";
        pub const SSO_CLIENT_ID: &'static str = "ssocid";
    }

    pub mod sso_auth_info {
        pub const VERSION: &'static str = "aV";
        pub const AUTHENTICATION_URL: &'static str = "aU";
        pub const ISSUER_URL: &'static str = "iU";
        pub const CENTRAL_URL: &'static str = "aCU";
        pub const NONCE: &'static str = "aN";
        pub const STATE: &'static str = "aS";
        pub const CLIENT_ID: &'static str = "aCID";
    }
}

/// Network configuration object sent to nodes by network controllers.
#[derive(Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct NetworkConfig {
    pub id: u64,
    pub name: String,
    pub motd: String,
    pub issued_to: Address,
    pub private: bool,

    pub timestamp: i64,
    pub max_delta: i64,
    pub revision: u64,

    pub mtu: u32,
    pub multicast_limit: u32,
    pub routes: Vec<IpRoute>,
    pub static_ips: Vec<InetAddress>,
    pub rules: Vec<Rule>,
    pub dns: Vec<Nameserver>,

    pub certificate_of_membership: CertificateOfMembership,
    pub certificates_of_ownership: Vec<CertificateOfOwnership>,
    pub tags: Vec<Tag>,

    pub node_info: HashMap<Address, NodeInfo>,

    pub central_url: String,

    pub sso_enabled: bool,
    pub sso_version: u32,
    pub sso_authentication_url: String,
    pub sso_authentication_expiry_time: i64,
    pub sso_issuer_url: String,
    pub sso_nonce: String,
    pub sso_state: String,
    pub sso_client_id: String,
}

/// Information about nodes on the network that can be included in a network config.
#[derive(Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct NodeInfo {
    pub flags: u64,
    pub ip: Option<InetAddress>,
    pub name: Option<String>,
    pub services: HashMap<String, Option<String>>,
}

/// Statically pushed L3 IP routes included with a network configuration.
#[derive(Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct IpRoute {
    pub target: InetAddress,
    pub via: Option<InetAddress>,
    pub flags: u16,
    pub metric: u16,
}

/// ZeroTier-pushed DNS nameserver configuration.
#[derive(Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct Nameserver {
    pub ip: InetAddress,
    pub domain: String,
}
