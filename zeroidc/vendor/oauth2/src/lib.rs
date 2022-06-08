#![warn(missing_docs)]
//!
//! An extensible, strongly-typed implementation of OAuth2
//! ([RFC 6749](https://tools.ietf.org/html/rfc6749)) including token introspection ([RFC 7662](https://tools.ietf.org/html/rfc7662))
//! and token revocation ([RFC 7009](https://tools.ietf.org/html/rfc7009)).
//!
//! # Contents
//! * [Importing `oauth2`: selecting an HTTP client interface](#importing-oauth2-selecting-an-http-client-interface)
//! * [Getting started: Authorization Code Grant w/ PKCE](#getting-started-authorization-code-grant-w-pkce)
//!   * [Example: Synchronous (blocking) API](#example-synchronous-blocking-api)
//!   * [Example: Async/Await API](#example-asyncawait-api)
//! * [Implicit Grant](#implicit-grant)
//! * [Resource Owner Password Credentials Grant](#resource-owner-password-credentials-grant)
//! * [Client Credentials Grant](#client-credentials-grant)
//! * [Device Code Flow](#device-code-flow)
//! * [Other examples](#other-examples)
//!   * [Contributed Examples](#contributed-examples)
//!
//! # Importing `oauth2`: selecting an HTTP client interface
//!
//! This library offers a flexible HTTP client interface with two modes:
//!  * **Synchronous (blocking)**
//!
//!    The synchronous interface is available for any combination of feature flags.
//!
//!    Example import in `Cargo.toml`:
//!    ```toml
//!    oauth2 = "4.1"
//!    ```
//!
//! For the HTTP client modes described above, the following HTTP client implementations can be
//! used:
//!  * **[`reqwest`]**
//!
//!    The `reqwest` HTTP client supports both modes. By default, `reqwest` 0.11 is enabled,
//!    which supports the synchronous and asynchronous `futures` 0.3 APIs.
//!
//!    Synchronous client: [`reqwest::http_client`]
//!
//!    Async/await `futures` 0.3 client: [`reqwest::async_http_client`]
//!
//!  * **[`curl`]**
//!
//!    The `curl` HTTP client only supports the synchronous HTTP client mode and can be enabled in
//!    `Cargo.toml` via the `curl` feature flag.
//!
//!    Synchronous client: [`curl::http_client`]
//!
//! * **[`ureq`]**
//!
//!    The `ureq` HTTP client is a simple HTTP client with minimal dependencies. It only supports
//!    the synchronous HTTP client mode and can be enabled in `Cargo.toml` via the `ureq` feature flag.
//!
//!  * **Custom**
//!
//!    In addition to the clients above, users may define their own HTTP clients, which must accept
//!    an [`HttpRequest`] and return an [`HttpResponse`] or error. Users writing their own clients
//!    may wish to disable the default `reqwest` 0.10 dependency by specifying
//!    `default-features = false` in `Cargo.toml`:
//!    ```toml
//!    oauth2 = { version = "4.1", default-features = false }
//!    ```
//!
//!    Synchronous HTTP clients should implement the following trait:
//!    ```rust,ignore
//!    FnOnce(HttpRequest) -> Result<HttpResponse, RE>
//!    where RE: std::error::Error + 'static
//!    ```
//!
//!    Async/await HTTP clients should implement the following trait:
//!    ```rust,ignore
//!    FnOnce(HttpRequest) -> F
//!    where
//!      F: Future<Output = Result<HttpResponse, RE>>,
//!      RE: std::error::Error + 'static
//!    ```
//!
//! # Getting started: Authorization Code Grant w/ PKCE
//!
//! This is the most common OAuth2 flow. PKCE is recommended whenever the OAuth2 client has no
//! client secret or has a client secret that cannot remain confidential (e.g., native, mobile, or
//! client-side web applications).
//!
//! ## Example: Synchronous (blocking) API
//!
//! This example works with `oauth2`'s default feature flags, which include `reqwest` 0.10.
//!
//! ```rust,no_run
//! use anyhow;
//! use oauth2::{
//!     AuthorizationCode,
//!     AuthUrl,
//!     ClientId,
//!     ClientSecret,
//!     CsrfToken,
//!     PkceCodeChallenge,
//!     RedirectUrl,
//!     Scope,
//!     TokenResponse,
//!     TokenUrl
//! };
//! use oauth2::basic::BasicClient;
//! use oauth2::reqwest::http_client;
//! use url::Url;
//!
//! # fn err_wrapper() -> Result<(), anyhow::Error> {
//! // Create an OAuth2 client by specifying the client ID, client secret, authorization URL and
//! // token URL.
//! let client =
//!     BasicClient::new(
//!         ClientId::new("client_id".to_string()),
//!         Some(ClientSecret::new("client_secret".to_string())),
//!         AuthUrl::new("http://authorize".to_string())?,
//!         Some(TokenUrl::new("http://token".to_string())?)
//!     )
//!     // Set the URL the user will be redirected to after the authorization process.
//!     .set_redirect_uri(RedirectUrl::new("http://redirect".to_string())?);
//!
//! // Generate a PKCE challenge.
//! let (pkce_challenge, pkce_verifier) = PkceCodeChallenge::new_random_sha256();
//!
//! // Generate the full authorization URL.
//! let (auth_url, csrf_token) = client
//!     .authorize_url(CsrfToken::new_random)
//!     // Set the desired scopes.
//!     .add_scope(Scope::new("read".to_string()))
//!     .add_scope(Scope::new("write".to_string()))
//!     // Set the PKCE code challenge.
//!     .set_pkce_challenge(pkce_challenge)
//!     .url();
//!
//! // This is the URL you should redirect the user to, in order to trigger the authorization
//! // process.
//! println!("Browse to: {}", auth_url);
//!
//! // Once the user has been redirected to the redirect URL, you'll have access to the
//! // authorization code. For security reasons, your code should verify that the `state`
//! // parameter returned by the server matches `csrf_state`.
//!
//! // Now you can trade it for an access token.
//! let token_result =
//!     client
//!         .exchange_code(AuthorizationCode::new("some authorization code".to_string()))
//!         // Set the PKCE code verifier.
//!         .set_pkce_verifier(pkce_verifier)
//!         .request(http_client)?;
//!
//! // Unwrapping token_result will either produce a Token or a RequestTokenError.
//! # Ok(())
//! # }
//! ```
//!
//! ## Example: Async/Await API
//!
//! One can use async/await as follows:
//!
//! ```rust,no_run
//! use anyhow;
//! use oauth2::{
//!     AuthorizationCode,
//!     AuthUrl,
//!     ClientId,
//!     ClientSecret,
//!     CsrfToken,
//!     PkceCodeChallenge,
//!     RedirectUrl,
//!     Scope,
//!     TokenResponse,
//!     TokenUrl
//! };
//! use oauth2::basic::BasicClient;
//! # #[cfg(feature = "reqwest")]
//! use oauth2::reqwest::async_http_client;
//! use url::Url;
//!
//! # #[cfg(feature = "reqwest")]
//! # async fn err_wrapper() -> Result<(), anyhow::Error> {
//! // Create an OAuth2 client by specifying the client ID, client secret, authorization URL and
//! // token URL.
//! let client =
//!     BasicClient::new(
//!         ClientId::new("client_id".to_string()),
//!         Some(ClientSecret::new("client_secret".to_string())),
//!         AuthUrl::new("http://authorize".to_string())?,
//!         Some(TokenUrl::new("http://token".to_string())?)
//!     )
//!     // Set the URL the user will be redirected to after the authorization process.
//!     .set_redirect_uri(RedirectUrl::new("http://redirect".to_string())?);
//!
//! // Generate a PKCE challenge.
//! let (pkce_challenge, pkce_verifier) = PkceCodeChallenge::new_random_sha256();
//!
//! // Generate the full authorization URL.
//! let (auth_url, csrf_token) = client
//!     .authorize_url(CsrfToken::new_random)
//!     // Set the desired scopes.
//!     .add_scope(Scope::new("read".to_string()))
//!     .add_scope(Scope::new("write".to_string()))
//!     // Set the PKCE code challenge.
//!     .set_pkce_challenge(pkce_challenge)
//!     .url();
//!
//! // This is the URL you should redirect the user to, in order to trigger the authorization
//! // process.
//! println!("Browse to: {}", auth_url);
//!
//! // Once the user has been redirected to the redirect URL, you'll have access to the
//! // authorization code. For security reasons, your code should verify that the `state`
//! // parameter returned by the server matches `csrf_state`.
//!
//! // Now you can trade it for an access token.
//! let token_result = client
//!     .exchange_code(AuthorizationCode::new("some authorization code".to_string()))
//!     // Set the PKCE code verifier.
//!     .set_pkce_verifier(pkce_verifier)
//!     .request_async(async_http_client)
//!     .await?;
//!
//! // Unwrapping token_result will either produce a Token or a RequestTokenError.
//! # Ok(())
//! # }
//! ```
//!
//! # Implicit Grant
//!
//! This flow fetches an access token directly from the authorization endpoint. Be sure to
//! understand the security implications of this flow before using it. In most cases, the
//! Authorization Code Grant flow is preferable to the Implicit Grant flow.
//!
//! ## Example
//!
//! ```rust,no_run
//! use anyhow;
//! use oauth2::{
//!     AuthUrl,
//!     ClientId,
//!     ClientSecret,
//!     CsrfToken,
//!     RedirectUrl,
//!     Scope
//! };
//! use oauth2::basic::BasicClient;
//! use url::Url;
//!
//! # fn err_wrapper() -> Result<(), anyhow::Error> {
//! let client =
//!     BasicClient::new(
//!         ClientId::new("client_id".to_string()),
//!         Some(ClientSecret::new("client_secret".to_string())),
//!         AuthUrl::new("http://authorize".to_string())?,
//!         None
//!     );
//!
//! // Generate the full authorization URL.
//! let (auth_url, csrf_token) = client
//!     .authorize_url(CsrfToken::new_random)
//!     .use_implicit_flow()
//!     .url();
//!
//! // This is the URL you should redirect the user to, in order to trigger the authorization
//! // process.
//! println!("Browse to: {}", auth_url);
//!
//! // Once the user has been redirected to the redirect URL, you'll have the access code.
//! // For security reasons, your code should verify that the `state` parameter returned by the
//! // server matches `csrf_state`.
//!
//! # Ok(())
//! # }
//! ```
//!
//! # Resource Owner Password Credentials Grant
//!
//! You can ask for a *password* access token by calling the `Client::exchange_password` method,
//! while including the username and password.
//!
//! ## Example
//!
//! ```rust,no_run
//! use anyhow;
//! use oauth2::{
//!     AuthUrl,
//!     ClientId,
//!     ClientSecret,
//!     ResourceOwnerPassword,
//!     ResourceOwnerUsername,
//!     Scope,
//!     TokenResponse,
//!     TokenUrl
//! };
//! use oauth2::basic::BasicClient;
//! use oauth2::reqwest::http_client;
//! use url::Url;
//!
//! # fn err_wrapper() -> Result<(), anyhow::Error> {
//! let client =
//!     BasicClient::new(
//!         ClientId::new("client_id".to_string()),
//!         Some(ClientSecret::new("client_secret".to_string())),
//!         AuthUrl::new("http://authorize".to_string())?,
//!         Some(TokenUrl::new("http://token".to_string())?)
//!     );
//!
//! let token_result =
//!     client
//!         .exchange_password(
//!             &ResourceOwnerUsername::new("user".to_string()),
//!             &ResourceOwnerPassword::new("pass".to_string())
//!         )
//!         .add_scope(Scope::new("read".to_string()))
//!         .request(http_client)?;
//! # Ok(())
//! # }
//! ```
//!
//! # Client Credentials Grant
//!
//! You can ask for a *client credentials* access token by calling the
//! `Client::exchange_client_credentials` method.
//!
//! ## Example
//!
//! ```rust,no_run
//! use anyhow;
//! use oauth2::{
//!     AuthUrl,
//!     ClientId,
//!     ClientSecret,
//!     Scope,
//!     TokenResponse,
//!     TokenUrl
//! };
//! use oauth2::basic::BasicClient;
//! use oauth2::reqwest::http_client;
//! use url::Url;
//!
//! # fn err_wrapper() -> Result<(), anyhow::Error> {
//! let client =
//!     BasicClient::new(
//!         ClientId::new("client_id".to_string()),
//!         Some(ClientSecret::new("client_secret".to_string())),
//!         AuthUrl::new("http://authorize".to_string())?,
//!         Some(TokenUrl::new("http://token".to_string())?),
//!     );
//!
//! let token_result = client
//!     .exchange_client_credentials()
//!     .add_scope(Scope::new("read".to_string()))
//!     .request(http_client)?;
//! # Ok(())
//! # }
//! ```
//!
//! # Device Code Flow
//!
//! Device Code Flow allows users to sign in on browserless or input-constrained
//! devices.  This is a two-stage process; first a user-code and verification
//! URL are obtained by using the `Client::exchange_client_credentials`
//! method. Those are displayed to the user, then are used in a second client
//! to poll the token endpoint for a token.
//!
//! ## Example
//!
//! ```rust,no_run
//! use anyhow;
//! use oauth2::{
//!     AuthUrl,
//!     ClientId,
//!     ClientSecret,
//!     DeviceAuthorizationUrl,
//!     Scope,
//!     TokenResponse,
//!     TokenUrl
//! };
//! use oauth2::basic::BasicClient;
//! use oauth2::devicecode::StandardDeviceAuthorizationResponse;
//! use oauth2::reqwest::http_client;
//! use url::Url;
//!
//! # fn err_wrapper() -> Result<(), anyhow::Error> {
//! let device_auth_url = DeviceAuthorizationUrl::new("http://deviceauth".to_string())?;
//! let client =
//!     BasicClient::new(
//!         ClientId::new("client_id".to_string()),
//!         Some(ClientSecret::new("client_secret".to_string())),
//!         AuthUrl::new("http://authorize".to_string())?,
//!         Some(TokenUrl::new("http://token".to_string())?),
//!     )
//!     .set_device_authorization_url(device_auth_url);
//!
//! let details: StandardDeviceAuthorizationResponse = client
//!     .exchange_device_code()?
//!     .add_scope(Scope::new("read".to_string()))
//!     .request(http_client)?;
//!
//! println!(
//!     "Open this URL in your browser:\n{}\nand enter the code: {}",
//!     details.verification_uri().to_string(),
//!     details.user_code().secret().to_string()
//! );
//!
//! let token_result =
//!     client
//!     .exchange_device_access_token(&details)
//!     .request(http_client, std::thread::sleep, None)?;
//!
//! # Ok(())
//! # }
//! ```
//!
//! # Other examples
//!
//! More specific implementations are available as part of the examples:
//!
//! - [Google](https://github.com/ramosbugs/oauth2-rs/blob/main/examples/google.rs) (includes token revocation)
//! - [Github](https://github.com/ramosbugs/oauth2-rs/blob/main/examples/github.rs)
//! - [Microsoft Graph](https://github.com/ramosbugs/oauth2-rs/blob/main/examples/msgraph.rs)
//! - [Wunderlist](https://github.com/ramosbugs/oauth2-rs/blob/main/examples/wunderlist.rs)
//!
//! ## Contributed Examples
//!
//! - [`actix-web-oauth2`](https://github.com/pka/actix-web-oauth2) (version 2.x of this crate)
//!
use chrono::serde::ts_seconds_option;
use chrono::{DateTime, Utc};
use std::borrow::Cow;
use std::error::Error;
use std::fmt::Error as FormatterError;
use std::fmt::{Debug, Display, Formatter};
use std::future::Future;
use std::marker::PhantomData;
use std::sync::Arc;
use std::time::Duration;

