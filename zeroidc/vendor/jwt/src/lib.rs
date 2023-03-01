//! ### Only Claims
//! If you don't care about that header as long as the header is verified, signing
//! and verification can be done with just a few traits.
//! #### Signing
//! Claims can be any `serde::Serialize` type, usually derived with
//! `serde_derive`.
//! ```rust
//! use hmac::{Hmac, Mac};
//! use jwt::SignWithKey;
//! use sha2::Sha256;
//! use std::collections::BTreeMap;
//!
//! # use jwt::Error;
//! # fn try_main() -> Result<(), Error> {
//! let key: Hmac<Sha256> = Hmac::new_from_slice(b"some-secret")?;
//! let mut claims = BTreeMap::new();
//! claims.insert("sub", "someone");
//! let token_str = claims.sign_with_key(&key)?;
//! assert_eq!(token_str, "eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJzb21lb25lIn0.5wwE1sBrs-vftww_BGIuTVDeHtc1Jsjo-fiHhDwR8m0");
//! # Ok(())
//! # }
//! # try_main().unwrap()
//! ```
//! #### Verification
//! Claims can be any `serde::Deserialize` type, usually derived with
//! `serde_derive`.
//! ```rust
//! use hmac::{Hmac, Mac};
//! use jwt::VerifyWithKey;
//! use sha2::Sha256;
//! use std::collections::BTreeMap;
//!
//! # use jwt::Error;
//! # fn try_main() -> Result<(), Error> {
//! let key: Hmac<Sha256> = Hmac::new_from_slice(b"some-secret")?;
//! let token_str = "eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJzb21lb25lIn0.5wwE1sBrs-vftww_BGIuTVDeHtc1Jsjo-fiHhDwR8m0";
//! let claims: BTreeMap<String, String> = token_str.verify_with_key(&key)?;
//! assert_eq!(claims["sub"], "someone");
//! # Ok(())
//! # }
//! # try_main().unwrap()
//! ```
//! ### Header and Claims
//! If you need to customize the header, you can use the `Token` struct. For
//! convenience, a `Header` struct is provided for all of the commonly defined
//! fields, but any type that implements `JoseHeader` can be used.
//! #### Signing
//! Both header and claims have to implement `serde::Serialize`.
//! ```rust
//! use hmac::{Hmac, Mac};
//! use jwt::{AlgorithmType, Header, SignWithKey, Token};
//! use sha2::Sha384;
//! use std::collections::BTreeMap;
//!
//! # use jwt::Error;
//! # fn try_main() -> Result<(), Error> {
//! let key: Hmac<Sha384> = Hmac::new_from_slice(b"some-secret")?;
//! let header = Header {
//!     algorithm: AlgorithmType::Hs384,
//!     ..Default::default()
//! };
//! let mut claims = BTreeMap::new();
//! claims.insert("sub", "someone");
//! let token = Token::new(header, claims).sign_with_key(&key)?;
//! assert_eq!(token.as_str(), "eyJhbGciOiJIUzM4NCJ9.eyJzdWIiOiJzb21lb25lIn0.WM_WnPUkHK6zm6Wz7zk1kmIxz990Te7nlDjQ3vzcye29szZ-Sj47rLNSTJNzpQd_");
//! # Ok(())
//! # }
//! # try_main().unwrap()
//! ```
//! #### Verification
//! Both header and claims have to implement `serde::Deserialize`.
//! ```rust
//! use hmac::{Hmac, Mac};
//! use jwt::{AlgorithmType, Header, Token, VerifyWithKey};
//! use sha2::Sha384;
//! use std::collections::BTreeMap;
//!
//! # use jwt::Error;
//! # fn try_main() -> Result<(), Error> {
//! let key: Hmac<Sha384> = Hmac::new_from_slice(b"some-secret")?;
//! let token_str = "eyJhbGciOiJIUzM4NCJ9.eyJzdWIiOiJzb21lb25lIn0.WM_WnPUkHK6zm6Wz7zk1kmIxz990Te7nlDjQ3vzcye29szZ-Sj47rLNSTJNzpQd_";
//! let token: Token<Header, BTreeMap<String, String>, _> = token_str.verify_with_key(&key)?;
//! let header = token.header();
//! let claims = token.claims();
//! assert_eq!(header.algorithm, AlgorithmType::Hs384);
//! assert_eq!(claims["sub"], "someone");
//! # Ok(())
//! # }
//! # try_main().unwrap()
//! ```

#[cfg(doctest)]
doctest!("../README.md");

use std::borrow::Cow;

#[cfg(doctest)]
use doc_comment::doctest;
use serde::{Deserialize, Serialize};

