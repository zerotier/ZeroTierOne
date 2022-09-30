use std::io::Write;

use crate::vl1::identity;
use crate::vl1::identity::Identity;
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
    pub signature: ArrayVec<u8, { identity::IDENTITY_MAX_SIGNATURE_SIZE }>,
}

impl Tag {
    fn internal_v1_proto_to_bytes(&self, for_sign: bool) -> Option<Vec<u8>> {
        if self.signature.len() == 96 {
            let mut v = Vec::with_capacity(256);
            if for_sign {
                let _ = v.write_all(&[0x7f; 8]);
            }
            let _ = v.write_all(&self.network_id.to_bytes());
            let _ = v.write_all(&self.timestamp.to_be_bytes());
            let _ = v.write_all(&self.id.to_be_bytes());
            let _ = v.write_all(&self.value.to_be_bytes());
            let _ = v.write_all(&self.issued_to.to_bytes());
            let _ = v.write_all(&self.signed_by.to_bytes());
            if !for_sign {
                v.push(1);
                v.push(0);
                v.push(96); // size of legacy signatures, 16-bit
                let _ = v.write_all(self.signature.as_bytes());
            }
            v.push(0);
            v.push(0);
            if for_sign {
                let _ = v.write_all(&[0x7f; 8]);
            }
            return Some(v);
        }
        return None;
    }

    #[inline(always)]
    pub fn v1_proto_to_bytes(&self) -> Option<Vec<u8>> {
        self.internal_v1_proto_to_bytes(false)
    }

    pub fn v1_proto_sign(&mut self, issuer: &Identity, issued_to: &Identity) -> bool {
        self.issued_to = issued_to.address;
        self.signed_by = issuer.address;
        if let Some(to_sign) = self.internal_v1_proto_to_bytes(true) {
            if let Some(signature) = issuer.sign(&to_sign.as_slice(), true) {
                self.signature = signature;
                return true;
            }
        }
        return false;
    }
}
