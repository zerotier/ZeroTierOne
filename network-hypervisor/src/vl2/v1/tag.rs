use std::io::Write;

use crate::vl1::identity::{Identity, IdentitySecret};
use crate::vl1::{Address, PartialAddress};
use crate::vl2::NetworkId;

use serde::{Deserialize, Serialize};

use zerotier_utils::arrayvec::ArrayVec;
use zerotier_utils::blob::Blob;
use zerotier_utils::error::InvalidParameterError;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct Tag {
    pub network_id: u64, // legacy 64-bit network ID
    pub timestamp: i64,
    pub issued_to: u64, // legacy 40-bit address
    pub id: u32,
    pub value: u32,
    pub signature: Blob<96>,
}

impl Tag {
    pub fn new(
        id: u32,
        value: u32,
        issuer_address: &Address,
        issuer: &IdentitySecret,
        network_id: &NetworkId,
        issued_to: &Identity,
        timestamp: i64,
    ) -> Self {
        let mut tag = Self {
            network_id: network_id.to_legacy_u64(),
            timestamp,
            issued_to: issued_to.address.legacy_u64(),
            id,
            value,
            signature: Blob::default(),
        };
        let to_sign = tag.internal_to_bytes(true, issuer_address);
        tag.signature.as_mut().copy_from_slice(issuer.sign(to_sign.as_ref()).as_bytes());
        tag
    }

    fn internal_to_bytes(&self, for_sign: bool, signed_by: &Address) -> ArrayVec<u8, 256> {
        let mut v = ArrayVec::new();
        if for_sign {
            let _ = v.write_all(&[0x7f; 8]);
        }
        let _ = v.write_all(&self.network_id.to_be_bytes());
        let _ = v.write_all(&self.timestamp.to_be_bytes());
        let _ = v.write_all(&self.id.to_be_bytes());
        let _ = v.write_all(&self.value.to_be_bytes());
        let _ = v.write_all(&self.issued_to.to_be_bytes()[3..8]);
        let _ = v.write_all(signed_by.legacy_bytes());
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
        v
    }

    #[inline(always)]
    pub fn to_bytes(&self, signed_by: &Address) -> ArrayVec<u8, 256> {
        self.internal_to_bytes(false, signed_by)
    }

    pub fn from_bytes(b: &[u8]) -> Result<(Self, &[u8]), InvalidParameterError> {
        const LEN: usize = 8 + 8 + 4 + 4 + 5 + 5 + 3 + 96 + 2;
        if b.len() < LEN {
            return Err(InvalidParameterError("incomplete"));
        }
        Ok((
            Self {
                network_id: NetworkId::from_bytes(&b[0..8])?.to_legacy_u64(),
                timestamp: i64::from_be_bytes(b[8..16].try_into().unwrap()),
                issued_to: PartialAddress::from_bytes(&b[24..29])?.legacy_u64(),
                id: u32::from_be_bytes(b[16..20].try_into().unwrap()),
                value: u32::from_be_bytes(b[20..24].try_into().unwrap()),
                signature: {
                    let mut s = Blob::default();
                    s.as_mut().copy_from_slice(&b[37..133]);
                    s
                },
            },
            &b[LEN..],
        ))
    }
}