use http::header::{HeaderMap, HeaderValue, ACCEPT, AUTHORIZATION, CONTENT_TYPE};
use http::status::StatusCode;
use serde::de::DeserializeOwned;
use serde::{Deserialize, Serialize};
use url::{form_urlencoded, Url};

///
/// Basic OAuth2 implementation with no extensions
/// ([RFC 6749](https://tools.ietf.org/html/rfc6749)).
///
pub mod basic;

///
/// HTTP client backed by the [curl](https://crates.io/crates/curl) crate.
/// Requires "curl" feature.
///
#[cfg(all(feature = "curl", not(target_arch = "wasm32")))]
pub mod curl;

#[cfg(all(feature = "curl", target_arch = "wasm32"))]
compile_error!("wasm32 is not supported with the `curl` feature. Use the `reqwest` backend or a custom backend for wasm32 support");

///
/// Device Code Flow OAuth2 implementation
/// ([RFC 8628](https://tools.ietf.org/html/rfc8628)).
///
pub mod devicecode;
use devicecode::{
    DeviceAccessTokenPollResult, DeviceAuthorizationResponse, DeviceCodeErrorResponse,
    DeviceCodeErrorResponseType, ExtraDeviceAuthorizationFields,
};

///
/// OAuth 2.0 Token Revocation implementation
/// ([RFC 7009](https://tools.ietf.org/html/rfc7009)).
///
pub mod revocation;

///
/// Helper methods used by OAuth2 implementations/extensions.
///
pub mod helpers;

///
/// HTTP client backed by the [reqwest](https://crates.io/crates/reqwest) crate.
/// Requires "reqwest" feature.
///
#[cfg(feature = "reqwest")]
pub mod reqwest;

#[cfg(test)]
mod tests;

mod types;

///
/// HTTP client backed by the [ureq](https://crates.io/crates/ureq) crate.
/// Requires "ureq" feature.
///
#[cfg(feature = "ureq")]
pub mod ureq;

///
/// Public re-exports of types used for HTTP client interfaces.
///
pub use http;
pub use url;

pub use types::{
    AccessToken, AuthUrl, AuthorizationCode, ClientId, ClientSecret, CsrfToken,
    DeviceAuthorizationUrl, DeviceCode, EndUserVerificationUrl, IntrospectionUrl,
    PkceCodeChallenge, PkceCodeChallengeMethod, PkceCodeVerifier, RedirectUrl, RefreshToken,
    ResourceOwnerPassword, ResourceOwnerUsername, ResponseType, RevocationUrl, Scope, TokenUrl,
    UserCode,
};

pub use revocation::{RevocableToken, RevocationErrorResponseType, StandardRevocableToken};

const CONTENT_TYPE_JSON: &str = "application/json";
const CONTENT_TYPE_FORMENCODED: &str = "application/x-www-form-urlencoded";

///
/// There was a problem configuring the request.
///
#[non_exhaustive]
#[derive(Debug, thiserror::Error)]
pub enum ConfigurationError {
    ///
    /// The endpoint URL tp be contacted is missing.
    ///
    #[error("No {0} endpoint URL specified")]
    MissingUrl(&'static str),
    ///
    /// The endpoint URL to be contacted MUST be HTTPS.
    ///
    #[error("Scheme for {0} endpoint URL must be HTTPS")]
    InsecureUrl(&'static str),
}

///
/// Indicates whether requests to the authorization server should use basic authentication or
/// include the parameters in the request body for requests in which either is valid.
///
/// The default AuthType is *BasicAuth*, following the recommendation of
/// [Section 2.3.1 of RFC 6749](https://tools.ietf.org/html/rfc6749#section-2.3.1).
///
#[derive(Clone, Debug)]
#[non_exhaustive]
pub enum AuthType {
    /// The client_id and client_secret will be included as part of the request body.
    RequestBody,
    /// The client_id and client_secret will be included using the basic auth authentication scheme.
    BasicAuth,
}

///
/// Stores the configuration for an OAuth2 client.
///
/// # Error Types
///
/// To enable compile time verification that only the correct and complete set of errors for the `Client` function being
/// invoked are exposed to the caller, the `Client` type is specialized on multiple implementations of the
/// [`ErrorResponse`] trait. The exact [`ErrorResponse`] implementation returned varies by the RFC that the invoked
/// `Client` function implements:
///
///   - Generic type `TE` (aka Token Error) for errors defined by [RFC 6749 OAuth 2.0 Authorization Framework](https://tools.ietf.org/html/rfc6749).
///   - Generic type `TRE` (aka Token Revocation Error) for errors defined by [RFC 7009 OAuth 2.0 Token Revocation](https://tools.ietf.org/html/rfc7009).
///
/// For example when revoking a token, error code `unsupported_token_type` (from RFC 7009) may be returned:
/// ```rust
/// # use thiserror::Error;
/// # use http::status::StatusCode;
/// # use http::header::{HeaderValue, CONTENT_TYPE};
/// # use oauth2::{*, basic::*};
/// # let client = BasicClient::new(
/// #     ClientId::new("aaa".to_string()),
/// #     Some(ClientSecret::new("bbb".to_string())),
/// #     AuthUrl::new("https://example.com/auth".to_string()).unwrap(),
/// #     Some(TokenUrl::new("https://example.com/token".to_string()).unwrap()),
/// # )
/// # .set_revocation_uri(RevocationUrl::new("https://revocation/url".to_string()).unwrap());
/// #
/// # #[derive(Debug, Error)]
/// # enum FakeError {
/// #     #[error("error")]
/// #     Err,
/// # }
/// #
/// # let http_client = |_| -> Result<HttpResponse, FakeError> {
/// #     Ok(HttpResponse {
/// #         status_code: StatusCode::BAD_REQUEST,
/// #         headers: vec![(
/// #             CONTENT_TYPE,
/// #             HeaderValue::from_str("application/json").unwrap(),
/// #         )]
/// #         .into_iter()
/// #         .collect(),
/// #         body: "{\"error\": \"unsupported_token_type\", \"error_description\": \"stuff happened\", \
/// #                \"error_uri\": \"https://errors\"}"
/// #             .to_string()
/// #             .into_bytes(),
/// #     })
/// # };
/// #
/// let res = client
///     .revoke_token(AccessToken::new("some token".to_string()).into())
///     .unwrap()
///     .request(http_client);
///
/// assert!(matches!(res, Err(
///     RequestTokenError::ServerResponse(err)) if matches!(err.error(),
///         RevocationErrorResponseType::UnsupportedTokenType)));
/// ```
///
#[derive(Clone, Debug)]
pub struct Client<TE, TR, TT, TIR, RT, TRE>
where
    TE: ErrorResponse,
    TR: TokenResponse<TT>,
    TT: TokenType,
    TIR: TokenIntrospectionResponse<TT>,
    RT: RevocableToken,
    TRE: ErrorResponse,
{
    client_id: ClientId,
    client_secret: Option<ClientSecret>,
    auth_url: AuthUrl,
    auth_type: AuthType,
    token_url: Option<TokenUrl>,
    redirect_url: Option<RedirectUrl>,
    introspection_url: Option<IntrospectionUrl>,
    revocation_url: Option<RevocationUrl>,
    device_authorization_url: Option<DeviceAuthorizationUrl>,
    phantom: PhantomData<(TE, TR, TT, TIR, RT, TRE)>,
}

impl<TE, TR, TT, TIR, RT, TRE> Client<TE, TR, TT, TIR, RT, TRE>
where
    TE: ErrorResponse + 'static,
    TR: TokenResponse<TT>,
    TT: TokenType,
    TIR: TokenIntrospectionResponse<TT>,
    RT: RevocableToken,
    TRE: ErrorResponse + 'static,
{
    ///
    /// Initializes an OAuth2 client with the fields common to most OAuth2 flows.
    ///
    /// # Arguments
    ///
    /// * `client_id` -  Client ID
    /// * `client_secret` -  Optional client secret. A client secret is generally used for private
    ///   (server-side) OAuth2 clients and omitted from public (client-side or native app) OAuth2
    ///   clients (see [RFC 8252](https://tools.ietf.org/html/rfc8252)).
    /// * `auth_url` -  Authorization endpoint: used by the client to obtain authorization from
    ///   the resource owner via user-agent redirection. This URL is used in all standard OAuth2
    ///   flows except the [Resource Owner Password Credentials
    ///   Grant](https://tools.ietf.org/html/rfc6749#section-4.3) and the
    ///   [Client Credentials Grant](https://tools.ietf.org/html/rfc6749#section-4.4).
    /// * `token_url` - Token endpoint: used by the client to exchange an authorization grant
    ///   (code) for an access token, typically with client authentication. This URL is used in
    ///   all standard OAuth2 flows except the
    ///   [Implicit Grant](https://tools.ietf.org/html/rfc6749#section-4.2). If this value is set
    ///   to `None`, the `exchange_*` methods will return `Err(RequestTokenError::Other(_))`.
    ///
    pub fn new(
        client_id: ClientId,
        client_secret: Option<ClientSecret>,
        auth_url: AuthUrl,
        token_url: Option<TokenUrl>,
    ) -> Self {
        Client {
            client_id,
            client_secret,
            auth_url,
            auth_type: AuthType::BasicAuth,
            token_url,
            redirect_url: None,
            introspection_url: None,
            revocation_url: None,
            device_authorization_url: None,
            phantom: PhantomData,
        }
    }

    ///
    /// Configures the type of client authentication used for communicating with the authorization
    /// server.
    ///
    /// The default is to use HTTP Basic authentication, as recommended in
    /// [Section 2.3.1 of RFC 6749](https://tools.ietf.org/html/rfc6749#section-2.3.1).
    ///
    pub fn set_auth_type(mut self, auth_type: AuthType) -> Self {
        self.auth_type = auth_type;

        self
    }

    ///
    /// Sets the redirect URL used by the authorization endpoint.
    ///
    pub fn set_redirect_uri(mut self, redirect_url: RedirectUrl) -> Self {
        self.redirect_url = Some(redirect_url);

        self
    }

    ///
    /// Sets the introspection URL for contacting the ([RFC 7662](https://tools.ietf.org/html/rfc7662))
    /// introspection endpoint.
    ///
    pub fn set_introspection_uri(mut self, introspection_url: IntrospectionUrl) -> Self {
        self.introspection_url = Some(introspection_url);

        self
    }

    ///
    /// Sets the revocation URL for contacting the revocation endpoint ([RFC 7009](https://tools.ietf.org/html/rfc7009)).
    ///
    /// See: [`revoke_token()`](Self::revoke_token())
    ///
    pub fn set_revocation_uri(mut self, revocation_url: RevocationUrl) -> Self {
        self.revocation_url = Some(revocation_url);

        self
    }

    ///
    /// Sets the the device authorization URL used by the device authorization endpoint.
    /// Used for Device Code Flow, as per [RFC 8628](https://tools.ietf.org/html/rfc8628).
    ///
    pub fn set_device_authorization_url(
        mut self,
        device_authorization_url: DeviceAuthorizationUrl,
    ) -> Self {
        self.device_authorization_url = Some(device_authorization_url);

        self
    }

    ///
    /// Generates an authorization URL for a new authorization request.
    ///
    /// # Arguments
    ///
    /// * `state_fn` - A function that returns an opaque value used by the client to maintain state
    ///   between the request and callback. The authorization server includes this value when
    ///   redirecting the user-agent back to the client.
    ///
    /// # Security Warning
    ///
    /// Callers should use a fresh, unpredictable `state` for each authorization request and verify
    /// that this value matches the `state` parameter passed by the authorization server to the
    /// redirect URI. Doing so mitigates
    /// [Cross-Site Request Forgery](https://tools.ietf.org/html/rfc6749#section-10.12)
    ///  attacks. To disable CSRF protections (NOT recommended), use `insecure::authorize_url`
    ///  instead.
    ///
    pub fn authorize_url<S>(&self, state_fn: S) -> AuthorizationRequest
    where
        S: FnOnce() -> CsrfToken,
    {
        AuthorizationRequest {
            auth_url: &self.auth_url,
            client_id: &self.client_id,
            extra_params: Vec::new(),
            pkce_challenge: None,
            redirect_url: self.redirect_url.as_ref().map(Cow::Borrowed),
            response_type: "code".into(),
            scopes: Vec::new(),
            state: state_fn(),
        }
    }

    ///
    /// Exchanges a code produced by a successful authorization process with an access token.
    ///
    /// Acquires ownership of the `code` because authorization codes may only be used once to
    /// retrieve an access token from the authorization server.
    ///
    /// See https://tools.ietf.org/html/rfc6749#section-4.1.3
    ///
    pub fn exchange_code(&self, code: AuthorizationCode) -> CodeTokenRequest<TE, TR, TT> {
        CodeTokenRequest {
            auth_type: &self.auth_type,
            client_id: &self.client_id,
            client_secret: self.client_secret.as_ref(),
            code,
            extra_params: Vec::new(),
            pkce_verifier: None,
            token_url: self.token_url.as_ref(),
            redirect_url: self.redirect_url.as_ref().map(Cow::Borrowed),
            _phantom: PhantomData,
        }
    }

    ///
    /// Requests an access token for the *password* grant type.
    ///
    /// See https://tools.ietf.org/html/rfc6749#section-4.3.2
    ///
    pub fn exchange_password<'a, 'b>(
        &'a self,
        username: &'b ResourceOwnerUsername,
        password: &'b ResourceOwnerPassword,
    ) -> PasswordTokenRequest<'b, TE, TR, TT>
    where
        'a: 'b,
    {
        PasswordTokenRequest::<'b> {
            auth_type: &self.auth_type,
            client_id: &self.client_id,
            client_secret: self.client_secret.as_ref(),
            username,
            password,
            extra_params: Vec::new(),
            scopes: Vec::new(),
            token_url: self.token_url.as_ref(),
            _phantom: PhantomData,
        }
    }

    ///
    /// Requests an access token for the *client credentials* grant type.
    ///
    /// See https://tools.ietf.org/html/rfc6749#section-4.4.2
    ///
    pub fn exchange_client_credentials(&self) -> ClientCredentialsTokenRequest<TE, TR, TT> {
        ClientCredentialsTokenRequest {
            auth_type: &self.auth_type,
            client_id: &self.client_id,
            client_secret: self.client_secret.as_ref(),
            extra_params: Vec::new(),
            scopes: Vec::new(),
            token_url: self.token_url.as_ref(),
            _phantom: PhantomData,
        }
    }

    ///
    /// Exchanges a refresh token for an access token
    ///
    /// See https://tools.ietf.org/html/rfc6749#section-6
    ///
    pub fn exchange_refresh_token<'a, 'b>(
        &'a self,
        refresh_token: &'b RefreshToken,
    ) -> RefreshTokenRequest<'b, TE, TR, TT>
    where
        'a: 'b,
    {
        RefreshTokenRequest {
            auth_type: &self.auth_type,
            client_id: &self.client_id,
            client_secret: self.client_secret.as_ref(),
            extra_params: Vec::new(),
            refresh_token,
            scopes: Vec::new(),
            token_url: self.token_url.as_ref(),
            _phantom: PhantomData,
        }
    }

    ///
    /// Perform a device authorization request as per
    /// https://tools.ietf.org/html/rfc8628#section-3.1
    ///
    pub fn exchange_device_code(
        &self,
    ) -> Result<DeviceAuthorizationRequest<TE>, ConfigurationError> {
        Ok(DeviceAuthorizationRequest {
            auth_type: &self.auth_type,
            client_id: &self.client_id,
            client_secret: self.client_secret.as_ref(),
            extra_params: Vec::new(),
            scopes: Vec::new(),
            device_authorization_url: self
                .device_authorization_url
                .as_ref()
                .ok_or(ConfigurationError::MissingUrl("device authorization_url"))?,
            _phantom: PhantomData,
        })
    }

    ///
    /// Perform a device access token request as per
    /// https://tools.ietf.org/html/rfc8628#section-3.4
    ///
    pub fn exchange_device_access_token<'a, 'b, 'c, EF>(
        &'a self,
        auth_response: &'b DeviceAuthorizationResponse<EF>,
    ) -> DeviceAccessTokenRequest<'b, 'c, TR, TT, EF>
    where
        'a: 'b,
        EF: ExtraDeviceAuthorizationFields,
    {
        DeviceAccessTokenRequest {
            auth_type: &self.auth_type,
            client_id: &self.client_id,
            client_secret: self.client_secret.as_ref(),
            extra_params: Vec::new(),
            token_url: self.token_url.as_ref(),
            dev_auth_resp: auth_response,
            time_fn: Arc::new(Utc::now),
            _phantom: PhantomData,
        }
    }

