use std::error::Error;
use std::fmt::Error as FormatterError;
use std::fmt::{Debug, Display, Formatter};
use std::marker::PhantomData;
use std::time::Duration;

use serde::de::DeserializeOwned;
use serde::{Deserialize, Serialize};

use super::{
    DeviceCode, EndUserVerificationUrl, ErrorResponse, ErrorResponseType, RequestTokenError,
    StandardErrorResponse, TokenResponse, TokenType, UserCode,
};
use crate::basic::BasicErrorResponseType;
use crate::types::VerificationUriComplete;

/// The minimum amount of time in seconds that the client SHOULD wait
/// between polling requests to the token endpoint.  If no value is
/// provided, clients MUST use 5 as the default.
fn default_devicecode_interval() -> u64 {
    5
}

///
/// Trait for adding extra fields to the `DeviceAuthorizationResponse`.
///
pub trait ExtraDeviceAuthorizationFields: DeserializeOwned + Debug + Serialize {}

#[derive(Clone, Debug, Deserialize, Serialize)]
///
/// Empty (default) extra token fields.
///
pub struct EmptyExtraDeviceAuthorizationFields {}
impl ExtraDeviceAuthorizationFields for EmptyExtraDeviceAuthorizationFields {}

///
/// Standard OAuth2 device authorization response.
///
#[derive(Clone, Debug, Deserialize, Serialize)]
pub struct DeviceAuthorizationResponse<EF>
where
    EF: ExtraDeviceAuthorizationFields,
{
    /// The device verification code.
    device_code: DeviceCode,

    /// The end-user verification code.
    user_code: UserCode,

    /// The end-user verification URI on the authorization The URI should be
    /// short and easy to remember as end users will be asked to manually type
    /// it into their user agent.
    ///
    /// The `verification_url` alias here is a deviation from the RFC, as
    /// implementations of device code flow predate RFC 8628.
    #[serde(alias = "verification_url")]
    verification_uri: EndUserVerificationUrl,

    /// A verification URI that includes the "user_code" (or other information
    /// with the same function as the "user_code"), which is designed for
    /// non-textual transmission.
    #[serde(skip_serializing_if = "Option::is_none")]
    verification_uri_complete: Option<VerificationUriComplete>,

    /// The lifetime in seconds of the "device_code" and "user_code".
    expires_in: u64,

    /// The minimum amount of time in seconds that the client SHOULD wait
    /// between polling requests to the token endpoint.  If no value is
    /// provided, clients MUST use 5 as the default.
    #[serde(default = "default_devicecode_interval")]
    interval: u64,

    #[serde(bound = "EF: ExtraDeviceAuthorizationFields", flatten)]
    extra_fields: EF,
}

impl<EF> DeviceAuthorizationResponse<EF>
where
    EF: ExtraDeviceAuthorizationFields,
{
    /// The device verification code.
    pub fn device_code(&self) -> &DeviceCode {
        &self.device_code
    }

    /// The end-user verification code.
    pub fn user_code(&self) -> &UserCode {
        &self.user_code
    }

    /// The end-user verification URI on the authorization The URI should be
    /// short and easy to remember as end users will be asked to manually type
    /// it into their user agent.
    pub fn verification_uri(&self) -> &EndUserVerificationUrl {
        &self.verification_uri
    }

    /// A verification URI that includes the "user_code" (or other information
    /// with the same function as the "user_code"), which is designed for
    /// non-textual transmission.
    pub fn verification_uri_complete(&self) -> Option<&VerificationUriComplete> {
        self.verification_uri_complete.as_ref()
    }

    /// The lifetime in seconds of the "device_code" and "user_code".
    pub fn expires_in(&self) -> Duration {
        Duration::from_secs(self.expires_in)
    }

    /// The minimum amount of time in seconds that the client SHOULD wait
    /// between polling requests to the token endpoint.  If no value is
    /// provided, clients MUST use 5 as the default.
    pub fn interval(&self) -> Duration {
        Duration::from_secs(self.interval)
    }

    /// Any extra fields returned on the response.
    pub fn extra_fields(&self) -> &EF {
        &self.extra_fields
    }
}

///
/// Standard implementation of DeviceAuthorizationResponse which throws away
/// extra received response fields.
///
pub type StandardDeviceAuthorizationResponse =
    DeviceAuthorizationResponse<EmptyExtraDeviceAuthorizationFields>;

