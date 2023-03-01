use std::fmt::Debug;
use std::future::Future;
use std::marker::PhantomData;

use http::header::{HeaderValue, ACCEPT};
use http::method::Method;
use http::status::StatusCode;
use oauth2::{AuthUrl, Scope, TokenUrl};
use serde::de::DeserializeOwned;
use serde::Serialize;
use serde_with::{serde_as, skip_serializing_none, VecSkipError};
use thiserror::Error;

use super::http_utils::{check_content_type, MIME_TYPE_JSON};
use super::types::{
    AuthDisplay, AuthenticationContextClass, ClaimName, ClaimType, ClientAuthMethod, GrantType,
    IssuerUrl, JsonWebKey, JsonWebKeySet, JsonWebKeySetUrl, JsonWebKeyType, JsonWebKeyUse,
    JweContentEncryptionAlgorithm, JweKeyManagementAlgorithm, JwsSigningAlgorithm, LanguageTag,
    OpPolicyUrl, OpTosUrl, RegistrationUrl, ResponseMode, ResponseType, ResponseTypes,
    ServiceDocUrl, SubjectIdentifierType,
};
use super::{HttpRequest, HttpResponse, UserInfoUrl, CONFIG_URL_SUFFIX};

///
/// Trait for adding extra fields to [`ProviderMetadata`].
///
pub trait AdditionalProviderMetadata: Clone + Debug + DeserializeOwned + Serialize {}

// In order to support serde flatten, this must be an empty struct rather than an empty
// tuple struct.
///
/// Empty (default) extra [`ProviderMetadata`] fields.
///
#[derive(Clone, Debug, Default, Deserialize, PartialEq, Serialize)]
pub struct EmptyAdditionalProviderMetadata {}
impl AdditionalProviderMetadata for EmptyAdditionalProviderMetadata {}

///
/// Provider metadata returned by [OpenID Connect Discovery](
/// https://openid.net/specs/openid-connect-discovery-1_0.html#ProviderMetadata).
///
#[serde_as]
#[skip_serializing_none]
#[derive(Clone, Debug, Deserialize, PartialEq, Serialize)]
#[allow(clippy::type_complexity)]
pub struct ProviderMetadata<A, AD, CA, CN, CT, G, JE, JK, JS, JT, JU, K, RM, RT, S>
where
    A: AdditionalProviderMetadata,
    AD: AuthDisplay,
    CA: ClientAuthMethod,
    CN: ClaimName,
    CT: ClaimType,
    G: GrantType,
    JE: JweContentEncryptionAlgorithm<JT>,
    JK: JweKeyManagementAlgorithm,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    JU: JsonWebKeyUse,
    K: JsonWebKey<JS, JT, JU>,
    RM: ResponseMode,
    RT: ResponseType,
    S: SubjectIdentifierType,
{
    issuer: IssuerUrl,
    authorization_endpoint: AuthUrl,
    token_endpoint: Option<TokenUrl>,
    userinfo_endpoint: Option<UserInfoUrl>,
    jwks_uri: JsonWebKeySetUrl,
    #[serde(default = "JsonWebKeySet::default", skip)]
    jwks: JsonWebKeySet<JS, JT, JU, K>,
    registration_endpoint: Option<RegistrationUrl>,
    scopes_supported: Option<Vec<Scope>>,
    #[serde(bound(deserialize = "RT: ResponseType"))]
    response_types_supported: Vec<ResponseTypes<RT>>,
    #[serde(bound(deserialize = "RM: ResponseMode"))]
    response_modes_supported: Option<Vec<RM>>,
    #[serde(bound(deserialize = "G: GrantType"))]
    grant_types_supported: Option<Vec<G>>,
    acr_values_supported: Option<Vec<AuthenticationContextClass>>,
    #[serde(bound(deserialize = "S: SubjectIdentifierType"))]
    subject_types_supported: Vec<S>,
    #[serde(bound(deserialize = "JS: JwsSigningAlgorithm<JT>"))]
    #[serde_as(as = "VecSkipError<_>")]
    id_token_signing_alg_values_supported: Vec<JS>,
    #[serde(
        bound(deserialize = "JK: JweKeyManagementAlgorithm"),
        default = "Option::default"
    )]
    #[serde_as(as = "Option<VecSkipError<_>>")]
    id_token_encryption_alg_values_supported: Option<Vec<JK>>,
    #[serde(
        bound(deserialize = "JE: JweContentEncryptionAlgorithm<JT>"),
        default = "Option::default"
    )]
    #[serde_as(as = "Option<VecSkipError<_>>")]
    id_token_encryption_enc_values_supported: Option<Vec<JE>>,
    #[serde(
        bound(deserialize = "JS: JwsSigningAlgorithm<JT>"),
        default = "Option::default"
    )]
    #[serde_as(as = "Option<VecSkipError<_>>")]
    userinfo_signing_alg_values_supported: Option<Vec<JS>>,
    #[serde(
        bound(deserialize = "JK: JweKeyManagementAlgorithm"),
        default = "Option::default"
    )]
    #[serde_as(as = "Option<VecSkipError<_>>")]
    userinfo_encryption_alg_values_supported: Option<Vec<JK>>,
    #[serde(
        bound(deserialize = "JE: JweContentEncryptionAlgorithm<JT>"),
        default = "Option::default"
    )]
    #[serde_as(as = "Option<VecSkipError<_>>")]
    userinfo_encryption_enc_values_supported: Option<Vec<JE>>,
    #[serde(
        bound(deserialize = "JS: JwsSigningAlgorithm<JT>"),
        default = "Option::default"
    )]
    #[serde_as(as = "Option<VecSkipError<_>>")]
    request_object_signing_alg_values_supported: Option<Vec<JS>>,
    #[serde(
        bound(deserialize = "JK: JweKeyManagementAlgorithm"),
        default = "Option::default"
    )]
    #[serde_as(as = "Option<VecSkipError<_>>")]
    request_object_encryption_alg_values_supported: Option<Vec<JK>>,
    #[serde(
        bound(deserialize = "JE: JweContentEncryptionAlgorithm<JT>"),
        default = "Option::default"
    )]
    #[serde_as(as = "Option<VecSkipError<_>>")]
    request_object_encryption_enc_values_supported: Option<Vec<JE>>,
    #[serde(bound(deserialize = "CA: ClientAuthMethod"))]
    token_endpoint_auth_methods_supported: Option<Vec<CA>>,
    #[serde(
        bound(deserialize = "JS: JwsSigningAlgorithm<JT>"),
        default = "Option::default"
    )]
    #[serde_as(as = "Option<VecSkipError<_>>")]
    token_endpoint_auth_signing_alg_values_supported: Option<Vec<JS>>,
    #[serde(bound(deserialize = "AD: AuthDisplay"))]
    display_values_supported: Option<Vec<AD>>,
    #[serde(bound(deserialize = "CT: ClaimType"))]
    claim_types_supported: Option<Vec<CT>>,
    #[serde(bound(deserialize = "CN: ClaimName"))]
    claims_supported: Option<Vec<CN>>,
    service_documentation: Option<ServiceDocUrl>,
    claims_locales_supported: Option<Vec<LanguageTag>>,
    ui_locales_supported: Option<Vec<LanguageTag>>,
    claims_parameter_supported: Option<bool>,
    request_parameter_supported: Option<bool>,
    request_uri_parameter_supported: Option<bool>,
    require_request_uri_registration: Option<bool>,
    op_policy_uri: Option<OpPolicyUrl>,
    op_tos_uri: Option<OpTosUrl>,

    #[serde(bound(deserialize = "A: AdditionalProviderMetadata"), flatten)]
    additional_metadata: A,

    #[serde(skip)]
    _phantom_jt: PhantomData<JT>,
}
impl<A, AD, CA, CN, CT, G, JE, JK, JS, JT, JU, K, RM, RT, S>
    ProviderMetadata<A, AD, CA, CN, CT, G, JE, JK, JS, JT, JU, K, RM, RT, S>
