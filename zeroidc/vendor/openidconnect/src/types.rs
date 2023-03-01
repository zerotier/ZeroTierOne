use std::collections::HashMap;
use std::fmt::{Debug, Display, Error as FormatterError, Formatter};
use std::future::Future;
use std::hash::Hash;
use std::iter::FromIterator;
use std::marker::PhantomData;
use std::ops::Deref;

use http::header::{HeaderValue, ACCEPT};
use http::method::Method;
use http::status::StatusCode;
use oauth2::helpers::deserialize_space_delimited_vec;
use rand::{thread_rng, Rng};
use serde::de::DeserializeOwned;
use serde::{Deserialize, Serialize};
use serde_with::{serde_as, VecSkipError};
use thiserror::Error;
use url::Url;

use super::http_utils::{check_content_type, MIME_TYPE_JSON, MIME_TYPE_JWKS};
use super::{
    AccessToken, AuthorizationCode, DiscoveryError, HttpRequest, HttpResponse,
    SignatureVerificationError,
};

///
/// A [locale-aware](https://openid.net/specs/openid-connect-core-1_0.html#IndividualClaimsLanguages)
/// claim.
///
/// This structure associates one more `Option<LanguageTag>` locales with the corresponding
/// claims values.
///
#[derive(Clone, Debug, PartialEq)]
pub struct LocalizedClaim<T>(HashMap<LanguageTag, T>, Option<T>);
impl<T> LocalizedClaim<T> {
    ///
    /// Initialize an empty claim.
    ///
    pub fn new() -> Self {
        Self::default()
    }

    ///
    /// Returns true if the claim contains a value for the specified locale.
    ///
    pub fn contains_key(&self, locale: Option<&LanguageTag>) -> bool {
        if let Some(l) = locale {
            self.0.contains_key(l)
        } else {
            self.1.is_some()
        }
    }

    ///
    /// Returns the entry for the specified locale or `None` if there is no such entry.
    ///
    pub fn get(&self, locale: Option<&LanguageTag>) -> Option<&T> {
        if let Some(l) = locale {
            self.0.get(l)
        } else {
            self.1.as_ref()
        }
    }

    ///
    /// Returns an iterator over the locales and claim value entries.
    ///
    pub fn iter(&self) -> impl Iterator<Item = (Option<&LanguageTag>, &T)> {
        self.1
            .iter()
            .map(|value| (None, value))
            .chain(self.0.iter().map(|(locale, value)| (Some(locale), value)))
    }

    ///
    /// Inserts or updates an entry for the specified locale.
    ///
    /// Returns the current value associated with the given locale, or `None` if there is no
    /// such entry.
    ///
    pub fn insert(&mut self, locale: Option<LanguageTag>, value: T) -> Option<T> {
        if let Some(l) = locale {
            self.0.insert(l, value)
        } else {
            self.1.replace(value)
        }
    }

    ///
    /// Removes an entry for the specified locale.
    ///
    /// Returns the current value associated with the given locale, or `None` if there is no
    /// such entry.
    ///
    pub fn remove(&mut self, locale: Option<&LanguageTag>) -> Option<T> {
        if let Some(l) = locale {
            self.0.remove(l)
        } else {
            self.1.take()
        }
    }
}
impl<T> Default for LocalizedClaim<T> {
    fn default() -> Self {
        Self(HashMap::new(), None)
    }
}
impl<T> From<T> for LocalizedClaim<T> {
    fn from(default: T) -> Self {
        Self(HashMap::new(), Some(default))
    }
}
impl<T> FromIterator<(Option<LanguageTag>, T)> for LocalizedClaim<T> {
    fn from_iter<I: IntoIterator<Item = (Option<LanguageTag>, T)>>(iter: I) -> Self {
        let mut temp: HashMap<Option<LanguageTag>, T> = iter.into_iter().collect();
        let default = temp.remove(&None);
        Self(
            temp.into_iter()
                .filter_map(|(locale, value)| locale.map(|l| (l, value)))
                .collect(),
            default,
        )
    }
}
impl<T> IntoIterator for LocalizedClaim<T>
where
    T: 'static,
{
    type Item = <LocalizedClaimIterator<T> as Iterator>::Item;
    type IntoIter = LocalizedClaimIterator<T>;

    fn into_iter(self) -> Self::IntoIter {
        LocalizedClaimIterator {
            inner: Box::new(
                self.1.into_iter().map(|value| (None, value)).chain(
                    self.0
                        .into_iter()
                        .map(|(locale, value)| (Some(locale), value)),
                ),
            ),
        }
    }
}

///
/// Owned iterator over a LocalizedClaim.
///
pub struct LocalizedClaimIterator<T> {
    inner: Box<dyn Iterator<Item = (Option<LanguageTag>, T)>>,
}
impl<T> Iterator for LocalizedClaimIterator<T> {
    type Item = (Option<LanguageTag>, T);
    fn next(&mut self) -> Option<Self::Item> {
        self.inner.next()
    }
}