    ///
    /// Query the authorization server [`RFC 7662 compatible`](https://tools.ietf.org/html/rfc7662) introspection
    /// endpoint to determine the set of metadata for a previously received token.
    ///
    /// Requires that [`set_introspection_uri()`](Self::set_introspection_uri()) have already been called to set the
    /// introspection endpoint URL.
    ///
    /// Attempting to submit the generated request without calling [`set_introspection_uri()`](Self::set_introspection_uri())
    /// first will result in an error.
    ///
    pub fn introspect<'a>(
        &'a self,
        token: &'a AccessToken,
    ) -> Result<IntrospectionRequest<'a, TE, TIR, TT>, ConfigurationError> {
        Ok(IntrospectionRequest {
            auth_type: &self.auth_type,
            client_id: &self.client_id,
            client_secret: self.client_secret.as_ref(),
            extra_params: Vec::new(),
            introspection_url: self
                .introspection_url
                .as_ref()
                .ok_or(ConfigurationError::MissingUrl("introspection"))?,
            token,
            token_type_hint: None,
            _phantom: PhantomData,
        })
    }

    ///
    /// Attempts to revoke the given previously received token using an [RFC 7009 OAuth 2.0 Token Revocation](https://tools.ietf.org/html/rfc7009)
    /// compatible endpoint.
    ///
    /// Requires that [`set_revocation_uri()`](Self::set_revocation_uri()) have already been called to set the
    /// revocation endpoint URL.
    ///
    /// Attempting to submit the generated request without calling [`set_revocation_uri()`](Self::set_revocation_uri())
    /// first will result in an error.
    ///
    pub fn revoke_token(
        &self,
        token: RT,
    ) -> Result<RevocationRequest<RT, TRE>, ConfigurationError> {
        // https://tools.ietf.org/html/rfc7009#section-2 states:
        //   "The client requests the revocation of a particular token by making an
        //    HTTP POST request to the token revocation endpoint URL.  This URL
        //    MUST conform to the rules given in [RFC6749], Section 3.1.  Clients
        //    MUST verify that the URL is an HTTPS URL."
        let revocation_url = match self.revocation_url.as_ref() {
            Some(url) if url.url().scheme() == "https" => Ok(url),
            Some(_) => Err(ConfigurationError::InsecureUrl("revocation")),
            None => Err(ConfigurationError::MissingUrl("revocation")),
        }?;

        Ok(RevocationRequest {
            auth_type: &self.auth_type,
            client_id: &self.client_id,
            client_secret: self.client_secret.as_ref(),
            extra_params: Vec::new(),
            revocation_url,
            token,
            _phantom: PhantomData,
        })
    }

    ///
    /// Returns the Client ID.
    ///
    pub fn client_id(&self) -> &ClientId {
        &self.client_id
    }

    ///
    /// Returns the authorization endpoint.
    ///
    pub fn auth_url(&self) -> &AuthUrl {
        &self.auth_url
    }

    ///
    /// Returns the type of client authentication used for communicating with the authorization
    /// server.
    ///
    pub fn auth_type(&self) -> &AuthType {
        &self.auth_type
    }

    ///
    /// Returns the token endpoint.
    ///
    pub fn token_url(&self) -> Option<&TokenUrl> {
        self.token_url.as_ref()
    }

    ///
    /// Returns the redirect URL used by the authorization endpoint.
    ///
    pub fn redirect_url(&self) -> Option<&RedirectUrl> {
        self.redirect_url.as_ref()
    }

    ///
    /// Returns the introspection URL for contacting the ([RFC 7662](https://tools.ietf.org/html/rfc7662))
    /// introspection endpoint.
    ///
    pub fn introspection_url(&self) -> Option<&IntrospectionUrl> {
        self.introspection_url.as_ref()
    }

    ///
    /// Returns the revocation URL for contacting the revocation endpoint ([RFC 7009](https://tools.ietf.org/html/rfc7009)).
    ///
    /// See: [`revoke_token()`](Self::revoke_token())
    ///
    pub fn revocation_url(&self) -> Option<&RevocationUrl> {
        self.revocation_url.as_ref()
    }

    ///
    /// Returns the the device authorization URL used by the device authorization endpoint.
    ///
    pub fn device_authorization_url(&self) -> Option<&DeviceAuthorizationUrl> {
        self.device_authorization_url.as_ref()
    }
}

///
/// A request to the authorization endpoint
///
#[derive(Debug)]
pub struct AuthorizationRequest<'a> {
    auth_url: &'a AuthUrl,
    client_id: &'a ClientId,
    extra_params: Vec<(Cow<'a, str>, Cow<'a, str>)>,
    pkce_challenge: Option<PkceCodeChallenge>,
    redirect_url: Option<Cow<'a, RedirectUrl>>,
    response_type: Cow<'a, str>,
    scopes: Vec<Cow<'a, Scope>>,
    state: CsrfToken,
}
impl<'a> AuthorizationRequest<'a> {
    ///
    /// Appends a new scope to the authorization URL.
    ///
    pub fn add_scope(mut self, scope: Scope) -> Self {
        self.scopes.push(Cow::Owned(scope));
        self
    }

    ///
    /// Appends a collection of scopes to the token request.
    ///
    pub fn add_scopes<I>(mut self, scopes: I) -> Self
    where
        I: IntoIterator<Item = Scope>,
    {
        self.scopes.extend(scopes.into_iter().map(Cow::Owned));
        self
    }

    ///
    /// Appends an extra param to the authorization URL.
    ///
    /// This method allows extensions to be used without direct support from
    /// this crate. If `name` conflicts with a parameter managed by this crate, the
    /// behavior is undefined. In particular, do not set parameters defined by
    /// [RFC 6749](https://tools.ietf.org/html/rfc6749) or
    /// [RFC 7636](https://tools.ietf.org/html/rfc7636).
    ///
    /// # Security Warning
    ///
    /// Callers should follow the security recommendations for any OAuth2 extensions used with
    /// this function, which are beyond the scope of
    /// [RFC 6749](https://tools.ietf.org/html/rfc6749).
    ///
    pub fn add_extra_param<N, V>(mut self, name: N, value: V) -> Self
    where
        N: Into<Cow<'a, str>>,
        V: Into<Cow<'a, str>>,
    {
        self.extra_params.push((name.into(), value.into()));
        self
    }

    ///
    /// Enables the [Implicit Grant](https://tools.ietf.org/html/rfc6749#section-4.2) flow.
    ///
    pub fn use_implicit_flow(mut self) -> Self {
        self.response_type = "token".into();
        self
    }

    ///
    /// Enables custom flows other than the `code` and `token` (implicit flow) grant.
    ///
    pub fn set_response_type(mut self, response_type: &ResponseType) -> Self {
        self.response_type = (&**response_type).to_owned().into();
        self
    }

    ///
    /// Enables the use of [Proof Key for Code Exchange](https://tools.ietf.org/html/rfc7636)
    /// (PKCE).
    ///
    /// PKCE is *highly recommended* for all public clients (i.e., those for which there
    /// is no client secret or for which the client secret is distributed with the client,
    /// such as in a native, mobile app, or browser app).
    ///
    pub fn set_pkce_challenge(mut self, pkce_code_challenge: PkceCodeChallenge) -> Self {
        self.pkce_challenge = Some(pkce_code_challenge);
        self
    }

