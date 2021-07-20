use std::io::Write;

use crate::crypto::c25519::{C25519_PUBLIC_KEY_SIZE, ED25519_PUBLIC_KEY_SIZE, C25519_SECRET_KEY_SIZE, ED25519_SECRET_KEY_SIZE, C25519KeyPair, Ed25519KeyPair};
use crate::crypto::p521::{P521KeyPair, P521PublicKey, P521_ECDSA_SIGNATURE_SIZE, P521_PUBLIC_KEY_SIZE, P521_SECRET_KEY_SIZE};
use crate::vl1::Address;
use crate::crypto::hash::SHA384;

#[derive(Copy, Clone)]
#[repr(u8)]
pub enum Type {
    /// Curve25519 / Ed25519 identity (type 0)
    C25519 = 0,
    /// NIST P-521 ECDH / ECDSA identity (also has c25519/ed25519 keys for backward compability) (type 1)
    P521 = 1
}

struct IdentitySecrets {
    c25519: C25519KeyPair,
    ed25519: Ed25519KeyPair,
    p521: Option<(P521KeyPair, P521KeyPair)>, // ecdh key, ecdsa key
}

pub struct Identity {
    address: Address,
    c25519_public: [u8; C25519_PUBLIC_KEY_SIZE],
    ed25519_public: [u8; ED25519_PUBLIC_KEY_SIZE],
    p521_public: Option<(P521PublicKey, P521PublicKey, [u8; P521_ECDSA_SIGNATURE_SIZE])>, // ecdh key, ecdsa key, ecdsa signature of all keys
    secrets: Option<IdentitySecrets>,
}

impl Identity {
    fn generate_c25519() {
    }

    fn generate_p521() {
    }

    /// Generate a new identity.
    /// This is time consuming due to the one-time anti-collision proof of work required
    /// to generate an address corresponding with a set of identity keys. V0 identities
    /// take tens to hundreds of milliseconds on a typical 2020 system, while V1 identites
    /// take about 500ms. Generation can take a lot longer on low power devices, but only
    /// has to be done once.
    pub fn generate(id_type: Type) {
        match id_type {
            Type::C25519 => Self::generate_c25519(),
            Type::P521 => Self::generate_p521()
        }
    }

    /// Execute ECDH key agreement and return SHA384(shared secret).
    /// If both keys are type 1, key agreement is done with NIST P-521. Otherwise it's done
    /// with Curve25519. None is returned if there is an error such as this identity missing
    /// its secrets or a key being invalid.
    pub fn agree(&self, other_identity: &Identity) -> Option<[u8; 48]> {
        self.secrets.as_ref().map_or(None, |secrets| {
            secrets.p521.as_ref().map_or_else(|| {
                Some(SHA384::hash(&secrets.c25519.agree(&other_identity.c25519_public)))
            }, |p521_secret| {
                other_identity.p521_public.as_ref().map_or_else(|| {
                    Some(SHA384::hash(&secrets.c25519.agree(&other_identity.c25519_public)))
                }, |other_p521_public| {
                    p521_secret.0.agree(&other_p521_public.0).map_or(None, |secret| Some(SHA384::hash(&secret)))
                })
            })
        })
    }

    /// Sign this message with this identity.
    /// Signature is performed using ed25519 EDDSA or NIST P-521 ECDSA depending on the identity
    /// type. None is returned if this identity lacks secret keys or another error occurs.
    pub fn sign(&self, msg: &[u8]) -> Option<Vec<u8>> {
        self.secrets.as_ref().map_or(None, |secrets| {
            secrets.p521.as_ref().map_or_else(|| {
                Some(secrets.ed25519.sign(msg).to_vec())
            }, |p521_secret| {
                p521_secret.1.sign(msg).map_or(None, |sig| Some(sig.to_vec()))
            })
        })
    }

    /// Get this identity's type.
    #[inline(always)]
    pub fn id_type(&self) -> Type {
        if self.p521_public.is_some() {
            Type::P521
        } else {
            Type::C25519
        }
    }

    /// Returns true if this identity also holds its secret keys.
    #[inline(always)]
    pub fn has_secrets(&self) -> bool {
        self.secrets.is_some()
    }

    /// Get this identity in string format, including its secret keys.
    pub fn to_secret_string(&self) -> String {
        self.secrets.as_ref().map_or_else(|| {
            self.to_string()
        }, |secrets| {
            secrets.p521.as_ref().map_or_else(|| {
                format!("{}:{}{}", self.to_string(), crate::util::hex::to_string(&secrets.c25519.secret_bytes()), crate::util::hex::to_string(&secrets.ed25519.secret_bytes()))
            }, |p521_secret| {
                let mut secret_key_blob: Vec<u8> = Vec::new();
                secret_key_blob.reserve(C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE);
                let _ = secret_key_blob.write_all(&secrets.c25519.secret_bytes());
                let _ = secret_key_blob.write_all(&secrets.ed25519.secret_bytes());
                let _ = secret_key_blob.write_all(p521_secret.0.secret_key_bytes());
                let _ = secret_key_blob.write_all(p521_secret.1.secret_key_bytes());
                format!("{}:{}", self.to_string(), base64::encode_config(secret_key_blob.as_slice(), base64::URL_SAFE_NO_PAD))
            })
        })
    }
}

impl ToString for Identity {
    fn to_string(&self) -> String {
        self.p521_public.as_ref().map_or_else(|| {
            format!("{:0>10x}:0:{}{}", self.address.to_u64(), crate::util::hex::to_string(&self.c25519_public), crate::util::hex::to_string(&self.ed25519_public))
        }, |p521_public| {
            let mut public_key_blob: Vec<u8> = Vec::new();
            public_key_blob.reserve(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_ECDSA_SIGNATURE_SIZE);
            let _ = public_key_blob.write_all(&self.c25519_public);
            let _ = public_key_blob.write_all(&self.ed25519_public);
            let _ = public_key_blob.write_all(p521_public.0.public_key_bytes());
            let _ = public_key_blob.write_all(p521_public.1.public_key_bytes());
            let _ = public_key_blob.write_all(&p521_public.2);
            format!("{:0>10x}:1:{}", self.address.to_u64(), base64::encode_config(public_key_blob.as_slice(), base64::URL_SAFE_NO_PAD))
        })
    }
}