///
/// Client application type.
///
pub trait ApplicationType: Debug + DeserializeOwned + Serialize + 'static {}

///
/// How the Authorization Server displays the authentication and consent user interface pages to
/// the End-User.
///
pub trait AuthDisplay: AsRef<str> + Debug + DeserializeOwned + Serialize + 'static {}

///
/// Whether the Authorization Server should prompt the End-User for reauthentication and consent.
///
pub trait AuthPrompt: AsRef<str> + 'static {}

///
/// Claim name.
///
pub trait ClaimName: Debug + DeserializeOwned + Serialize + 'static {}

///
/// Claim type (e.g., normal, aggregated, or distributed).
///
pub trait ClaimType: Debug + DeserializeOwned + Serialize + 'static {}

///
/// Client authentication method.
///
pub trait ClientAuthMethod: Debug + DeserializeOwned + Serialize + 'static {}

///
/// Grant type.
///
pub trait GrantType: Debug + DeserializeOwned + Serialize + 'static {}

///
/// Error signing a message.
///
#[derive(Clone, Debug, Error, PartialEq)]
#[non_exhaustive]
pub enum SigningError {
    /// Failed to sign the message using the given key and parameters.
    #[error("Crypto error")]
    CryptoError,
    /// Unsupported signature algorithm.
    #[error("Unsupported signature algorithm: {0}")]
    UnsupportedAlg(String),
    /// An unexpected error occurred.
    #[error("Other error: {0}")]
    Other(String),
}

///
/// JSON Web Key.
///
pub trait JsonWebKey<JS, JT, JU>: Clone + Debug + DeserializeOwned + Serialize + 'static
where
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    JU: JsonWebKeyUse,
{
    ///
    /// Returns the key ID, or `None` if no key ID is specified.
    ///
    fn key_id(&self) -> Option<&JsonWebKeyId>;

    ///
    /// Returns the key type (e.g., RSA).
    ///
    fn key_type(&self) -> &JT;

    ///
    /// Returns the allowed key usage (e.g., signing or encryption), or `None` if no usage is
    /// specified.
    ///
    fn key_use(&self) -> Option<&JU>;

    ///
    /// Initializes a new symmetric key or shared signing secret from the specified raw bytes.
    ///
    fn new_symmetric(key: Vec<u8>) -> Self;

    ///
    /// Verifies the given `signature` using the given signature algorithm (`signature_alg`) over
    /// the given `message`.
    ///
    /// Returns `Ok` if the signature is valid, or an `Err` otherwise.
    ///
    fn verify_signature(
        &self,
        signature_alg: &JS,
        message: &[u8],
        signature: &[u8],
    ) -> Result<(), SignatureVerificationError>;
}

///
/// Private or symmetric key for signing.
///
pub trait PrivateSigningKey<JS, JT, JU, K>
where
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    JU: JsonWebKeyUse,
    K: JsonWebKey<JS, JT, JU>,
{
    ///
    /// Signs the given `message` using the given signature algorithm.
    ///
    fn sign(&self, signature_alg: &JS, message: &[u8]) -> Result<Vec<u8>, SigningError>;

    ///
    /// Converts this key to a JSON Web Key that can be used for verifying signatures.
    ///
    fn as_verification_key(&self) -> K;
}

///
/// Key type (e.g., RSA).
///
pub trait JsonWebKeyType:
    Clone + Debug + DeserializeOwned + PartialEq + Serialize + 'static
{
}

///
/// Curve type (e.g., P256).
///
pub trait JsonCurveType:
    Clone + Debug + DeserializeOwned + PartialEq + Serialize + 'static
{
}

///
/// Allowed key usage.
///
pub trait JsonWebKeyUse: Debug + DeserializeOwned + Serialize + 'static {
    ///
    /// Returns true if the associated key may be used for digital signatures, or false otherwise.
    ///
    fn allows_signature(&self) -> bool;

    ///
    /// Returns true if the associated key may be used for encryption, or false otherwise.
    ///
    fn allows_encryption(&self) -> bool;
}

///
/// JSON Web Encryption (JWE) content encryption algorithm.
///
pub trait JweContentEncryptionAlgorithm<JT>:
    Clone + Debug + DeserializeOwned + Serialize + 'static
where
    JT: JsonWebKeyType,
{
    ///
    /// Returns the type of key required to use this encryption algorithm.
    ///
    fn key_type(&self) -> Result<JT, String>;
}

///
/// JSON Web Encryption (JWE) key management algorithm.
///
pub trait JweKeyManagementAlgorithm: Debug + DeserializeOwned + Serialize + 'static {
    // TODO: add a key_type() method
}

///
/// JSON Web Signature (JWS) algorithm.
///
pub trait JwsSigningAlgorithm<JT>:
    Clone + Debug + DeserializeOwned + Eq + Hash + PartialEq + Serialize + 'static
