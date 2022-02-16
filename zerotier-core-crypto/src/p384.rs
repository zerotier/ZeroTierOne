/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::convert::TryInto;
use std::os::raw::{c_int, c_ulong, c_void};
use std::ptr::{null, write_volatile};

use lazy_static::lazy_static;
use openssl::bn::{BigNum, BigNumContext};
use openssl::ec::{EcKey, EcPoint, EcPointRef, PointConversionForm};
use openssl::ecdsa::EcdsaSig;
use openssl::nid::Nid;
use openssl::pkey::{Private, Public};
use foreign_types::{ForeignType, ForeignTypeRef};

use crate::hash::SHA384;
use crate::secret::Secret;

pub const P384_PUBLIC_KEY_SIZE: usize = 49;
pub const P384_SECRET_KEY_SIZE: usize = 48;
pub const P384_ECDSA_SIGNATURE_SIZE: usize = 96;
pub const P384_ECDH_SHARED_SECRET_SIZE: usize = 48;

//#[link(name="crypto")]
extern "C" {
    fn ECDH_compute_key(out: *mut c_void, outlen: c_ulong, pub_key: *mut c_void, ecdh: *mut c_void, kdf: *const c_void) -> c_int;
}

lazy_static! {
    static ref GROUP_P384: openssl::ec::EcGroup = openssl::ec::EcGroup::from_curve_name(Nid::SECP384R1).unwrap();
}

/// A NIST P-384 ECDH/ECDSA public key.
#[derive(Clone)]
pub struct P384PublicKey {
    key: EcKey<Public>,
    bytes: [u8; 49]
}

impl P384PublicKey {
    fn new_from_point(key: &EcPointRef) -> Self {
        let mut bnc = BigNumContext::new().unwrap();
        let kb = key.to_bytes(GROUP_P384.as_ref(), PointConversionForm::COMPRESSED, &mut bnc).unwrap();
        let mut bytes = [0_u8; 49];
        bytes[(49 - kb.len())..].copy_from_slice(kb.as_slice());
        Self {
            key: EcKey::from_public_key(GROUP_P384.as_ref(), key).unwrap(),
            bytes
        }
    }

    pub fn from_bytes(b: &[u8]) -> Option<P384PublicKey> {
        if b.len() == 49 {
            let mut bnc = BigNumContext::new().unwrap();
            let key = EcPoint::from_bytes(GROUP_P384.as_ref(), b, &mut bnc);
            if key.is_ok() {
                let key = key.unwrap();
                if key.is_on_curve(GROUP_P384.as_ref(), &mut bnc).unwrap_or(false) {
                    let key = EcKey::from_public_key(GROUP_P384.as_ref(), key.as_ref());
                    if key.is_ok() {
                        return Some(Self {
                            key: key.unwrap(),
                            bytes: b.try_into().unwrap()
                        });
                    }
                }
            }
        }
        return None;
    }

    pub fn verify(&self, msg: &[u8], signature: &[u8]) -> bool {
        if signature.len() == 96 {
            let r = BigNum::from_slice(&signature[0..48]);
            let s = BigNum::from_slice(&signature[48..96]);
            if r.is_ok() && s.is_ok() {
                let sig = EcdsaSig::from_private_components(r.unwrap(), s.unwrap());
                if sig.is_ok() {
                    return sig.unwrap().verify(&SHA384::hash(msg), self.key.as_ref()).unwrap_or(false);
                }
            }
        }
        return false;
    }

    #[inline(always)]
    pub fn as_bytes(&self) -> &[u8; 49] { &self.bytes }
}

impl PartialEq for P384PublicKey {
    #[inline(always)]
    fn eq(&self, other: &Self) -> bool { self.bytes == other.bytes }
}

unsafe impl Send for P384PublicKey {}

unsafe impl Sync for P384PublicKey {}

/// A NIST P-384 ECDH/ECDSA public/private key pair.
#[derive(Clone)]
pub struct P384KeyPair {
    pair: EcKey<Private>,
    public: P384PublicKey,
}

impl P384KeyPair {
    pub fn generate() -> P384KeyPair {
        let pair = EcKey::generate(GROUP_P384.as_ref()).unwrap(); // failure implies a serious problem
        assert!(pair.check_key().is_ok()); // also would imply a serious problem
        let public = P384PublicKey::new_from_point(pair.public_key());
        Self {
            pair,
            public,
        }
    }

