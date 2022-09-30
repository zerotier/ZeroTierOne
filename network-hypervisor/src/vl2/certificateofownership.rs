use std::io::Write;

use crate::vl1::{Address, Identity, InetAddress, MAC};
use crate::vl2::NetworkId;

use serde::{Deserialize, Serialize};

use zerotier_utils::arrayvec::ArrayVec;

#[derive(Clone, Serialize, Deserialize, PartialEq, Eq)]
pub enum Thing {
    Ip(InetAddress),
    Mac(MAC),
}

#[derive(Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct CertificateOfOwnership {
    pub network_id: NetworkId,
    pub timestamp: i64,
    pub flags: u64,
    pub id: u32,
    pub things: Vec<Thing>,
    pub issued_to: Address,
    pub signed_by: Address,
    pub signature: ArrayVec<u8, { crate::vl1::identity::IDENTITY_MAX_SIGNATURE_SIZE }>,
}

impl CertificateOfOwnership {
    fn internal_v1_proto_to_bytes(&self, for_sign: bool) -> Option<Vec<u8>> {
        if self.things.len() > 0xffff || self.signature.len() != 96 {
            return None;
        }
        let mut v = Vec::with_capacity(256);
        if for_sign {
            let _ = v.write_all(&[0x7fu8; 8]);
        }
        let _ = v.write_all(&self.network_id.to_bytes());
        let _ = v.write_all(&self.timestamp.to_be_bytes());
        let _ = v.write_all(&self.flags.to_be_bytes());
        let _ = v.write_all(&self.id.to_be_bytes());
        let _ = v.write_all(&(self.things.len() as u16).to_be_bytes());
        for t in self.things.iter() {
            match t {
                Thing::Ip(ip) => {
                    if ip.is_ipv4() {
                        v.push(2);
                        let mut tmp = [0u8; 16];
                        tmp[..4].copy_from_slice(&ip.ip_bytes());
                        let _ = v.write_all(&tmp);
                    } else if ip.is_ipv6() {
                        v.push(3);
                        let _ = v.write_all(ip.ip_bytes());
                    } else {
                        return None;
                    }
                }
                Thing::Mac(m) => {
                    v.push(1);
                    let mut tmp = [0u8; 16];
                    tmp[..6].copy_from_slice(&m.to_bytes());
                    let _ = v.write_all(&tmp);
                }
            }
        }
        let _ = v.write_all(&self.issued_to.to_bytes());
        let _ = v.write_all(&self.signed_by.to_bytes());
        if for_sign {
            v.push(0);
            v.push(0);
            let _ = v.write_all(&[0x7fu8; 8]);
        } else {
            v.push(1);
            v.push(0);
            v.push(96); // size of legacy signature, 16 bits
            let _ = v.write_all(self.signature.as_bytes());
            v.push(0);
            v.push(0);
        }
        return Some(v);
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