where
    JT: JsonWebKeyType,
{
    ///
    /// Returns the type of key required to use this signature algorithm, or `None` if this
    /// algorithm does not require a key.
    ///
    fn key_type(&self) -> Option<JT>;

    ///
    /// Returns true if the signature algorithm uses a shared secret (symmetric key).
    ///
    fn uses_shared_secret(&self) -> bool;

    ///
    /// Hashes the given `bytes` using the hash algorithm associated with this signing
    /// algorithm, and returns the hashed bytes.
    ///
    /// If hashing fails or this signing algorithm does not have an associated hash function, an
    /// `Err` is returned with a string describing the cause of the error.
    ///
    fn hash_bytes(&self, bytes: &[u8]) -> Result<Vec<u8>, String>;

    ///
    /// Returns the RS256 algorithm.
    ///
    /// This is the default algorithm for OpenID Connect ID tokens and must be supported by all
    /// implementations.
    ///
    fn rsa_sha_256() -> Self;
}

///
/// Response mode indicating how the OpenID Connect Provider should return the Authorization
/// Response to the Relying Party (client).
///
pub trait ResponseMode: Debug + DeserializeOwned + Serialize + 'static {}

///
/// Response type indicating the desired authorization processing flow, including what
/// parameters are returned from the endpoints used.
///
pub trait ResponseType: AsRef<str> + Debug + DeserializeOwned + Serialize + 'static {
    ///
    /// Converts this OpenID Connect response type to an [`oauth2::ResponseType`] used by the
    /// underlying [`oauth2`] crate.
    ///
    fn to_oauth2(&self) -> oauth2::ResponseType;
}

///
/// Subject identifier type returned by an OpenID Connect Provider to uniquely identify its users.
///
pub trait SubjectIdentifierType: Debug + DeserializeOwned + Serialize + 'static {}

new_type![
    ///
    /// Set of authentication methods or procedures that are considered to be equivalent to each
    /// other in a particular context.
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    AuthenticationContextClass(String)
];
impl AsRef<str> for AuthenticationContextClass {
    fn as_ref(&self) -> &str {
        self
    }
}

new_type![
    ///
    /// Identifier for an authentication method (e.g., `password` or `totp`).
    ///
    /// Defining specific AMR identifiers is beyond the scope of the OpenID Connect Core spec.
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    AuthenticationMethodReference(String)
];

new_type![
    ///
    /// Access token hash.
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    AccessTokenHash(String)
    impl {
        ///
        /// Initialize a new access token hash from an [`AccessToken`] and signature algorithm.
        ///
        pub fn from_token<JS, JT>(
            access_token: &AccessToken,
            alg: &JS
        ) -> Result<Self, SigningError>
        where
            JS: JwsSigningAlgorithm<JT>,
            JT: JsonWebKeyType,
        {
            alg.hash_bytes(access_token.secret().as_bytes())
                .map(|hash| {
                    Self::new(
                        base64::encode_config(&hash[0..hash.len() / 2], base64::URL_SAFE_NO_PAD)
                    )
                })
                .map_err(SigningError::UnsupportedAlg)
        }
    }
];

new_type![
    ///
    /// Country portion of address.
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    AddressCountry(String)
];

new_type![
    ///
    /// Locality portion of address.
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    AddressLocality(String)
];

new_type![
    ///
    /// Postal code portion of address.
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    AddressPostalCode(String)
];

new_type![
    ///
    /// Region portion of address.
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    AddressRegion(String)
];

new_type![
    ///
    /// Audience claim value.
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    Audience(String)
];

new_type![
    ///
    /// Authorization code hash.
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    AuthorizationCodeHash(String)
    impl {
        ///
        /// Initialize a new authorization code hash from an [`AuthorizationCode`] and signature
        /// algorithm.
        ///
        pub fn from_code<JS, JT>(
            code: &AuthorizationCode,
            alg: &JS
        ) -> Result<Self, SigningError>
        where
            JS: JwsSigningAlgorithm<JT>,
            JT: JsonWebKeyType,
        {
            alg.hash_bytes(code.secret().as_bytes())
                .map(|hash| {
                    Self::new(
                        base64::encode_config(&hash[0..hash.len() / 2], base64::URL_SAFE_NO_PAD)
                    )
                })
                .map_err(SigningError::UnsupportedAlg)
        }
    }
];

new_type![
    #[derive(Deserialize, Eq, Hash, Serialize)]
    pub(crate) Base64UrlEncodedBytes(
        #[serde(with = "serde_base64url_byte_array")]
        Vec<u8>
    )
];

new_type![
    ///
    /// OpenID Connect client name.
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    ClientName(String)
];

new_url_type![
    ///
    /// Client configuration endpoint URL.
    ///
    ClientConfigUrl
];

new_url_type![
    ///
    /// Client homepage URL.
    ///
    ClientUrl
];

