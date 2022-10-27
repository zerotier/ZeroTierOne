use std::io::Write;

use crate::vl1::identity::Identity;
use crate::vl1::Address;
use crate::vl2::NetworkId;

use serde::{Deserialize, Serialize};

use zerotier_crypto::hash::SHA384;
use zerotier_crypto::verified::Verified;
use zerotier_utils::arrayvec::ArrayVec;
use zerotier_utils::blob::Blob;
use zerotier_utils::error::InvalidParameterError;
use zerotier_utils::memory;

#[derive(Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct CertificateOfMembership {
    pub network_id: NetworkId,
    pub timestamp: i64,
    pub max_delta: i64,
    pub issued_to: Address,
    pub issued_to_fingerprint: Blob<{ Identity::FINGERPRINT_SIZE }>,
    pub signature: ArrayVec<u8, { Identity::MAX_SIGNATURE_SIZE }>,
}

impl CertificateOfMembership {
    /// Create a new signed certificate of membership.
    /// None is returned if an error occurs, such as the issuer missing its secrets.
    pub fn new(issuer: &Identity, network_id: NetworkId, issued_to: &Identity, timestamp: i64, max_delta: i64) -> Option<Self> {
        let mut com = CertificateOfMembership {
            network_id,
            timestamp,
            max_delta,
            issued_to: issued_to.address,
            issued_to_fingerprint: Blob::default(),
            signature: ArrayVec::new(),
        };

        com.issued_to_fingerprint = Blob::from(Self::v1_proto_issued_to_fingerprint(issued_to, Some(issuer.address)));
        if let Some(signature) = issuer.sign(&com.v1_proto_get_qualifier_bytes(), true) {
            com.signature = signature;
            Some(com)
        } else {
            None
        }
    }

    fn v1_proto_get_qualifier_bytes(&self) -> [u8; 168] {
        let mut q = [0u64; 21];

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

        let fp = self.issued_to_fingerprint.as_bytes();
        q[9] = 3;
        q[10] = u64::from_ne_bytes(fp[0..8].try_into().unwrap());
        q[11] = 0xffffffffffffffffu64;
        q[12] = 4;
        q[13] = u64::from_ne_bytes(fp[8..16].try_into().unwrap());
        q[14] = 0xffffffffffffffffu64;
        q[15] = 5;
        q[16] = u64::from_ne_bytes(fp[16..24].try_into().unwrap());
        q[17] = 0xffffffffffffffffu64;
        q[18] = 6;
        q[19] = u64::from_ne_bytes(fp[24..32].try_into().unwrap());
        q[20] = 0xffffffffffffffffu64;

        *memory::as_byte_array(&q)
    }

    fn v1_proto_issued_to_fingerprint(issued_to: &Identity, signed_by: Option<Address>) -> [u8; 48] {
        let mut v1_signee_hasher = SHA384::new();
        v1_signee_hasher.update(&issued_to.address.to_bytes());
        v1_signee_hasher.update(&issued_to.x25519);
        v1_signee_hasher.update(&issued_to.ed25519);
        let mut fp = v1_signee_hasher.finish();
        fp[32..].fill(0);
        if let Some(signed_by) = signed_by {
            fp[32..37].copy_from_slice(&signed_by.to_bytes());
        }
        fp
    }

    /// Get this certificate of membership in byte encoded format.
    pub fn to_bytes(&self) -> Option<ArrayVec<u8, 384>> {
        if self.signature.len() == 96 {
            let mut v = ArrayVec::new();
            v.push(1); // version byte from v1 protocol
            v.push(0);
            v.push(7); // 7 qualifiers, big-endian 16-bit
            let _ = v.write_all(&self.v1_proto_get_qualifier_bytes());
            let _ = v.write_all(&self.issued_to_fingerprint.as_bytes()[32..38]); // issuer address
            let _ = v.write_all(self.signature.as_bytes());
            return Some(v);
        }
        return None;
    }

    /// Decode a V1 legacy format certificate of membership in byte format.
    pub fn from_bytes(mut b: &[u8]) -> Result<Self, InvalidParameterError> {
        if b.len() <= 3 || b[0] != 1 {
            return Err(InvalidParameterError("version mismatch"));
        }
        let qualifier_count = (b[1] as usize).wrapping_shl(8) | (b[2] as usize);
        b = &b[3..];

        if b.len() < ((qualifier_count * 24) + (5 + 96)) {
            return Err(InvalidParameterError("incomplete"));
        }

        let (mut network_id, mut issued_to, mut timestamp, mut max_delta, mut v1_fingerprint) = (0, 0, 0, 0, [0u8; 48]);
        for _ in 0..qualifier_count {
            let qt = u64::from_be_bytes(b[..8].try_into().unwrap());
            let q: [u8; 8] = b[8..16].try_into().unwrap();
            let qd = u64::from_be_bytes(b[16..24].try_into().unwrap());
            match qt {
                0 => {
                    timestamp = i64::from_be_bytes(q);
                    max_delta = qd as i64;
                }
                1 => {
                    network_id = u64::from_be_bytes(q);
                }
                2 => {
                    issued_to = u64::from_be_bytes(q);
                }
                3 => {
                    v1_fingerprint[0..8].copy_from_slice(&q);
                }
                4 => {
                    v1_fingerprint[8..16].copy_from_slice(&q);
                }
                5 => {
                    v1_fingerprint[16..24].copy_from_slice(&q);
                }
                6 => {
                    v1_fingerprint[24..32].copy_from_slice(&q);
                }
                _ => {}
            }
            b = &b[24..];
        }

        v1_fingerprint[32..38].copy_from_slice(&b[..5]); // issuer address
        b = &b[5..];

        Ok(Self {
            network_id: NetworkId::from_u64(network_id).ok_or(InvalidParameterError("invalid network ID"))?,
            timestamp,
            max_delta,
            issued_to: Address::from_u64(issued_to).ok_or(InvalidParameterError("invalid issued to address"))?,
            issued_to_fingerprint: Blob::from(v1_fingerprint),
            signature: {
                let mut s = ArrayVec::new();
                s.push_slice(&b[..96]);
                s
            },
        })
    }

    /// Verify this certificate of membership.
    pub fn verify(self, issuer: &Identity, expect_issued_to: &Identity) -> Option<Verified<Self>> {
        if Self::v1_proto_issued_to_fingerprint(expect_issued_to, None).eq(&self.issued_to_fingerprint.as_bytes()[..32]) {
            if issuer.verify(&self.v1_proto_get_qualifier_bytes(), self.signature.as_bytes()) {
                return Some(Verified(self));
            }
        }
        return None;
    }
}
