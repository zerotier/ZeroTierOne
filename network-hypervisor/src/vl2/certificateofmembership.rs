use std::io::Write;

use crate::vl1::identity;
use crate::vl1::identity::Identity;
use crate::vl1::Address;
use crate::vl2::NetworkId;

use serde::{Deserialize, Serialize};

use zerotier_crypto::hash::SHA384;
use zerotier_utils::arrayvec::ArrayVec;
use zerotier_utils::blob::Blob;
use zerotier_utils::memory;

#[derive(Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct CertificateOfMembership {
    pub network_id: NetworkId,
    pub issued_to: Address,
    pub timestamp: i64,
    pub max_delta: i64,
    pub issued_to_fingerprint: Blob<48>,
    pub v1: Option<[u8; 32]>,
    pub signed_by: Address,
    pub signature: ArrayVec<u8, { identity::IDENTITY_MAX_SIGNATURE_SIZE }>,
}

impl CertificateOfMembership {
    /// Generate the first three "qualifiers" for V1 nodes.
    fn v1_proto_write_first_3_qualifiers(&self, q: &mut [u64]) {
        q[0] = 0;
        q[1] = self.timestamp.to_be() as u64;
        q[2] = self.max_delta.to_be() as u64;
        q[3] = 1u64.to_be();
        let nwid: u64 = self.network_id.into();
        q[4] = nwid.to_be();
        q[5] = 0;
        q[6] = 2u64.to_be();
        let a: u64 = self.issued_to.into();
        q[7] = a.to_be();
        q[8] = 0xffffffffffffffffu64; // no to_be needed
    }

    /// Generate all the qualifiers for V1 nodes, which is part of marshaling for those.
    fn v1_proto_get_qualifier_bytes(&self) -> Option<[u8; 168]> {
        self.v1.as_ref().map(|v1| {
            let mut q = [0u64; 21];

            self.v1_proto_write_first_3_qualifiers(&mut q);

            q[9] = 3;
            q[10] = u64::from_ne_bytes(v1[0..8].try_into().unwrap());
            q[11] = 0xffffffffffffffffu64;
            q[12] = 4;
            q[13] = u64::from_ne_bytes(v1[8..16].try_into().unwrap());
            q[14] = 0xffffffffffffffffu64;
            q[15] = 5;
            q[16] = u64::from_ne_bytes(v1[16..24].try_into().unwrap());
            q[17] = 0xffffffffffffffffu64;
            q[18] = 6;
            q[19] = u64::from_ne_bytes(v1[24..32].try_into().unwrap());
            q[20] = 0xffffffffffffffffu64;

            *memory::as_byte_array(&q)
        })
    }

    /// Sign this certificate of membership for use by V1 nodes.
    ///
    /// This should be used in conjunction with v1_proto_to_bytes() to generate a COM for v1
    /// nodes. This sets the issued_to and v1 fields.
    pub fn v1_proto_sign(&mut self, issuer: &Identity, issued_to: &Identity) -> bool {
        let mut v1_signee_hasher = SHA384::new();
        v1_signee_hasher.update(&issued_to.address.to_bytes());
        v1_signee_hasher.update(&issued_to.x25519);
        v1_signee_hasher.update(&issued_to.ed25519);
        let v1_signee_hash = v1_signee_hasher.finish();

        let mut to_sign = [0u64; 9];
        self.v1_proto_write_first_3_qualifiers(&mut to_sign);
        if let Some(signature) = issuer.sign(memory::as_byte_array::<[u64; 9], 27>(&to_sign), true) {
            self.issued_to = issued_to.address;
            self.v1 = Some(v1_signee_hash[..32].try_into().unwrap());
            self.signed_by = issuer.address;
            self.signature = signature;
            true
        } else {
            false
        }
    }

    /// Get this certificate of membership encoded in the format expected by old V1 nodes.
    pub fn v1_proto_to_bytes(&self) -> Option<Vec<u8>> {
        if self.signature.is_empty() || self.v1.is_none() {
            return None;
        }
        let mut v: Vec<u8> = Vec::with_capacity(384);
        v.push(1);
        v.push(0);
        v.push(7); // 7 qualifiers, big-endian 16-bit
        let _ = v.write_all(&self.v1_proto_get_qualifier_bytes().unwrap());
        let _ = v.write_all(&self.signed_by.to_bytes());
        let _ = v.write_all(self.signature.as_bytes());
        return Some(v);
    }
}