new_type![
    ///
    /// Client contact e-mail address.
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    ClientContactEmail(String)
];

new_type![
    ///
    /// End user's birthday, represented as an
    /// [ISO 8601:2004](https://www.iso.org/standard/40874.html) `YYYY-MM-DD` format.
    ///
    /// The year MAY be `0000`, indicating that it is omitted. To represent only the year, `YYYY`
    /// format is allowed. Note that depending on the underlying platform's date related function,
    /// providing just year can result in varying month and day, so the implementers need to take
    /// this factor into account to correctly process the dates.
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    EndUserBirthday(String)
];

new_type![
    ///
    /// End user's e-mail address.
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    EndUserEmail(String)
];

new_type![
    ///
    /// End user's family name.
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    EndUserFamilyName(String)
];

new_type![
    ///
    /// End user's given name.
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    EndUserGivenName(String)
];

new_type![
    ///
    /// End user's middle name.
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    EndUserMiddleName(String)
];

new_type![
    ///
    /// End user's name.
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    EndUserName(String)
];

new_type![
    ///
    /// End user's nickname.
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    EndUserNickname(String)
];

new_type![
    ///
    /// End user's phone number.
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    EndUserPhoneNumber(String)
];

new_type![
    ///
    /// URL of end user's profile picture.
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    EndUserPictureUrl(String)
];

new_type![
    ///
    /// URL of end user's profile page.
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    EndUserProfileUrl(String)
];

new_type![
    ///
    /// End user's time zone as a string from the
    /// [time zone database](https://www.iana.org/time-zones).
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    EndUserTimezone(String)
];

new_type![
    ///
    /// URL of end user's website.
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    EndUserWebsiteUrl(String)
];

new_type![
    ///
    /// End user's username.
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    EndUserUsername(String)
];

new_type![
    ///
    /// Full mailing address, formatted for display or use on a mailing label.
    ///
    /// This field MAY contain multiple lines, separated by newlines. Newlines can be represented
    /// either as a carriage return/line feed pair (`"\r\n"`) or as a single line feed character
    /// (`"\n"`).
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    FormattedAddress(String)
];

new_url_type![
    ///
    /// URI using the `https` scheme that a third party can use to initiate a login by the Relying
    /// Party.
    ///
    InitiateLoginUrl
];

new_url_type![
    ///
    /// URL using the `https` scheme with no query or fragment component that the OP asserts as its
    /// Issuer Identifier.
    ///
    IssuerUrl
    impl {
        ///
        /// Parse a string as a URL, with this URL as the base URL.
        ///
        /// See [`Url::parse`].
        ///
        pub fn join(&self, suffix: &str) -> Result<Url, url::ParseError> {
            if let Some('/') = self.1.chars().next_back() {
                Url::parse(&(self.1.clone() + suffix))
            } else {
                Url::parse(&(self.1.clone() + "/" + suffix))
            }
        }
    }
];

new_type![
    ///
    /// ID of a JSON Web Key.
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    JsonWebKeyId(String)
];