    pub fn from_bytes(public_bytes: &[u8], secret_bytes: &[u8]) -> Option<P384KeyPair> {
        if public_bytes.len() == 49 && secret_bytes.len() == 48 {
            P384PublicKey::from_bytes(public_bytes).map_or(None, |public| {
                BigNum::from_slice(secret_bytes).map_or(None, |private| {
                    let pair = EcKey::from_private_components(GROUP_P384.as_ref(), private.as_ref(), public.key.public_key());
                    if pair.is_ok() {
                        let pair = pair.unwrap();
                        if pair.check_key().is_ok() {
                            Some(Self {
                                pair,
                                public
                            })
                        } else {
                            None
                        }
                    } else {
                        None
                    }
                })
            })
        } else {
            None
        }
    }

    #[inline(always)]
    pub fn public_key(&self) -> &P384PublicKey { &self.public }

    #[inline(always)]
    pub fn public_key_bytes(&self) -> &[u8; P384_PUBLIC_KEY_SIZE] { &self.public.bytes }

    pub fn secret_key_bytes(&self) -> Secret<P384_SECRET_KEY_SIZE> {
        let mut tmp: Secret<P384_SECRET_KEY_SIZE> = Secret::default();
        let mut k = self.pair.private_key().to_vec();
        tmp.0[(48 - k.len())..].copy_from_slice(k.as_slice());
        unsafe {
            // Force zero memory occupied by temporary vector before releasing.
            let kp = k.as_mut_ptr();
            for i in 0..k.len() {
                write_volatile(kp.add(i), 0);
            }
        }
        tmp
    }

    /// Sign a message with ECDSA/SHA384.
    pub fn sign(&self, msg: &[u8]) -> [u8; P384_ECDSA_SIGNATURE_SIZE] {
        let sig = EcdsaSig::sign(&SHA384::hash(msg), self.pair.as_ref()).unwrap();
        let r = sig.r().to_vec();
        let s = sig.s().to_vec();
        assert!(!r.is_empty() && !s.is_empty() && r.len() <= 48 && s.len() <= 48);
        let mut b = [0_u8; P384_ECDSA_SIGNATURE_SIZE];
        b[(48 - r.len())..48].copy_from_slice(r.as_slice());
        b[(96 - s.len())..96].copy_from_slice(s.as_slice());
        b
    }

    /// Perform ECDH key agreement, returning the raw (un-hashed!) ECDH secret.
    pub fn agree(&self, other_public: &P384PublicKey) -> Option<Secret<P384_ECDH_SHARED_SECRET_SIZE>> {
        unsafe {
            let mut s: Secret<P384_ECDH_SHARED_SECRET_SIZE> = Secret::default();
            if ECDH_compute_key(s.0.as_mut_ptr().cast(), 48, other_public.key.public_key().as_ptr().cast(), self.pair.as_ptr().cast(), null()) == 48 {
                Some(s)
            } else {
                None
            }
        }
    }
}

impl PartialEq for P384KeyPair {
    #[inline(always)]
    fn eq(&self, other: &Self) -> bool {
        self.pair.private_key().eq(other.pair.private_key()) && self.public.bytes.eq(&other.public.bytes)
    }
}

impl Eq for P384KeyPair {}

unsafe impl Send for P384KeyPair {}

unsafe impl Sync for P384KeyPair {}

#[cfg(test)]
mod tests {
    use crate::p384::P384KeyPair;

    #[test]
    fn generate_sign_verify_agree() {
        let kp = P384KeyPair::generate();
        let kp2 = P384KeyPair::generate();

        let sig = kp.sign(&[0_u8; 16]);
        if !kp.public_key().verify(&[0_u8; 16], &sig) {
            panic!("ECDSA verify failed");
        }
        if kp.public_key().verify(&[1_u8; 16], &sig) {
            panic!("ECDSA verify succeeded for incorrect message");
        }

        let sec0 = kp.agree(kp2.public_key()).unwrap();
        let sec1 = kp2.agree(kp.public_key()).unwrap();
        if !sec0.eq(&sec1) {
            panic!("ECDH secrets do not match");
        }

        let kp3 = P384KeyPair::from_bytes(kp.public_key_bytes(), kp.secret_key_bytes().as_ref()).unwrap();
        let sig = kp3.sign(&[3_u8; 16]);
        if !kp.public_key().verify(&[3_u8; 16], &sig) {
            panic!("ECDSA verify failed (from key reconstructed from bytes)");
        }
    }
}
