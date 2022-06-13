//! Algorithms capable of signing and verifying tokens. By default only the
//! `hmac` crate's `Hmac` type is supported. For more algorithms, enable the
//! feature `openssl` and see the [openssl](openssl/index.html)
//! module. The `none` algorithm is explicitly not supported.
//! ## Examples
//! ```
//! use hmac::{Hmac, Mac};
//! use sha2::Sha256;
//!
//! let hs256_key: Hmac<Sha256> = Hmac::new_from_slice(b"some-secret").unwrap();
//! ```

use serde::{Deserialize, Serialize};

use crate::error::Error;

#[cfg(feature = "openssl")]
pub mod openssl;
pub mod rust_crypto;
pub mod store;

/// The type of an algorithm, corresponding to the
/// [JWA](https://tools.ietf.org/html/rfc7518) specification.
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "UPPERCASE")]
pub enum AlgorithmType {
    Hs256,
    Hs384,
    Hs512,
    Rs256,
    Rs384,
    Rs512,
    Es256,
    Es384,
    Es512,
    Ps256,
    Ps384,
    Ps512,
    #[serde(rename = "none")]
    None,
}

impl Default for AlgorithmType {
    fn default() -> Self {
        AlgorithmType::Hs256
    }
}

/// An algorithm capable of signing base64 encoded header and claims strings.
/// strings.
pub trait SigningAlgorithm {
    fn algorithm_type(&self) -> AlgorithmType;

    fn sign(&self, header: &str, claims: &str) -> Result<String, Error>;
}

/// An algorithm capable of verifying base64 encoded header and claims strings.
pub trait VerifyingAlgorithm {
    fn algorithm_type(&self) -> AlgorithmType;

    fn verify_bytes(&self, header: &str, claims: &str, signature: &[u8]) -> Result<bool, Error>;

    fn verify(&self, header: &str, claims: &str, signature: &str) -> Result<bool, Error> {
        let signature_bytes = base64::decode_config(signature, base64::URL_SAFE_NO_PAD)?;
        self.verify_bytes(header, claims, &*signature_bytes)
    }
}

// TODO: investigate if these AsRef impls are necessary
impl<T: AsRef<dyn VerifyingAlgorithm>> VerifyingAlgorithm for T {
    fn algorithm_type(&self) -> AlgorithmType {
        self.as_ref().algorithm_type()
    }

    fn verify_bytes(&self, header: &str, claims: &str, signature: &[u8]) -> Result<bool, Error> {
        self.as_ref().verify_bytes(header, claims, signature)
    }
}

impl<T: AsRef<dyn SigningAlgorithm>> SigningAlgorithm for T {
    fn algorithm_type(&self) -> AlgorithmType {
        self.as_ref().algorithm_type()
    }

    fn sign(&self, header: &str, claims: &str) -> Result<String, Error> {
        self.as_ref().sign(header, claims)
    }
}