///
/// JSON Web Key Set.
///
#[serde_as]
#[derive(Debug, Deserialize, PartialEq, Serialize)]
pub struct JsonWebKeySet<JS, JT, JU, K>
where
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    JU: JsonWebKeyUse,
    K: JsonWebKey<JS, JT, JU>,
{
    // FIXME: write a test that ensures duplicate object member names cause an error
    // (see https://tools.ietf.org/html/rfc7517#section-5)
    #[serde(bound = "K: JsonWebKey<JS, JT, JU>")]
    // Ignores invalid keys rather than failing. That way, clients can function using the keys that
    // they do understand, which is fine if they only ever get JWTs signed with those keys.
    #[serde_as(as = "VecSkipError<_>")]
    keys: Vec<K>,
    #[serde(skip)]
    _phantom: PhantomData<(JS, JT, JU)>,
}
impl<JS, JT, JU, K> JsonWebKeySet<JS, JT, JU, K>
where
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    JU: JsonWebKeyUse,
    K: JsonWebKey<JS, JT, JU>,
{
    ///
    /// Create a new JSON Web Key Set.
    ///
    pub fn new(keys: Vec<K>) -> Self {
        Self {
            keys,
            _phantom: PhantomData,
        }
    }

    ///
    /// Fetch a remote JSON Web Key Set from the specified `url` using the given `http_client`
    /// (e.g., [`crate::reqwest::http_client`] or [`crate::curl::http_client`]).
    ///
    pub fn fetch<HC, RE>(
        url: &JsonWebKeySetUrl,
        http_client: HC,
    ) -> Result<Self, DiscoveryError<RE>>
    where
        HC: FnOnce(HttpRequest) -> Result<HttpResponse, RE>,
        RE: std::error::Error + 'static,
    {
        http_client(Self::fetch_request(url))
            .map_err(DiscoveryError::Request)
            .and_then(Self::fetch_response)
    }

    ///
    /// Fetch a remote JSON Web Key Set from the specified `url` using the given async `http_client`
    /// (e.g., [`crate::reqwest::async_http_client`]).
    ///
    pub async fn fetch_async<F, HC, RE>(
        url: &JsonWebKeySetUrl,
        http_client: HC,
    ) -> Result<Self, DiscoveryError<RE>>
    where
        F: Future<Output = Result<HttpResponse, RE>>,
        HC: FnOnce(HttpRequest) -> F,
        RE: std::error::Error + 'static,
    {
        http_client(Self::fetch_request(url))
            .await
            .map_err(DiscoveryError::Request)
            .and_then(Self::fetch_response)
    }

    fn fetch_request(url: &JsonWebKeySetUrl) -> HttpRequest {
        HttpRequest {
            url: url.url().clone(),
            method: Method::GET,
            headers: vec![(ACCEPT, HeaderValue::from_static(MIME_TYPE_JSON))]
                .into_iter()
                .collect(),
            body: Vec::new(),
        }
    }

    fn fetch_response<RE>(http_response: HttpResponse) -> Result<Self, DiscoveryError<RE>>
    where
        RE: std::error::Error + 'static,
    {
        if http_response.status_code != StatusCode::OK {
            return Err(DiscoveryError::Response(
                http_response.status_code,
                http_response.body,
                format!("HTTP status code {}", http_response.status_code),
            ));
        }

        check_content_type(&http_response.headers, MIME_TYPE_JSON)
            .or_else(|err| {
                check_content_type(&http_response.headers, MIME_TYPE_JWKS).map_err(|_| err)
            })
            .map_err(|err_msg| {
                DiscoveryError::Response(
                    http_response.status_code,
                    http_response.body.clone(),
                    err_msg,
                )
            })?;

        serde_path_to_error::deserialize(&mut serde_json::Deserializer::from_slice(
            &http_response.body,
        ))
        .map_err(DiscoveryError::Parse)
    }

    ///
    /// Return the keys in this JSON Web Key Set.
    ///
    pub fn keys(&self) -> &Vec<K> {
        &self.keys
    }
}
impl<JS, JT, JU, K> Clone for JsonWebKeySet<JS, JT, JU, K>
where
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    JU: JsonWebKeyUse,
    K: JsonWebKey<JS, JT, JU>,
{
    fn clone(&self) -> Self {
        Self::new(self.keys.clone())
    }
}
impl<JS, JT, JU, K> Default for JsonWebKeySet<JS, JT, JU, K>
where
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    JU: JsonWebKeyUse,
    K: JsonWebKey<JS, JT, JU>,
{
    fn default() -> Self {
        Self::new(Vec::new())
    }
}

new_url_type![
    ///
    /// JSON Web Key Set URL.
    ///
    JsonWebKeySetUrl
];

new_type![
    ///
    /// Language tag adhering to RFC 5646 (e.g., `fr` or `fr-CA`).
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    LanguageTag(String)
];
impl AsRef<str> for LanguageTag {
    fn as_ref(&self) -> &str {
        self
    }
}

new_secret_type![
    ///
    /// Hint about the login identifier the End-User might use to log in.
    ///
    /// The use of this parameter is left to the OpenID Connect Provider's discretion.
    ///
    #[derive(Clone, Deserialize, Serialize)]
    LoginHint(String)
];

new_url_type![
    ///
    /// URL that references a logo for the Client application.
    ///
    LogoUrl
];

new_secret_type![
    ///
    /// String value used to associate a client session with an ID Token, and to mitigate replay
    /// attacks.
    ///
    #[derive(Clone, Deserialize, Serialize)]
    Nonce(String)
    impl {
        ///
        /// Generate a new random, base64-encoded 128-bit nonce.
        ///
        pub fn new_random() -> Self {
            Nonce::new_random_len(16)
        }
        ///
        /// Generate a new random, base64-encoded nonce of the specified length.
        ///
        /// # Arguments
        ///
        /// * `num_bytes` - Number of random bytes to generate, prior to base64-encoding.
        ///
        pub fn new_random_len(num_bytes: u32) -> Self {
            let random_bytes: Vec<u8> = (0..num_bytes).map(|_| thread_rng().gen::<u8>()).collect();
            Nonce::new(base64::encode_config(&random_bytes, base64::URL_SAFE_NO_PAD))
        }
    }
];
impl PartialEq for Nonce {
    fn eq(&self, other: &Self) -> bool {
        use subtle::ConstantTimeEq;
        self.secret()
            .as_bytes()
            .ct_eq(other.secret().as_bytes())
            .into()
    }
}

new_url_type![
    ///
    /// URL providing the OpenID Connect Provider's data usage policies for client applications.
    ///
    OpPolicyUrl
];

new_url_type![
    ///
    /// URL providing the OpenID Connect Provider's Terms of Service.
    ///
    OpTosUrl
];