    ///
    /// Overrides the `redirect_url` to the one specified.
    ///
    pub fn set_redirect_uri(mut self, redirect_url: Cow<'a, RedirectUrl>) -> Self {
        self.redirect_url = Some(redirect_url);
        self
    }

    ///
    /// Returns the full authorization URL and CSRF state for this authorization
    /// request.
    ///
    pub fn url(self) -> (Url, CsrfToken) {
        let scopes = self
            .scopes
            .iter()
            .map(|s| s.to_string())
            .collect::<Vec<_>>()
            .join(" ");

        let url = {
            let mut pairs: Vec<(&str, &str)> = vec![
                ("response_type", self.response_type.as_ref()),
                ("client_id", &self.client_id),
                ("state", self.state.secret()),
            ];

            if let Some(ref pkce_challenge) = self.pkce_challenge {
                pairs.push(("code_challenge", &pkce_challenge.as_str()));
                pairs.push(("code_challenge_method", &pkce_challenge.method().as_str()));
            }

            if let Some(ref redirect_url) = self.redirect_url {
                pairs.push(("redirect_uri", redirect_url.as_str()));
            }

            if !scopes.is_empty() {
                pairs.push(("scope", &scopes));
            }

            let mut url: Url = self.auth_url.url().to_owned();

            url.query_pairs_mut()
                .extend_pairs(pairs.iter().map(|&(k, v)| (k, &v[..])));

            url.query_pairs_mut()
                .extend_pairs(self.extra_params.iter().cloned());
            url
        };

        (url, self.state)
    }
}

///
/// An HTTP request.
///
#[derive(Clone, Debug)]
pub struct HttpRequest {
    // These are all owned values so that the request can safely be passed between
    // threads.
    /// URL to which the HTTP request is being made.
    pub url: Url,
    /// HTTP request method for this request.
    pub method: http::method::Method,
    /// HTTP request headers to send.
    pub headers: HeaderMap,
    /// HTTP request body (typically for POST requests only).
    pub body: Vec<u8>,
}

///
/// An HTTP response.
///
#[derive(Clone, Debug)]
pub struct HttpResponse {
    /// HTTP status code returned by the server.
    pub status_code: http::status::StatusCode,
    /// HTTP response headers returned by the server.
    pub headers: HeaderMap,
    /// HTTP response body returned by the server.
    pub body: Vec<u8>,
}

///
/// A request to exchange an authorization code for an access token.
///
/// See https://tools.ietf.org/html/rfc6749#section-4.1.3.
///
#[derive(Debug)]
pub struct CodeTokenRequest<'a, TE, TR, TT>
where
    TE: ErrorResponse,
    TR: TokenResponse<TT>,
    TT: TokenType,
{
    auth_type: &'a AuthType,
    client_id: &'a ClientId,
    client_secret: Option<&'a ClientSecret>,
    code: AuthorizationCode,
    extra_params: Vec<(Cow<'a, str>, Cow<'a, str>)>,
    pkce_verifier: Option<PkceCodeVerifier>,
    token_url: Option<&'a TokenUrl>,
    redirect_url: Option<Cow<'a, RedirectUrl>>,
    _phantom: PhantomData<(TE, TR, TT)>,
}
impl<'a, TE, TR, TT> CodeTokenRequest<'a, TE, TR, TT>
where
    TE: ErrorResponse + 'static,
    TR: TokenResponse<TT>,
    TT: TokenType,
{
    ///
    /// Appends an extra param to the token request.
    ///
    /// This method allows extensions to be used without direct support from
    /// this crate. If `name` conflicts with a parameter managed by this crate, the
    /// behavior is undefined. In particular, do not set parameters defined by
    /// [RFC 6749](https://tools.ietf.org/html/rfc6749) or
    /// [RFC 7636](https://tools.ietf.org/html/rfc7636).
    ///
    /// # Security Warning
    ///
    /// Callers should follow the security recommendations for any OAuth2 extensions used with
    /// this function, which are beyond the scope of
    /// [RFC 6749](https://tools.ietf.org/html/rfc6749).
    ///
    pub fn add_extra_param<N, V>(mut self, name: N, value: V) -> Self
    where
        N: Into<Cow<'a, str>>,
        V: Into<Cow<'a, str>>,
    {
        self.extra_params.push((name.into(), value.into()));
        self
    }

    ///
    /// Completes the [Proof Key for Code Exchange](https://tools.ietf.org/html/rfc7636)
    /// (PKCE) protocol flow.
    ///
    /// This method must be called if `set_pkce_challenge` was used during the authorization
    /// request.
    ///
    pub fn set_pkce_verifier(mut self, pkce_verifier: PkceCodeVerifier) -> Self {
        self.pkce_verifier = Some(pkce_verifier);
        self
    }

    ///
    /// Overrides the `redirect_url` to the one specified.
    ///
    pub fn set_redirect_uri(mut self, redirect_url: Cow<'a, RedirectUrl>) -> Self {
        self.redirect_url = Some(redirect_url);
        self
    }

    fn prepare_request<RE>(self) -> Result<HttpRequest, RequestTokenError<RE, TE>>
    where
        RE: Error + 'static,
    {
        let mut params = vec![
            ("grant_type", "authorization_code"),
            ("code", self.code.secret()),
        ];
        if let Some(ref pkce_verifier) = self.pkce_verifier {
            params.push(("code_verifier", pkce_verifier.secret()));
        }

        Ok(endpoint_request(
            self.auth_type,
            self.client_id,
            self.client_secret,
            &self.extra_params,
            self.redirect_url,
            None,
            self.token_url
                .ok_or_else(|| RequestTokenError::Other("no token_url provided".to_string()))?
                .url(),
            params,
        ))
    }

    ///
    /// Synchronously sends the request to the authorization server and awaits a response.
    ///
    pub fn request<F, RE>(self, http_client: F) -> Result<TR, RequestTokenError<RE, TE>>
    where
        F: FnOnce(HttpRequest) -> Result<HttpResponse, RE>,
        RE: Error + 'static,
    {
        http_client(self.prepare_request()?)
            .map_err(RequestTokenError::Request)
            .and_then(endpoint_response)
    }

    ///
    /// Asynchronously sends the request to the authorization server and returns a Future.
    ///
    pub async fn request_async<C, F, RE>(
        self,
        http_client: C,
    ) -> Result<TR, RequestTokenError<RE, TE>>
    where
        C: FnOnce(HttpRequest) -> F,
        F: Future<Output = Result<HttpResponse, RE>>,
        RE: Error + 'static,
    {
        let http_request = self.prepare_request()?;
        let http_response = http_client(http_request)
            .await
            .map_err(RequestTokenError::Request)?;
        endpoint_response(http_response)
    }
}

///
/// A request to exchange a refresh token for an access token.
///
/// See https://tools.ietf.org/html/rfc6749#section-6.
///
#[derive(Debug)]
pub struct RefreshTokenRequest<'a, TE, TR, TT>
where
    TE: ErrorResponse,
    TR: TokenResponse<TT>,
    TT: TokenType,
{
    auth_type: &'a AuthType,
    client_id: &'a ClientId,
    client_secret: Option<&'a ClientSecret>,
    extra_params: Vec<(Cow<'a, str>, Cow<'a, str>)>,
    refresh_token: &'a RefreshToken,
    scopes: Vec<Cow<'a, Scope>>,
    token_url: Option<&'a TokenUrl>,
    _phantom: PhantomData<(TE, TR, TT)>,
}
impl<'a, TE, TR, TT> RefreshTokenRequest<'a, TE, TR, TT>
where
    TE: ErrorResponse + 'static,
    TR: TokenResponse<TT>,
    TT: TokenType,
{
    ///
    /// Appends an extra param to the token request.
    ///
    /// This method allows extensions to be used without direct support from
    /// this crate. If `name` conflicts with a parameter managed by this crate, the
    /// behavior is undefined. In particular, do not set parameters defined by
    /// [RFC 6749](https://tools.ietf.org/html/rfc6749) or
    /// [RFC 7636](https://tools.ietf.org/html/rfc7636).
    ///
    /// # Security Warning
    ///
    /// Callers should follow the security recommendations for any OAuth2 extensions used with
    /// this function, which are beyond the scope of
    /// [RFC 6749](https://tools.ietf.org/html/rfc6749).
    ///
    pub fn add_extra_param<N, V>(mut self, name: N, value: V) -> Self
    where
        N: Into<Cow<'a, str>>,
        V: Into<Cow<'a, str>>,
    {
        self.extra_params.push((name.into(), value.into()));
        self
    }

    ///
    /// Appends a new scope to the token request.
    ///
    pub fn add_scope(mut self, scope: Scope) -> Self {
        self.scopes.push(Cow::Owned(scope));
        self
    }

    ///
    /// Appends a collection of scopes to the token request.
    ///
    pub fn add_scopes<I>(mut self, scopes: I) -> Self
    where
        I: IntoIterator<Item = Scope>,
    {
        self.scopes.extend(scopes.into_iter().map(Cow::Owned));
        self
    }

    ///
    /// Synchronously sends the request to the authorization server and awaits a response.
    ///
    pub fn request<F, RE>(self, http_client: F) -> Result<TR, RequestTokenError<RE, TE>>
    where
        F: FnOnce(HttpRequest) -> Result<HttpResponse, RE>,
        RE: Error + 'static,
    {
        http_client(self.prepare_request()?)
            .map_err(RequestTokenError::Request)
            .and_then(endpoint_response)
    }
    ///
    /// Asynchronously sends the request to the authorization server and awaits a response.
    ///
    pub async fn request_async<C, F, RE>(
        self,
        http_client: C,
    ) -> Result<TR, RequestTokenError<RE, TE>>
    where
        C: FnOnce(HttpRequest) -> F,
        F: Future<Output = Result<HttpResponse, RE>>,
        RE: Error + 'static,
    {
        let http_request = self.prepare_request()?;
        let http_response = http_client(http_request)
            .await
            .map_err(RequestTokenError::Request)?;
        endpoint_response(http_response)
    }

    fn prepare_request<RE>(&self) -> Result<HttpRequest, RequestTokenError<RE, TE>>
    where
        RE: Error + 'static,
    {
        Ok(endpoint_request(
            self.auth_type,
            self.client_id,
            self.client_secret,
            &self.extra_params,
            None,
            Some(&self.scopes),
            self.token_url
                .ok_or_else(|| RequestTokenError::Other("no token_url provided".to_string()))?
                .url(),
            vec![
                ("grant_type", "refresh_token"),
                ("refresh_token", self.refresh_token.secret()),
            ],
        ))
    }
}

///
/// A request to exchange resource owner credentials for an access token.
///
/// See https://tools.ietf.org/html/rfc6749#section-4.3.
///
#[derive(Debug)]
pub struct PasswordTokenRequest<'a, TE, TR, TT>
where
    TE: ErrorResponse,
    TR: TokenResponse<TT>,
    TT: TokenType,
{
    auth_type: &'a AuthType,
    client_id: &'a ClientId,
    client_secret: Option<&'a ClientSecret>,
    extra_params: Vec<(Cow<'a, str>, Cow<'a, str>)>,
    username: &'a ResourceOwnerUsername,
    password: &'a ResourceOwnerPassword,
    scopes: Vec<Cow<'a, Scope>>,
    token_url: Option<&'a TokenUrl>,
    _phantom: PhantomData<(TE, TR, TT)>,
}
impl<'a, TE, TR, TT> PasswordTokenRequest<'a, TE, TR, TT>
where
    TE: ErrorResponse + 'static,
    TR: TokenResponse<TT>,
    TT: TokenType,
{
    ///
    /// Appends an extra param to the token request.
    ///
    /// This method allows extensions to be used without direct support from
    /// this crate. If `name` conflicts with a parameter managed by this crate, the
    /// behavior is undefined. In particular, do not set parameters defined by
    /// [RFC 6749](https://tools.ietf.org/html/rfc6749) or
    /// [RFC 7636](https://tools.ietf.org/html/rfc7636).
    ///
    /// # Security Warning
    ///
    /// Callers should follow the security recommendations for any OAuth2 extensions used with
    /// this function, which are beyond the scope of
    /// [RFC 6749](https://tools.ietf.org/html/rfc6749).
    ///
    pub fn add_extra_param<N, V>(mut self, name: N, value: V) -> Self
    where
        N: Into<Cow<'a, str>>,
        V: Into<Cow<'a, str>>,
    {
        self.extra_params.push((name.into(), value.into()));
        self
    }

    ///
    /// Appends a new scope to the token request.
    ///
    pub fn add_scope(mut self, scope: Scope) -> Self {
        self.scopes.push(Cow::Owned(scope));
        self
    }

    ///
    /// Appends a collection of scopes to the token request.
    ///
    pub fn add_scopes<I>(mut self, scopes: I) -> Self
    where
        I: IntoIterator<Item = Scope>,
    {
        self.scopes.extend(scopes.into_iter().map(Cow::Owned));
        self
    }

    ///
    /// Synchronously sends the request to the authorization server and awaits a response.
    ///
    pub fn request<F, RE>(self, http_client: F) -> Result<TR, RequestTokenError<RE, TE>>
    where
        F: FnOnce(HttpRequest) -> Result<HttpResponse, RE>,
        RE: Error + 'static,
    {
        http_client(self.prepare_request()?)
            .map_err(RequestTokenError::Request)
            .and_then(endpoint_response)
    }

    ///
    /// Asynchronously sends the request to the authorization server and awaits a response.
    ///
    pub async fn request_async<C, F, RE>(
        self,
        http_client: C,
    ) -> Result<TR, RequestTokenError<RE, TE>>
    where
        C: FnOnce(HttpRequest) -> F,
        F: Future<Output = Result<HttpResponse, RE>>,
        RE: Error + 'static,
    {
        let http_request = self.prepare_request()?;
        let http_response = http_client(http_request)
            .await
            .map_err(RequestTokenError::Request)?;
        endpoint_response(http_response)
    }

    fn prepare_request<RE>(&self) -> Result<HttpRequest, RequestTokenError<RE, TE>>
    where
        RE: Error + 'static,
    {
        Ok(endpoint_request(
            self.auth_type,
            self.client_id,
            self.client_secret,
            &self.extra_params,
            None,
            Some(&self.scopes),
            self.token_url
                .ok_or_else(|| RequestTokenError::Other("no token_url provided".to_string()))?
                .url(),
            vec![
                ("grant_type", "password"),
                ("username", self.username),
                ("password", self.password.secret()),
            ],
        ))
    }
}

