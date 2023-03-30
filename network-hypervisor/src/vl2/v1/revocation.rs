use std::io::Write;

use zerotier_utils::arrayvec::ArrayVec;

use serde::{Deserialize, Serialize};

use crate::vl1::identity::IdentitySecret;
use crate::vl1::Address;
use crate::vl2::v1::CredentialType;
use crate::vl2::NetworkId;

/// "Anti-credential" revoking a network member's permission to communicate on a network.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct Revocation {
    pub network_id: u64, // legacy 64-bit network ID
    pub threshold: i64,
    pub target: u64,    // legacy 40-bit address
    pub issued_to: u64, // legacy 40-bit address
    pub signature: ArrayVec<u8, 96>,
    pub fast_propagate: bool,
}

impl Revocation {
    pub fn new(network_id: &NetworkId, threshold: i64, target: &Address, issued_to: &Address, signer: &IdentitySecret, fast_propagate: bool) -> Self {
        let mut r = Self {
            network_id: network_id.to_legacy_u64(),
            threshold,
            target: target.legacy_u64(),
            issued_to: issued_to.legacy_u64(),
            signature: ArrayVec::new(),
            fast_propagate,
        };
        r.signature = signer.sign(r.internal_to_bytes(true, &signer.public.address).as_bytes());
        r
    }

    fn internal_to_bytes(&self, for_sign: bool, signed_by: &Address) -> ArrayVec<u8, 256> {
        let mut v = ArrayVec::new();
        if for_sign {
            let _ = v.write_all(&[0x7f; 8]);
        }

        let _ = v.write_all(&[0; 4]);
        let _ = v.write_all(&((self.threshold as u32) ^ (self.target as u32)).to_be_bytes()); // ID is arbitrary
        let _ = v.write_all(&self.network_id.to_be_bytes());
        let _ = v.write_all(&[0; 8]);
        let _ = v.write_all(&self.threshold.to_be_bytes());
        let _ = v.write_all(&(self.fast_propagate as u64).to_be_bytes()); // 0x1 is the flag for this
        let _ = v.write_all(&self.target.to_be_bytes()[3..8]);
        let _ = v.write_all(signed_by.as_bytes());
        v.push(CredentialType::CertificateOfMembership as u8);

        if for_sign {
            let _ = v.write_all(&[0x7f; 8]);
        } else {
            v.push(1); // ed25519 signature
            assert!(self.signature.len() <= 255);
            let _ = v.write_all(&[0u8, self.signature.len() as u8]);
            let _ = v.write_all(self.signature.as_ref());
        }

        v
    }

    #[inline(always)]
    pub fn v1_proto_to_bytes(&self, controller_address: &Address) -> ArrayVec<u8, 256> {
        self.internal_to_bytes(false, controller_address)
    }
}
