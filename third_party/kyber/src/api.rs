use crate::{
    error::KyberError,
    kem::*,
    kex::{Decapsulated, Encapsulated, PublicKey, SecretKey},
    params::*,
    CryptoRng, RngCore,
};

/// Keypair generation with a provided RNG.
///
/// ### Example
/// ```
/// # use pqc_kyber::*;
/// # fn main() -> Result<(), KyberError> {
/// let mut rng = rand::thread_rng();
/// let keys = keypair(&mut rng);
/// # Ok(())}
/// ```
pub fn keypair<R>(rng: &mut R) -> Keypair
where
    R: RngCore + CryptoRng,
{
    let mut public = [0u8; KYBER_PUBLICKEYBYTES];
    let mut secret = [0u8; KYBER_SECRETKEYBYTES];
    crypto_kem_keypair(&mut public, &mut secret, rng, None);
    Keypair { public, secret }
}

/// Encapsulates a public key returning the ciphertext to send
/// and the shared secret
///
/// ### Example
/// ```
/// # use pqc_kyber::*;
/// # fn main() -> Result<(), KyberError> {
/// let mut rng = rand::thread_rng();
/// let keys = keypair(&mut rng);
/// let (ciphertext, shared_secret) = encapsulate(&keys.public, &mut rng)?;
/// # Ok(())}
/// ```
pub fn encapsulate<R>(pk: &[u8], rng: &mut R) -> Encapsulated
where
    R: CryptoRng + RngCore,
{
    if pk.len() != KYBER_PUBLICKEYBYTES {
        return Err(KyberError::InvalidInput);
    }
    let mut ct = [0u8; KYBER_CIPHERTEXTBYTES];
    let mut ss = [0u8; KYBER_SSBYTES];
    crypto_kem_enc(&mut ct, &mut ss, pk, rng, None);
    Ok((ct, ss))
}

/// Decapsulates ciphertext with a secret key, the result will contain
/// a KyberError if decapsulation fails
///
/// ### Example
/// ```
/// # use pqc_kyber::*;
/// # fn main() -> Result<(), KyberError> {
/// let mut rng = rand::thread_rng();
/// let keys = keypair(&mut rng);
/// let (ct, ss1) = encapsulate(&keys.public, &mut rng)?;
/// let ss2 = decapsulate(&ct, &keys.secret)?;
/// assert_eq!(ss1, ss2);
/// #  Ok(())}
/// ```
pub fn decapsulate(ct: &[u8], sk: &[u8]) -> Decapsulated {
    if ct.len() != KYBER_CIPHERTEXTBYTES || sk.len() != KYBER_SECRETKEYBYTES {
        return Err(KyberError::InvalidInput);
    }
    let mut ss = [0u8; KYBER_SSBYTES];
    match crypto_kem_dec(&mut ss, ct, sk) {
        Ok(_) => Ok(ss),
        Err(e) => Err(e),
    }
}

/// A public/secret keypair for use with Kyber.
///
/// Byte lengths of the keys are determined by the security level chosen.
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub struct Keypair {
    pub public: PublicKey,
    pub secret: SecretKey,
}

impl Keypair {
    /// Securely generates a new keypair`
    /// ```
    /// # use pqc_kyber::*;
    /// # fn main() -> Result<(), KyberError> {
    /// let mut rng = rand::thread_rng();
    /// let keys = Keypair::generate(&mut rng);
    /// # let empty_keys = Keypair{
    ///   public: [0u8; KYBER_PUBLICKEYBYTES], secret: [0u8; KYBER_SECRETKEYBYTES]
    /// };
    /// # assert!(empty_keys != keys);
    /// # Ok(()) }
    /// ```
    pub fn generate<R: CryptoRng + RngCore>(rng: &mut R) -> Keypair {
        keypair(rng)
    }
}
