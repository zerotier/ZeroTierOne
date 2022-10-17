use std::io::Write;

use crate::vl1::identity;
use crate::vl1::identity::Identity;
use crate::vl1::Address;
use crate::vl2::NetworkId;

use serde::{Deserialize, Serialize};

use zerotier_utils::arrayvec::ArrayVec;
use zerotier_utils::error::InvalidParameterError;

#[derive(Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct Tag {
    pub network_id: NetworkId,
    pub timestamp: i64,
    pub issued_to: Address,
    pub id: u32,
    pub value: u32,
    pub signature: ArrayVec<u8, { identity::IDENTITY_MAX_SIGNATURE_SIZE }>,
    pub version: u8,
}

impl Tag {
    pub fn new(
        id: u32,
        value: u32,
        issuer: &Identity,
        network_id: NetworkId,
        issued_to: &Identity,
        timestamp: i64,
        legacy_v1: bool,
    ) -> Option<Self> {
        let mut tag = Self {
            network_id,
            timestamp,
            issued_to: issued_to.address,
            id,
            value,
            signature: ArrayVec::new(),
            version: if legacy_v1 {
                1
            } else {
                2
            },
        };
        if legacy_v1 {
            let to_sign = tag.internal_v1_proto_to_bytes(true, issuer.address)?;
            if let Some(signature) = issuer.sign(to_sign.as_slice(), true) {
                tag.signature = signature;
                return Some(tag);
            }
        } else {
            todo!()
        }
        return None;
    }

    fn internal_v1_proto_to_bytes(&self, for_sign: bool, signed_by: Address) -> Option<Vec<u8>> {
        if self.version == 1 && self.signature.len() == 96 {
            let mut v = Vec::with_capacity(256);
            if for_sign {
                let _ = v.write_all(&[0x7f; 8]);
            }
            let _ = v.write_all(&self.network_id.to_bytes());
            let _ = v.write_all(&self.timestamp.to_be_bytes());
            let _ = v.write_all(&self.id.to_be_bytes());
            let _ = v.write_all(&self.value.to_be_bytes());
            let _ = v.write_all(&self.issued_to.to_bytes());
            let _ = v.write_all(&signed_by.to_bytes());
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
    pub fn v1_proto_to_bytes(&self, signed_by: Address) -> Option<Vec<u8>> {
        self.internal_v1_proto_to_bytes(false, signed_by)
    }

    pub fn v1_proto_sign(&mut self, issuer: &Identity, issued_to: &Identity) -> bool {
        self.issued_to = issued_to.address;
        if let Some(to_sign) = self.internal_v1_proto_to_bytes(true, issuer.address) {
            if let Some(signature) = issuer.sign(&to_sign.as_slice(), true) {
                self.signature = signature;
                return true;
            }
        }
        return false;
    }

    pub fn v1_proto_from_bytes(b: &[u8]) -> Result<(Self, &[u8]), InvalidParameterError> {
        const LEN: usize = 8 + 8 + 4 + 4 + 5 + 5 + 3 + 96 + 2;
        if b.len() < LEN {
            return Err(InvalidParameterError("incomplete"));
        }
        Ok((
            Self {
                network_id: NetworkId::from_bytes(&b[0..8]).ok_or(InvalidParameterError("invalid network ID"))?,
                timestamp: i64::from_be_bytes(b[8..16].try_into().unwrap()),
                issued_to: Address::from_bytes(&b[24..29]).ok_or(InvalidParameterError("invalid address"))?,
                id: u32::from_be_bytes(b[16..20].try_into().unwrap()),
                value: u32::from_be_bytes(b[20..24].try_into().unwrap()),
                signature: {
                    let mut s = ArrayVec::new();
                    s.push_slice(&b[37..133]);
                    s
                },
                version: 1,
            },
            &b[LEN..],
        ))
    }
}