///
/// A request to exchange client credentials for an access token.
///
/// See https://tools.ietf.org/html/rfc6749#section-4.4.
///
#[derive(Debug)]
pub struct ClientCredentialsTokenRequest<'a, TE, TR, TT>
where
    TE: ErrorResponse,
    TR: TokenResponse<TT>,
    TT: TokenType,
{
    auth_type: &'a AuthType,
    client_id: &'a ClientId,
    client_secret: Option<&'a ClientSecret>,
    extra_params: Vec<(Cow<'a, str>, Cow<'a, str>)>,
    scopes: Vec<Cow<'a, Scope>>,
    token_url: Option<&'a TokenUrl>,
    _phantom: PhantomData<(TE, TR, TT)>,
}
impl<'a, TE, TR, TT> ClientCredentialsTokenRequest<'a, TE, TR, TT>
where
    TE: ErrorResponse + 'static,
    TR: TokenResponse<TT>,
    TT: TokenType,
{
    ///
    /// Appends an extra param to the token request.
    ///
    /// This method allows extensions to be used without direct support from
    /// this crate. If `name` conflicts with a parameter managed by this crate, the
    /// behavior is undefined. In particular, do not set parameters defined by
    /// [RFC 6749](https://tools.ietf.org/html/rfc6749) or
    /// [RFC 7636](https://tools.ietf.org/html/rfc7636).
    ///
    /// # Security Warning
    ///
    /// Callers should follow the security recommendations for any OAuth2 extensions used with
    /// this function, which are beyond the scope of
    /// [RFC 6749](https://tools.ietf.org/html/rfc6749).
    ///
    pub fn add_extra_param<N, V>(mut self, name: N, value: V) -> Self
    where
        N: Into<Cow<'a, str>>,
        V: Into<Cow<'a, str>>,
    {
        self.extra_params.push((name.into(), value.into()));
        self
    }

    ///
    /// Appends a new scope to the token request.
    ///
    pub fn add_scope(mut self, scope: Scope) -> Self {
        self.scopes.push(Cow::Owned(scope));
        self
    }

    ///
    /// Appends a collection of scopes to the token request.
    ///
    pub fn add_scopes<I>(mut self, scopes: I) -> Self
    where
        I: IntoIterator<Item = Scope>,
    {
        self.scopes.extend(scopes.into_iter().map(Cow::Owned));
        self
    }

    ///
    /// Synchronously sends the request to the authorization server and awaits a response.
    ///
    pub fn request<F, RE>(self, http_client: F) -> Result<TR, RequestTokenError<RE, TE>>
    where
        F: FnOnce(HttpRequest) -> Result<HttpResponse, RE>,
        RE: Error + 'static,
    {
        http_client(self.prepare_request()?)
            .map_err(RequestTokenError::Request)
            .and_then(endpoint_response)
    }

    ///
    /// Asynchronously sends the request to the authorization server and awaits a response.
    ///
    pub async fn request_async<C, F, RE>(
        self,
        http_client: C,
    ) -> Result<TR, RequestTokenError<RE, TE>>
    where
        C: FnOnce(HttpRequest) -> F,
        F: Future<Output = Result<HttpResponse, RE>>,
        RE: Error + 'static,
    {
        let http_request = self.prepare_request()?;
        let http_response = http_client(http_request)
            .await
            .map_err(RequestTokenError::Request)?;
        endpoint_response(http_response)
    }

    fn prepare_request<RE>(&self) -> Result<HttpRequest, RequestTokenError<RE, TE>>
    where
        RE: Error + 'static,
    {
        Ok(endpoint_request(
            self.auth_type,
            self.client_id,
            self.client_secret,
            &self.extra_params,
            None,
            Some(&self.scopes),
            self.token_url
                .ok_or_else(|| RequestTokenError::Other("no token_url provided".to_string()))?
                .url(),
            vec![("grant_type", "client_credentials")],
        ))
    }
}

///
/// A request to introspect an access token.
///
/// See https://tools.ietf.org/html/rfc7662#section-2.1
///
#[derive(Debug)]
pub struct IntrospectionRequest<'a, TE, TIR, TT>
where
    TE: ErrorResponse,
    TIR: TokenIntrospectionResponse<TT>,
    TT: TokenType,
{
    token: &'a AccessToken,
    token_type_hint: Option<Cow<'a, str>>,

    auth_type: &'a AuthType,
    client_id: &'a ClientId,
    client_secret: Option<&'a ClientSecret>,
    extra_params: Vec<(Cow<'a, str>, Cow<'a, str>)>,
    introspection_url: &'a IntrospectionUrl,

    _phantom: PhantomData<(TE, TIR, TT)>,
}

impl<'a, TE, TIR, TT> IntrospectionRequest<'a, TE, TIR, TT>
where
    TE: ErrorResponse + 'static,
    TIR: TokenIntrospectionResponse<TT>,
    TT: TokenType,
{
    ///
    /// Sets the optional token_type_hint parameter.
    ///
    /// See: https://tools.ietf.org/html/rfc7662#section-2.1
    ///
    /// OPTIONAL.  A hint about the type of the token submitted for
    ///       introspection.  The protected resource MAY pass this parameter to
    ///       help the authorization server optimize the token lookup.  If the
    ///       server is unable to locate the token using the given hint, it MUST
    ///      extend its search across all of its supported token types.  An
    ///      authorization server MAY ignore this parameter, particularly if it
    ///      is able to detect the token type automatically.  Values for this
    ///      field are defined in the "OAuth Token Type Hints" registry defined
    ///      in OAuth Token Revocation [RFC7009](https://tools.ietf.org/html/rfc7009).
    ///
    pub fn set_token_type_hint<V>(mut self, value: V) -> Self
    where
        V: Into<Cow<'a, str>>,
    {
        self.token_type_hint = Some(value.into());

        self
    }

    ///
    /// Appends an extra param to the token introspection request.
    ///
    /// This method allows extensions to be used without direct support from
    /// this crate. If `name` conflicts with a parameter managed by this crate, the
    /// behavior is undefined. In particular, do not set parameters defined by
    /// [RFC 6749](https://tools.ietf.org/html/rfc6749) or
    /// [RFC 7662](https://tools.ietf.org/html/rfc7662).
    ///
    /// # Security Warning
    ///
    /// Callers should follow the security recommendations for any OAuth2 extensions used with
    /// this function, which are beyond the scope of
    /// [RFC 6749](https://tools.ietf.org/html/rfc6749).
    ///
    pub fn add_extra_param<N, V>(mut self, name: N, value: V) -> Self
    where
        N: Into<Cow<'a, str>>,
        V: Into<Cow<'a, str>>,
    {
        self.extra_params.push((name.into(), value.into()));
        self
    }

    fn prepare_request<RE>(self) -> Result<HttpRequest, RequestTokenError<RE, TE>>
    where
        RE: Error + 'static,
    {
        let mut params: Vec<(&str, &str)> = vec![("token", self.token.secret())];
        if let Some(ref token_type_hint) = self.token_type_hint {
            params.push(("token_type_hint", token_type_hint));
        }

        Ok(endpoint_request(
            self.auth_type,
            self.client_id,
            self.client_secret,
            &self.extra_params,
            None,
            None,
            self.introspection_url.url(),
            params,
        ))
    }

    ///
    /// Synchronously sends the request to the authorization server and awaits a response.
    ///
    pub fn request<F, RE>(self, http_client: F) -> Result<TIR, RequestTokenError<RE, TE>>
    where
        F: FnOnce(HttpRequest) -> Result<HttpResponse, RE>,
        RE: Error + 'static,
    {
        http_client(self.prepare_request()?)
            .map_err(RequestTokenError::Request)
            .and_then(endpoint_response)
    }

    ///
    /// Asynchronously sends the request to the authorization server and returns a Future.
    ///
    pub async fn request_async<C, F, RE>(
        self,
        http_client: C,
    ) -> Result<TIR, RequestTokenError<RE, TE>>
    where
        C: FnOnce(HttpRequest) -> F,
        F: Future<Output = Result<HttpResponse, RE>>,
        RE: Error + 'static,
    {
        let http_request = self.prepare_request()?;
        let http_response = http_client(http_request)
            .await
            .map_err(RequestTokenError::Request)?;
        endpoint_response(http_response)
    }
}

///
/// A request to revoke a token via an [`RFC 7009`](https://tools.ietf.org/html/rfc7009#section-2.1) compatible
/// endpoint.
///
#[derive(Debug)]
pub struct RevocationRequest<'a, RT, TE>
where
    RT: RevocableToken,
    TE: ErrorResponse,
{
    token: RT,

    auth_type: &'a AuthType,
    client_id: &'a ClientId,
    client_secret: Option<&'a ClientSecret>,
    extra_params: Vec<(Cow<'a, str>, Cow<'a, str>)>,
    revocation_url: &'a RevocationUrl,

    _phantom: PhantomData<(RT, TE)>,
}

impl<'a, RT, TE> RevocationRequest<'a, RT, TE>
where
    RT: RevocableToken,
    TE: ErrorResponse + 'static,
{
    ///
    /// Appends an extra param to the token revocation request.
    ///
    /// This method allows extensions to be used without direct support from
    /// this crate. If `name` conflicts with a parameter managed by this crate, the
    /// behavior is undefined. In particular, do not set parameters defined by
    /// [RFC 6749](https://tools.ietf.org/html/rfc6749) or
    /// [RFC 7662](https://tools.ietf.org/html/rfc7662).
    ///
    /// # Security Warning
    ///
    /// Callers should follow the security recommendations for any OAuth2 extensions used with
    /// this function, which are beyond the scope of
    /// [RFC 6749](https://tools.ietf.org/html/rfc6749).
    ///
    pub fn add_extra_param<N, V>(mut self, name: N, value: V) -> Self
    where
        N: Into<Cow<'a, str>>,
        V: Into<Cow<'a, str>>,
    {
        self.extra_params.push((name.into(), value.into()));
        self
    }

    fn prepare_request<RE>(self) -> Result<HttpRequest, RequestTokenError<RE, TE>>
    where
        RE: Error + 'static,
    {
        let mut params: Vec<(&str, &str)> = vec![("token", self.token.secret())];
        if let Some(type_hint) = self.token.type_hint() {
            params.push(("token_type_hint", type_hint));
        }

        Ok(endpoint_request(
            self.auth_type,
            self.client_id,
            self.client_secret,
            &self.extra_params,
            None,
            None,
            self.revocation_url.url(),
            params,
        ))
    }

    ///
    /// Synchronously sends the request to the authorization server and awaits a response.
    ///
    /// A successful response indicates that the server either revoked the token or the token was not known to the
    /// server.
    ///
    /// Error [`UnsupportedTokenType`](crate::revocation::RevocationErrorResponseType::UnsupportedTokenType) will be returned if the
    /// type of token type given is not supported by the server.
    ///
    pub fn request<F, RE>(self, http_client: F) -> Result<(), RequestTokenError<RE, TE>>
    where
        F: FnOnce(HttpRequest) -> Result<HttpResponse, RE>,
        RE: Error + 'static,
    {
        // From https://tools.ietf.org/html/rfc7009#section-2.2:
        //   "The content of the response body is ignored by the client as all
        //    necessary information is conveyed in the response code."
        http_client(self.prepare_request()?)
            .map_err(RequestTokenError::Request)
            .and_then(endpoint_response_status_only)
    }

    ///
    /// Asynchronously sends the request to the authorization server and returns a Future.
    ///
    pub async fn request_async<C, F, RE>(
        self,
        http_client: C,
    ) -> Result<(), RequestTokenError<RE, TE>>
    where
        C: FnOnce(HttpRequest) -> F,
        F: Future<Output = Result<HttpResponse, RE>>,
        RE: Error + 'static,
    {
        let http_request = self.prepare_request()?;
        let http_response = http_client(http_request)
            .await
            .map_err(RequestTokenError::Request)?;
        endpoint_response_status_only(http_response)
    }
}

