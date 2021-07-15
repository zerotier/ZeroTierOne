use std::convert::TryInto;
use ed25519_dalek::Digest;
use std::io::Write;

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
    pub fn new() -> C25519KeyPair {
        let sk = x25519_dalek::StaticSecret::new(rand_core::OsRng);
        let pk = x25519_dalek::PublicKey::from(&sk);
        C25519KeyPair(sk, pk)
    }

    #[inline(always)]
    pub fn from_keys(public_key: &[u8], secret_key: &[u8]) -> Option<C25519KeyPair> {
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
    pub fn public_bytes(&self) -> [u8; C25519_PUBLIC_KEY_SIZE] {
        self.1.to_bytes()
    }

    #[inline(always)]
    pub fn secret_bytes(&self) -> [u8; C25519_SECRET_KEY_SIZE] {
        self.0.to_bytes()
    }

    /// Execute ECDH agreement and return a raw (un-hashed) shared secret key.
    #[inline(always)]
    pub fn agree(&self, their_public: &[u8]) -> [u8; C25519_SHARED_SECRET_SIZE] {
        let pk: [u8; 32] = their_public.try_into().unwrap();
        let pk = x25519_dalek::PublicKey::from(pk);
        let sec = self.0.diffie_hellman(&pk);
        sec.to_bytes()
    }
}

/// Ed25519 key pair for EDDSA signatures.
pub struct Ed25519KeyPair(ed25519_dalek::Keypair);

impl Ed25519KeyPair {
    #[inline(always)]
    pub fn new() -> Ed25519KeyPair {
        let mut rng = rand_core::OsRng::default();
        Ed25519KeyPair(ed25519_dalek::Keypair::generate(&mut rng))
    }

    #[inline(always)]
    pub fn from_keys(public_key: &[u8], secret_key: &[u8]) -> Ed25519KeyPair {
        let mut tmp = [0_u8; 64];
        tmp[0..32].copy_from_slice(secret_key);
        tmp[32..64].copy_from_slice(public_key);
        Ed25519KeyPair(ed25519_dalek::Keypair::from_bytes(&tmp).unwrap())
    }

    #[inline(always)]
    pub fn public_bytes(&self) -> [u8; ED25519_PUBLIC_KEY_SIZE] {
        self.0.public.to_bytes()
    }

    #[inline(always)]
    pub fn secret_bytes(&self) -> [u8; ED25519_SECRET_KEY_SIZE] {
        self.0.secret.to_bytes()
    }

    #[inline(always)]
    pub fn sign(&self, msg: &[u8]) -> [u8; ED25519_SIGNATURE_SIZE] {
        let mut h = ed25519_dalek::Sha512::new();
        let _ = h.write_all(msg);
        self.0.sign_prehashed(h.clone(), None).unwrap().to_bytes()
    }

    /// Create a signature with the first 32 bytes of the SHA512 hash appended.
    /// ZeroTier does this for legacy reasons, but it's ignored in newer versions.
    #[inline(always)]
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

#[inline(always)]
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
