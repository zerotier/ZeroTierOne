/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::convert::TryInto;
use std::io::Write;

use ed25519_dalek::Digest;

use crate::random::SecureRandom;
use crate::secret::Secret;

pub const C25519_PUBLIC_KEY_SIZE: usize = 32;
pub const C25519_SECRET_KEY_SIZE: usize = 32;
pub const C25519_SHARED_SECRET_SIZE: usize = 32;
pub const ED25519_PUBLIC_KEY_SIZE: usize = 32;
pub const ED25519_SECRET_KEY_SIZE: usize = 32;
pub const ED25519_SIGNATURE_SIZE: usize = 64;

/// Curve25519 key pair for ECDH key agreement.
pub struct C25519KeyPair(x25519_dalek::StaticSecret, x25519_dalek::PublicKey);

impl C25519KeyPair {
    #[inline(always)]
    pub fn generate(_transient: bool) -> C25519KeyPair {
        let sk = x25519_dalek::StaticSecret::new(SecureRandom::get());
        let pk = x25519_dalek::PublicKey::from(&sk);
        C25519KeyPair(sk, pk)
    }

    pub fn from_bytes(public_key: &[u8], secret_key: &[u8]) -> Option<C25519KeyPair> {
        if public_key.len() == 32 && secret_key.len() == 32 {
            let pk: [u8; 32] = public_key.try_into().unwrap();
            let sk: [u8; 32] = secret_key.try_into().unwrap();
            let pk = x25519_dalek::PublicKey::from(pk);
            let sk = x25519_dalek::StaticSecret::from(sk);
            Some(C25519KeyPair(sk, pk))
        } else {
            None
        }
    }

    #[inline(always)]
    pub fn public_bytes(&self) -> [u8; C25519_PUBLIC_KEY_SIZE] { self.1.to_bytes() }

    #[inline(always)]
    pub fn secret_bytes(&self) -> Secret<{ C25519_SECRET_KEY_SIZE }> { Secret(self.0.to_bytes()) }

    /// Execute ECDH agreement and return a raw (un-hashed) shared secret key.
    pub fn agree(&self, their_public: &[u8]) -> Secret<{ C25519_SHARED_SECRET_SIZE }> {
        let pk: [u8; 32] = their_public.try_into().unwrap();
        let pk = x25519_dalek::PublicKey::from(pk);
        let sec = self.0.diffie_hellman(&pk);
        Secret(sec.to_bytes())
    }
}

/// Ed25519 key pair for EDDSA signatures.
pub struct Ed25519KeyPair(ed25519_dalek::Keypair);

impl Ed25519KeyPair {
    #[inline(always)]
    pub fn generate(_transient: bool) -> Ed25519KeyPair {
        let mut rng = SecureRandom::get();
        Ed25519KeyPair(ed25519_dalek::Keypair::generate(&mut rng))
    }

    pub fn from_bytes(public_bytes: &[u8], secret_bytes: &[u8]) -> Option<Ed25519KeyPair> {
        if public_bytes.len() == ED25519_PUBLIC_KEY_SIZE && secret_bytes.len() == ED25519_SECRET_KEY_SIZE {
            let pk = ed25519_dalek::PublicKey::from_bytes(public_bytes);
            let sk = ed25519_dalek::SecretKey::from_bytes(secret_bytes);
            if pk.is_ok() && sk.is_ok() {
                Some(Ed25519KeyPair(ed25519_dalek::Keypair {
                    public: pk.unwrap(),
                    secret: sk.unwrap(),
                }))
            } else {
                None
            }
        } else {
            None
        }
    }

    #[inline(always)]
    pub fn public_bytes(&self) -> [u8; ED25519_PUBLIC_KEY_SIZE] { self.0.public.to_bytes() }

    #[inline(always)]
    pub fn secret_bytes(&self) -> Secret<{ ED25519_SECRET_KEY_SIZE }> { Secret(self.0.secret.to_bytes()) }

    pub fn sign(&self, msg: &[u8]) -> [u8; ED25519_SIGNATURE_SIZE] {
        let mut h = ed25519_dalek::Sha512::new();
        let _ = h.write_all(msg);
        self.0.sign_prehashed(h.clone(), None).unwrap().to_bytes()
    }

    /// Create a signature with the first 32 bytes of the SHA512 hash appended.
    /// ZeroTier does this for legacy reasons, but it's ignored in newer versions.
    pub fn sign_zt(&self, msg: &[u8]) -> [u8; 96] {
        let mut h = ed25519_dalek::Sha512::new();
        let _ = h.write_all(msg);
        let sig = self.0.sign_prehashed(h.clone(), None).unwrap();
        let s = sig.as_ref();
        let mut s2 = [0_u8; 96];
        s2[0..64].copy_from_slice(s);
        let h = h.finalize();
        s2[64..96].copy_from_slice(&h.as_slice()[0..32]);
        s2
    }
}

pub fn ed25519_verify(public_key: &[u8], signature: &[u8], msg: &[u8]) -> bool {
    if public_key.len() == 32 && signature.len() >= 64 {
        ed25519_dalek::PublicKey::from_bytes(public_key).map_or(false, |pk| {
            let mut h = ed25519_dalek::Sha512::new();
            let _ = h.write_all(msg);
            let sig: [u8; 64] = signature[0..64].try_into().unwrap();
            pk.verify_prehashed(h, None, &ed25519_dalek::Signature::from(sig)).is_ok()
        })
    } else {
        false
    }
}