#[allow(clippy::too_many_arguments)]
fn endpoint_request<'a>(
    auth_type: &'a AuthType,
    client_id: &'a ClientId,
    client_secret: Option<&'a ClientSecret>,
    extra_params: &'a [(Cow<'a, str>, Cow<'a, str>)],
    redirect_url: Option<Cow<'a, RedirectUrl>>,
    scopes: Option<&'a Vec<Cow<'a, Scope>>>,
    url: &'a Url,
    params: Vec<(&'a str, &'a str)>,
) -> HttpRequest {
    let mut headers = HeaderMap::new();
    headers.append(ACCEPT, HeaderValue::from_static(CONTENT_TYPE_JSON));
    headers.append(
        CONTENT_TYPE,
        HeaderValue::from_static(CONTENT_TYPE_FORMENCODED),
    );

    let scopes_opt = scopes.and_then(|scopes| {
        if !scopes.is_empty() {
            Some(
                scopes
                    .iter()
                    .map(|s| s.to_string())
                    .collect::<Vec<_>>()
                    .join(" "),
            )
        } else {
            None
        }
    });

    let mut params: Vec<(&str, &str)> = params;
    if let Some(ref scopes) = scopes_opt {
        params.push(("scope", scopes));
    }

    // FIXME: add support for auth extensions? e.g., client_secret_jwt and private_key_jwt
    match auth_type {
        AuthType::RequestBody => {
            params.push(("client_id", client_id));
            if let Some(ref client_secret) = client_secret {
                params.push(("client_secret", client_secret.secret()));
            }
        }
        AuthType::BasicAuth => {
            // Section 2.3.1 of RFC 6749 requires separately url-encoding the id and secret
            // before using them as HTTP Basic auth username and password. Note that this is
            // not standard for ordinary Basic auth, so curl won't do it for us.
            let urlencoded_id: String =
                form_urlencoded::byte_serialize(&client_id.as_bytes()).collect();

            let urlencoded_secret = client_secret.map(|secret| {
                form_urlencoded::byte_serialize(secret.secret().as_bytes()).collect::<String>()
            });
            let b64_credential = base64::encode(&format!(
                "{}:{}",
                &urlencoded_id,
                urlencoded_secret.as_deref().unwrap_or("")
            ));
            headers.append(
                AUTHORIZATION,
                HeaderValue::from_str(&format!("Basic {}", &b64_credential)).unwrap(),
            );
        }
    }

    if let Some(ref redirect_url) = redirect_url {
        params.push(("redirect_uri", redirect_url.as_str()));
    }

    params.extend_from_slice(
        extra_params
            .iter()
            .map(|&(ref k, ref v)| (k.as_ref(), v.as_ref()))
            .collect::<Vec<_>>()
            .as_slice(),
    );

    let body = url::form_urlencoded::Serializer::new(String::new())
        .extend_pairs(params)
        .finish()
        .into_bytes();

    HttpRequest {
        url: url.to_owned(),
        method: http::method::Method::POST,
        headers,
        body,
    }
}

fn endpoint_response<RE, TE, DO>(
    http_response: HttpResponse,
) -> Result<DO, RequestTokenError<RE, TE>>
where
    RE: Error + 'static,
    TE: ErrorResponse,
    DO: DeserializeOwned,
{
    check_response_status(&http_response)?;

    check_response_body(&http_response)?;

    let response_body = http_response.body.as_slice();
    serde_path_to_error::deserialize(&mut serde_json::Deserializer::from_slice(response_body))
        .map_err(|e| RequestTokenError::Parse(e, response_body.to_vec()))
}

fn endpoint_response_status_only<RE, TE>(
    http_response: HttpResponse,
) -> Result<(), RequestTokenError<RE, TE>>
where
    RE: Error + 'static,
    TE: ErrorResponse,
{
    check_response_status(&http_response)
}

fn check_response_status<RE, TE>(
    http_response: &HttpResponse,
) -> Result<(), RequestTokenError<RE, TE>>
where
    RE: Error + 'static,
    TE: ErrorResponse,
{
    if http_response.status_code != StatusCode::OK {
        let reason = http_response.body.as_slice();
        if reason.is_empty() {
            return Err(RequestTokenError::Other(
                "Server returned empty error response".to_string(),
            ));
        } else {
            let error = match serde_path_to_error::deserialize::<_, TE>(
                &mut serde_json::Deserializer::from_slice(reason),
            ) {
                Ok(error) => RequestTokenError::ServerResponse(error),
                Err(error) => RequestTokenError::Parse(error, reason.to_vec()),
            };
            return Err(error);
        }
    }

    Ok(())
}

fn check_response_body<RE, TE>(
    http_response: &HttpResponse,
) -> Result<(), RequestTokenError<RE, TE>>
where
    RE: Error + 'static,
    TE: ErrorResponse,
{
    // Validate that the response Content-Type is JSON.
    http_response
        .headers
        .get(CONTENT_TYPE)
        .map_or(Ok(()), |content_type|
            // Section 3.1.1.1 of RFC 7231 indicates that media types are case insensitive and
            // may be followed by optional whitespace and/or a parameter (e.g., charset).
            // See https://tools.ietf.org/html/rfc7231#section-3.1.1.1.
            if content_type.to_str().ok().filter(|ct| ct.to_lowercase().starts_with(CONTENT_TYPE_JSON)).is_none() {
                Err(
                    RequestTokenError::Other(
                        format!(
                            "Unexpected response Content-Type: {:?}, should be `{}`",
                            content_type,
                            CONTENT_TYPE_JSON
                        )
                    )
                )
            } else {
                Ok(())
            }
        )?;

    if http_response.body.is_empty() {
        return Err(RequestTokenError::Other(
            "Server returned empty response body".to_string(),
        ));
    }

    Ok(())
}

///
/// The request for a set of verification codes from the authorization server.
///
/// See https://tools.ietf.org/html/rfc8628#section-3.1.
///
#[derive(Debug)]
pub struct DeviceAuthorizationRequest<'a, TE>
where
    TE: ErrorResponse,
{
    auth_type: &'a AuthType,
    client_id: &'a ClientId,
    client_secret: Option<&'a ClientSecret>,
    extra_params: Vec<(Cow<'a, str>, Cow<'a, str>)>,
    scopes: Vec<Cow<'a, Scope>>,
    device_authorization_url: &'a DeviceAuthorizationUrl,
    _phantom: PhantomData<TE>,
}

impl<'a, TE> DeviceAuthorizationRequest<'a, TE>
where
    TE: ErrorResponse + 'static,
{
    ///
    /// Appends an extra param to the token request.
    ///
    /// This method allows extensions to be used without direct support from
    /// this crate. If `name` conflicts with a parameter managed by this crate, the
    /// behavior is undefined. In particular, do not set parameters defined by
    /// [RFC 6749](https://tools.ietf.org/html/rfc6749) or
    /// [RFC 7636](https://tools.ietf.org/html/rfc7636).
    ///
    /// # Security Warning
    ///
    /// Callers should follow the security recommendations for any OAuth2 extensions used with
    /// this function, which are beyond the scope of
    /// [RFC 6749](https://tools.ietf.org/html/rfc6749).
    ///
    pub fn add_extra_param<N, V>(mut self, name: N, value: V) -> Self
    where
        N: Into<Cow<'a, str>>,
        V: Into<Cow<'a, str>>,
    {
        self.extra_params.push((name.into(), value.into()));
        self
    }

    ///
    /// Appends a new scope to the token request.
    ///
    pub fn add_scope(mut self, scope: Scope) -> Self {
        self.scopes.push(Cow::Owned(scope));
        self
    }

    ///
    /// Appends a collection of scopes to the token request.
    ///
    pub fn add_scopes<I>(mut self, scopes: I) -> Self
    where
        I: IntoIterator<Item = Scope>,
    {
        self.scopes.extend(scopes.into_iter().map(Cow::Owned));
        self
    }

    fn prepare_request<RE>(self) -> Result<HttpRequest, RequestTokenError<RE, TE>>
    where
        RE: Error + 'static,
    {
        Ok(endpoint_request(
            self.auth_type,
            self.client_id,
            self.client_secret,
            &self.extra_params,
            None,
            Some(&self.scopes),
            self.device_authorization_url.url(),
            vec![],
        ))
    }

    ///
    /// Synchronously sends the request to the authorization server and awaits a response.
    ///
    pub fn request<F, RE, EF>(
        self,
        http_client: F,
    ) -> Result<DeviceAuthorizationResponse<EF>, RequestTokenError<RE, TE>>
    where
        F: FnOnce(HttpRequest) -> Result<HttpResponse, RE>,
        RE: Error + 'static,
        EF: ExtraDeviceAuthorizationFields,
    {
        http_client(self.prepare_request()?)
            .map_err(RequestTokenError::Request)
            .and_then(endpoint_response)
    }

    ///
    /// Asynchronously sends the request to the authorization server and returns a Future.
    ///
    pub async fn request_async<C, F, RE, EF>(
        self,
        http_client: C,
    ) -> Result<DeviceAuthorizationResponse<EF>, RequestTokenError<RE, TE>>
    where
        C: FnOnce(HttpRequest) -> F,
        F: Future<Output = Result<HttpResponse, RE>>,
        RE: Error + 'static,
        EF: ExtraDeviceAuthorizationFields,
    {
        let http_request = self.prepare_request()?;
        let http_response = http_client(http_request)
            .await
            .map_err(RequestTokenError::Request)?;
        endpoint_response(http_response)
    }
}

///
/// The request for an device access token from the authorization server.
///
/// See https://tools.ietf.org/html/rfc8628#section-3.4.
///
#[derive(Clone)]
pub struct DeviceAccessTokenRequest<'a, 'b, TR, TT, EF>
where
    TR: TokenResponse<TT>,
    TT: TokenType,
    EF: ExtraDeviceAuthorizationFields,
{
    auth_type: &'a AuthType,
    client_id: &'a ClientId,
    client_secret: Option<&'a ClientSecret>,
    extra_params: Vec<(Cow<'a, str>, Cow<'a, str>)>,
    token_url: Option<&'a TokenUrl>,
    dev_auth_resp: &'a DeviceAuthorizationResponse<EF>,
    time_fn: Arc<dyn Fn() -> DateTime<Utc> + 'b + Send + Sync>,
    _phantom: PhantomData<(TR, TT, EF)>,
}

