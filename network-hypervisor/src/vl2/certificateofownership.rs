use crate::vl2::NetworkId;

use serde::{Deserialize, Serialize};

#[derive(Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct CertificateOfOwnership {
    pub network_id: NetworkId,
    pub timestamp: i64,
    pub flags: u64,
    pub id: u32,
    // TODO
}
