use crate::vl1::identity;
use crate::vl1::Address;
use crate::vl2::NetworkId;

use serde::{Deserialize, Serialize};

use zerotier_utils::arrayvec::ArrayVec;
use zerotier_utils::blob::Blob;

#[derive(Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct CertificateOfMembership {
    pub issued_to: Address,
    pub issued_to_fingerprint: Blob<48>,
    pub network_id: NetworkId,
    pub timestamp: i64,
    pub max_delta: i64,
    pub signature: ArrayVec<u8, { identity::MAX_SIGNATURE_SIZE }>,
}