impl<'a, 'b, TR, TT, EF> DeviceAccessTokenRequest<'a, 'b, TR, TT, EF>
where
    TR: TokenResponse<TT>,
    TT: TokenType,
    EF: ExtraDeviceAuthorizationFields,
{
    ///
    /// Appends an extra param to the token request.
    ///
    /// This method allows extensions to be used without direct support from
    /// this crate. If `name` conflicts with a parameter managed by this crate, the
    /// behavior is undefined. In particular, do not set parameters defined by
    /// [RFC 6749](https://tools.ietf.org/html/rfc6749) or
    /// [RFC 7636](https://tools.ietf.org/html/rfc7636).
    ///
    /// # Security Warning
    ///
    /// Callers should follow the security recommendations for any OAuth2 extensions used with
    /// this function, which are beyond the scope of
    /// [RFC 6749](https://tools.ietf.org/html/rfc6749).
    ///
    pub fn add_extra_param<N, V>(mut self, name: N, value: V) -> Self
    where
        N: Into<Cow<'a, str>>,
        V: Into<Cow<'a, str>>,
    {
        self.extra_params.push((name.into(), value.into()));
        self
    }

    ///
    /// Specifies a function for returning the current time.
    ///
    /// This function is used while polling the authorization server.
    ///
    pub fn set_time_fn<T>(mut self, time_fn: T) -> Self
    where
        T: Fn() -> DateTime<Utc> + 'b + Send + Sync,
    {
        self.time_fn = Arc::new(time_fn);
        self
    }

    ///
    /// Synchronously polls the authorization server for a response, waiting
    /// using a user defined sleep function.
    ///
    pub fn request<F, S, RE>(
        self,
        http_client: F,
        sleep_fn: S,
        timeout: Option<Duration>,
    ) -> Result<TR, RequestTokenError<RE, DeviceCodeErrorResponse>>
    where
        F: Fn(HttpRequest) -> Result<HttpResponse, RE>,
        S: Fn(Duration),
        RE: Error + 'static,
    {
        // Get the request timeout and starting interval
        let timeout_dt = self.compute_timeout(timeout)?;
        let mut interval = self.dev_auth_resp.interval();

        // Loop while requesting a token.
        loop {
            let now = (*self.time_fn)();
            if now > timeout_dt {
                break Err(RequestTokenError::Other("Device code expired".to_string()));
            }

            match self.process_response(http_client(self.prepare_request()?), interval) {
                DeviceAccessTokenPollResult::ContinueWithNewPollInterval(new_interval) => {
                    interval = new_interval
                }
                DeviceAccessTokenPollResult::Done(res, _) => break res,
            }

            // Sleep here using the provided sleep function.
            sleep_fn(interval);
        }
    }

    ///
    /// Asynchronously sends the request to the authorization server and awaits a response.
    ///
    pub async fn request_async<C, F, S, SF, RE>(
        self,
        http_client: C,
        sleep_fn: S,
        timeout: Option<Duration>,
    ) -> Result<TR, RequestTokenError<RE, DeviceCodeErrorResponse>>
    where
        C: Fn(HttpRequest) -> F,
        F: Future<Output = Result<HttpResponse, RE>>,
        S: Fn(Duration) -> SF,
        SF: Future<Output = ()>,
        RE: Error + 'static,
    {
        // Get the request timeout and starting interval
        let timeout_dt = self.compute_timeout(timeout)?;
        let mut interval = self.dev_auth_resp.interval();

        // Loop while requesting a token.
        loop {
            let now = (*self.time_fn)();
            if now > timeout_dt {
                break Err(RequestTokenError::Other("Device code expired".to_string()));
            }

            match self.process_response(http_client(self.prepare_request()?).await, interval) {
                DeviceAccessTokenPollResult::ContinueWithNewPollInterval(new_interval) => {
                    interval = new_interval
                }
                DeviceAccessTokenPollResult::Done(res, _) => break res,
            }

            // Sleep here using the provided sleep function.
            sleep_fn(interval).await;
        }
    }

    fn prepare_request<RE>(
        &self,
    ) -> Result<HttpRequest, RequestTokenError<RE, DeviceCodeErrorResponse>>
    where
        RE: Error + 'static,
    {
        Ok(endpoint_request(
            self.auth_type,
            self.client_id,
            self.client_secret,
            &self.extra_params,
            None,
            None,
            self.token_url
                .ok_or_else(|| RequestTokenError::Other("no token_url provided".to_string()))?
                .url(),
            vec![
                ("grant_type", "urn:ietf:params:oauth:grant-type:device_code"),
                ("device_code", self.dev_auth_resp.device_code().secret()),
            ],
        ))
    }

    fn process_response<RE>(
        &self,
        res: Result<HttpResponse, RE>,
        current_interval: Duration,
    ) -> DeviceAccessTokenPollResult<TR, RE, DeviceCodeErrorResponse, TT>
    where
        RE: Error + 'static,
    {
        let http_response = match res {
            Ok(inner) => inner,
            Err(_) => {
                // Try and double the current interval. If that fails, just use the current one.
                let new_interval = current_interval.checked_mul(2).unwrap_or(current_interval);
                return DeviceAccessTokenPollResult::ContinueWithNewPollInterval(new_interval);
            }
        };

        // Explicitly process the response with a DeviceCodeErrorResponse
        let res = endpoint_response::<RE, DeviceCodeErrorResponse, TR>(http_response);
        match res {
            // On a ServerResponse error, the error needs inspecting as a DeviceCodeErrorResponse
            // to work out whether a retry needs to happen.
            Err(RequestTokenError::ServerResponse(dcer)) => {
                match dcer.error() {
                    // On AuthorizationPending, a retry needs to happen with the same poll interval.
                    DeviceCodeErrorResponseType::AuthorizationPending => {
                        DeviceAccessTokenPollResult::ContinueWithNewPollInterval(current_interval)
                    }
                    // On SlowDown, a retry needs to happen with a larger poll interval.
                    DeviceCodeErrorResponseType::SlowDown => {
                        DeviceAccessTokenPollResult::ContinueWithNewPollInterval(
                            current_interval + Duration::from_secs(5),
                        )
                    }

                    // On any other error, just return the error.
                    _ => DeviceAccessTokenPollResult::Done(
                        Err(RequestTokenError::ServerResponse(dcer)),
                        PhantomData,
                    ),
                }
            }

            // On any other success or failure, return the failure.
            res => DeviceAccessTokenPollResult::Done(res, PhantomData),
        }
    }

    fn compute_timeout<RE>(
        &self,
        timeout: Option<Duration>,
    ) -> Result<DateTime<Utc>, RequestTokenError<RE, DeviceCodeErrorResponse>>
    where
        RE: Error + 'static,
    {
        // Calculate the request timeout - if the user specified a timeout,
        // use that, otherwise use the value given by the device authorization
        // response.
        let timeout_dur = timeout.unwrap_or_else(|| self.dev_auth_resp.expires_in());
        let chrono_timeout = chrono::Duration::from_std(timeout_dur)
            .map_err(|_| RequestTokenError::Other("Failed to convert duration".to_string()))?;

        // Calculate the DateTime at which the request times out.
        let timeout_dt = (*self.time_fn)()
            .checked_add_signed(chrono_timeout)
            .ok_or_else(|| RequestTokenError::Other("Failed to calculate timeout".to_string()))?;

        Ok(timeout_dt)
    }
}

///
/// Trait for OAuth2 access tokens.
///
pub trait TokenType: Clone + DeserializeOwned + Debug + PartialEq + Serialize {}

///
/// Trait for adding extra fields to the `TokenResponse`.
///
pub trait ExtraTokenFields: DeserializeOwned + Debug + Serialize {}

///
/// Empty (default) extra token fields.
///
#[derive(Clone, Debug, Deserialize, PartialEq, Serialize)]
pub struct EmptyExtraTokenFields {}
impl ExtraTokenFields for EmptyExtraTokenFields {}

///
/// Common methods shared by all OAuth2 token implementations.
///
/// The methods in this trait are defined in
/// [Section 5.1 of RFC 6749](https://tools.ietf.org/html/rfc6749#section-5.1). This trait exists
/// separately from the `StandardTokenResponse` struct to support customization by clients,
/// such as supporting interoperability with non-standards-complaint OAuth2 providers.
///
pub trait TokenResponse<TT>: Debug + DeserializeOwned + Serialize
where
    TT: TokenType,
{
    ///
    /// REQUIRED. The access token issued by the authorization server.
    ///
    fn access_token(&self) -> &AccessToken;
    ///
    /// REQUIRED. The type of the token issued as described in
    /// [Section 7.1](https://tools.ietf.org/html/rfc6749#section-7.1).
    /// Value is case insensitive and deserialized to the generic `TokenType` parameter.
    ///
    fn token_type(&self) -> &TT;
    ///
    /// RECOMMENDED. The lifetime in seconds of the access token. For example, the value 3600
    /// denotes that the access token will expire in one hour from the time the response was
    /// generated. If omitted, the authorization server SHOULD provide the expiration time via
    /// other means or document the default value.
    ///
    fn expires_in(&self) -> Option<Duration>;
    ///
    /// OPTIONAL. The refresh token, which can be used to obtain new access tokens using the same
    /// authorization grant as described in
    /// [Section 6](https://tools.ietf.org/html/rfc6749#section-6).
    ///
    fn refresh_token(&self) -> Option<&RefreshToken>;
    ///
    /// OPTIONAL, if identical to the scope requested by the client; otherwise, REQUIRED. The
    /// scope of the access token as described by
    /// [Section 3.3](https://tools.ietf.org/html/rfc6749#section-3.3). If included in the response,
    /// this space-delimited field is parsed into a `Vec` of individual scopes. If omitted from
    /// the response, this field is `None`.
    ///
    fn scopes(&self) -> Option<&Vec<Scope>>;
}

///
/// Standard OAuth2 token response.
///
/// This struct includes the fields defined in
/// [Section 5.1 of RFC 6749](https://tools.ietf.org/html/rfc6749#section-5.1), as well as
/// extensions defined by the `EF` type parameter.
///
#[derive(Clone, Debug, Deserialize, Serialize)]
pub struct StandardTokenResponse<EF, TT>
where
    EF: ExtraTokenFields,
    TT: TokenType,
{
    access_token: AccessToken,
    #[serde(bound = "TT: TokenType")]
    #[serde(deserialize_with = "helpers::deserialize_untagged_enum_case_insensitive")]
    token_type: TT,
    #[serde(skip_serializing_if = "Option::is_none")]
    expires_in: Option<u64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    refresh_token: Option<RefreshToken>,
    #[serde(rename = "scope")]
    #[serde(deserialize_with = "helpers::deserialize_space_delimited_vec")]
    #[serde(serialize_with = "helpers::serialize_space_delimited_vec")]
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(default)]
    scopes: Option<Vec<Scope>>,

    #[serde(bound = "EF: ExtraTokenFields")]
    #[serde(flatten)]
    extra_fields: EF,
}
impl<EF, TT> StandardTokenResponse<EF, TT>
where
    EF: ExtraTokenFields,
    TT: TokenType,
{
    ///
    /// Instantiate a new OAuth2 token response.
    ///
    pub fn new(access_token: AccessToken, token_type: TT, extra_fields: EF) -> Self {
        Self {
            access_token,
            token_type,
            expires_in: None,
            refresh_token: None,
            scopes: None,
            extra_fields,
        }
    }

    ///
    /// Set the `access_token` field.
    ///
    pub fn set_access_token(&mut self, access_token: AccessToken) {
        self.access_token = access_token;
    }

    ///
    /// Set the `token_type` field.
    ///
    pub fn set_token_type(&mut self, token_type: TT) {
        self.token_type = token_type;
    }

    ///
    /// Set the `expires_in` field.
    ///
    pub fn set_expires_in(&mut self, expires_in: Option<&Duration>) {
        self.expires_in = expires_in.map(Duration::as_secs);
    }

    ///
    /// Set the `refresh_token` field.
    ///
    pub fn set_refresh_token(&mut self, refresh_token: Option<RefreshToken>) {
        self.refresh_token = refresh_token;
    }

    ///
    /// Set the `scopes` field.
    ///
    pub fn set_scopes(&mut self, scopes: Option<Vec<Scope>>) {
        self.scopes = scopes;
    }

    ///
    /// Extra fields defined by the client application.
    ///
    pub fn extra_fields(&self) -> &EF {
        &self.extra_fields
    }

    ///
    /// Set the extra fields defined by the client application.
    ///
    pub fn set_extra_fields(&mut self, extra_fields: EF) {
        self.extra_fields = extra_fields;
    }
}
impl<EF, TT> TokenResponse<TT> for StandardTokenResponse<EF, TT>
where
    EF: ExtraTokenFields,
    TT: TokenType,
{
    ///
    /// REQUIRED. The access token issued by the authorization server.
    ///
    fn access_token(&self) -> &AccessToken {
        &self.access_token
    }
    ///
    /// REQUIRED. The type of the token issued as described in
    /// [Section 7.1](https://tools.ietf.org/html/rfc6749#section-7.1).
    /// Value is case insensitive and deserialized to the generic `TokenType` parameter.
    ///
    fn token_type(&self) -> &TT {
        &self.token_type
    }
    ///
    /// RECOMMENDED. The lifetime in seconds of the access token. For example, the value 3600
    /// denotes that the access token will expire in one hour from the time the response was
    /// generated. If omitted, the authorization server SHOULD provide the expiration time via
    /// other means or document the default value.
    ///
    fn expires_in(&self) -> Option<Duration> {
        self.expires_in.map(Duration::from_secs)
    }
    ///
    /// OPTIONAL. The refresh token, which can be used to obtain new access tokens using the same
    /// authorization grant as described in
    /// [Section 6](https://tools.ietf.org/html/rfc6749#section-6).
    ///
    fn refresh_token(&self) -> Option<&RefreshToken> {
        self.refresh_token.as_ref()
    }
    ///
    /// OPTIONAL, if identical to the scope requested by the client; otherwise, REQUIRED. The
    /// scope of the access token as described by
    /// [Section 3.3](https://tools.ietf.org/html/rfc6749#section-3.3). If included in the response,
    /// this space-delimited field is parsed into a `Vec` of individual scopes. If omitted from
    /// the response, this field is `None`.
    ///
    fn scopes(&self) -> Option<&Vec<Scope>> {
        self.scopes.as_ref()
    }
}