new_url_type![
    ///
    /// URL providing a client application's data usage policy.
    ///
    PolicyUrl
];

new_secret_type![
    ///
    /// Access token used by a client application to access the Client Registration endpoint.
    ///
    #[derive(Clone, Deserialize, Serialize)]
    RegistrationAccessToken(String)
];

new_url_type![
    ///
    /// URL of the Client Registration endpoint.
    ///
    RegistrationUrl
];

new_url_type![
    ///
    /// URL used to pass request parameters as JWTs by reference.
    ///
    RequestUrl
];

///
/// Informs the Authorization Server of the desired authorization processing flow, including what
/// parameters are returned from the endpoints used.
///
/// See [OAuth 2.0 Multiple Response Type Encoding Practices](
///     http://openid.net/specs/oauth-v2-multiple-response-types-1_0.html#ResponseTypesAndModes)
/// for further details.
///
#[derive(Clone, Debug, Deserialize, PartialEq, Serialize)]
pub struct ResponseTypes<RT: ResponseType>(
    #[serde(
        deserialize_with = "deserialize_space_delimited_vec",
        serialize_with = "helpers::serialize_space_delimited_vec"
    )]
    Vec<RT>,
);
impl<RT: ResponseType> ResponseTypes<RT> {
    ///
    /// Create a new ResponseTypes<RT> to wrap the given Vec<RT>.
    ///
    pub fn new(s: Vec<RT>) -> Self {
        ResponseTypes::<RT>(s)
    }
}
impl<RT: ResponseType> Deref for ResponseTypes<RT> {
    type Target = Vec<RT>;
    fn deref(&self) -> &Vec<RT> {
        &self.0
    }
}

///
/// Timestamp as seconds since the unix epoch, or optionally an ISO 8601 string.
///
#[derive(Debug, Deserialize, Serialize)]
#[serde(untagged)]
pub(crate) enum Timestamp {
    Seconds(serde_json::Number),
    #[cfg(feature = "accept-rfc3339-timestamps")]
    Rfc3339(String),
}

impl Display for Timestamp {
    fn fmt(&self, f: &mut Formatter) -> Result<(), FormatterError> {
        match self {
            Timestamp::Seconds(seconds) => Display::fmt(seconds, f),
            #[cfg(feature = "accept-rfc3339-timestamps")]
            Timestamp::Rfc3339(iso) => Display::fmt(iso, f),
        }
    }
}

///
/// Newtype around a bool, optionally supporting string values.
///
#[derive(Debug, Deserialize, Serialize)]
#[serde(transparent)]
pub(crate) struct Boolean(
    #[cfg_attr(
        feature = "accept-string-booleans",
        serde(deserialize_with = "helpers::serde_string_bool::deserialize")
    )]
    pub bool,
);

impl Display for Boolean {
    fn fmt(&self, f: &mut Formatter) -> Result<(), FormatterError> {
        Display::fmt(&self.0, f)
    }
}

new_url_type![
    ///
    /// URL for retrieving redirect URIs that should receive identical pairwise subject identifiers.
    ///
    SectorIdentifierUrl
];

new_url_type![
    ///
    /// URL for developer documentation for an OpenID Connect Provider.
    ///
    ServiceDocUrl
];

new_type![
    ///
    /// A user's street address.
    ///
    /// Full street address component, which MAY include house number, street name, Post Office Box,
    /// and multi-line extended street address information. This field MAY contain multiple lines,
    /// separated by newlines. Newlines can be represented either as a carriage return/line feed
    /// pair (`\r\n`) or as a single line feed character (`\n`).
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    StreetAddress(String)
];

new_type![
    ///
    /// Locally unique and never reassigned identifier within the Issuer for the End-User, which is
    /// intended to be consumed by the client application.
    ///
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    SubjectIdentifier(String)
];

new_url_type![
    ///
    /// URL for the relying party's Terms of Service.
    ///
    ToSUrl
];

// FIXME: Add tests
pub(crate) mod helpers {
    use chrono::{DateTime, TimeZone, Utc};
    use serde::de::DeserializeOwned;
    use serde::{Deserialize, Deserializer, Serializer};
    use serde_json::{from_value, Value};

    use super::{LanguageTag, Timestamp};

    pub fn deserialize_string_or_vec<'de, T, D>(deserializer: D) -> Result<Vec<T>, D::Error>
    where
        T: DeserializeOwned,
        D: Deserializer<'de>,
    {
        use serde::de::Error;

        let value: Value = Deserialize::deserialize(deserializer)?;
        match from_value::<Vec<T>>(value.clone()) {
            Ok(val) => Ok(val),
            Err(_) => {
                let single_val: T = from_value(value).map_err(Error::custom)?;
                Ok(vec![single_val])
            }
        }
    }