where
    A: AdditionalProviderMetadata,
    AD: AuthDisplay,
    CA: ClientAuthMethod,
    CN: ClaimName,
    CT: ClaimType,
    G: GrantType,
    JE: JweContentEncryptionAlgorithm<JT>,
    JK: JweKeyManagementAlgorithm,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    JU: JsonWebKeyUse,
    K: JsonWebKey<JS, JT, JU>,
    RM: ResponseMode,
    RT: ResponseType,
    S: SubjectIdentifierType,
{
    ///
    /// Instantiates new provider metadata.
    ///
    pub fn new(
        issuer: IssuerUrl,
        authorization_endpoint: AuthUrl,
        jwks_uri: JsonWebKeySetUrl,
        response_types_supported: Vec<ResponseTypes<RT>>,
        subject_types_supported: Vec<S>,
        id_token_signing_alg_values_supported: Vec<JS>,
        additional_metadata: A,
    ) -> Self {
        Self {
            issuer,
            authorization_endpoint,
            token_endpoint: None,
            userinfo_endpoint: None,
            jwks_uri,
            jwks: JsonWebKeySet::new(Vec::new()),
            registration_endpoint: None,
            scopes_supported: None,
            response_types_supported,
            response_modes_supported: None,
            grant_types_supported: None,
            acr_values_supported: None,
            subject_types_supported,
            id_token_signing_alg_values_supported,
            id_token_encryption_alg_values_supported: None,
            id_token_encryption_enc_values_supported: None,
            userinfo_signing_alg_values_supported: None,
            userinfo_encryption_alg_values_supported: None,
            userinfo_encryption_enc_values_supported: None,
            request_object_signing_alg_values_supported: None,
            request_object_encryption_alg_values_supported: None,
            request_object_encryption_enc_values_supported: None,
            token_endpoint_auth_methods_supported: None,
            token_endpoint_auth_signing_alg_values_supported: None,
            display_values_supported: None,
            claim_types_supported: None,
            claims_supported: None,
            service_documentation: None,
            claims_locales_supported: None,
            ui_locales_supported: None,
            claims_parameter_supported: None,
            request_parameter_supported: None,
            request_uri_parameter_supported: None,
            require_request_uri_registration: None,
            op_policy_uri: None,
            op_tos_uri: None,
            additional_metadata,
            _phantom_jt: PhantomData,
        }
    }

    field_getters_setters![
        pub self [self] ["provider metadata value"] {
            set_issuer -> issuer[IssuerUrl],
            set_authorization_endpoint -> authorization_endpoint[AuthUrl],
            set_token_endpoint -> token_endpoint[Option<TokenUrl>],
            set_userinfo_endpoint -> userinfo_endpoint[Option<UserInfoUrl>],
            set_jwks_uri -> jwks_uri[JsonWebKeySetUrl],
            set_jwks -> jwks[JsonWebKeySet<JS, JT, JU, K>],
            set_registration_endpoint -> registration_endpoint[Option<RegistrationUrl>],
            set_scopes_supported -> scopes_supported[Option<Vec<Scope>>],
            set_response_types_supported -> response_types_supported[Vec<ResponseTypes<RT>>],
            set_response_modes_supported -> response_modes_supported[Option<Vec<RM>>],
            set_grant_types_supported -> grant_types_supported[Option<Vec<G>>],
            set_acr_values_supported
                -> acr_values_supported[Option<Vec<AuthenticationContextClass>>],
            set_subject_types_supported -> subject_types_supported[Vec<S>],
            set_id_token_signing_alg_values_supported
                -> id_token_signing_alg_values_supported[Vec<JS>],
            set_id_token_encryption_alg_values_supported
                -> id_token_encryption_alg_values_supported[Option<Vec<JK>>],
            set_id_token_encryption_enc_values_supported
                -> id_token_encryption_enc_values_supported[Option<Vec<JE>>],
            set_userinfo_signing_alg_values_supported
                -> userinfo_signing_alg_values_supported[Option<Vec<JS>>],
            set_userinfo_encryption_alg_values_supported
                -> userinfo_encryption_alg_values_supported[Option<Vec<JK>>],
            set_userinfo_encryption_enc_values_supported
                -> userinfo_encryption_enc_values_supported[Option<Vec<JE>>],
            set_request_object_signing_alg_values_supported
                -> request_object_signing_alg_values_supported[Option<Vec<JS>>],
            set_request_object_encryption_alg_values_supported
                -> request_object_encryption_alg_values_supported[Option<Vec<JK>>],
            set_request_object_encryption_enc_values_supported
                -> request_object_encryption_enc_values_supported[Option<Vec<JE>>],
            set_token_endpoint_auth_methods_supported
                -> token_endpoint_auth_methods_supported[Option<Vec<CA>>],
            set_token_endpoint_auth_signing_alg_values_supported
                -> token_endpoint_auth_signing_alg_values_supported[Option<Vec<JS>>],
            set_display_values_supported -> display_values_supported[Option<Vec<AD>>],
            set_claim_types_supported -> claim_types_supported[Option<Vec<CT>>],
            set_claims_supported -> claims_supported[Option<Vec<CN>>],
            set_service_documentation -> service_documentation[Option<ServiceDocUrl>],
            set_claims_locales_supported -> claims_locales_supported[Option<Vec<LanguageTag>>],
            set_ui_locales_supported -> ui_locales_supported[Option<Vec<LanguageTag>>],
            set_claims_parameter_supported -> claims_parameter_supported[Option<bool>],
            set_request_parameter_supported -> request_parameter_supported[Option<bool>],
            set_request_uri_parameter_supported -> request_uri_parameter_supported[Option<bool>],
            set_require_request_uri_registration -> require_request_uri_registration[Option<bool>],
            set_op_policy_uri -> op_policy_uri[Option<OpPolicyUrl>],
            set_op_tos_uri -> op_tos_uri[Option<OpTosUrl>],
        }
    ];

    ///
    /// Fetches the OpenID Connect Discovery document and associated JSON Web Key Set from the
    /// OpenID Connect Provider.
    ///
    pub fn discover<HC, RE>(
        issuer_url: &IssuerUrl,
        http_client: HC,
    ) -> Result<Self, DiscoveryError<RE>>
    where
        HC: Fn(HttpRequest) -> Result<HttpResponse, RE>,
        RE: std::error::Error + 'static,
    {
        let discovery_url = issuer_url
            .join(CONFIG_URL_SUFFIX)
            .map_err(DiscoveryError::UrlParse)?;

        http_client(Self::discovery_request(discovery_url))
            .map_err(DiscoveryError::Request)
            .and_then(|http_response| Self::discovery_response(issuer_url, http_response))
            .and_then(|provider_metadata| {
                JsonWebKeySet::fetch(provider_metadata.jwks_uri(), http_client).map(|jwks| Self {
                    jwks,
                    ..provider_metadata
                })
            })
    }

    ///
    /// Asynchronously fetches the OpenID Connect Discovery document and associated JSON Web Key Set
    /// from the OpenID Connect Provider.
    ///
    pub async fn discover_async<F, HC, RE>(
        issuer_url: IssuerUrl,
        http_client: HC,
    ) -> Result<Self, DiscoveryError<RE>>
    where
        F: Future<Output = Result<HttpResponse, RE>>,
        HC: Fn(HttpRequest) -> F + 'static,
        RE: std::error::Error + 'static,
    {
        let discovery_url = issuer_url
            .join(CONFIG_URL_SUFFIX)
            .map_err(DiscoveryError::UrlParse)?;

        let provider_metadata = http_client(Self::discovery_request(discovery_url))
            .await
            .map_err(DiscoveryError::Request)
            .and_then(|http_response| Self::discovery_response(&issuer_url, http_response))?;

        JsonWebKeySet::fetch_async(provider_metadata.jwks_uri(), http_client)
            .await
            .map(|jwks| Self {
                jwks,
                ..provider_metadata
            })
    }

    fn discovery_request(discovery_url: url::Url) -> HttpRequest {
        HttpRequest {
            url: discovery_url,
            method: Method::GET,
            headers: vec![(ACCEPT, HeaderValue::from_static(MIME_TYPE_JSON))]
                .into_iter()
                .collect(),
            body: Vec::new(),
        }
    }

    fn discovery_response<RE>(
        issuer_url: &IssuerUrl,
        discovery_response: HttpResponse,
    ) -> Result<Self, DiscoveryError<RE>>
    where
        RE: std::error::Error + 'static,
    {
        if discovery_response.status_code != StatusCode::OK {
            return Err(DiscoveryError::Response(
                discovery_response.status_code,
                discovery_response.body,
                format!("HTTP status code {}", discovery_response.status_code),
            ));
        }

        check_content_type(&discovery_response.headers, MIME_TYPE_JSON).map_err(|err_msg| {
            DiscoveryError::Response(
                discovery_response.status_code,
                discovery_response.body.clone(),
                err_msg,
            )
        })?;

        let provider_metadata = serde_path_to_error::deserialize::<_, Self>(
            &mut serde_json::Deserializer::from_slice(&discovery_response.body),
        )
        .map_err(DiscoveryError::Parse)?;

        if provider_metadata.issuer() != issuer_url {
            Err(DiscoveryError::Validation(format!(
                "unexpected issuer URI `{}` (expected `{}`)",
                provider_metadata.issuer().as_str(),
                issuer_url.as_str()
            )))
        } else {
            Ok(provider_metadata)
        }
    }

    ///
    /// Returns additional provider metadata fields.
    ///
    pub fn additional_metadata(&self) -> &A {
        &self.additional_metadata
    }
    ///
    /// Returns mutable additional provider metadata fields.
    ///
    pub fn additional_metadata_mut(&mut self) -> &mut A {
        &mut self.additional_metadata
    }
}