///
/// Common methods shared by all OAuth2 token introspection implementations.
///
/// The methods in this trait are defined in
/// [Section 2.2 of RFC 7662](https://tools.ietf.org/html/rfc7662#section-2.2). This trait exists
/// separately from the `StandardTokenIntrospectionResponse` struct to support customization by
/// clients, such as supporting interoperability with non-standards-complaint OAuth2 providers.
///
pub trait TokenIntrospectionResponse<TT>: Debug + DeserializeOwned + Serialize
where
    TT: TokenType,
{
    ///
    /// REQUIRED.  Boolean indicator of whether or not the presented token
    /// is currently active.  The specifics of a token's "active" state
    /// will vary depending on the implementation of the authorization
    /// server and the information it keeps about its tokens, but a "true"
    /// value return for the "active" property will generally indicate
    /// that a given token has been issued by this authorization server,
    /// has not been revoked by the resource owner, and is within its
    /// given time window of validity (e.g., after its issuance time and
    /// before its expiration time).
    ///
    fn active(&self) -> bool;
    ///
    ///
    /// OPTIONAL.  A JSON string containing a space-separated list of
    /// scopes associated with this token, in the format described in
    /// [Section 3.3 of OAuth 2.0](https://tools.ietf.org/html/rfc7662#section-3.3).
    /// If included in the response,
    /// this space-delimited field is parsed into a `Vec` of individual scopes. If omitted from
    /// the response, this field is `None`.
    ///
    fn scopes(&self) -> Option<&Vec<Scope>>;
    ///
    /// OPTIONAL.  Client identifier for the OAuth 2.0 client that
    /// requested this token.
    ///
    fn client_id(&self) -> Option<&ClientId>;
    ///
    /// OPTIONAL.  Human-readable identifier for the resource owner who
    /// authorized this token.
    ///
    fn username(&self) -> Option<&str>;
    ///
    /// OPTIONAL.  Type of the token as defined in [Section 5.1](https://tools.ietf.org/html/rfc7662#section-5.1) of OAuth
    /// 2.0 [RFC6749].
    /// Value is case insensitive and deserialized to the generic `TokenType` parameter.
    ///
    fn token_type(&self) -> Option<&TT>;
    ///
    /// OPTIONAL.  Integer timestamp, measured in the number of seconds
    /// since January 1 1970 UTC, indicating when this token will expire,
    /// as defined in JWT [RFC7519](https://tools.ietf.org/html/rfc7519).
    ///
    fn exp(&self) -> Option<DateTime<Utc>>;
    ///
    /// OPTIONAL.  Integer timestamp, measured in the number of seconds
    /// since January 1 1970 UTC, indicating when this token was
    /// originally issued, as defined in JWT [RFC7519](https://tools.ietf.org/html/rfc7519).
    ///
    fn iat(&self) -> Option<DateTime<Utc>>;
    ///
    /// OPTIONAL.  Integer timestamp, measured in the number of seconds
    /// since January 1 1970 UTC, indicating when this token is not to be
    /// used before, as defined in JWT [RFC7519](https://tools.ietf.org/html/rfc7519).
    ///
    fn nbf(&self) -> Option<DateTime<Utc>>;
    ///
    /// OPTIONAL.  Subject of the token, as defined in JWT [RFC7519](https://tools.ietf.org/html/rfc7519).
    /// Usually a machine-readable identifier of the resource owner who
    /// authorized this token.
    ///
    fn sub(&self) -> Option<&str>;
    ///
    /// OPTIONAL.  Service-specific string identifier or list of string
    /// identifiers representing the intended audience for this token, as
    /// defined in JWT [RFC7519](https://tools.ietf.org/html/rfc7519).
    ///
    fn aud(&self) -> Option<&Vec<String>>;
    ///
    /// OPTIONAL.  String representing the issuer of this token, as
    /// defined in JWT [RFC7519](https://tools.ietf.org/html/rfc7519).
    ///
    fn iss(&self) -> Option<&str>;
    ///
    /// OPTIONAL.  String identifier for the token, as defined in JWT
    /// [RFC7519](https://tools.ietf.org/html/rfc7519).
    ///
    fn jti(&self) -> Option<&str>;
}

///
/// Standard OAuth2 token introspection response.
///
/// This struct includes the fields defined in
/// [Section 2.2 of RFC 7662](https://tools.ietf.org/html/rfc7662#section-2.2), as well as
/// extensions defined by the `EF` type parameter.
///
#[derive(Clone, Debug, Deserialize, Serialize)]
pub struct StandardTokenIntrospectionResponse<EF, TT>
where
    EF: ExtraTokenFields,
    TT: TokenType + 'static,
{
    active: bool,
    #[serde(rename = "scope")]
    #[serde(deserialize_with = "helpers::deserialize_space_delimited_vec")]
    #[serde(serialize_with = "helpers::serialize_space_delimited_vec")]
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(default)]
    scopes: Option<Vec<Scope>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    client_id: Option<ClientId>,
    #[serde(skip_serializing_if = "Option::is_none")]
    username: Option<String>,
    #[serde(
        bound = "TT: TokenType",
        skip_serializing_if = "Option::is_none",
        deserialize_with = "helpers::deserialize_untagged_enum_case_insensitive",
        default = "none_field"
    )]
    token_type: Option<TT>,
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(with = "ts_seconds_option")]
    #[serde(default)]
    exp: Option<DateTime<Utc>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(with = "ts_seconds_option")]
    #[serde(default)]
    iat: Option<DateTime<Utc>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(with = "ts_seconds_option")]
    #[serde(default)]
    nbf: Option<DateTime<Utc>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    sub: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(default)]
    #[serde(deserialize_with = "helpers::deserialize_optional_string_or_vec_string")]
    aud: Option<Vec<String>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    iss: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    jti: Option<String>,

    #[serde(bound = "EF: ExtraTokenFields")]
    #[serde(flatten)]
    extra_fields: EF,
}

fn none_field<T>() -> Option<T> {
    None
}

impl<EF, TT> StandardTokenIntrospectionResponse<EF, TT>
where
    EF: ExtraTokenFields,
    TT: TokenType,
{
    ///
    /// Instantiate a new OAuth2 token introspection response.
    ///
    pub fn new(active: bool, extra_fields: EF) -> Self {
        Self {
            active,

            scopes: None,
            client_id: None,
            username: None,
            token_type: None,
            exp: None,
            iat: None,
            nbf: None,
            sub: None,
            aud: None,
            iss: None,
            jti: None,
            extra_fields,
        }
    }

    ///
    /// Sets the `set_active` field.
    ///
    pub fn set_active(&mut self, active: bool) {
        self.active = active;
    }
    ///
    /// Sets the `set_scopes` field.
    ///
    pub fn set_scopes(&mut self, scopes: Option<Vec<Scope>>) {
        self.scopes = scopes;
    }
    ///
    /// Sets the `set_client_id` field.
    ///
    pub fn set_client_id(&mut self, client_id: Option<ClientId>) {
        self.client_id = client_id;
    }
    ///
    /// Sets the `set_username` field.
    ///
    pub fn set_username(&mut self, username: Option<String>) {
        self.username = username;
    }
    ///
    /// Sets the `set_token_type` field.
    ///
    pub fn set_token_type(&mut self, token_type: Option<TT>) {
        self.token_type = token_type;
    }
    ///
    /// Sets the `set_exp` field.
    ///
    pub fn set_exp(&mut self, exp: Option<DateTime<Utc>>) {
        self.exp = exp;
    }
    ///
    /// Sets the `set_iat` field.
    ///
    pub fn set_iat(&mut self, iat: Option<DateTime<Utc>>) {
        self.iat = iat;
    }
    ///
    /// Sets the `set_nbf` field.
    ///
    pub fn set_nbf(&mut self, nbf: Option<DateTime<Utc>>) {
        self.nbf = nbf;
    }
    ///
    /// Sets the `set_sub` field.
    ///
    pub fn set_sub(&mut self, sub: Option<String>) {
        self.sub = sub;
    }
    ///
    /// Sets the `set_aud` field.
    ///
    pub fn set_aud(&mut self, aud: Option<Vec<String>>) {
        self.aud = aud;
    }
    ///
    /// Sets the `set_iss` field.
    ///
    pub fn set_iss(&mut self, iss: Option<String>) {
        self.iss = iss;
    }
    ///
    /// Sets the `set_jti` field.
    ///
    pub fn set_jti(&mut self, jti: Option<String>) {
        self.jti = jti;
    }
    ///
    /// Extra fields defined by the client application.
    ///
    pub fn extra_fields(&self) -> &EF {
        &self.extra_fields
    }
    ///
    /// Sets the `set_extra_fields` field.
    ///
    pub fn set_extra_fields(&mut self, extra_fields: EF) {
        self.extra_fields = extra_fields;
    }
}
impl<EF, TT> TokenIntrospectionResponse<TT> for StandardTokenIntrospectionResponse<EF, TT>
where
    EF: ExtraTokenFields,
    TT: TokenType,
{
    fn active(&self) -> bool {
        self.active
    }

    fn scopes(&self) -> Option<&Vec<Scope>> {
        self.scopes.as_ref()
    }

    fn client_id(&self) -> Option<&ClientId> {
        self.client_id.as_ref()
    }

    fn username(&self) -> Option<&str> {
        self.username.as_deref()
    }

    fn token_type(&self) -> Option<&TT> {
        self.token_type.as_ref()
    }

    fn exp(&self) -> Option<DateTime<Utc>> {
        self.exp
    }

    fn iat(&self) -> Option<DateTime<Utc>> {
        self.iat
    }

    fn nbf(&self) -> Option<DateTime<Utc>> {
        self.nbf
    }

    fn sub(&self) -> Option<&str> {
        self.sub.as_deref()
    }

    fn aud(&self) -> Option<&Vec<String>> {
        self.aud.as_ref()
    }

    fn iss(&self) -> Option<&str> {
        self.iss.as_deref()
    }

    fn jti(&self) -> Option<&str> {
        self.jti.as_deref()
    }
}

///
/// Server Error Response
///
/// This trait exists separately from the `StandardErrorResponse` struct
/// to support customization by clients, such as supporting interoperability with
/// non-standards-complaint OAuth2 providers
///
pub trait ErrorResponse: Debug + DeserializeOwned + Serialize {}

///
/// Error types enum.
///
/// NOTE: The serialization must return the `snake_case` representation of
/// this error type. This value must match the error type from the relevant OAuth 2.0 standards
/// (RFC 6749 or an extension).
///
pub trait ErrorResponseType: Debug + DeserializeOwned + Serialize {}

///
/// Error response returned by server after requesting an access token.
///
/// The fields in this structure are defined in
/// [Section 5.2 of RFC 6749](https://tools.ietf.org/html/rfc6749#section-5.2). This
/// trait is parameterized by a `ErrorResponseType` to support error types specific to future OAuth2
/// authentication schemes and extensions.
///
#[derive(Clone, Debug, Deserialize, PartialEq, Serialize)]
pub struct StandardErrorResponse<T: ErrorResponseType> {
    #[serde(bound = "T: ErrorResponseType")]
    error: T,
    #[serde(default)]
    #[serde(skip_serializing_if = "Option::is_none")]
    error_description: Option<String>,
    #[serde(default)]
    #[serde(skip_serializing_if = "Option::is_none")]
    error_uri: Option<String>,
}

impl<T: ErrorResponseType> StandardErrorResponse<T> {
    ///
    /// Instantiate a new `ErrorResponse`.
    ///
    /// # Arguments
    ///
    /// * `error` - REQUIRED. A single ASCII error code deserialized to the generic parameter.
    ///   `ErrorResponseType`.
    /// * `error_description` - OPTIONAL. Human-readable ASCII text providing additional
    ///   information, used to assist the client developer in understanding the error that
    ///   occurred. Values for this parameter MUST NOT include characters outside the set
    ///   `%x20-21 / %x23-5B / %x5D-7E`.
    /// * `error_uri` - OPTIONAL. A URI identifying a human-readable web page with information
    ///   about the error used to provide the client developer with additional information about
    ///   the error. Values for the "error_uri" parameter MUST conform to the URI-reference
    ///   syntax and thus MUST NOT include characters outside the set `%x21 / %x23-5B / %x5D-7E`.
    ///
    pub fn new(error: T, error_description: Option<String>, error_uri: Option<String>) -> Self {
        Self {
            error,
            error_description,
            error_uri,
        }
    }

    ///
    /// REQUIRED. A single ASCII error code deserialized to the generic parameter
    /// `ErrorResponseType`.
    ///
    pub fn error(&self) -> &T {
        &self.error
    }
    ///
    /// OPTIONAL. Human-readable ASCII text providing additional information, used to assist
    /// the client developer in understanding the error that occurred. Values for this
    /// parameter MUST NOT include characters outside the set `%x20-21 / %x23-5B / %x5D-7E`.
    ///
    pub fn error_description(&self) -> Option<&String> {
        self.error_description.as_ref()
    }
    ///
    /// OPTIONAL. URI identifying a human-readable web page with information about the error,
    /// used to provide the client developer with additional information about the error.
    /// Values for the "error_uri" parameter MUST conform to the URI-reference syntax and
    /// thus MUST NOT include characters outside the set `%x21 / %x23-5B / %x5D-7E`.
    ///
    pub fn error_uri(&self) -> Option<&String> {
        self.error_uri.as_ref()
    }
}

impl<T> ErrorResponse for StandardErrorResponse<T> where T: ErrorResponseType + 'static {}

impl<TE> Display for StandardErrorResponse<TE>
where
    TE: ErrorResponseType + Display,
{
    fn fmt(&self, f: &mut Formatter) -> Result<(), FormatterError> {
        let mut formatted = self.error().to_string();

        if let Some(error_description) = self.error_description() {
            formatted.push_str(": ");
            formatted.push_str(error_description);
        }

        if let Some(error_uri) = self.error_uri() {
            formatted.push_str(" / See ");
            formatted.push_str(error_uri);
        }

        write!(f, "{}", formatted)
    }
}

///
/// Error encountered while requesting access token.
///
#[derive(Debug, thiserror::Error)]
pub enum RequestTokenError<RE, T>
where
    RE: Error + 'static,
    T: ErrorResponse + 'static,
{
    ///
    /// Error response returned by authorization server. Contains the parsed `ErrorResponse`
    /// returned by the server.
    ///
    #[error("Server returned error response")]
    ServerResponse(T),
    ///
    /// An error occurred while sending the request or receiving the response (e.g., network
    /// connectivity failed).
    ///
    #[error("Request failed")]
    Request(#[source] RE),
    ///
    /// Failed to parse server response. Parse errors may occur while parsing either successful
    /// or error responses.
    ///
    #[error("Failed to parse server response")]
    Parse(
        #[source] serde_path_to_error::Error<serde_json::error::Error>,
        Vec<u8>,
    ),
    ///
    /// Some other type of error occurred (e.g., an unexpected server response).
    ///
    #[error("Other error: {}", _0)]
    Other(String),
}
