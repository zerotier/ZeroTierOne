use std::io::Write;

use crate::vl1::identity::Identity;
use crate::vl1::Address;
use crate::vl2::NetworkId;

use serde::{Deserialize, Serialize};

use zerotier_crypto::hash::SHA384;
use zerotier_crypto::secure_eq;
use zerotier_crypto::typestate::Valid;
use zerotier_utils::arrayvec::ArrayVec;
use zerotier_utils::blob::Blob;
use zerotier_utils::error::InvalidParameterError;
use zerotier_utils::memory;

/// ZeroTier V1 certificate of membership.
///
/// The somewhat odd encoding of this is an artifact of an old V1 design choice: certificates are
/// tuples of arbitrary values coupled by how different they are permitted to be (max delta).
///
/// This was done to permit some things such as geo-fencing that were never implemented, so it's
/// a bit of a case of YAGNI. In V2 this is deprecated in favor of a more standard sort of
/// certificate.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct CertificateOfMembership {
    pub network_id: NetworkId,
    pub timestamp: i64,
    pub max_delta: u64,
    pub issued_to: Address,
    pub issued_to_fingerprint: Blob<32>,
    pub signature: ArrayVec<u8, { Identity::MAX_SIGNATURE_SIZE }>,
}

impl CertificateOfMembership {
    /// Create a new signed certificate of membership.
    /// None is returned if an error occurs, such as the issuer missing its secrets.
    pub fn new(issuer: &Identity, network_id: NetworkId, issued_to: &Identity, timestamp: i64, max_delta: u64) -> Option<Self> {
        let mut com = CertificateOfMembership {
            network_id,
            timestamp,
            max_delta,
            issued_to: issued_to.address,
            issued_to_fingerprint: Blob::default(),
            signature: ArrayVec::new(),
        };

        com.issued_to_fingerprint = Blob::from(Self::v1_proto_issued_to_fingerprint(issued_to));
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
        q[2] = self.max_delta.to_be() as u64; // TTL / "window" in V1
        q[3] = 1u64.to_be();
        q[4] = u64::from(self.network_id).to_be();
        q[5] = 0; // no disagreement permitted
        q[6] = 2u64.to_be();
        q[7] = u64::from(self.issued_to).to_be();
        q[8] = u64::MAX; // no to_be needed for all-1s

        // This is a fix for a security issue in V1 in which an attacker could (with much CPU use)
        // duplicate an identity and insert themselves in place of one after 30-60 days when local
        // identity caches expire. The full hash should have been included from the beginning, and
        // V2 only ever uses the full hash of the identity to verify credentials.
        let fp = self.issued_to_fingerprint.as_bytes();
        q[9] = 3;
        q[10] = u64::from_ne_bytes(fp[0..8].try_into().unwrap());
        q[11] = u64::MAX; // these will never agree; they're explicitly checked in V1
        q[12] = 4;
        q[13] = u64::from_ne_bytes(fp[8..16].try_into().unwrap());
        q[14] = u64::MAX;
        q[15] = 5;
        q[16] = u64::from_ne_bytes(fp[16..24].try_into().unwrap());
        q[17] = u64::MAX;
        q[18] = 6;
        q[19] = u64::from_ne_bytes(fp[24..32].try_into().unwrap());
        q[20] = u64::MAX;

        memory::to_byte_array(q)
    }

    /// Get the identity fingerprint used in V1, which only covers the curve25519 keys.
    fn v1_proto_issued_to_fingerprint(issued_to: &Identity) -> [u8; 32] {
        let mut v1_signee_hasher = SHA384::new();
        v1_signee_hasher.update(&issued_to.address.to_bytes());
        v1_signee_hasher.update(&issued_to.x25519);
        v1_signee_hasher.update(&issued_to.ed25519);
        (&v1_signee_hasher.finish()[..32]).try_into().unwrap()
    }

    /// Get this certificate of membership in byte encoded format.
    pub fn to_bytes(&self, controller_address: Address) -> ArrayVec<u8, 384> {
        let mut v = ArrayVec::new();
        v.push(1); // version byte from v1 protocol
        v.push(0);
        v.push(7); // 7 qualifiers, big-endian 16-bit
        let _ = v.write_all(&self.v1_proto_get_qualifier_bytes());
        let _ = v.write_all(&controller_address.to_bytes());
        let _ = v.write_all(self.signature.as_bytes());
        v
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

        let (mut network_id, mut issued_to, mut timestamp, mut max_delta, mut v1_fingerprint) = (0, 0, 0, 0, [0u8; 32]);
        for _ in 0..qualifier_count {
            let qt = u64::from_be_bytes(b[..8].try_into().unwrap());
            let q: [u8; 8] = b[8..16].try_into().unwrap();
            let qd = u64::from_be_bytes(b[16..24].try_into().unwrap());
            match qt {
                0 => {
                    timestamp = i64::from_be_bytes(q);
                    max_delta = qd;
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

        b = &b[5..]; // skip issuer address which is always the controller

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
    pub fn verify(self, issuer: &Identity, expect_issued_to: &Identity) -> Option<Valid<Self>> {
        if secure_eq(
            &Self::v1_proto_issued_to_fingerprint(expect_issued_to),
            &self.issued_to_fingerprint.as_bytes()[..32],
        ) {
            if issuer.verify(&self.v1_proto_get_qualifier_bytes(), self.signature.as_bytes()) {
                return Some(Valid::assume_verified(self));
            }
        }
        return None;
    }
}
