use crate::vl1::identity;
use crate::vl1::Address;
use crate::vl2::NetworkId;

use serde::{Deserialize, Serialize};

use zerotier_utils::arrayvec::ArrayVec;

#[derive(Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct Tag {
    pub id: u32,
    pub value: u32,
    pub network_id: NetworkId,
    pub timestamp: i64,
    pub issued_to: Address,
    pub signed_by: Address,
    //pub signature: ArrayVec<u8, { identity::MAX_SIGNATURE_SIZE }>,
}
