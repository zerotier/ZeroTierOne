use serde::{Deserialize, Serialize};
use std::fmt::Error as FormatterError;
use std::fmt::{Debug, Display, Formatter};

use crate::{basic::BasicErrorResponseType, ErrorResponseType};
use crate::{AccessToken, RefreshToken};

///
/// A revocable token.
///
/// Implement this trait to indicate support for token revocation per [RFC 7009 OAuth 2.0 Token Revocation](https://tools.ietf.org/html/rfc7009#section-2.2).
///
pub trait RevocableToken {
    ///
    /// The actual token value to be revoked.
    ///
    fn secret(&self) -> &str;

    ///
    /// Indicates the type of the token being revoked, as defined by [RFC 7009, Section 2.1](https://tools.ietf.org/html/rfc7009#section-2.1).
    ///
    /// Implementations should return `Some(...)` values for token types that the target authorization servers are
    /// expected to know (e.g. because they are registered in the [OAuth Token Type Hints Registry](https://tools.ietf.org/html/rfc7009#section-4.1.2))
    /// so that they can potentially optimize their search for the token to be revoked.
    ///
    fn type_hint(&self) -> Option<&str>;
}

///
/// A token representation usable with authorization servers that support [RFC 7009](https://tools.ietf.org/html/rfc7009) token revocation.
///
/// For use with [`revoke_token()`].
///
/// Automatically reports the correct RFC 7009 [`token_type_hint`](https://tools.ietf.org/html/rfc7009#section-2.1) value corresponding to the token type variant used, i.e.
/// `access_token` for [`AccessToken`] and `secret_token` for [`RefreshToken`].
///
/// # Example
///
/// Per [RFC 7009, Section 2](https://tools.ietf.org/html/rfc7009#section-2) prefer revocation by refresh token which,
/// if issued to the client, must be supported by the server, otherwise fallback to access token (which may or may not
/// be supported by the server).
///
/// ```ignore
/// let token_to_revoke: StandardRevocableToken = match token_response.refresh_token() {
///     Some(token) => token.into(),
///     None => token_response.access_token().into(),
/// };
///
/// client
///     .revoke_token(token_to_revoke)
///     .request(http_client)
///     .unwrap();
/// ```
///
/// [`revoke_token()`]: crate::Client::revoke_token()
///
#[derive(Clone, Debug, Deserialize, Serialize)]
#[non_exhaustive]
pub enum StandardRevocableToken {
    /// A representation of an [`AccessToken`] suitable for use with [`revoke_token()`](crate::Client::revoke_token()).
    AccessToken(AccessToken),
    /// A representation of an [`RefreshToken`] suitable for use with [`revoke_token()`](crate::Client::revoke_token()).
    RefreshToken(RefreshToken),
}
impl RevocableToken for StandardRevocableToken {
    fn secret(&self) -> &str {
        match self {
            Self::AccessToken(token) => token.secret(),
            Self::RefreshToken(token) => token.secret(),
        }
    }

    ///
    /// Indicates the type of the token to be revoked, as defined by [RFC 7009, Section 2.1](https://tools.ietf.org/html/rfc7009#section-2.1), i.e.:
    ///
    /// * `access_token`: An access token as defined in [RFC 6749,
    ///   Section 1.4](https://tools.ietf.org/html/rfc6749#section-1.4)
    ///
    /// * `refresh_token`: A refresh token as defined in [RFC 6749,
    ///   Section 1.5](https://tools.ietf.org/html/rfc6749#section-1.5)
    ///
    fn type_hint(&self) -> Option<&str> {
        match self {
            StandardRevocableToken::AccessToken(_) => Some("access_token"),
            StandardRevocableToken::RefreshToken(_) => Some("refresh_token"),
        }
    }
}

impl From<AccessToken> for StandardRevocableToken {
    fn from(token: AccessToken) -> Self {
        Self::AccessToken(token)
    }
}

impl From<&AccessToken> for StandardRevocableToken {
    fn from(token: &AccessToken) -> Self {
        Self::AccessToken(token.clone())
    }
}

impl From<RefreshToken> for StandardRevocableToken {
    fn from(token: RefreshToken) -> Self {
        Self::RefreshToken(token)
    }
}

impl From<&RefreshToken> for StandardRevocableToken {
    fn from(token: &RefreshToken) -> Self {
        Self::RefreshToken(token.clone())
    }
}

///
/// OAuth 2.0 Token Revocation error response types.
///
/// These error types are defined in
/// [Section 2.2.1 of RFC 7009](https://tools.ietf.org/html/rfc7009#section-2.2.1) and
/// [Section 5.2 of RFC 6749](https://tools.ietf.org/html/rfc8628#section-5.2)
///
#[derive(Clone, PartialEq)]
pub enum RevocationErrorResponseType {
    ///
    /// The authorization server does not support the revocation of the presented token type.
    ///
    UnsupportedTokenType,
    ///
    /// The authorization server responded with some other error as defined [RFC 6749](https://tools.ietf.org/html/rfc6749) error.
    ///
    Basic(BasicErrorResponseType),
}
impl RevocationErrorResponseType {
    fn from_str(s: &str) -> Self {
        match BasicErrorResponseType::from_str(s) {
            BasicErrorResponseType::Extension(ext) => match ext.as_str() {
                "unsupported_token_type" => RevocationErrorResponseType::UnsupportedTokenType,
                _ => RevocationErrorResponseType::Basic(BasicErrorResponseType::Extension(ext)),
            },
            basic => RevocationErrorResponseType::Basic(basic),
        }
    }
}
impl AsRef<str> for RevocationErrorResponseType {
    fn as_ref(&self) -> &str {
        match self {
            RevocationErrorResponseType::UnsupportedTokenType => "unsupported_token_type",
            RevocationErrorResponseType::Basic(basic) => basic.as_ref(),
        }
    }
}
impl<'de> serde::Deserialize<'de> for RevocationErrorResponseType {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::de::Deserializer<'de>,
    {
        let variant_str = String::deserialize(deserializer)?;
        Ok(Self::from_str(&variant_str))
    }
}
impl serde::ser::Serialize for RevocationErrorResponseType {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::ser::Serializer,
    {
        serializer.serialize_str(self.as_ref())
    }
}
impl ErrorResponseType for RevocationErrorResponseType {}
impl Debug for RevocationErrorResponseType {
    fn fmt(&self, f: &mut Formatter) -> Result<(), FormatterError> {
        Display::fmt(self, f)
    }
}

impl Display for RevocationErrorResponseType {
    fn fmt(&self, f: &mut Formatter) -> Result<(), FormatterError> {
        write!(f, "{}", self.as_ref())
    }
}