    pub fn deserialize_string_or_vec_opt<'de, T, D>(
        deserializer: D,
    ) -> Result<Option<Vec<T>>, D::Error>
    where
        T: DeserializeOwned,
        D: Deserializer<'de>,
    {
        use serde::de::Error;

        let value: Value = Deserialize::deserialize(deserializer)?;
        match from_value::<Option<Vec<T>>>(value.clone()) {
            Ok(val) => Ok(val),
            Err(_) => {
                let single_val: T = from_value(value).map_err(Error::custom)?;
                Ok(Some(vec![single_val]))
            }
        }
    }

    // Attempt to deserialize the value; if the value is null or an error occurs, return None.
    // This is useful when deserializing fields that may mean different things in different
    // contexts, and where we would rather ignore the result than fail to deserialize. For example,
    // the fields in JWKs are not well defined; extensions could theoretically define their own
    // field names that overload field names used by other JWK types.
    pub fn deserialize_option_or_none<'de, T, D>(deserializer: D) -> Result<Option<T>, D::Error>
    where
        T: DeserializeOwned,
        D: Deserializer<'de>,
    {
        let value: Value = Deserialize::deserialize(deserializer)?;
        match from_value::<Option<T>>(value) {
            Ok(val) => Ok(val),
            Err(_) => Ok(None),
        }
    }

    ///
    /// Serde space-delimited string serializer for an `Option<Vec<String>>`.
    ///
    /// This function serializes a string vector into a single space-delimited string.
    /// If `string_vec_opt` is `None`, the function serializes it as `None` (e.g., `null`
    /// in the case of JSON serialization).
    ///
    pub fn serialize_space_delimited_vec<T, S>(vec: &[T], serializer: S) -> Result<S::Ok, S::Error>
    where
        T: AsRef<str>,
        S: Serializer,
    {
        let space_delimited = vec
            .iter()
            .map(AsRef::<str>::as_ref)
            .collect::<Vec<_>>()
            .join(" ");

        serializer.serialize_str(&space_delimited)
    }

    pub fn split_language_tag_key(key: &str) -> (&str, Option<LanguageTag>) {
        let mut lang_tag_sep = key.splitn(2, '#');

        // String::splitn(2) always returns at least one element.
        let field_name = lang_tag_sep.next().unwrap();

        let language_tag = lang_tag_sep
            .next()
            .filter(|language_tag| !language_tag.is_empty())
            .map(|language_tag| LanguageTag::new(language_tag.to_string()));

        (field_name, language_tag)
    }

    pub(crate) fn timestamp_to_utc(timestamp: &Timestamp) -> Result<DateTime<Utc>, ()> {
        match timestamp {
            Timestamp::Seconds(seconds) => {
                let (secs, nsecs) = if seconds.is_i64() {
                    (seconds.as_i64().ok_or(())?, 0u32)
                } else {
                    let secs_f64 = seconds.as_f64().ok_or(())?;
                    let secs = secs_f64.floor();
                    (
                        secs as i64,
                        ((secs_f64 - secs) * 1_000_000_000.).floor() as u32,
                    )
                };
                Utc.timestamp_opt(secs, nsecs).single().ok_or(())
            }
            #[cfg(feature = "accept-rfc3339-timestamps")]
            Timestamp::Rfc3339(iso) => {
                let datetime = DateTime::parse_from_rfc3339(iso).map_err(|_| ())?;
                Ok(datetime.into())
            }
        }
    }

    // The spec is ambiguous about whether seconds should be expressed as integers, or
    // whether floating-point values are allowed. For compatibility with a wide range of
    // clients, we round down to the nearest second.
    pub(crate) fn utc_to_seconds(utc: &DateTime<Utc>) -> Timestamp {
        Timestamp::Seconds(utc.timestamp().into())
    }

    // Some providers return boolean values as strings. Provide support for
    // parsing using stdlib.
    #[cfg(feature = "accept-string-booleans")]
    pub mod serde_string_bool {
        use serde::{de, Deserializer};

        use std::fmt;

        pub fn deserialize<'de, D>(deserializer: D) -> Result<bool, D::Error>
        where
            D: Deserializer<'de>,
        {
            struct BooleanLikeVisitor;

            impl<'de> de::Visitor<'de> for BooleanLikeVisitor {
                type Value = bool;

                fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                    formatter.write_str("A boolean-like value")
                }

                fn visit_bool<E>(self, v: bool) -> Result<Self::Value, E>
                where
                    E: de::Error,
                {
                    Ok(v)
                }

                fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
                where
                    E: de::Error,
                {
                    v.parse().map_err(E::custom)
                }
            }
            deserializer.deserialize_any(BooleanLikeVisitor)
        }
    }

    pub mod serde_utc_seconds {
        use crate::types::Timestamp;
        use chrono::{DateTime, Utc};
        use serde::{Deserialize, Deserializer, Serialize, Serializer};

        pub fn deserialize<'de, D>(deserializer: D) -> Result<DateTime<Utc>, D::Error>
        where
            D: Deserializer<'de>,
        {
            let seconds: Timestamp = Deserialize::deserialize(deserializer)?;
            super::timestamp_to_utc(&seconds).map_err(|_| {
                serde::de::Error::custom(format!(
                    "failed to parse `{}` as UTC datetime (in seconds)",
                    seconds
                ))
            })
        }

        pub fn serialize<S>(v: &DateTime<Utc>, serializer: S) -> Result<S::Ok, S::Error>
        where
            S: Serializer,
        {
            super::utc_to_seconds(v).serialize(serializer)
        }
    }

    pub mod serde_utc_seconds_opt {
        use crate::types::Timestamp;
        use chrono::{DateTime, Utc};
        use serde::{Deserialize, Deserializer, Serialize, Serializer};

        pub fn deserialize<'de, D>(deserializer: D) -> Result<Option<DateTime<Utc>>, D::Error>
        where
            D: Deserializer<'de>,
        {
            let seconds: Option<Timestamp> = Deserialize::deserialize(deserializer)?;
            seconds
                .map(|sec| {
                    super::timestamp_to_utc(&sec).map_err(|_| {
                        serde::de::Error::custom(format!(
                            "failed to parse `{}` as UTC datetime (in seconds)",
                            sec
                        ))
                    })
                })
                .transpose()
        }

        pub fn serialize<S>(v: &Option<DateTime<Utc>>, serializer: S) -> Result<S::Ok, S::Error>
        where
            S: Serializer,
        {
            v.map(|sec| super::utc_to_seconds(&sec))
                .serialize(serializer)
        }
    }
}