///
/// Basic access token error types.
///
/// These error types are defined in
/// [Section 5.2 of RFC 6749](https://tools.ietf.org/html/rfc6749#section-5.2) and
/// [Section 3.5 of RFC 6749](https://tools.ietf.org/html/rfc8628#section-3.5)
///
#[derive(Clone, PartialEq)]
pub enum DeviceCodeErrorResponseType {
    ///
    /// The authorization request is still pending as the end user hasn't
    /// yet completed the user-interaction steps.  The client SHOULD repeat the
    /// access token request to the token endpoint.  Before each new request,
    /// the client MUST wait at least the number of seconds specified by the
    /// "interval" parameter of the device authorization response, or 5 seconds
    /// if none was provided, and respect any increase in the polling interval
    /// required by the "slow_down" error.
    ///
    AuthorizationPending,
    ///
    /// A variant of "authorization_pending", the authorization request is
    /// still pending and polling should continue, but the interval MUST be
    /// increased by 5 seconds for this and all subsequent requests.
    SlowDown,
    ///
    /// The authorization request was denied.
    ///
    AccessDenied,
    ///
    /// The "device_code" has expired, and the device authorization session has
    /// concluded.  The client MAY commence a new device authorization request
    /// but SHOULD wait for user interaction before restarting to avoid
    /// unnecessary polling.
    ExpiredToken,
    ///
    /// A Basic response type
    ///
    Basic(BasicErrorResponseType),
}
impl DeviceCodeErrorResponseType {
    fn from_str(s: &str) -> Self {
        match BasicErrorResponseType::from_str(s) {
            BasicErrorResponseType::Extension(ext) => match ext.as_str() {
                "authorization_pending" => DeviceCodeErrorResponseType::AuthorizationPending,
                "slow_down" => DeviceCodeErrorResponseType::SlowDown,
                "access_denied" => DeviceCodeErrorResponseType::AccessDenied,
                "expired_token" => DeviceCodeErrorResponseType::ExpiredToken,
                _ => DeviceCodeErrorResponseType::Basic(BasicErrorResponseType::Extension(ext)),
            },
            basic => DeviceCodeErrorResponseType::Basic(basic),
        }
    }
}
impl AsRef<str> for DeviceCodeErrorResponseType {
    fn as_ref(&self) -> &str {
        match self {
            DeviceCodeErrorResponseType::AuthorizationPending => "authorization_pending",
            DeviceCodeErrorResponseType::SlowDown => "slow_down",
            DeviceCodeErrorResponseType::AccessDenied => "access_denied",
            DeviceCodeErrorResponseType::ExpiredToken => "expired_token",
            DeviceCodeErrorResponseType::Basic(basic) => basic.as_ref(),
        }
    }
}
impl<'de> serde::Deserialize<'de> for DeviceCodeErrorResponseType {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::de::Deserializer<'de>,
    {
        let variant_str = String::deserialize(deserializer)?;
        Ok(Self::from_str(&variant_str))
    }
}
impl serde::ser::Serialize for DeviceCodeErrorResponseType {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::ser::Serializer,
    {
        serializer.serialize_str(self.as_ref())
    }
}
impl ErrorResponseType for DeviceCodeErrorResponseType {}
impl Debug for DeviceCodeErrorResponseType {
    fn fmt(&self, f: &mut Formatter) -> Result<(), FormatterError> {
        Display::fmt(self, f)
    }
}

impl Display for DeviceCodeErrorResponseType {
    fn fmt(&self, f: &mut Formatter) -> Result<(), FormatterError> {
        write!(f, "{}", self.as_ref())
    }
}

///
/// Error response specialization for device code OAuth2 implementation.
///
pub type DeviceCodeErrorResponse = StandardErrorResponse<DeviceCodeErrorResponseType>;

pub(crate) enum DeviceAccessTokenPollResult<TR, RE, TE, TT>
where
    TE: ErrorResponse + 'static,
    TR: TokenResponse<TT>,
    TT: TokenType,
    RE: Error + 'static,
{
    ContinueWithNewPollInterval(Duration),
    Done(Result<TR, RequestTokenError<RE, TE>>, PhantomData<TT>),
}
