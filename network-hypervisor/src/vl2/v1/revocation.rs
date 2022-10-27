use std::io::Write;

use zerotier_crypto::random;
use zerotier_utils::arrayvec::ArrayVec;
use zerotier_utils::blob::Blob;

use serde::{Deserialize, Serialize};

use crate::vl1::{Address, Identity};
use crate::vl2::v1::CredentialType;
use crate::vl2::NetworkId;

#[derive(Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct Revocation {
    pub id: u32,
    pub network_id: NetworkId,
    pub threshold: i64,
    pub target: Address,
    pub issued_to: Address,
    pub signature: Blob<96>,
    pub type_being_revoked: u8,
    pub fast_propagate: bool,
}

impl Revocation {
    pub fn new(
        network_id: NetworkId,
        threshold: i64,
        target: Address,
        issued_to: Address,
        signer: &Identity,
        type_being_revoked: CredentialType,
        fast_propagate: bool,
    ) -> Option<Self> {
        let mut r = Self {
            id: random::xorshift64_random() as u32, // arbitrary
            network_id,
            threshold,
            target,
            issued_to,
            signature: Blob::default(),
            type_being_revoked: type_being_revoked as u8,
            fast_propagate,
        };
        if let Some(sig) = signer.sign(r.internal_to_bytes(true, signer.address).as_bytes(), true) {
            r.signature.as_mut().copy_from_slice(sig.as_bytes());
            Some(r)
        } else {
            None
        }
    }

    fn internal_to_bytes(&self, for_sign: bool, signed_by: Address) -> ArrayVec<u8, 256> {
        let mut v = ArrayVec::new();
        if for_sign {
            let _ = v.write_all(&[0x7f; 8]);
        }

        let _ = v.write_all(&[0; 4]);
        let _ = v.write_all(&self.id.to_be_bytes());
        let _ = v.write_all(&self.network_id.to_bytes());
        let _ = v.write_all(&[0; 8]);
        let _ = v.write_all(&self.threshold.to_be_bytes());
        let _ = v.write_all(&(self.fast_propagate as u64).to_be_bytes()); // 0x1 is the flag for this
        let _ = v.write_all(&self.target.to_bytes());
        let _ = v.write_all(&signed_by.to_bytes());
        v.push(self.type_being_revoked);

        if for_sign {
            let _ = v.write_all(&[0x7f; 8]);
        } else {
            v.push(1); // ed25519 signature
            let _ = v.write_all(&[0u8, 96u8]);
            let _ = v.write_all(self.signature.as_bytes());
        }

        v
    }
}
