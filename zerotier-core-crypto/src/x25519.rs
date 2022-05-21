// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

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
pub struct C25519KeyPair(x25519_dalek::StaticSecret, Secret<32>, x25519_dalek::PublicKey);

impl C25519KeyPair {
    #[inline(always)]
    pub fn generate() -> C25519KeyPair {
        let sk = x25519_dalek::StaticSecret::new(SecureRandom::get());
        let sk2 = Secret(sk.to_bytes());
        let pk = x25519_dalek::PublicKey::from(&sk);
        C25519KeyPair(sk, sk2, pk)
    }

    pub fn from_bytes(public_key: &[u8], secret_key: &[u8]) -> Option<C25519KeyPair> {
        if public_key.len() == 32 && secret_key.len() == 32 {
            /* NOTE: we keep the original secret separately from x25519_dalek's StaticSecret
             * due to how "clamping" is done in the old C++ code vs x25519_dalek. Clamping
             * is explained here:
             *
             * https://www.jcraige.com/an-explainer-on-ed25519-clamping
             *
             * The old code does clamping at the time of use. In other words the code that
             * performs things like key agreement or signing clamps the secret before doing
             * the operation. The x25519_dalek code does clamping at generation or when
             * from() is used to get a key from a raw byte array.
             *
             * Unfortunately this introduces issues when interoperating with old code. The
             * old system generates secrets that are not clamped (since they're clamped at
             * use!) and assumes that these exact binary keys will be preserved in e.g.
             * identities. So to preserve this behavior we store the secret separately
             * so secret_bytes() will return it as-is.
             *
             * The new code will still clamp at generation resulting in secrets that are
             * pre-clamped, but the old code won't care about this. It's only a problem when
             * going the other way.
             *
             * This has no cryptographic implication since regardless of where, the clamping
             * is done. It's just an API thing.
             */
            let pk: [u8; 32] = public_key.try_into().unwrap();
            let sk_orig: Secret<32> = Secret(secret_key.try_into().unwrap());
            let pk = x25519_dalek::PublicKey::from(pk);
            let sk = x25519_dalek::StaticSecret::from(sk_orig.0.clone());
            Some(C25519KeyPair(sk, sk_orig, pk))
        } else {
            None
        }
    }

    #[inline(always)]
    pub fn public_bytes(&self) -> [u8; C25519_PUBLIC_KEY_SIZE] {
        self.2.to_bytes()
    }

    #[inline(always)]
    pub fn secret_bytes(&self) -> &Secret<32> {
        &self.1
    }

    /// Execute ECDH agreement and return a raw (un-hashed) shared secret key.
    pub fn agree(&self, their_public: &[u8]) -> Secret<{ C25519_SHARED_SECRET_SIZE }> {
        let pk: [u8; 32] = their_public.try_into().unwrap();
        let pk = x25519_dalek::PublicKey::from(pk);
        let sec = self.0.diffie_hellman(&pk);
        Secret(sec.to_bytes())
    }
}

impl Clone for C25519KeyPair {
    fn clone(&self) -> Self {
        Self(x25519_dalek::StaticSecret::from(self.0.to_bytes()), self.1.clone(), x25519_dalek::PublicKey::from(self.1 .0.clone()))
    }
}

/// Ed25519 key pair for EDDSA signatures.
pub struct Ed25519KeyPair(ed25519_dalek::Keypair, Secret<32>);

impl Ed25519KeyPair {
    #[inline(always)]
    pub fn generate() -> Ed25519KeyPair {
        let mut rng = SecureRandom::get();
        let kp = ed25519_dalek::Keypair::generate(&mut rng);
        let sk2 = Secret(kp.secret.to_bytes());
        Ed25519KeyPair(kp, sk2)
    }

    pub fn from_bytes(public_bytes: &[u8], secret_bytes: &[u8]) -> Option<Ed25519KeyPair> {
        if public_bytes.len() == ED25519_PUBLIC_KEY_SIZE && secret_bytes.len() == ED25519_SECRET_KEY_SIZE {
            let pk = ed25519_dalek::PublicKey::from_bytes(public_bytes);
            let sk = ed25519_dalek::SecretKey::from_bytes(secret_bytes);
            if pk.is_ok() && sk.is_ok() {
                // See comment in from_bytes() in C25519KeyPair for an explanation of the copy of the secret here.
                let pk = pk.unwrap();
                let sk = sk.unwrap();
                let sk2 = Secret(sk.to_bytes());
                Some(Ed25519KeyPair(ed25519_dalek::Keypair { public: pk, secret: sk }, sk2))
            } else {
                None
            }
        } else {
            None
        }
    }

    #[inline(always)]
    pub fn public_bytes(&self) -> [u8; ED25519_PUBLIC_KEY_SIZE] {
        self.0.public.to_bytes()
    }

    #[inline(always)]
    pub fn secret_bytes(&self) -> &Secret<32> {
        &self.1
    }

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

impl Clone for Ed25519KeyPair {
    fn clone(&self) -> Self {
        Self(ed25519_dalek::Keypair::from_bytes(&self.0.to_bytes()).unwrap(), self.1.clone())
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
