use crate::vl1::InetAddress;
use serde::{Deserialize, Serialize};

/// ZeroTier-managed L3 route on a virtual network.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct IpRoute {
    pub target: InetAddress,
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(default)]
    pub via: Option<InetAddress>,
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(default)]
    pub flags: Option<u16>,
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(default)]
    pub metric: Option<u16>,
}
