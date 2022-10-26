use zerotier_utils::arrayvec::ArrayVec;

use serde::{Deserialize, Serialize};

use crate::vl1::{Address, Identity};
use crate::vl2::NetworkId;

#[derive(Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct Revocation {
    pub network_id: NetworkId,
    pub threshold: i64,
    pub issued_to: Address,
    pub signature: ArrayVec<u8, { Identity::MAX_SIGNATURE_SIZE }>,
    pub version: u8,
}