///
/// Error retrieving provider metadata.
///
#[derive(Debug, Error)]
#[non_exhaustive]
pub enum DiscoveryError<RE>
where
    RE: std::error::Error + 'static,
{
    ///
    /// An unexpected error occurred.
    ///
    #[error("Other error: {0}")]
    Other(String),
    ///
    /// Failed to parse server response.
    ///
    #[error("Failed to parse server response")]
    Parse(#[source] serde_path_to_error::Error<serde_json::Error>),
    ///
    /// An error occurred while sending the request or receiving the response (e.g., network
    /// connectivity failed).
    ///
    #[error("Request failed")]
    Request(#[source] RE),
    ///
    /// Server returned an invalid response.
    ///
    #[error("Server returned invalid response: {2}")]
    Response(StatusCode, Vec<u8>, String),
    ///
    /// Failed to parse discovery URL from issuer URL.
    ///
    #[error("Failed to parse URL")]
    UrlParse(#[source] url::ParseError),
    ///
    /// Failed to validate provider metadata.
    ///
    #[error("Validation error: {0}")]
    Validation(String),
}

#[cfg(test)]
mod tests {
    use oauth2::{AuthUrl, Scope, TokenUrl};

    use crate::core::{
        CoreAuthDisplay, CoreClaimName, CoreClaimType, CoreClientAuthMethod, CoreGrantType,
        CoreJweContentEncryptionAlgorithm, CoreJweKeyManagementAlgorithm, CoreJwsSigningAlgorithm,
        CoreProviderMetadata, CoreResponseMode, CoreResponseType, CoreSubjectIdentifierType,
    };

    use super::{
        AuthenticationContextClass, IssuerUrl, JsonWebKeySetUrl, LanguageTag, OpPolicyUrl,
        OpTosUrl, RegistrationUrl, ResponseTypes, ServiceDocUrl, UserInfoUrl,
    };

    #[test]
    fn test_discovery_deserialization() {
        // Fetched from: https://rp.certification.openid.net:8080/openidconnect-rs/
        //     rp-response_type-code/.well-known/openid-configuration
        let json_response_standard = "\
            \"issuer\":\"https://rp.certification.openid.net:8080/openidconnect-rs/rp-response_type-code\",\
            \"authorization_endpoint\":\"https://rp.certification.openid.net:8080/openidconnect-rs/rp-response_type-code/authorization\",\
            \"token_endpoint\":\"https://rp.certification.openid.net:8080/openidconnect-rs/rp-response_type-code/token\",\
            \"userinfo_endpoint\":\"https://rp.certification.openid.net:8080/openidconnect-rs/rp-response_type-code/userinfo\",\
            \"jwks_uri\":\"https://rp.certification.openid.net:8080/static/jwks_3INbZl52IrrPCp2j.json\",\
            \"registration_endpoint\":\"https://rp.certification.openid.net:8080/openidconnect-rs/rp-response_type-code/registration\",\
            \"scopes_supported\":[\
               \"email\",\
               \"phone\",\
               \"profile\",\
               \"openid\",\
               \"address\",\
               \"offline_access\",\
               \"openid\"\
            ],\
            \"response_types_supported\":[\
               \"code\"\
            ],\
            \"response_modes_supported\":[\
               \"query\",\
               \"fragment\",\
               \"form_post\"\
            ],\
            \"grant_types_supported\":[\
               \"authorization_code\",\
               \"implicit\",\
               \"urn:ietf:params:oauth:grant-type:jwt-bearer\",\
               \"refresh_token\"\
            ],\
            \"acr_values_supported\":[\
               \"PASSWORD\"\
            ],\
            \"subject_types_supported\":[\
               \"public\",\
               \"pairwise\"\
            ],\
            \"id_token_signing_alg_values_supported\":[\
               \"RS256\",\
               \"RS384\",\
               \"RS512\",\
               \"ES256\",\
               \"ES384\",\
               \"ES512\",\
               \"HS256\",\
               \"HS384\",\
               \"HS512\",\
               \"PS256\",\
               \"PS384\",\
               \"PS512\",\
               \"none\"\
            ],\
            \"id_token_encryption_alg_values_supported\":[\
               \"RSA1_5\",\
               \"RSA-OAEP\",\
               \"RSA-OAEP-256\",\
               \"A128KW\",\
               \"A192KW\",\
               \"A256KW\",\
               \"ECDH-ES\",\
               \"ECDH-ES+A128KW\",\
               \"ECDH-ES+A192KW\",\
               \"ECDH-ES+A256KW\"\
            ],\
            \"id_token_encryption_enc_values_supported\":[\
               \"A128CBC-HS256\",\
               \"A192CBC-HS384\",\
               \"A256CBC-HS512\",\
               \"A128GCM\",\
               \"A192GCM\",\
               \"A256GCM\"\
            ],\
            \"userinfo_signing_alg_values_supported\":[\
               \"RS256\",\
               \"RS384\",\
               \"RS512\",\
               \"ES256\",\
               \"ES384\",\
               \"ES512\",\
               \"HS256\",\
               \"HS384\",\
               \"HS512\",\
               \"PS256\",\
               \"PS384\",\
               \"PS512\",\
               \"none\"\
            ],\
            \"userinfo_encryption_alg_values_supported\":[\
               \"RSA1_5\",\
               \"RSA-OAEP\",\
               \"RSA-OAEP-256\",\
               \"A128KW\",\
               \"A192KW\",\
               \"A256KW\",\
               \"ECDH-ES\",\
               \"ECDH-ES+A128KW\",\
               \"ECDH-ES+A192KW\",\
               \"ECDH-ES+A256KW\"\
            ],\
            \"userinfo_encryption_enc_values_supported\":[\
               \"A128CBC-HS256\",\
               \"A192CBC-HS384\",\
               \"A256CBC-HS512\",\
               \"A128GCM\",\
               \"A192GCM\",\
               \"A256GCM\"\
            ],\
            \"request_object_signing_alg_values_supported\":[\
               \"RS256\",\
               \"RS384\",\
               \"RS512\",\
               \"ES256\",\
               \"ES384\",\
               \"ES512\",\
               \"HS256\",\
               \"HS384\",\
               \"HS512\",\
               \"PS256\",\
               \"PS384\",\
               \"PS512\",\
               \"none\"\
            ],\
            \"request_object_encryption_alg_values_supported\":[\
               \"RSA1_5\",\
               \"RSA-OAEP\",\
               \"RSA-OAEP-256\",\
               \"A128KW\",\
               \"A192KW\",\
               \"A256KW\",\
               \"ECDH-ES\",\
               \"ECDH-ES+A128KW\",\
               \"ECDH-ES+A192KW\",\
               \"ECDH-ES+A256KW\"\
            ],\
            \"request_object_encryption_enc_values_supported\":[\
               \"A128CBC-HS256\",\
               \"A192CBC-HS384\",\
               \"A256CBC-HS512\",\
               \"A128GCM\",\
               \"A192GCM\",\
               \"A256GCM\"\
            ],\
            \"token_endpoint_auth_methods_supported\":[\
               \"client_secret_post\",\
               \"client_secret_basic\",\
               \"client_secret_jwt\",\
               \"private_key_jwt\"\
            ],\
            \"token_endpoint_auth_signing_alg_values_supported\":[\
               \"RS256\",\
               \"RS384\",\
               \"RS512\",\
               \"ES256\",\
               \"ES384\",\
               \"ES512\",\
               \"HS256\",\
               \"HS384\",\
               \"HS512\",\
               \"PS256\",\
               \"PS384\",\
               \"PS512\"\
            ],\
            \"claim_types_supported\":[\
               \"normal\",\
               \"aggregated\",\
               \"distributed\"\
            ],\
            \"claims_supported\":[\
               \"name\",\
               \"given_name\",\
               \"middle_name\",\
               \"picture\",\
               \"email_verified\",\
               \"birthdate\",\
               \"sub\",\
               \"address\",\
               \"zoneinfo\",\
               \"email\",\
               \"gender\",\
               \"preferred_username\",\
               \"family_name\",\
               \"website\",\
               \"profile\",\
               \"phone_number_verified\",\
               \"nickname\",\
               \"updated_at\",\
               \"phone_number\",\
               \"locale\"\
            ],\
            \"claims_parameter_supported\":true,\
            \"request_parameter_supported\":true,\
            \"request_uri_parameter_supported\":true,\
            \"require_request_uri_registration\":true";

        let json_response = format!(
            "{{{},{}}}",
            json_response_standard,
            "\"end_session_endpoint\":\"https://rp.certification.openid.net:8080/openidconnect-rs/rp-response_type-code/end_session\",\
            \"version\":\"3.0\""
        );
        dbg!(&json_response);

        let all_signing_algs = vec![
            CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha256,
            CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha384,
            CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha512,
            CoreJwsSigningAlgorithm::EcdsaP256Sha256,
            CoreJwsSigningAlgorithm::EcdsaP384Sha384,
            CoreJwsSigningAlgorithm::EcdsaP521Sha512,
            CoreJwsSigningAlgorithm::HmacSha256,
            CoreJwsSigningAlgorithm::HmacSha384,
            CoreJwsSigningAlgorithm::HmacSha512,
            CoreJwsSigningAlgorithm::RsaSsaPssSha256,
            CoreJwsSigningAlgorithm::RsaSsaPssSha384,
            CoreJwsSigningAlgorithm::RsaSsaPssSha512,
            CoreJwsSigningAlgorithm::None,
        ];
        let all_encryption_algs = vec![
            CoreJweKeyManagementAlgorithm::RsaPkcs1V15,
            CoreJweKeyManagementAlgorithm::RsaOaep,
            CoreJweKeyManagementAlgorithm::RsaOaepSha256,
            CoreJweKeyManagementAlgorithm::AesKeyWrap128,
            CoreJweKeyManagementAlgorithm::AesKeyWrap192,
            CoreJweKeyManagementAlgorithm::AesKeyWrap256,
            CoreJweKeyManagementAlgorithm::EcdhEs,
            CoreJweKeyManagementAlgorithm::EcdhEsAesKeyWrap128,
            CoreJweKeyManagementAlgorithm::EcdhEsAesKeyWrap192,
            CoreJweKeyManagementAlgorithm::EcdhEsAesKeyWrap256,
        ];
        let new_provider_metadata = CoreProviderMetadata::new(
            IssuerUrl::new(
                "https://rp.certification.openid.net:8080/openidconnect-rs/rp-response_type-code"
                    .to_string(),
            )
            .unwrap(),
            AuthUrl::new(
                "https://rp.certification.openid.net:8080/openidconnect-rs/\
                 rp-response_type-code/authorization"
                    .to_string(),
            )
            .unwrap(),
            JsonWebKeySetUrl::new(
                "https://rp.certification.openid.net:8080/static/jwks_3INbZl52IrrPCp2j.json"
                    .to_string(),
            )
            .unwrap(),
            vec![ResponseTypes::new(vec![CoreResponseType::Code])],
            vec![
                CoreSubjectIdentifierType::Public,
                CoreSubjectIdentifierType::Pairwise,
            ],
            all_signing_algs.clone(),
            Default::default(),
        )
        .set_request_object_signing_alg_values_supported(Some(all_signing_algs.clone()))
        .set_token_endpoint_auth_signing_alg_values_supported(Some(vec![
            CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha256,
            CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha384,
            CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha512,
            CoreJwsSigningAlgorithm::EcdsaP256Sha256,
            CoreJwsSigningAlgorithm::EcdsaP384Sha384,
            CoreJwsSigningAlgorithm::EcdsaP521Sha512,
            CoreJwsSigningAlgorithm::HmacSha256,
            CoreJwsSigningAlgorithm::HmacSha384,
            CoreJwsSigningAlgorithm::HmacSha512,
            CoreJwsSigningAlgorithm::RsaSsaPssSha256,
            CoreJwsSigningAlgorithm::RsaSsaPssSha384,
            CoreJwsSigningAlgorithm::RsaSsaPssSha512,
        ]))
        .set_scopes_supported(Some(vec![
            Scope::new("email".to_string()),
            Scope::new("phone".to_string()),
            Scope::new("profile".to_string()),
            Scope::new("openid".to_string()),
            Scope::new("address".to_string()),
            Scope::new("offline_access".to_string()),
            Scope::new("openid".to_string()),
        ]))
        .set_userinfo_signing_alg_values_supported(Some(all_signing_algs))
        .set_id_token_encryption_enc_values_supported(Some(vec![
            CoreJweContentEncryptionAlgorithm::Aes128CbcHmacSha256,
            CoreJweContentEncryptionAlgorithm::Aes192CbcHmacSha384,
            CoreJweContentEncryptionAlgorithm::Aes256CbcHmacSha512,
            CoreJweContentEncryptionAlgorithm::Aes128Gcm,
            CoreJweContentEncryptionAlgorithm::Aes192Gcm,
            CoreJweContentEncryptionAlgorithm::Aes256Gcm,
        ]))
        .set_grant_types_supported(Some(vec![
            CoreGrantType::AuthorizationCode,
            CoreGrantType::Implicit,
            CoreGrantType::JwtBearer,
            CoreGrantType::RefreshToken,
        ]))
        .set_response_modes_supported(Some(vec![
            CoreResponseMode::Query,
            CoreResponseMode::Fragment,
            CoreResponseMode::FormPost,
        ]))
        .set_require_request_uri_registration(Some(true))
        .set_registration_endpoint(Some(
            RegistrationUrl::new(
                "https://rp.certification.openid.net:8080/openidconnect-rs/\
                 rp-response_type-code/registration"
                    .to_string(),
            )
            .unwrap(),
        ))
        .set_claims_parameter_supported(Some(true))
        .set_request_object_encryption_enc_values_supported(Some(vec![
            CoreJweContentEncryptionAlgorithm::Aes128CbcHmacSha256,
            CoreJweContentEncryptionAlgorithm::Aes192CbcHmacSha384,
            CoreJweContentEncryptionAlgorithm::Aes256CbcHmacSha512,
            CoreJweContentEncryptionAlgorithm::Aes128Gcm,
            CoreJweContentEncryptionAlgorithm::Aes192Gcm,
            CoreJweContentEncryptionAlgorithm::Aes256Gcm,
        ]))
        .set_userinfo_endpoint(Some(
            UserInfoUrl::new(
                "https://rp.certification.openid.net:8080/openidconnect-rs/\
                 rp-response_type-code/userinfo"
                    .to_string(),
            )
            .unwrap(),
        ))
        .set_token_endpoint_auth_methods_supported(Some(vec![
            CoreClientAuthMethod::ClientSecretPost,
            CoreClientAuthMethod::ClientSecretBasic,
            CoreClientAuthMethod::ClientSecretJwt,
            CoreClientAuthMethod::PrivateKeyJwt,
        ]))
        .set_claims_supported(Some(
            vec![
                "name",
                "given_name",
                "middle_name",
                "picture",
                "email_verified",
                "birthdate",
                "sub",
                "address",
                "zoneinfo",
                "email",
                "gender",
                "preferred_username",
                "family_name",
                "website",
                "profile",
                "phone_number_verified",
                "nickname",
                "updated_at",
                "phone_number",
                "locale",
            ]
            .iter()
            .map(|claim| CoreClaimName::new((*claim).to_string()))
            .collect(),
        ))
        .set_request_object_encryption_alg_values_supported(Some(all_encryption_algs.clone()))
        .set_claim_types_supported(Some(vec![
            CoreClaimType::Normal,
            CoreClaimType::Aggregated,
            CoreClaimType::Distributed,
        ]))
        .set_request_uri_parameter_supported(Some(true))
        .set_request_parameter_supported(Some(true))
        .set_token_endpoint(Some(
            TokenUrl::new(
                "https://rp.certification.openid.net:8080/openidconnect-rs/\
                 rp-response_type-code/token"
                    .to_string(),
            )
            .unwrap(),
        ))
        .set_id_token_encryption_alg_values_supported(Some(all_encryption_algs.clone()))
        .set_userinfo_encryption_alg_values_supported(Some(all_encryption_algs))
        .set_userinfo_encryption_enc_values_supported(Some(vec![
            CoreJweContentEncryptionAlgorithm::Aes128CbcHmacSha256,
            CoreJweContentEncryptionAlgorithm::Aes192CbcHmacSha384,
            CoreJweContentEncryptionAlgorithm::Aes256CbcHmacSha512,
            CoreJweContentEncryptionAlgorithm::Aes128Gcm,
            CoreJweContentEncryptionAlgorithm::Aes192Gcm,
            CoreJweContentEncryptionAlgorithm::Aes256Gcm,
        ]))
        .set_acr_values_supported(Some(vec![AuthenticationContextClass::new(
            "PASSWORD".to_string(),
        )]));

        let provider_metadata: CoreProviderMetadata = serde_json::from_str(&json_response).unwrap();
        assert_eq!(provider_metadata, new_provider_metadata);

        let serialized = serde_json::to_string(&provider_metadata).unwrap();
        assert_eq!(serialized, format!("{{{}}}", json_response_standard));

        assert_eq!(
            IssuerUrl::new(
                "https://rp.certification.openid.net:8080/openidconnect-rs/rp-response_type-code"
                    .to_string()
            )
            .unwrap(),
            *provider_metadata.issuer()
        );
        assert_eq!(
            AuthUrl::new(
                "https://rp.certification.openid.net:8080/openidconnect-rs/rp-response_type-code\
                 /authorization"
                    .to_string()
            )
            .unwrap(),
            *provider_metadata.authorization_endpoint()
        );
        assert_eq!(
            Some(
                &TokenUrl::new(
                    "https://rp.certification.openid.net:8080/openidconnect-rs\
                     /rp-response_type-code/token"
                        .to_string()
                )
                .unwrap()
            ),
            provider_metadata.token_endpoint()
        );
        assert_eq!(
            Some(
                &UserInfoUrl::new(
                    "https://rp.certification.openid.net:8080/openidconnect-rs\
                     /rp-response_type-code/userinfo"
                        .to_string()
                )
                .unwrap()
            ),
            provider_metadata.userinfo_endpoint()
        );
        assert_eq!(
            &JsonWebKeySetUrl::new(
                "https://rp.certification.openid.net:8080/static/jwks_3INbZl52IrrPCp2j.json"
                    .to_string()
            )
            .unwrap(),
            provider_metadata.jwks_uri()
        );
        assert_eq!(
            Some(
                &RegistrationUrl::new(
                    "https://rp.certification.openid.net:8080/openidconnect-rs\
                     /rp-response_type-code/registration"
                        .to_string()
                )
                .unwrap()
            ),
            provider_metadata.registration_endpoint()
        );
        assert_eq!(
            Some(
                &vec![
                    "email",
                    "phone",
                    "profile",
                    "openid",
                    "address",
                    "offline_access",
                    "openid",
                ]
                .iter()
                .map(|s| (*s).to_string())
                .map(Scope::new)
                .collect::<Vec<_>>()
            ),
            provider_metadata.scopes_supported()
        );
        assert_eq!(
            vec![ResponseTypes::new(vec![CoreResponseType::Code])],
            *provider_metadata.response_types_supported()
        );
        assert_eq!(
            Some(&vec![
                CoreResponseMode::Query,
                CoreResponseMode::Fragment,
                CoreResponseMode::FormPost,
            ]),
            provider_metadata.response_modes_supported()
        );
        assert_eq!(
            Some(
                &vec![
                    CoreGrantType::AuthorizationCode,
                    CoreGrantType::Implicit,
                    CoreGrantType::JwtBearer,
                    CoreGrantType::RefreshToken,
                ]
                .into_iter()
                .collect::<Vec<_>>()
            ),
            provider_metadata.grant_types_supported()
        );
        assert_eq!(
            Some(&vec![AuthenticationContextClass::new(
                "PASSWORD".to_string(),
            )]),
            provider_metadata.acr_values_supported()
        );
        assert_eq!(
            vec![
                CoreSubjectIdentifierType::Public,
                CoreSubjectIdentifierType::Pairwise,
            ],
            *provider_metadata.subject_types_supported()
        );
        assert_eq!(
            vec![
                CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha256,
                CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha384,
                CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha512,
                CoreJwsSigningAlgorithm::EcdsaP256Sha256,
                CoreJwsSigningAlgorithm::EcdsaP384Sha384,
                CoreJwsSigningAlgorithm::EcdsaP521Sha512,
                CoreJwsSigningAlgorithm::HmacSha256,
                CoreJwsSigningAlgorithm::HmacSha384,
                CoreJwsSigningAlgorithm::HmacSha512,
                CoreJwsSigningAlgorithm::RsaSsaPssSha256,
                CoreJwsSigningAlgorithm::RsaSsaPssSha384,
                CoreJwsSigningAlgorithm::RsaSsaPssSha512,
                CoreJwsSigningAlgorithm::None,
            ],
            *provider_metadata.id_token_signing_alg_values_supported()
        );
        assert_eq!(
            Some(&vec![
                CoreJweKeyManagementAlgorithm::RsaPkcs1V15,
                CoreJweKeyManagementAlgorithm::RsaOaep,
                CoreJweKeyManagementAlgorithm::RsaOaepSha256,
                CoreJweKeyManagementAlgorithm::AesKeyWrap128,
                CoreJweKeyManagementAlgorithm::AesKeyWrap192,
                CoreJweKeyManagementAlgorithm::AesKeyWrap256,
                CoreJweKeyManagementAlgorithm::EcdhEs,
                CoreJweKeyManagementAlgorithm::EcdhEsAesKeyWrap128,
                CoreJweKeyManagementAlgorithm::EcdhEsAesKeyWrap192,
                CoreJweKeyManagementAlgorithm::EcdhEsAesKeyWrap256,
            ]),
            provider_metadata.id_token_encryption_alg_values_supported()
        );
        assert_eq!(
            Some(&vec![
                CoreJweContentEncryptionAlgorithm::Aes128CbcHmacSha256,
                CoreJweContentEncryptionAlgorithm::Aes192CbcHmacSha384,
                CoreJweContentEncryptionAlgorithm::Aes256CbcHmacSha512,
                CoreJweContentEncryptionAlgorithm::Aes128Gcm,
                CoreJweContentEncryptionAlgorithm::Aes192Gcm,
                CoreJweContentEncryptionAlgorithm::Aes256Gcm,
            ]),
            provider_metadata.id_token_encryption_enc_values_supported()
        );
        assert_eq!(
            Some(&vec![
                CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha256,
                CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha384,
                CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha512,
                CoreJwsSigningAlgorithm::EcdsaP256Sha256,
                CoreJwsSigningAlgorithm::EcdsaP384Sha384,
                CoreJwsSigningAlgorithm::EcdsaP521Sha512,
                CoreJwsSigningAlgorithm::HmacSha256,
                CoreJwsSigningAlgorithm::HmacSha384,
                CoreJwsSigningAlgorithm::HmacSha512,
                CoreJwsSigningAlgorithm::RsaSsaPssSha256,
                CoreJwsSigningAlgorithm::RsaSsaPssSha384,
                CoreJwsSigningAlgorithm::RsaSsaPssSha512,
                CoreJwsSigningAlgorithm::None,
            ]),
            provider_metadata.userinfo_signing_alg_values_supported()
        );
        assert_eq!(
            Some(&vec![
                CoreJweKeyManagementAlgorithm::RsaPkcs1V15,
                CoreJweKeyManagementAlgorithm::RsaOaep,
                CoreJweKeyManagementAlgorithm::RsaOaepSha256,
                CoreJweKeyManagementAlgorithm::AesKeyWrap128,
                CoreJweKeyManagementAlgorithm::AesKeyWrap192,
                CoreJweKeyManagementAlgorithm::AesKeyWrap256,
                CoreJweKeyManagementAlgorithm::EcdhEs,
                CoreJweKeyManagementAlgorithm::EcdhEsAesKeyWrap128,
                CoreJweKeyManagementAlgorithm::EcdhEsAesKeyWrap192,
                CoreJweKeyManagementAlgorithm::EcdhEsAesKeyWrap256,
            ]),
            provider_metadata.userinfo_encryption_alg_values_supported()
        );
        assert_eq!(
            Some(&vec![
                CoreJweContentEncryptionAlgorithm::Aes128CbcHmacSha256,
                CoreJweContentEncryptionAlgorithm::Aes192CbcHmacSha384,
                CoreJweContentEncryptionAlgorithm::Aes256CbcHmacSha512,
                CoreJweContentEncryptionAlgorithm::Aes128Gcm,
                CoreJweContentEncryptionAlgorithm::Aes192Gcm,
                CoreJweContentEncryptionAlgorithm::Aes256Gcm,
            ]),
            provider_metadata.userinfo_encryption_enc_values_supported()
        );
        assert_eq!(
            Some(&vec![
                CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha256,
                CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha384,
                CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha512,
                CoreJwsSigningAlgorithm::EcdsaP256Sha256,
                CoreJwsSigningAlgorithm::EcdsaP384Sha384,
                CoreJwsSigningAlgorithm::EcdsaP521Sha512,
                CoreJwsSigningAlgorithm::HmacSha256,
                CoreJwsSigningAlgorithm::HmacSha384,
                CoreJwsSigningAlgorithm::HmacSha512,
                CoreJwsSigningAlgorithm::RsaSsaPssSha256,
                CoreJwsSigningAlgorithm::RsaSsaPssSha384,
                CoreJwsSigningAlgorithm::RsaSsaPssSha512,
                CoreJwsSigningAlgorithm::None,
            ]),
            provider_metadata.request_object_signing_alg_values_supported()
        );
        assert_eq!(
            Some(&vec![
                CoreJweKeyManagementAlgorithm::RsaPkcs1V15,
                CoreJweKeyManagementAlgorithm::RsaOaep,
                CoreJweKeyManagementAlgorithm::RsaOaepSha256,
                CoreJweKeyManagementAlgorithm::AesKeyWrap128,
                CoreJweKeyManagementAlgorithm::AesKeyWrap192,
                CoreJweKeyManagementAlgorithm::AesKeyWrap256,
                CoreJweKeyManagementAlgorithm::EcdhEs,
                CoreJweKeyManagementAlgorithm::EcdhEsAesKeyWrap128,
                CoreJweKeyManagementAlgorithm::EcdhEsAesKeyWrap192,
                CoreJweKeyManagementAlgorithm::EcdhEsAesKeyWrap256,
            ]),
            provider_metadata.request_object_encryption_alg_values_supported()
        );
        assert_eq!(
            Some(&vec![
                CoreJweContentEncryptionAlgorithm::Aes128CbcHmacSha256,
                CoreJweContentEncryptionAlgorithm::Aes192CbcHmacSha384,
                CoreJweContentEncryptionAlgorithm::Aes256CbcHmacSha512,
                CoreJweContentEncryptionAlgorithm::Aes128Gcm,
                CoreJweContentEncryptionAlgorithm::Aes192Gcm,
                CoreJweContentEncryptionAlgorithm::Aes256Gcm,
            ]),
            provider_metadata.request_object_encryption_enc_values_supported()
        );
        assert_eq!(
            Some(&vec![
                CoreClientAuthMethod::ClientSecretPost,
                CoreClientAuthMethod::ClientSecretBasic,
                CoreClientAuthMethod::ClientSecretJwt,
                CoreClientAuthMethod::PrivateKeyJwt,
            ]),
            provider_metadata.token_endpoint_auth_methods_supported()
        );
        assert_eq!(
            Some(&vec![
                CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha256,
                CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha384,
                CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha512,
                CoreJwsSigningAlgorithm::EcdsaP256Sha256,
                CoreJwsSigningAlgorithm::EcdsaP384Sha384,
                CoreJwsSigningAlgorithm::EcdsaP521Sha512,
                CoreJwsSigningAlgorithm::HmacSha256,
                CoreJwsSigningAlgorithm::HmacSha384,
                CoreJwsSigningAlgorithm::HmacSha512,
                CoreJwsSigningAlgorithm::RsaSsaPssSha256,
                CoreJwsSigningAlgorithm::RsaSsaPssSha384,
                CoreJwsSigningAlgorithm::RsaSsaPssSha512,
            ]),
            provider_metadata.token_endpoint_auth_signing_alg_values_supported()
        );
        assert_eq!(None, provider_metadata.display_values_supported());
        assert_eq!(
            Some(&vec![
                CoreClaimType::Normal,
                CoreClaimType::Aggregated,
                CoreClaimType::Distributed,
            ]),
            provider_metadata.claim_types_supported()
        );
        assert_eq!(
            Some(&vec![
                CoreClaimName::new("name".to_string()),
                CoreClaimName::new("given_name".to_string()),
                CoreClaimName::new("middle_name".to_string()),
                CoreClaimName::new("picture".to_string()),
                CoreClaimName::new("email_verified".to_string()),
                CoreClaimName::new("birthdate".to_string()),
                CoreClaimName::new("sub".to_string()),
                CoreClaimName::new("address".to_string()),
                CoreClaimName::new("zoneinfo".to_string()),
                CoreClaimName::new("email".to_string()),
                CoreClaimName::new("gender".to_string()),
                CoreClaimName::new("preferred_username".to_string()),
                CoreClaimName::new("family_name".to_string()),
                CoreClaimName::new("website".to_string()),
                CoreClaimName::new("profile".to_string()),
                CoreClaimName::new("phone_number_verified".to_string()),
                CoreClaimName::new("nickname".to_string()),
                CoreClaimName::new("updated_at".to_string()),
                CoreClaimName::new("phone_number".to_string()),
                CoreClaimName::new("locale".to_string()),
            ]),
            provider_metadata.claims_supported()
        );
        assert_eq!(None, provider_metadata.service_documentation());
        assert_eq!(None, provider_metadata.claims_locales_supported());
        assert_eq!(None, provider_metadata.ui_locales_supported());
        assert_eq!(Some(true), provider_metadata.claims_parameter_supported());
        assert_eq!(Some(true), provider_metadata.request_parameter_supported());
        assert_eq!(
            Some(true),
            provider_metadata.request_uri_parameter_supported()
        );
        assert_eq!(
            Some(true),
            provider_metadata.require_request_uri_registration()
        );
        assert_eq!(None, provider_metadata.op_policy_uri());
        assert_eq!(None, provider_metadata.op_tos_uri());

        // Note: the following fields provided by the response above are not part of the OpenID
        // Connect Discovery 1.0 spec:
        // - end_session_endpoint
        // - version

        let serialized_json = serde_json::to_string(&provider_metadata).unwrap();

        let redeserialized_metadata: CoreProviderMetadata =
            serde_json::from_str(&serialized_json).unwrap();
        assert_eq!(provider_metadata, redeserialized_metadata);
    }

    // Tests the fields missing from the example response in test_discovery_deserialization().
    #[test]
    fn test_discovery_deserialization_other_fields() {
        let json_response = "{
        \"issuer\" : \"https://rp.certification.openid.net:8080/openidconnect-rs/rp-response_type-code\",
        \"authorization_endpoint\" : \"https://rp.certification.openid.net:8080/openidconnect-rs/rp-response_type-code/authorization\",
        \"jwks_uri\" : \"https://rp.certification.openid.net:8080/static/jwks_oMXD5waO08Q1GEnv.json\",
        \"response_types_supported\" : [
           \"code\",
           \"code token\",
           \"code id_token\",
           \"id_token token\",
           \"code id_token token\",
           \"token id_token\",
           \"token id_token code\",
           \"id_token\",
           \"token\"
        ],
        \"subject_types_supported\" : [
           \"public\",
           \"pairwise\"
        ],
        \"id_token_signing_alg_values_supported\" : [
           \"HS256\",
           \"HS384\",
           \"HS512\"
        ],
        \"display_values_supported\" : [
           \"page\",
           \"popup\",
           \"touch\",
           \"wap\"
        ],
        \"service_documentation\" : \"https://rp.certification.openid.net:8080/openidconnect-rs/rp-response_type-code/documentation\",
        \"claims_locales_supported\" : [
           \"de\",
           \"fr\",
           \"de-CH-1901\"
        ],
        \"ui_locales_supported\" : [
           \"ja\",
           \"sr-Latn\",
           \"yue-HK\"
        ],
        \"op_policy_uri\" : \"https://rp.certification.openid.net:8080/openidconnect-rs/rp-response_type-code/op_policy\",
        \"op_tos_uri\" : \"https://rp.certification.openid.net:8080/openidconnect-rs/rp-response_type-code/op_tos\"
    }";

        let provider_metadata: CoreProviderMetadata = serde_json::from_str(json_response).unwrap();

        assert_eq!(
            IssuerUrl::new(
                "https://rp.certification.openid.net:8080/openidconnect-rs/rp-response_type-code"
                    .to_string()
            )
            .unwrap(),
            *provider_metadata.issuer()
        );
        assert_eq!(
            AuthUrl::new(
                "https://rp.certification.openid.net:8080/openidconnect-rs/rp-response_type-code\
                 /authorization"
                    .to_string()
            )
            .unwrap(),
            *provider_metadata.authorization_endpoint()
        );
        assert_eq!(None, provider_metadata.token_endpoint());
        assert_eq!(None, provider_metadata.userinfo_endpoint());
        assert_eq!(
            JsonWebKeySetUrl::new(
                "https://rp.certification.openid.net:8080/static/jwks_oMXD5waO08Q1GEnv.json"
                    .to_string()
            )
            .unwrap(),
            *provider_metadata.jwks_uri()
        );
        assert_eq!(None, provider_metadata.registration_endpoint());
        assert_eq!(None, provider_metadata.scopes_supported());
        assert_eq!(
            vec![
                ResponseTypes::new(vec![CoreResponseType::Code]),
                ResponseTypes::new(vec![CoreResponseType::Code, CoreResponseType::Token]),
                ResponseTypes::new(vec![CoreResponseType::Code, CoreResponseType::IdToken]),
                ResponseTypes::new(vec![CoreResponseType::IdToken, CoreResponseType::Token]),
                ResponseTypes::new(vec![
                    CoreResponseType::Code,
                    CoreResponseType::IdToken,
                    CoreResponseType::Token,
                ]),
                ResponseTypes::new(vec![CoreResponseType::Token, CoreResponseType::IdToken]),
                ResponseTypes::new(vec![
                    CoreResponseType::Token,
                    CoreResponseType::IdToken,
                    CoreResponseType::Code,
                ]),
                ResponseTypes::new(vec![CoreResponseType::IdToken]),
                ResponseTypes::new(vec![CoreResponseType::Token]),
            ],
            *provider_metadata.response_types_supported()
        );
        assert_eq!(None, provider_metadata.response_modes_supported());
        assert_eq!(None, provider_metadata.grant_types_supported());
        assert_eq!(None, provider_metadata.acr_values_supported());
        assert_eq!(
            vec![
                CoreSubjectIdentifierType::Public,
                CoreSubjectIdentifierType::Pairwise,
            ],
            *provider_metadata.subject_types_supported()
        );
        assert_eq!(
            vec![
                CoreJwsSigningAlgorithm::HmacSha256,
                CoreJwsSigningAlgorithm::HmacSha384,
                CoreJwsSigningAlgorithm::HmacSha512,
            ],
            *provider_metadata.id_token_signing_alg_values_supported()
        );
        assert_eq!(
            None,
            provider_metadata.id_token_encryption_alg_values_supported()
        );
        assert_eq!(
            None,
            provider_metadata.id_token_encryption_enc_values_supported()
        );
        assert_eq!(
            None,
            provider_metadata.userinfo_signing_alg_values_supported()
        );
        assert_eq!(
            None,
            provider_metadata.userinfo_encryption_alg_values_supported()
        );
        assert_eq!(
            None,
            provider_metadata.userinfo_encryption_enc_values_supported()
        );
        assert_eq!(
            None,
            provider_metadata.request_object_signing_alg_values_supported()
        );
        assert_eq!(
            None,
            provider_metadata.request_object_encryption_alg_values_supported()
        );
        assert_eq!(
            None,
            provider_metadata.request_object_encryption_enc_values_supported()
        );
        assert_eq!(
            None,
            provider_metadata.token_endpoint_auth_methods_supported()
        );
        assert_eq!(
            None,
            provider_metadata.token_endpoint_auth_signing_alg_values_supported()
        );
        assert_eq!(
            Some(&vec![
                CoreAuthDisplay::Page,
                CoreAuthDisplay::Popup,
                CoreAuthDisplay::Touch,
                CoreAuthDisplay::Wap,
            ]),
            provider_metadata.display_values_supported()
        );
        assert_eq!(None, provider_metadata.claim_types_supported());
        assert_eq!(None, provider_metadata.claims_supported());

        assert_eq!(
            Some(
                &ServiceDocUrl::new(
                    "https://rp.certification.openid.net:8080/openidconnect-rs/rp-response_type-code\
                 /documentation"
                        .to_string()
                )
                    .unwrap()
            ),
            provider_metadata.service_documentation()
        );
        assert_eq!(
            Some(&vec![
                LanguageTag::new("de".to_string()),
                LanguageTag::new("fr".to_string()),
                LanguageTag::new("de-CH-1901".to_string()),
            ]),
            provider_metadata.claims_locales_supported()
        );
        assert_eq!(
            Some(&vec![
                LanguageTag::new("ja".to_string()),
                LanguageTag::new("sr-Latn".to_string()),
                LanguageTag::new("yue-HK".to_string()),
            ]),
            provider_metadata.ui_locales_supported()
        );
        assert_eq!(None, provider_metadata.claims_parameter_supported());
        assert_eq!(None, provider_metadata.request_parameter_supported());
        assert_eq!(None, provider_metadata.request_uri_parameter_supported());
        assert_eq!(None, provider_metadata.require_request_uri_registration());
        assert_eq!(
            Some(
                &OpPolicyUrl::new(
                    "https://rp.certification.openid.net:8080/openidconnect-rs/rp-response_type-code\
                 /op_policy"
                        .to_string()
                )
                    .unwrap()
            ),
            provider_metadata.op_policy_uri()
        );
        assert_eq!(
            Some(
                &OpTosUrl::new(
                    "https://rp.certification.openid.net:8080/openidconnect-rs/rp-response_type-code\
                 /op_tos"
                        .to_string()
                )
                    .unwrap()
            ),
            provider_metadata.op_tos_uri()
        );

        let serialized_json = serde_json::to_string(&provider_metadata).unwrap();

        let redeserialized_metadata: CoreProviderMetadata =
            serde_json::from_str(&serialized_json).unwrap();
        assert_eq!(provider_metadata, redeserialized_metadata);
    }

    // Tests that we ignore enum values that the OIDC provider supports but that the client does
    // not (which trigger serde deserialization errors while parsing the provider metadata).
    #[test]
    fn test_unsupported_enum_values() {
        let json_response = "{\
            \"issuer\":\"https://rp.certification.openid.net:8080/openidconnect-rs/rp-response_type-code\",\
            \"authorization_endpoint\":\"https://rp.certification.openid.net:8080/openidconnect-rs/rp-response_type-code/authorization\",\
            \"jwks_uri\":\"https://rp.certification.openid.net:8080/static/jwks_3INbZl52IrrPCp2j.json\",\
            \"response_types_supported\":[\
               \"code\"\
            ],\
            \"subject_types_supported\":[\
               \"public\",\
               \"pairwise\"\
            ],\
            \"id_token_signing_alg_values_supported\":[\
               \"RS256\",\
               \"MAGIC\",\
               \"none\"\
            ],\
            \"id_token_encryption_alg_values_supported\":[\
               \"RSA1_5\",\
               \"MAGIC\"\
            ],\
            \"id_token_encryption_enc_values_supported\":[\
               \"A128CBC-HS256\",\
               \"MAGIC\"\
            ],\
            \"userinfo_signing_alg_values_supported\":[\
               \"RS256\",\
               \"MAGIC\",\
               \"none\"\
            ],\
            \"userinfo_encryption_alg_values_supported\":[\
               \"RSA1_5\",\
               \"MAGIC\"\
            ],\
            \"userinfo_encryption_enc_values_supported\":[\
               \"A128CBC-HS256\",\
               \"MAGIC\"\
            ],\
            \"request_object_signing_alg_values_supported\":[\
               \"RS256\",\
               \"MAGIC\",\
               \"none\"\
            ],\
            \"request_object_encryption_alg_values_supported\":[\
               \"RSA1_5\",\
               \"MAGIC\"\
            ],\
            \"request_object_encryption_enc_values_supported\":[\
               \"A128CBC-HS256\",\
               \"MAGIC\"\
            ],\
            \"token_endpoint_auth_signing_alg_values_supported\":[\
               \"RS256\",\
               \"MAGIC\",\
               \"none\"\
            ]\
        }";

        let provider_metadata: CoreProviderMetadata = serde_json::from_str(json_response).unwrap();

        assert_eq!(
            IssuerUrl::new(
                "https://rp.certification.openid.net:8080/openidconnect-rs/rp-response_type-code"
                    .to_string()
            )
            .unwrap(),
            *provider_metadata.issuer()
        );
        assert_eq!(
            AuthUrl::new(
                "https://rp.certification.openid.net:8080/openidconnect-rs/rp-response_type-code\
                 /authorization"
                    .to_string()
            )
            .unwrap(),
            *provider_metadata.authorization_endpoint()
        );
        assert_eq!(None, provider_metadata.token_endpoint());
        assert_eq!(None, provider_metadata.userinfo_endpoint());
        assert_eq!(
            JsonWebKeySetUrl::new(
                "https://rp.certification.openid.net:8080/static/jwks_3INbZl52IrrPCp2j.json"
                    .to_string()
            )
            .unwrap(),
            *provider_metadata.jwks_uri()
        );
        assert_eq!(None, provider_metadata.registration_endpoint());
        assert_eq!(None, provider_metadata.scopes_supported());
        assert_eq!(
            vec![ResponseTypes::new(vec![CoreResponseType::Code])],
            *provider_metadata.response_types_supported()
        );
        assert_eq!(None, provider_metadata.response_modes_supported());
        assert_eq!(None, provider_metadata.grant_types_supported());
        assert_eq!(None, provider_metadata.acr_values_supported());
        assert_eq!(
            vec![
                CoreSubjectIdentifierType::Public,
                CoreSubjectIdentifierType::Pairwise,
            ],
            *provider_metadata.subject_types_supported()
        );
        assert_eq!(
            vec![
                CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha256,
                CoreJwsSigningAlgorithm::None,
            ],
            *provider_metadata.id_token_signing_alg_values_supported()
        );
        assert_eq!(
            Some(&vec![CoreJweKeyManagementAlgorithm::RsaPkcs1V15]),
            provider_metadata.id_token_encryption_alg_values_supported()
        );
        assert_eq!(
            Some(&vec![
                CoreJweContentEncryptionAlgorithm::Aes128CbcHmacSha256
            ]),
            provider_metadata.id_token_encryption_enc_values_supported()
        );
        assert_eq!(
            Some(&vec![
                CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha256,
                CoreJwsSigningAlgorithm::None,
            ]),
            provider_metadata.userinfo_signing_alg_values_supported()
        );
        assert_eq!(
            Some(&vec![CoreJweKeyManagementAlgorithm::RsaPkcs1V15]),
            provider_metadata.userinfo_encryption_alg_values_supported()
        );
        assert_eq!(
            Some(&vec![
                CoreJweContentEncryptionAlgorithm::Aes128CbcHmacSha256
            ]),
            provider_metadata.userinfo_encryption_enc_values_supported()
        );
        assert_eq!(
            Some(&vec![
                CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha256,
                CoreJwsSigningAlgorithm::None,
            ]),
            provider_metadata.request_object_signing_alg_values_supported()
        );
        assert_eq!(
            Some(&vec![CoreJweKeyManagementAlgorithm::RsaPkcs1V15]),
            provider_metadata.request_object_encryption_alg_values_supported()
        );
        assert_eq!(
            Some(&vec![
                CoreJweContentEncryptionAlgorithm::Aes128CbcHmacSha256
            ]),
            provider_metadata.request_object_encryption_enc_values_supported()
        );
        assert_eq!(
            None,
            provider_metadata.token_endpoint_auth_methods_supported()
        );
        assert_eq!(
            Some(&vec![
                CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha256,
                CoreJwsSigningAlgorithm::None,
            ]),
            provider_metadata.token_endpoint_auth_signing_alg_values_supported()
        );
        assert_eq!(None, provider_metadata.display_values_supported());
        assert_eq!(None, provider_metadata.claim_types_supported());
        assert_eq!(None, provider_metadata.claims_supported());

        assert_eq!(None, provider_metadata.service_documentation());
        assert_eq!(None, provider_metadata.claims_locales_supported());
        assert_eq!(None, provider_metadata.ui_locales_supported());
        assert_eq!(None, provider_metadata.claims_parameter_supported());
        assert_eq!(None, provider_metadata.request_parameter_supported());
        assert_eq!(None, provider_metadata.request_uri_parameter_supported());
        assert_eq!(None, provider_metadata.require_request_uri_registration());
        assert_eq!(None, provider_metadata.op_policy_uri());
        assert_eq!(None, provider_metadata.op_tos_uri());

        let serialized_json = serde_json::to_string(&provider_metadata).unwrap();

        let redeserialized_metadata: CoreProviderMetadata =
            serde_json::from_str(&serialized_json).unwrap();
        assert_eq!(provider_metadata, redeserialized_metadata);
    }
}