#[cfg(feature = "openssl")]
pub use crate::algorithm::openssl::PKeyWithDigest;
pub use crate::algorithm::store::Store;
pub use crate::algorithm::{AlgorithmType, SigningAlgorithm, VerifyingAlgorithm};
pub use crate::claims::Claims;
pub use crate::claims::RegisteredClaims;
pub use crate::error::Error;
pub use crate::header::{Header, JoseHeader};
pub use crate::token::signed::{SignWithKey, SignWithStore};
pub use crate::token::verified::{VerifyWithKey, VerifyWithStore};
pub use crate::token::{Unsigned, Unverified, Verified};

pub mod algorithm;
pub mod claims;
pub mod error;
pub mod header;
pub mod token;

const SEPARATOR: &str = ".";

/// Representation of a structured JWT. Methods vary based on the signature
/// type `S`.
pub struct Token<H, C, S> {
    header: H,
    claims: C,
    signature: S,
}

impl<H, C, S> Token<H, C, S> {
    pub fn header(&self) -> &H {
        &self.header
    }

    pub fn claims(&self) -> &C {
        &self.claims
    }

    pub fn remove_signature(self) -> Token<H, C, Unsigned> {
        Token {
            header: self.header,
            claims: self.claims,
            signature: Unsigned,
        }
    }
}

impl<H, C, S> From<Token<H, C, S>> for (H, C) {
    fn from(token: Token<H, C, S>) -> Self {
        (token.header, token.claims)
    }
}

/// A trait used to convert objects in base64 encoding. The return type can
/// be either owned if the header is dynamic, or it can be borrowed if the
/// header is a static, pre-computed value. It is implemented automatically
/// for every type that implements
/// [Serialize](../../serde/trait.Serialize.html). as a base64 encoding of
/// the object's JSON representation.
pub trait ToBase64 {
    fn to_base64(&self) -> Result<Cow<str>, Error>;
}

impl<T: Serialize> ToBase64 for T {
    fn to_base64(&self) -> Result<Cow<str>, Error> {
        let json_bytes = serde_json::to_vec(&self)?;
        let encoded_json_bytes = base64::encode_config(&json_bytes, base64::URL_SAFE_NO_PAD);
        Ok(Cow::Owned(encoded_json_bytes))
    }
}

/// A trait used to parse objects from base64 encoding. The return type can
/// be either owned if the header is dynamic, or it can be borrowed if the
/// header is a static, pre-computed value. It is implemented automatically
/// for every type that implements
/// [DeserializeOwned](../../serde/trait.Deserialize.html) for
/// the base64 encoded JSON representation.
pub trait FromBase64: Sized {
    fn from_base64<Input: ?Sized + AsRef<[u8]>>(raw: &Input) -> Result<Self, Error>;
}

impl<T: for<'de> Deserialize<'de> + Sized> FromBase64 for T {
    fn from_base64<Input: ?Sized + AsRef<[u8]>>(raw: &Input) -> Result<Self, Error> {
        let json_bytes = base64::decode_config(raw, base64::URL_SAFE_NO_PAD)?;
        Ok(serde_json::from_slice(&json_bytes)?)
    }
}

#[cfg(test)]
mod tests {
    use crate::algorithm::AlgorithmType::Hs256;
    use crate::error::Error;
    use crate::header::Header;
    use crate::token::signed::SignWithKey;
    use crate::token::verified::VerifyWithKey;
    use crate::Claims;
    use crate::Token;
    use hmac::Hmac;
    use hmac::Mac;
    use sha2::Sha256;

    #[test]
    pub fn raw_data() -> Result<(), Error> {
        let raw = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiYWRtaW4iOnRydWV9.TJVA95OrM7E2cBab30RMHrHDcEfxjoYZgeFONFh7HgQ";
        let token: Token<Header, Claims, _> = Token::parse_unverified(raw)?;

        assert_eq!(token.header.algorithm, Hs256);

        let verifier: Hmac<Sha256> = Hmac::new_from_slice(b"secret")?;
        assert!(token.verify_with_key(&verifier).is_ok());

        Ok(())
    }

    #[test]
    pub fn roundtrip() -> Result<(), Error> {
        let token: Token<Header, Claims, _> = Default::default();
        let key: Hmac<Sha256> = Hmac::new_from_slice(b"secret")?;
        let signed_token = token.sign_with_key(&key)?;
        let signed_token_str = signed_token.as_str();

        let recreated_token: Token<Header, Claims, _> = Token::parse_unverified(signed_token_str)?;

        assert_eq!(signed_token.header(), recreated_token.header());
        assert_eq!(signed_token.claims(), recreated_token.claims());
        recreated_token.verify_with_key(&key)?;
        Ok(())
    }
}
