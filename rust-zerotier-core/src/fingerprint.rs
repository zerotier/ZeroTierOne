use crate::*;
use crate::bindings::capi as ztcore;

pub struct Fingerprint {
    pub address: Address,
    pub hash: [u8; 48]
}

impl Fingerprint {
    fn new_from_capi_fingerprint(cfp: *const ztcore::ZT_Fingerprint) -> Fingerprint {
        unsafe {
            return Fingerprint {
                address: Address((*cfp).address),
                hash: (*cfp).hash
            }
        }
    }

    fn to_capi_fingerprint(&self) -> ztcore::ZT_Fingerprint {
        ztcore::ZT_Fingerprint {
            address: self.address.to_u64(),
            hash: self.hash
        }
    }
}
