/// The security level of Kyber
/// 
/// Defaults to 3 (kyber768), will be 2 or 4 repsectively when 
/// kyber512 or kyber1024 are selected with feature flags.
/// 
/// * Kyber-512 aims at security roughly equivalent to AES-128
/// * Kyber-768 aims at security roughly equivalent to AES-192
/// * Kyber-1024 aims at security roughly equivalent to AES-256
#[cfg(not(any(feature = "kyber512", feature = "kyber1024")))]
pub const KYBER_K: usize = 3; 

#[cfg(feature = "kyber512")]
pub const KYBER_K: usize = 2;

#[cfg(feature = "kyber1024")]
pub const KYBER_K: usize = 4;

/// A boolean flag for whether 90's mode is activated.
/// 
/// If true AES-CTR and SHA2 will be used as cryptographic primitives instead, 
/// which may have hardware speed-ups on certain platforms.
/// 
/// Defaults to false, set`features = ["90s"]` in Cargo.toml to enable. 
#[cfg(not(feature = "90s"))]
pub const KYBER_90S: bool = false;
#[cfg(feature = "90s")]
pub const KYBER_90S: bool = true;

pub(crate) const KYBER_N: usize = 256;
pub(crate) const KYBER_Q: usize = 3329;

#[cfg(feature = "kyber512")]
pub(crate) const KYBER_ETA1: usize = 3;
#[cfg(not(feature = "kyber512"))]
pub(crate) const KYBER_ETA1: usize = 2;

pub(crate) const KYBER_ETA2: usize = 2;

// Size of the hashes and seeds
pub(crate) const KYBER_SYMBYTES: usize = 32;

/// Size of the shared key 
pub const KYBER_SSBYTES: usize =  32; 

pub(crate) const KYBER_POLYBYTES: usize = 384;
pub(crate) const KYBER_POLYVECBYTES: usize =  KYBER_K * KYBER_POLYBYTES;

#[cfg(not(feature = "kyber1024"))]
pub(crate) const KYBER_POLYCOMPRESSEDBYTES: usize =     128;
#[cfg(not(feature = "kyber1024"))]
pub(crate) const KYBER_POLYVECCOMPRESSEDBYTES: usize =  KYBER_K * 320;

#[cfg(feature = "kyber1024")]
pub(crate) const KYBER_POLYCOMPRESSEDBYTES: usize =     160;
#[cfg(feature = "kyber1024")]
pub(crate) const KYBER_POLYVECCOMPRESSEDBYTES: usize = KYBER_K * 352;

pub(crate) const KYBER_INDCPA_PUBLICKEYBYTES: usize = KYBER_POLYVECBYTES + KYBER_SYMBYTES;
pub(crate) const KYBER_INDCPA_SECRETKEYBYTES: usize = KYBER_POLYVECBYTES;
pub(crate) const KYBER_INDCPA_BYTES: usize = KYBER_POLYVECCOMPRESSEDBYTES + KYBER_POLYCOMPRESSEDBYTES;

/// Size in bytes of the Kyber public key
pub const KYBER_PUBLICKEYBYTES: usize = KYBER_INDCPA_PUBLICKEYBYTES;
/// Size in bytes of the Kyber secret key 
pub const KYBER_SECRETKEYBYTES: usize =  KYBER_INDCPA_SECRETKEYBYTES +  KYBER_INDCPA_PUBLICKEYBYTES + 2*KYBER_SYMBYTES; 
/// Size in bytes of the Kyber ciphertext
pub const KYBER_CIPHERTEXTBYTES: usize =  KYBER_INDCPA_BYTES;