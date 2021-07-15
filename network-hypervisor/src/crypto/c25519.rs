use std::convert::TryInto;

pub const C25519_PUBLIC_KEY_SIZE: usize = 32;
pub const C25519_SECRET_KEY_SIZE: usize = 32;
pub const C25519_SHARED_SECRET_SIZE: usize = 32;
pub const ED25519_PUBLIC_KEY_SIZE: usize = 32;
pub const ED25519_SECRET_KEY_SIZE: usize = 32;
pub const ED25519_SIGNATURE_SIZE: usize = 64;

pub struct C25519KeyPair(x25519_dalek::StaticSecret, x25519_dalek::PublicKey);

impl C25519KeyPair {
    #[inline(always)]
    pub fn new() -> C25519KeyPair {
        let sk = x25519_dalek::StaticSecret::new(rand_core::OsRng);
        let pk = x25519_dalek::PublicKey::from(&sk);
        C25519KeyPair(sk, pk)
    }

    #[inline(always)]
    pub fn from_keys(public_key: &[u8], secret_key: &[u8]) -> C25519KeyPair {
        let pk = x25519_dalek::PublicKey::from(public_key.try_into().unwrap());
        let sk = x25519_dalek::StaticSecret::from(secret_key.try_into().unwrap());
        C25519KeyPair(sk, pk)
    }

    #[inline(always)]
    pub fn public_bytes(&self) -> [u8; C25519_PUBLIC_KEY_SIZE] {
        self.1.to_bytes()
    }

    #[inline(always)]
    pub fn secret_bytes(&self) -> [u8; C25519_SECRET_KEY_SIZE] {
        self.0.to_bytes()
    }

    #[inline(always)]
    pub fn agree(&self, their_public: &[u8]) -> [u8; C25519_SHARED_SECRET_SIZE] {
        let pk = x25519_dalek::PublicKey::from(their_public.try_into().unwrap());
        let sec = self.0.diffie_hellman(&pk);
        sec.to_bytes()
    }
}

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
        let h = crate::crypto::hash::SHA512::hash(msg);
        self.0.sign_prehashed(h, None).unwrap().to_bytes()
    }

    /// Create a signature with the first 32 bytes of the SHA512 hash appended.
    /// ZeroTier does this for legacy reasons.
    #[inline(always)]
    pub fn sign_zt(&self, msg: &[u8]) -> [u8; 96] {
        let h = crate::crypto::hash::SHA512::hash(msg);
        let s = self.0.sign_prehashed(h, None).unwrap().as_ref();
        let mut s2 = [0_u8; 96];
        s2[0..64].copy_from_slice(s);
        s2[64..96].copy_from_slice(&h[0..32]);
        s2
    }
}
