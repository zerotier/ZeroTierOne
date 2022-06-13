//! Convenience structs for commonly defined fields in headers.

use std::borrow::Cow;

use serde::{Deserialize, Serialize};

use crate::algorithm::AlgorithmType;
use crate::error::Error;
use crate::ToBase64;

/// A trait for any header than can conform to the
/// [JWT specification](https://tools.ietf.org/html/rfc7519#page-11).
pub trait JoseHeader {
    fn algorithm_type(&self) -> AlgorithmType;

    fn key_id(&self) -> Option<&str> {
        None
    }

    fn type_(&self) -> Option<HeaderType> {
        None
    }

    fn content_type(&self) -> Option<HeaderContentType> {
        None
    }
}

/// Generic [JWT header](https://tools.ietf.org/html/rfc7519#page-11) with
/// defined fields for common fields.
#[derive(Default, Debug, PartialEq, Serialize, Deserialize)]
pub struct Header {
    #[serde(rename = "alg")]
    pub algorithm: AlgorithmType,

    #[serde(rename = "kid", skip_serializing_if = "Option::is_none")]
    pub key_id: Option<String>,

    #[serde(rename = "typ", skip_serializing_if = "Option::is_none")]
    pub type_: Option<HeaderType>,

    #[serde(rename = "cty", skip_serializing_if = "Option::is_none")]
    pub content_type: Option<HeaderContentType>,
}

impl JoseHeader for Header {
    fn algorithm_type(&self) -> AlgorithmType {
        self.algorithm
    }

    fn key_id(&self) -> Option<&str> {
        self.key_id.as_deref()
    }

    fn type_(&self) -> Option<HeaderType> {
        self.type_
    }

    fn content_type(&self) -> Option<HeaderContentType> {
        self.content_type
    }
}

#[derive(Copy, Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "UPPERCASE")]
pub enum HeaderType {
    #[serde(rename = "JWT")]
    JsonWebToken,
}

#[derive(Copy, Clone, Debug, PartialEq, Serialize, Deserialize)]
pub enum HeaderContentType {
    #[serde(rename = "JWT")]
    JsonWebToken,
}

/// A header that only contains the algorithm type. The `ToBase64`
/// implementation uses static strings for faster serialization.
pub struct PrecomputedAlgorithmOnlyHeader(pub AlgorithmType);

impl JoseHeader for PrecomputedAlgorithmOnlyHeader {
    fn algorithm_type(&self) -> AlgorithmType {
        let PrecomputedAlgorithmOnlyHeader(algorithm_type) = *self;
        algorithm_type
    }
}

impl ToBase64 for PrecomputedAlgorithmOnlyHeader {
    fn to_base64(&self) -> Result<Cow<'static, str>, Error> {
        let precomputed_str = match self.algorithm_type() {
            AlgorithmType::Hs256 => "eyJhbGciOiAiSFMyNTYifQ",
            AlgorithmType::Hs384 => "eyJhbGciOiAiSFMzODQifQ",
            AlgorithmType::Hs512 => "eyJhbGciOiAiSFM1MTIifQ",
            AlgorithmType::Rs256 => "eyJhbGciOiAiUlMyNTYifQ",
            AlgorithmType::Rs384 => "eyJhbGciOiAiUlMzODQifQ",
            AlgorithmType::Rs512 => "eyJhbGciOiAiUlM1MTIifQ",
            AlgorithmType::Es256 => "eyJhbGciOiAiRVMyNTYifQ",
            AlgorithmType::Es384 => "eyJhbGciOiAiRVMzODQifQ",
            AlgorithmType::Es512 => "eyJhbGciOiAiRVM1MTIifQ",
            AlgorithmType::Ps256 => "eyJhbGciOiAiUFMyNTYifQ",
            AlgorithmType::Ps384 => "eyJhbGciOiAiUFMzODQifQ",
            AlgorithmType::Ps512 => "eyJhbGciOiAiUFM1MTIifQ",
            AlgorithmType::None => "eyJhbGciOiAibm9uZSJ9Cg",
        };

        Ok(Cow::Borrowed(precomputed_str))
    }
}

/// A header with a borrowed key. Used for signing claims with a Store
/// conveniently.
#[derive(Serialize)]
pub(crate) struct BorrowedKeyHeader<'a> {
    #[serde(rename = "alg")]
    pub algorithm: AlgorithmType,

    #[serde(rename = "kid")]
    pub key_id: &'a str,
}

impl<'a> JoseHeader for BorrowedKeyHeader<'a> {
    fn algorithm_type(&self) -> AlgorithmType {
        self.algorithm
    }

    fn key_id(&self) -> Option<&str> {
        Some(self.key_id)
    }
}

#[cfg(test)]
mod tests {
    use crate::algorithm::AlgorithmType;
    use crate::error::Error;
    use crate::header::{Header, HeaderType, PrecomputedAlgorithmOnlyHeader};
    use crate::{FromBase64, ToBase64};

    #[test]
    fn from_base64() -> Result<(), Error> {
        let enc = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9";
        let header = Header::from_base64(enc)?;

        assert_eq!(header.type_.unwrap(), HeaderType::JsonWebToken);
        assert_eq!(header.algorithm, AlgorithmType::Hs256);

        let enc = "eyJhbGciOiJSUzI1NiIsImtpZCI6IjFLU0YzZyJ9";
        let header = Header::from_base64(enc)?;

        assert_eq!(header.key_id.unwrap(), "1KSF3g".to_string());
        assert_eq!(header.algorithm, AlgorithmType::Rs256);

        Ok(())
    }

    #[test]
    fn roundtrip() -> Result<(), Error> {
        let header: Header = Default::default();
        let enc = header.to_base64()?;
        assert_eq!(header, Header::from_base64(&*enc)?);
        Ok(())
    }

    #[test]
    fn precomputed_headers() -> Result<(), Error> {
        let algorithms = [
            AlgorithmType::Hs256,
            AlgorithmType::Hs384,
            AlgorithmType::Hs512,
            AlgorithmType::Rs256,
            AlgorithmType::Rs384,
            AlgorithmType::Rs512,
            AlgorithmType::Es256,
            AlgorithmType::Es384,
            AlgorithmType::Es512,
            AlgorithmType::Ps256,
            AlgorithmType::Ps384,
            AlgorithmType::Ps512,
            AlgorithmType::None,
        ];

        for algorithm in algorithms.iter() {
            let precomputed = PrecomputedAlgorithmOnlyHeader(*algorithm);
            let precomputed_str = precomputed.to_base64()?;

            let header = Header::from_base64(&*precomputed_str)?;
            assert_eq!(*algorithm, header.algorithm);
        }

        Ok(())
    }
}
