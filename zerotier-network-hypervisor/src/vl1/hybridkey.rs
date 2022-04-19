/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::io::Write;

use zerotier_core_crypto::c25519::{C25519KeyPair, C25519_PUBLIC_KEY_SIZE};
use zerotier_core_crypto::hash::{hmac_sha512, SHA512};
use zerotier_core_crypto::p384::{P384KeyPair, P384PublicKey, P384_PUBLIC_KEY_SIZE};
use zerotier_core_crypto::secret::Secret;

pub const ALGORITHM_C25519: u8 = 0x01;
pub const ALGORITHM_ECC_NIST_P384: u8 = 0x02;

/// A bundle of key pairs for multiple algorithms that can be used to execute key agreement.
///
/// These are used in ephemeral session key negotiation.
#[derive(Clone)]
pub struct HybridKeyPair {
    pub c25519: C25519KeyPair,
    pub p384: P384KeyPair,
}

impl HybridKeyPair {
    pub fn generate() -> HybridKeyPair {
        Self {
            c25519: C25519KeyPair::generate(),
            p384: P384KeyPair::generate(),
        }
    }

    pub fn public_bytes(&self) -> Vec<u8> {
        let mut buf: Vec<u8> = Vec::with_capacity(1 + C25519_PUBLIC_KEY_SIZE + P384_PUBLIC_KEY_SIZE);
        buf.push(ALGORITHM_C25519 | ALGORITHM_ECC_NIST_P384);
        let _ = buf.write_all(&self.c25519.public_bytes());
        let _ = buf.write_all(self.p384.public_key_bytes());
        buf
    }

    /// Execute key agreement using all keys in common between this and the other public key.
    ///
    /// If there are FIPS/NIST approved keys present these will be used last in a chain of
    /// HMAC(previous, next) KDF operations, making the final result FIPS-compliant. Non-FIPS
    /// algorithms before it can be considered "salts" input to HKDF for FIPS purposes.
    pub fn agree(&self, other_public: &HybridPublicKey) -> Option<Secret<64>> {
        let mut k: Option<Secret<64>> = None;
        if other_public.c25519.is_some() {
            // k can't have anything in it yet since this is the first checked
            let _ = k.insert(Secret(SHA512::hash(self.c25519.agree(other_public.c25519.as_ref().unwrap()).as_bytes())));
        }
        if other_public.p384.is_some() {
            let p384_secret = self.p384.agree(other_public.p384.as_ref().unwrap());
            if p384_secret.is_none() {
                return None;
            }
            if k.is_some() {
                let prev_k = k.take().unwrap();
                let _ = k.insert(Secret(hmac_sha512(prev_k.as_bytes(), p384_secret.unwrap().as_bytes())));
            } else {
                let _ = k.insert(Secret(SHA512::hash(p384_secret.unwrap().as_bytes())));
            }
        }
        return k;
    }
}

unsafe impl Send for HybridKeyPair {}

/// A public key composed of multiple public keys for multiple algorithms.
///
/// The key pair above currently always uses every algorithm but the protocol permits
/// mix-and-match. At least one must be present though or agreement will fail.
#[derive(Clone)]
pub struct HybridPublicKey {
    pub c25519: Option<[u8; C25519_PUBLIC_KEY_SIZE]>,
    pub p384: Option<P384PublicKey>,
}

impl HybridPublicKey {
    pub fn from_bytes(mut b: &[u8]) -> Option<HybridPublicKey> {
        if !b.is_empty() {
            let mut hpk = Self { c25519: None, p384: None };
            let types = b[0];
            b = &b[1..];
            let mut have = false;
            if (types & ALGORITHM_C25519) != 0 {
                if b.len() < C25519_PUBLIC_KEY_SIZE {
                    return None;
                }
                let _ = hpk.c25519.insert((&b[0..C25519_PUBLIC_KEY_SIZE]).try_into().unwrap());
                b = &b[C25519_PUBLIC_KEY_SIZE..];
                have = true;
            }
            if (types & ALGORITHM_ECC_NIST_P384) != 0 {
                if b.len() < P384_PUBLIC_KEY_SIZE {
                    return None;
                }
                let pk = P384PublicKey::from_bytes(&b[0..P384_PUBLIC_KEY_SIZE]);
                if pk.is_none() {
                    return None;
                }
                let _ = hpk.p384.insert(pk.unwrap());
                b = &b[P384_PUBLIC_KEY_SIZE..];
                have = true;
            }
            if have {
                return Some(hpk);
            }
        }
        return None;
    }
}

unsafe impl Send for HybridPublicKey {}