mod serde_base64url_byte_array {
    use serde::de::Error;
    use serde::{Deserialize, Deserializer, Serializer};
    use serde_json::{from_value, Value};

    pub fn deserialize<'de, D>(deserializer: D) -> Result<Vec<u8>, D::Error>
    where
        D: Deserializer<'de>,
    {
        let value: Value = Deserialize::deserialize(deserializer)?;
        let base64_encoded: String = from_value(value).map_err(D::Error::custom)?;

        base64::decode_config(&base64_encoded, crate::core::base64_url_safe_no_pad()).map_err(
            |err| {
                D::Error::custom(format!(
                    "invalid base64url encoding `{}`: {:?}",
                    base64_encoded, err
                ))
            },
        )
    }

    pub fn serialize<S>(v: &[u8], serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let base64_encoded = base64::encode_config(v, base64::URL_SAFE_NO_PAD);
        serializer.serialize_str(&base64_encoded)
    }
}

#[cfg(test)]
mod tests {
    use super::IssuerUrl;

    #[test]
    fn test_issuer_url_append() {
        assert_eq!(
            "http://example.com/.well-known/openid-configuration",
            IssuerUrl::new("http://example.com".to_string())
                .unwrap()
                .join(".well-known/openid-configuration")
                .unwrap()
                .to_string()
        );
        assert_eq!(
            "http://example.com/.well-known/openid-configuration",
            IssuerUrl::new("http://example.com/".to_string())
                .unwrap()
                .join(".well-known/openid-configuration")
                .unwrap()
                .to_string()
        );
        assert_eq!(
            "http://example.com/x/.well-known/openid-configuration",
            IssuerUrl::new("http://example.com/x".to_string())
                .unwrap()
                .join(".well-known/openid-configuration")
                .unwrap()
                .to_string()
        );
        assert_eq!(
            "http://example.com/x/.well-known/openid-configuration",
            IssuerUrl::new("http://example.com/x/".to_string())
                .unwrap()
                .join(".well-known/openid-configuration")
                .unwrap()
                .to_string()
        );
    }

    #[test]
    fn test_url_serialize() {
        let issuer_url =
            IssuerUrl::new("http://example.com/.well-known/openid-configuration".to_string())
                .unwrap();
        let serialized_url = serde_json::to_string(&issuer_url).unwrap();

        assert_eq!(
            "\"http://example.com/.well-known/openid-configuration\"",
            serialized_url
        );

        let deserialized_url = serde_json::from_str(&serialized_url).unwrap();
        assert_eq!(issuer_url, deserialized_url);

        assert_eq!(
            serde_json::to_string(&IssuerUrl::new("http://example.com".to_string()).unwrap())
                .unwrap(),
            "\"http://example.com\"",
        );
    }

    #[cfg(feature = "accept-string-booleans")]
    #[test]
    fn test_string_bool_parse() {
        use crate::types::Boolean;

        fn test_case(input: &str, expect: bool) {
            let value: Boolean = serde_json::from_str(input).unwrap();
            assert_eq!(value.0, expect);
        }
        test_case("true", true);
        test_case("false", false);
        test_case("\"true\"", true);
        test_case("\"false\"", false);
        assert!(serde_json::from_str::<Boolean>("\"maybe\"").is_err());
    }
}
