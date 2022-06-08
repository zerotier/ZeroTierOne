use std::fmt;
use std::string::FromUtf8Error;

use base64::DecodeError;
use crypto_common::InvalidLength;
use digest::MacError;
use serde_json::Error as JsonError;

use self::Error::*;
use crate::algorithm::AlgorithmType;

#[derive(Debug)]
pub enum Error {
    AlgorithmMismatch(AlgorithmType, AlgorithmType),
    Base64(DecodeError),
    Format,
    InvalidSignature,
    Json(JsonError),
    NoClaimsComponent,
    NoHeaderComponent,
    NoKeyId,
    NoKeyWithKeyId(String),
    NoSignatureComponent,
    RustCryptoMac(MacError),
    RustCryptoMacKeyLength(InvalidLength),
    TooManyComponents,
    Utf8(FromUtf8Error),
    #[cfg(feature = "openssl")]
    OpenSsl(openssl::error::ErrorStack),
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        match *self {
            AlgorithmMismatch(a, b) => {
                write!(f, "Expected algorithm type {:?} but found {:?}", a, b)
            }
            NoKeyId => write!(f, "No key id found"),
            NoKeyWithKeyId(ref kid) => write!(f, "Key with key id {} not found", kid),
            NoHeaderComponent => write!(f, "No header component found in token string"),
            NoClaimsComponent => write!(f, "No claims component found in token string"),
            NoSignatureComponent => write!(f, "No signature component found in token string"),
            TooManyComponents => write!(f, "Too many components found in token string"),
            Format => write!(f, "Format"),
            InvalidSignature => write!(f, "Invalid signature"),
            Base64(ref x) => write!(f, "{}", x),
            Json(ref x) => write!(f, "{}", x),
            Utf8(ref x) => write!(f, "{}", x),
            RustCryptoMac(ref x) => write!(f, "{}", x),
            RustCryptoMacKeyLength(ref x) => write!(f, "{}", x),
            #[cfg(feature = "openssl")]
            OpenSsl(ref x) => write!(f, "{}", x),
        }
    }
}

impl std::error::Error for Error {}

macro_rules! error_wrap {
    ($f:ty, $e:expr) => {
        impl From<$f> for Error {
            fn from(f: $f) -> Error {
                $e(f)
            }
        }
    };
}

error_wrap!(DecodeError, Base64);
error_wrap!(JsonError, Json);
error_wrap!(FromUtf8Error, Utf8);
error_wrap!(MacError, RustCryptoMac);
error_wrap!(InvalidLength, RustCryptoMacKeyLength);
#[cfg(feature = "openssl")]
error_wrap!(openssl::error::ErrorStack, Error::OpenSsl);
