use std::fmt::{Debug, Formatter, Result as FormatterResult};
use std::future::Future;
use std::marker::PhantomData;
use std::time::Duration;

use chrono::{DateTime, Utc};
use http::header::{HeaderMap, HeaderValue, ACCEPT, CONTENT_TYPE};
use http::method::Method;
use http::status::StatusCode;
use serde::de::{Deserialize, DeserializeOwned, Deserializer, MapAccess, Visitor};
use serde::ser::SerializeMap;
use serde::{Serialize, Serializer};
use thiserror::Error;

use super::http_utils::{auth_bearer, check_content_type, MIME_TYPE_JSON};
use super::types::helpers::{serde_utc_seconds_opt, split_language_tag_key};
use super::types::{
    ApplicationType, AuthenticationContextClass, ClientAuthMethod, ClientConfigUrl,
    ClientContactEmail, ClientName, ClientUrl, GrantType, InitiateLoginUrl, JsonWebKeySetUrl,
    JsonWebKeyType, JsonWebKeyUse, JweContentEncryptionAlgorithm, JweKeyManagementAlgorithm,
    JwsSigningAlgorithm, LocalizedClaim, LogoUrl, PolicyUrl, RegistrationAccessToken,
    RegistrationUrl, RequestUrl, ResponseType, ResponseTypes, SectorIdentifierUrl,
    SubjectIdentifierType, ToSUrl,
};
use super::{
    AccessToken, ClientId, ClientSecret, ErrorResponseType, HttpRequest, HttpResponse, JsonWebKey,
    JsonWebKeySet, RedirectUrl, StandardErrorResponse,
};

///
/// Trait for adding extra fields to [`ClientMetadata`].
///
pub trait AdditionalClientMetadata: Debug + DeserializeOwned + Serialize {}

// In order to support serde flatten, this must be an empty struct rather than an empty
// tuple struct.
///
/// Empty (default) extra [`ClientMetadata`] fields.
///
#[derive(Clone, Debug, Default, Deserialize, PartialEq, Serialize)]
pub struct EmptyAdditionalClientMetadata {}
impl AdditionalClientMetadata for EmptyAdditionalClientMetadata {}

///
/// Client metadata used in dynamic client registration.
///
#[derive(Clone, Debug, Deserialize, PartialEq, Serialize)]
pub struct ClientMetadata<A, AT, CA, G, JE, JK, JS, JT, JU, K, RT, S>
where
    A: AdditionalClientMetadata,
    AT: ApplicationType,
    CA: ClientAuthMethod,
    G: GrantType,
    JE: JweContentEncryptionAlgorithm<JT>,
    JK: JweKeyManagementAlgorithm,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    JU: JsonWebKeyUse,
    K: JsonWebKey<JS, JT, JU>,
    RT: ResponseType,
    S: SubjectIdentifierType,
{
    // To avoid implementing a custom deserializer that handles both language tags and flatten,
    // we wrap the language tag handling in its own flattened struct.
    #[serde(bound = "AT: ApplicationType", flatten)]
    standard_metadata: StandardClientMetadata<AT, CA, G, JE, JK, JS, JT, JU, K, RT, S>,

    #[serde(bound = "A: AdditionalClientMetadata", flatten)]
    additional_metadata: A,
}
impl<A, AT, CA, G, JE, JK, JS, JT, JU, K, RT, S>
    ClientMetadata<A, AT, CA, G, JE, JK, JS, JT, JU, K, RT, S>
where
    A: AdditionalClientMetadata,
    AT: ApplicationType,
    CA: ClientAuthMethod,
    G: GrantType,
    JE: JweContentEncryptionAlgorithm<JT>,
    JK: JweKeyManagementAlgorithm,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    JU: JsonWebKeyUse,
    K: JsonWebKey<JS, JT, JU>,
    RT: ResponseType,
    S: SubjectIdentifierType,
{
    ///
    /// Instantiates new client metadata.
    ///
    pub fn new(redirect_uris: Vec<RedirectUrl>, additional_metadata: A) -> Self {
        Self {
            standard_metadata: StandardClientMetadata {
                redirect_uris,
                response_types: None,
                grant_types: None,
                application_type: None,
                contacts: None,
                client_name: None,
                logo_uri: None,
                client_uri: None,
                policy_uri: None,
                tos_uri: None,
                jwks_uri: None,
                jwks: None,
                sector_identifier_uri: None,
                subject_type: None,
                id_token_signed_response_alg: None,
                id_token_encrypted_response_alg: None,
                id_token_encrypted_response_enc: None,
                userinfo_signed_response_alg: None,
                userinfo_encrypted_response_alg: None,
                userinfo_encrypted_response_enc: None,
                request_object_signing_alg: None,
                request_object_encryption_alg: None,
                request_object_encryption_enc: None,
                token_endpoint_auth_method: None,
                token_endpoint_auth_signing_alg: None,
                default_max_age: None,
                require_auth_time: None,
                default_acr_values: None,
                initiate_login_uri: None,
                request_uris: None,
            },
            additional_metadata,
        }
    }
    field_getters_setters![
        pub self [self.standard_metadata] ["client metadata value"] {
            set_redirect_uris -> redirect_uris[Vec<RedirectUrl>],
            set_response_types -> response_types[Option<Vec<ResponseTypes<RT>>>],
            set_grant_types -> grant_types[Option<Vec<G>>],
            set_application_type -> application_type[Option<AT>],
            set_contacts -> contacts[Option<Vec<ClientContactEmail>>],
            set_client_name -> client_name[Option<LocalizedClaim<ClientName>>],
            set_logo_uri -> logo_uri[Option<LocalizedClaim<LogoUrl>>],
            set_client_uri -> client_uri[Option<LocalizedClaim<ClientUrl>>],
            set_policy_uri -> policy_uri[Option<LocalizedClaim<PolicyUrl>>],
            set_tos_uri -> tos_uri[Option<LocalizedClaim<ToSUrl>>],
            set_jwks_uri -> jwks_uri[Option<JsonWebKeySetUrl>],
            set_jwks -> jwks[Option<JsonWebKeySet<JS, JT, JU, K>>],
            set_sector_identifier_uri -> sector_identifier_uri[Option<SectorIdentifierUrl>],
            set_subject_type -> subject_type[Option<S>],
            set_id_token_signed_response_alg -> id_token_signed_response_alg[Option<JS>],
            set_id_token_encrypted_response_alg -> id_token_encrypted_response_alg[Option<JK>],
            set_id_token_encrypted_response_enc -> id_token_encrypted_response_enc[Option<JE>],
            set_userinfo_signed_response_alg -> userinfo_signed_response_alg[Option<JS>],
            set_userinfo_encrypted_response_alg -> userinfo_encrypted_response_alg[Option<JK>],
            set_userinfo_encrypted_response_enc -> userinfo_encrypted_response_enc[Option<JE>],
            set_request_object_signing_alg -> request_object_signing_alg[Option<JS>],
            set_request_object_encryption_alg -> request_object_encryption_alg[Option<JK>],
            set_request_object_encryption_enc -> request_object_encryption_enc[Option<JE>],
            set_token_endpoint_auth_method -> token_endpoint_auth_method[Option<CA>],
            set_token_endpoint_auth_signing_alg -> token_endpoint_auth_signing_alg[Option<JS>],
            set_default_max_age -> default_max_age[Option<Duration>],
            set_require_auth_time -> require_auth_time[Option<bool>],
            set_default_acr_values -> default_acr_values[Option<Vec<AuthenticationContextClass>>],
            set_initiate_login_uri -> initiate_login_uri[Option<InitiateLoginUrl>],
            set_request_uris -> request_uris[Option<Vec<RequestUrl>>],
        }
    ];

    ///
    /// Returns additional client metadata fields.
    ///
    pub fn additional_metadata(&self) -> &A {
        &self.additional_metadata
    }
    ///
    /// Returns mutable additional client metadata fields.
    ///
    pub fn additional_metadata_mut(&mut self) -> &mut A {
        &mut self.additional_metadata
    }
}

#[derive(Clone, Debug, PartialEq)]
struct StandardClientMetadata<AT, CA, G, JE, JK, JS, JT, JU, K, RT, S>
where
    AT: ApplicationType,
    CA: ClientAuthMethod,
    G: GrantType,
    JE: JweContentEncryptionAlgorithm<JT>,
    JK: JweKeyManagementAlgorithm,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    JU: JsonWebKeyUse,
    K: JsonWebKey<JS, JT, JU>,
    RT: ResponseType,
    S: SubjectIdentifierType,
{
    redirect_uris: Vec<RedirectUrl>,
    response_types: Option<Vec<ResponseTypes<RT>>>,
    grant_types: Option<Vec<G>>,
    application_type: Option<AT>,
    contacts: Option<Vec<ClientContactEmail>>,
    client_name: Option<LocalizedClaim<ClientName>>,
    logo_uri: Option<LocalizedClaim<LogoUrl>>,
    client_uri: Option<LocalizedClaim<ClientUrl>>,
    policy_uri: Option<LocalizedClaim<PolicyUrl>>,
    tos_uri: Option<LocalizedClaim<ToSUrl>>,
    jwks_uri: Option<JsonWebKeySetUrl>,
    jwks: Option<JsonWebKeySet<JS, JT, JU, K>>,
    sector_identifier_uri: Option<SectorIdentifierUrl>,
    subject_type: Option<S>,
    id_token_signed_response_alg: Option<JS>,
    id_token_encrypted_response_alg: Option<JK>,
    id_token_encrypted_response_enc: Option<JE>,
    userinfo_signed_response_alg: Option<JS>,
    userinfo_encrypted_response_alg: Option<JK>,
    userinfo_encrypted_response_enc: Option<JE>,
    request_object_signing_alg: Option<JS>,
    request_object_encryption_alg: Option<JK>,
    request_object_encryption_enc: Option<JE>,
    token_endpoint_auth_method: Option<CA>,
    token_endpoint_auth_signing_alg: Option<JS>,
    default_max_age: Option<Duration>,
    require_auth_time: Option<bool>,
    default_acr_values: Option<Vec<AuthenticationContextClass>>,
    initiate_login_uri: Option<InitiateLoginUrl>,
    request_uris: Option<Vec<RequestUrl>>,
}
impl<'de, AT, CA, G, JE, JK, JS, JT, JU, K, RT, S> Deserialize<'de>
    for StandardClientMetadata<AT, CA, G, JE, JK, JS, JT, JU, K, RT, S>
where
    AT: ApplicationType,
    CA: ClientAuthMethod,
    G: GrantType,
    JE: JweContentEncryptionAlgorithm<JT>,
    JK: JweKeyManagementAlgorithm,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    JU: JsonWebKeyUse,
    K: JsonWebKey<JS, JT, JU>,
    RT: ResponseType,
    S: SubjectIdentifierType,
{
    ///
    /// Special deserializer that supports [RFC 5646](https://tools.ietf.org/html/rfc5646) language
    /// tags associated with human-readable client metadata fields.
    ///
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct MetadataVisitor<
            AT: ApplicationType,
            CA: ClientAuthMethod,
            G: GrantType,
            JE: JweContentEncryptionAlgorithm<JT>,
            JK: JweKeyManagementAlgorithm,
            JS: JwsSigningAlgorithm<JT>,
            JT: JsonWebKeyType,
            JU: JsonWebKeyUse,
            K: JsonWebKey<JS, JT, JU>,
            RT: ResponseType,
            S: SubjectIdentifierType,
        >(
            PhantomData<AT>,
            PhantomData<CA>,
            PhantomData<G>,
            PhantomData<JE>,
            PhantomData<JK>,
            PhantomData<JS>,
            PhantomData<JT>,
            PhantomData<JU>,
            PhantomData<K>,
            PhantomData<RT>,
            PhantomData<S>,
        );
        impl<'de, AT, CA, G, JE, JK, JS, JT, JU, K, RT, S> Visitor<'de>
            for MetadataVisitor<AT, CA, G, JE, JK, JS, JT, JU, K, RT, S>
        where
            AT: ApplicationType,
            CA: ClientAuthMethod,
            G: GrantType,
            JE: JweContentEncryptionAlgorithm<JT>,
            JK: JweKeyManagementAlgorithm,
            JS: JwsSigningAlgorithm<JT>,
            JT: JsonWebKeyType,
            JU: JsonWebKeyUse,
            K: JsonWebKey<JS, JT, JU>,
            RT: ResponseType,
            S: SubjectIdentifierType,
        {
            type Value = StandardClientMetadata<AT, CA, G, JE, JK, JS, JT, JU, K, RT, S>;

            fn expecting(&self, formatter: &mut Formatter) -> FormatterResult {
                formatter.write_str("struct StandardClientMetadata")
            }
            fn visit_map<V>(self, mut map: V) -> Result<Self::Value, V::Error>
            where
                V: MapAccess<'de>,
            {
                deserialize_fields! {
                    map {
                        [redirect_uris]
                        [Option(response_types)]
                        [Option(grant_types)]
                        [Option(application_type)]
                        [Option(contacts)]
                        [LanguageTag(client_name)]
                        [LanguageTag(logo_uri)]
                        [LanguageTag(client_uri)]
                        [LanguageTag(policy_uri)]
                        [LanguageTag(tos_uri)]
                        [Option(jwks_uri)]
                        [Option(jwks)]
                        [Option(sector_identifier_uri)]
                        [Option(subject_type)]
                        [Option(id_token_signed_response_alg)]
                        [Option(id_token_encrypted_response_alg)]
                        [Option(id_token_encrypted_response_enc)]
                        [Option(userinfo_signed_response_alg)]
                        [Option(userinfo_encrypted_response_alg)]
                        [Option(userinfo_encrypted_response_enc)]
                        [Option(request_object_signing_alg)]
                        [Option(request_object_encryption_alg)]
                        [Option(request_object_encryption_enc)]
                        [Option(token_endpoint_auth_method)]
                        [Option(token_endpoint_auth_signing_alg)]
                        [Option(Seconds(default_max_age))]
                        [Option(require_auth_time)]
                        [Option(default_acr_values)]
                        [Option(initiate_login_uri)]
                        [Option(request_uris)]
                    }
                }
            }
        }
        deserializer.deserialize_map(MetadataVisitor(
            PhantomData,
            PhantomData,
            PhantomData,
            PhantomData,
            PhantomData,
            PhantomData,
            PhantomData,
            PhantomData,
            PhantomData,
            PhantomData,
            PhantomData,
        ))
    }
}
impl<AT, CA, G, JE, JK, JS, JT, JU, K, RT, S> Serialize
    for StandardClientMetadata<AT, CA, G, JE, JK, JS, JT, JU, K, RT, S>
where
    AT: ApplicationType,
    CA: ClientAuthMethod,
    G: GrantType,
    JE: JweContentEncryptionAlgorithm<JT>,
    JK: JweKeyManagementAlgorithm,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    JU: JsonWebKeyUse,
    K: JsonWebKey<JS, JT, JU>,
    RT: ResponseType,
    S: SubjectIdentifierType,
{
    #[allow(clippy::cognitive_complexity)]
    fn serialize<SE>(&self, serializer: SE) -> Result<SE::Ok, SE::Error>
    where
        SE: Serializer,
    {
        serialize_fields! {
            self -> serializer {
                [redirect_uris]
                [Option(response_types)]
                [Option(grant_types)]
                [Option(application_type)]
                [Option(contacts)]
                [LanguageTag(client_name)]
                [LanguageTag(logo_uri)]
                [LanguageTag(client_uri)]
                [LanguageTag(policy_uri)]
                [LanguageTag(tos_uri)]
                [Option(jwks_uri)]
                [Option(jwks)]
                [Option(sector_identifier_uri)]
                [Option(subject_type)]
                [Option(id_token_signed_response_alg)]
                [Option(id_token_encrypted_response_alg)]
                [Option(id_token_encrypted_response_enc)]
                [Option(userinfo_signed_response_alg)]
                [Option(userinfo_encrypted_response_alg)]
                [Option(userinfo_encrypted_response_enc)]
                [Option(request_object_signing_alg)]
                [Option(request_object_encryption_alg)]
                [Option(request_object_encryption_enc)]
                [Option(token_endpoint_auth_method)]
                [Option(token_endpoint_auth_signing_alg)]
                [Option(Seconds(default_max_age))]
                [Option(require_auth_time)]
                [Option(default_acr_values)]
                [Option(initiate_login_uri)]
                [Option(request_uris)]
            }
        }
    }
}

///
/// Dynamic client registration request.
///
#[derive(Clone, Debug)]
pub struct ClientRegistrationRequest<AC, AR, AT, CA, ET, G, JE, JK, JS, JT, JU, K, RT, S>
where
    AC: AdditionalClientMetadata,
    AR: AdditionalClientRegistrationResponse,
    AT: ApplicationType,
    CA: ClientAuthMethod,
    ET: RegisterErrorResponseType,
    G: GrantType,
    JE: JweContentEncryptionAlgorithm<JT>,
    JK: JweKeyManagementAlgorithm,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    JU: JsonWebKeyUse,
    K: JsonWebKey<JS, JT, JU>,
    RT: ResponseType,
    S: SubjectIdentifierType,
{
    client_metadata: ClientMetadata<AC, AT, CA, G, JE, JK, JS, JT, JU, K, RT, S>,
    initial_access_token: Option<AccessToken>,
    _phantom: PhantomData<(AR, ET)>,
}
impl<AC, AR, AT, CA, ET, G, JE, JK, JS, JT, JU, K, RT, S>
    ClientRegistrationRequest<AC, AR, AT, CA, ET, G, JE, JK, JS, JT, JU, K, RT, S>
where
    AC: AdditionalClientMetadata,
    AR: AdditionalClientRegistrationResponse,
    AT: ApplicationType,
    CA: ClientAuthMethod,
    ET: RegisterErrorResponseType + Send + Sync,
    G: GrantType,
    JE: JweContentEncryptionAlgorithm<JT>,
    JK: JweKeyManagementAlgorithm,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    JU: JsonWebKeyUse,
    K: JsonWebKey<JS, JT, JU>,
    RT: ResponseType,
    S: SubjectIdentifierType + Send + Sync,
{
    ///
    /// Instantiates a new dynamic client registration request.
    ///
    pub fn new(redirect_uris: Vec<RedirectUrl>, additional_metadata: AC) -> Self {
        Self {
            client_metadata: ClientMetadata::new(redirect_uris, additional_metadata),
            initial_access_token: None,
            _phantom: PhantomData,
        }
    }

    ///
    /// Submits this request to the specified registration endpoint using the specified synchronous
    /// HTTP client.
    ///
    pub fn register<HC, RE>(
        &self,
        registration_endpoint: &RegistrationUrl,
        http_client: HC,
    ) -> Result<
        ClientRegistrationResponse<AC, AR, AT, CA, G, JE, JK, JS, JT, JU, K, RT, S>,
        ClientRegistrationError<ET, RE>,
    >
    where
        HC: FnOnce(HttpRequest) -> Result<HttpResponse, RE>,
        RE: std::error::Error + 'static,
    {
        self.prepare_registration(registration_endpoint)
            .and_then(|http_request| {
                http_client(http_request).map_err(ClientRegistrationError::Request)
            })
            .and_then(Self::register_response)
    }

    ///
    /// Submits this request to the specified registration endpoint using the specified asynchronous
    /// HTTP client.
    ///
    pub async fn register_async<F, HC, RE>(
        &self,
        registration_endpoint: &RegistrationUrl,
        http_client: HC,
    ) -> Result<
        ClientRegistrationResponse<AC, AR, AT, CA, G, JE, JK, JS, JT, JU, K, RT, S>,
        ClientRegistrationError<ET, RE>,
    >
    where
        F: Future<Output = Result<HttpResponse, RE>>,
        HC: FnOnce(HttpRequest) -> F,
        RE: std::error::Error + 'static,
    {
        let http_request = self.prepare_registration(registration_endpoint)?;
        let http_response = http_client(http_request)
            .await
            .map_err(ClientRegistrationError::Request)?;
        Self::register_response(http_response)
    }

    fn prepare_registration<RE>(
        &self,
        registration_endpoint: &RegistrationUrl,
    ) -> Result<HttpRequest, ClientRegistrationError<ET, RE>>
    where
        RE: std::error::Error + 'static,
    {
        let request_json = serde_json::to_string(self.client_metadata())
            .map_err(ClientRegistrationError::Serialize)?
            .into_bytes();

        let auth_header_opt = self.initial_access_token().map(auth_bearer);

        let mut headers = HeaderMap::new();
        headers.append(ACCEPT, HeaderValue::from_static(MIME_TYPE_JSON));
        headers.append(CONTENT_TYPE, HeaderValue::from_static(MIME_TYPE_JSON));
        if let Some((header, value)) = auth_header_opt {
            headers.append(header, value);
        }

        Ok(HttpRequest {
            url: registration_endpoint.url().clone(),
            method: Method::POST,
            headers,
            body: request_json,
        })
    }

    fn register_response<RE>(
        http_response: HttpResponse,
    ) -> Result<
        ClientRegistrationResponse<AC, AR, AT, CA, G, JE, JK, JS, JT, JU, K, RT, S>,
        ClientRegistrationError<ET, RE>,
    >
    where
        RE: std::error::Error + 'static,
    {
        // TODO: check for WWW-Authenticate response header if bearer auth was used (see
        //   https://tools.ietf.org/html/rfc6750#section-3)
        // TODO: other necessary response validation? check spec

        // Spec says that a successful response SHOULD use 201 Created, and a registration error
        // condition returns (no "SHOULD") 400 Bad Request. For now, only accept these two status
        // codes. We may need to relax the success status to improve interoperability.
        if http_response.status_code != StatusCode::CREATED
            && http_response.status_code != StatusCode::BAD_REQUEST
        {
            return Err(ClientRegistrationError::Response(
                http_response.status_code,
                http_response.body,
                "unexpected HTTP status code".to_string(),
            ));
        }

        check_content_type(&http_response.headers, MIME_TYPE_JSON).map_err(|err_msg| {
            ClientRegistrationError::Response(
                http_response.status_code,
                http_response.body.clone(),
                err_msg,
            )
        })?;

        let response_body = String::from_utf8(http_response.body).map_err(|parse_error| {
            ClientRegistrationError::Other(format!(
                "couldn't parse response as UTF-8: {}",
                parse_error
            ))
        })?;

        if http_response.status_code == StatusCode::BAD_REQUEST {
            let response_error: StandardErrorResponse<ET> = serde_path_to_error::deserialize(
                &mut serde_json::Deserializer::from_str(&response_body),
            )
            .map_err(ClientRegistrationError::Parse)?;
            return Err(ClientRegistrationError::ServerResponse(response_error));
        }

        serde_path_to_error::deserialize(&mut serde_json::Deserializer::from_str(&response_body))
            .map_err(ClientRegistrationError::Parse)
    }

    ///
    /// Returns the client metadata associated with this registration request.
    ///
    pub fn client_metadata(&self) -> &ClientMetadata<AC, AT, CA, G, JE, JK, JS, JT, JU, K, RT, S> {
        &self.client_metadata
    }

    ///
    /// Returns the initial access token associated with this registration request.
    ///
    pub fn initial_access_token(&self) -> Option<&AccessToken> {
        self.initial_access_token.as_ref()
    }
    ///
    /// Sets the initial access token for this request.
    ///
    pub fn set_initial_access_token(mut self, access_token: Option<AccessToken>) -> Self {
        self.initial_access_token = access_token;
        self
    }

    field_getters_setters![
        pub self [self.client_metadata.standard_metadata] ["client metadata value"] {
            set_redirect_uris -> redirect_uris[Vec<RedirectUrl>],
            set_response_types -> response_types[Option<Vec<ResponseTypes<RT>>>],
            set_grant_types -> grant_types[Option<Vec<G>>],
            set_application_type -> application_type[Option<AT>],
            set_contacts -> contacts[Option<Vec<ClientContactEmail>>],
            set_client_name -> client_name[Option<LocalizedClaim<ClientName>>],
            set_logo_uri -> logo_uri[Option<LocalizedClaim<LogoUrl>>],
            set_client_uri -> client_uri[Option<LocalizedClaim<ClientUrl>>],
            set_policy_uri -> policy_uri[Option<LocalizedClaim<PolicyUrl>>],
            set_tos_uri -> tos_uri[Option<LocalizedClaim<ToSUrl>>],
            set_jwks_uri -> jwks_uri[Option<JsonWebKeySetUrl>],
            set_jwks -> jwks[Option<JsonWebKeySet<JS, JT, JU, K>>],
            set_sector_identifier_uri -> sector_identifier_uri[Option<SectorIdentifierUrl>],
            set_subject_type -> subject_type[Option<S>],
            set_id_token_signed_response_alg -> id_token_signed_response_alg[Option<JS>],
            set_id_token_encrypted_response_alg -> id_token_encrypted_response_alg[Option<JK>],
            set_id_token_encrypted_response_enc -> id_token_encrypted_response_enc[Option<JE>],
            set_userinfo_signed_response_alg -> userinfo_signed_response_alg[Option<JS>],
            set_userinfo_encrypted_response_alg -> userinfo_encrypted_response_alg[Option<JK>],
            set_userinfo_encrypted_response_enc -> userinfo_encrypted_response_enc[Option<JE>],
            set_request_object_signing_alg -> request_object_signing_alg[Option<JS>],
            set_request_object_encryption_alg -> request_object_encryption_alg[Option<JK>],
            set_request_object_encryption_enc -> request_object_encryption_enc[Option<JE>],
            set_token_endpoint_auth_method -> token_endpoint_auth_method[Option<CA>],
            set_token_endpoint_auth_signing_alg -> token_endpoint_auth_signing_alg[Option<JS>],
            set_default_max_age -> default_max_age[Option<Duration>],
            set_require_auth_time -> require_auth_time[Option<bool>],
            set_default_acr_values -> default_acr_values[Option<Vec<AuthenticationContextClass>>],
            set_initiate_login_uri -> initiate_login_uri[Option<InitiateLoginUrl>],
            set_request_uris -> request_uris[Option<Vec<RequestUrl>>],
        }
    ];

    ///
    /// Returns additional client metadata fields.
    ///
    pub fn additional_metadata(&self) -> &AC {
        &self.client_metadata.additional_metadata
    }
    ///
    /// Returns mutable additional client metadata fields.
    ///
    pub fn additional_metadata_mut(&mut self) -> &mut AC {
        &mut self.client_metadata.additional_metadata
    }
}

///
/// Trait for adding extra fields to the [`ClientRegistrationResponse`].
///
pub trait AdditionalClientRegistrationResponse: Debug + DeserializeOwned + Serialize {}

// In order to support serde flatten, this must be an empty struct rather than an empty
// tuple struct.
///
/// Empty (default) extra [`ClientRegistrationResponse`] fields.
///
#[derive(Clone, Debug, Default, Deserialize, PartialEq, Serialize)]
pub struct EmptyAdditionalClientRegistrationResponse {}
impl AdditionalClientRegistrationResponse for EmptyAdditionalClientRegistrationResponse {}

///
/// Response to a dynamic client registration request.
///
#[derive(Debug, Deserialize, Serialize)]
pub struct ClientRegistrationResponse<AC, AR, AT, CA, G, JE, JK, JS, JT, JU, K, RT, S>
where
    AC: AdditionalClientMetadata,
    AR: AdditionalClientRegistrationResponse,
    AT: ApplicationType,
    CA: ClientAuthMethod,
    G: GrantType,
    JE: JweContentEncryptionAlgorithm<JT>,
    JK: JweKeyManagementAlgorithm,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    JU: JsonWebKeyUse,
    K: JsonWebKey<JS, JT, JU>,
    RT: ResponseType,
    S: SubjectIdentifierType,
{
    client_id: ClientId,
    #[serde(skip_serializing_if = "Option::is_none")]
    client_secret: Option<ClientSecret>,
    #[serde(skip_serializing_if = "Option::is_none")]
    registration_access_token: Option<RegistrationAccessToken>,
    #[serde(skip_serializing_if = "Option::is_none")]
    registration_client_uri: Option<ClientConfigUrl>,
    #[serde(
        skip_serializing_if = "Option::is_none",
        with = "serde_utc_seconds_opt",
        default
    )]
    client_id_issued_at: Option<DateTime<Utc>>,
    #[serde(
        skip_serializing_if = "Option::is_none",
        with = "serde_utc_seconds_opt",
        default
    )]
    client_secret_expires_at: Option<DateTime<Utc>>,
    #[serde(bound = "AC: AdditionalClientMetadata", flatten)]
    client_metadata: ClientMetadata<AC, AT, CA, G, JE, JK, JS, JT, JU, K, RT, S>,

    #[serde(bound = "AR: AdditionalClientRegistrationResponse", flatten)]
    additional_response: AR,
}
impl<AC, AR, AT, CA, G, JE, JK, JS, JT, JU, K, RT, S>
    ClientRegistrationResponse<AC, AR, AT, CA, G, JE, JK, JS, JT, JU, K, RT, S>
where
    AC: AdditionalClientMetadata,
    AR: AdditionalClientRegistrationResponse,
    AT: ApplicationType,
    CA: ClientAuthMethod,
    G: GrantType,
    JE: JweContentEncryptionAlgorithm<JT>,
    JK: JweKeyManagementAlgorithm,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    JU: JsonWebKeyUse,
    K: JsonWebKey<JS, JT, JU>,
    RT: ResponseType,
    S: SubjectIdentifierType,
{
    ///
    /// Instantiates a new dynamic client registration response.
    ///
    pub fn new(
        client_id: ClientId,
        redirect_uris: Vec<RedirectUrl>,
        additional_metadata: AC,
        additional_response: AR,
    ) -> Self {
        Self {
            client_id,
            client_secret: None,
            registration_access_token: None,
            registration_client_uri: None,
            client_id_issued_at: None,
            client_secret_expires_at: None,
            client_metadata: ClientMetadata::new(redirect_uris, additional_metadata),
            additional_response,
        }
    }

    ///
    /// Instantiates a new dynamic client registration response using the specified client metadata.
    ///
    pub fn from_client_metadata(
        client_id: ClientId,
        client_metadata: ClientMetadata<AC, AT, CA, G, JE, JK, JS, JT, JU, K, RT, S>,
        additional_response: AR,
    ) -> Self {
        Self {
            client_id,
            client_secret: None,
            registration_access_token: None,
            registration_client_uri: None,
            client_id_issued_at: None,
            client_secret_expires_at: None,
            client_metadata,
            additional_response,
        }
    }

    field_getters_setters![
        pub self [self] ["response field"] {
            set_client_id -> client_id[ClientId],
            set_client_secret -> client_secret[Option<ClientSecret>],
            set_registration_access_token
              -> registration_access_token[Option<RegistrationAccessToken>],
            set_registration_client_uri -> registration_client_uri[Option<ClientConfigUrl>],
            set_client_id_issued_at -> client_id_issued_at[Option<DateTime<Utc>>],
            set_client_secret_expires_at -> client_secret_expires_at[Option<DateTime<Utc>>],
        }
    ];

    field_getters_setters![
        pub self [self.client_metadata.standard_metadata] ["client metadata value"] {
            set_redirect_uris -> redirect_uris[Vec<RedirectUrl>],
            set_response_types -> response_types[Option<Vec<ResponseTypes<RT>>>],
            set_grant_types -> grant_types[Option<Vec<G>>],
            set_application_type -> application_type[Option<AT>],
            set_contacts -> contacts[Option<Vec<ClientContactEmail>>],
            set_client_name -> client_name[Option<LocalizedClaim<ClientName>>],
            set_logo_uri -> logo_uri[Option<LocalizedClaim<LogoUrl>>],
            set_client_uri -> client_uri[Option<LocalizedClaim<ClientUrl>>],
            set_policy_uri -> policy_uri[Option<LocalizedClaim<PolicyUrl>>],
            set_tos_uri -> tos_uri[Option<LocalizedClaim<ToSUrl>>],
            set_jwks_uri -> jwks_uri[Option<JsonWebKeySetUrl>],
            set_jwks -> jwks[Option<JsonWebKeySet<JS, JT, JU, K>>],
            set_sector_identifier_uri -> sector_identifier_uri[Option<SectorIdentifierUrl>],
            set_subject_type -> subject_type[Option<S>],
            set_id_token_signed_response_alg -> id_token_signed_response_alg[Option<JS>],
            set_id_token_encrypted_response_alg -> id_token_encrypted_response_alg[Option<JK>],
            set_id_token_encrypted_response_enc -> id_token_encrypted_response_enc[Option<JE>],
            set_userinfo_signed_response_alg -> userinfo_signed_response_alg[Option<JS>],
            set_userinfo_encrypted_response_alg -> userinfo_encrypted_response_alg[Option<JK>],
            set_userinfo_encrypted_response_enc -> userinfo_encrypted_response_enc[Option<JE>],
            set_request_object_signing_alg -> request_object_signing_alg[Option<JS>],
            set_request_object_encryption_alg -> request_object_encryption_alg[Option<JK>],
            set_request_object_encryption_enc -> request_object_encryption_enc[Option<JE>],
            set_token_endpoint_auth_method -> token_endpoint_auth_method[Option<CA>],
            set_token_endpoint_auth_signing_alg -> token_endpoint_auth_signing_alg[Option<JS>],
            set_default_max_age -> default_max_age[Option<Duration>],
            set_require_auth_time -> require_auth_time[Option<bool>],
            set_default_acr_values -> default_acr_values[Option<Vec<AuthenticationContextClass>>],
            set_initiate_login_uri -> initiate_login_uri[Option<InitiateLoginUrl>],
            set_request_uris -> request_uris[Option<Vec<RequestUrl>>],
        }
    ];

    ///
    /// Returns additional client metadata fields.
    ///
    pub fn additional_metadata(&self) -> &AC {
        &self.client_metadata.additional_metadata
    }
    ///
    /// Returns mutable additional client metadata fields.
    ///
    pub fn additional_metadata_mut(&mut self) -> &mut AC {
        &mut self.client_metadata.additional_metadata
    }

    ///
    /// Returns additional response fields.
    ///
    pub fn additional_response(&self) -> &AR {
        &self.additional_response
    }
    ///
    /// Returns mutable additional response fields.
    ///
    pub fn additional_response_mut(&mut self) -> &mut AR {
        &mut self.additional_response
    }
}

// TODO: implement client configuration endpoint request (Section 4)

///
/// Trait representing an error returned by the dynamic client registration endpoint.
///
pub trait RegisterErrorResponseType: ErrorResponseType + 'static {}

///
/// Error registering a client.
///
#[derive(Debug, Error)]
#[non_exhaustive]
pub enum ClientRegistrationError<T, RE>
where
    RE: std::error::Error + 'static,
    T: RegisterErrorResponseType,
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
    /// Failed to serialize client metadata.
    ///
    #[error("Failed to serialize client metadata")]
    Serialize(#[source] serde_json::Error),
    ///
    /// Server returned an error.
    ///
    #[error("Server returned error")]
    ServerResponse(StandardErrorResponse<T>),
}

#[cfg(test)]
mod tests {
    use std::time::Duration;

    use chrono::{TimeZone, Utc};
    use itertools::sorted;
    use oauth2::{ClientId, RedirectUrl};

    use crate::core::{
        CoreApplicationType, CoreClientAuthMethod, CoreClientMetadata,
        CoreClientRegistrationResponse, CoreGrantType, CoreJweContentEncryptionAlgorithm,
        CoreJweKeyManagementAlgorithm, CoreJwsSigningAlgorithm, CoreResponseType,
        CoreSubjectIdentifierType,
    };
    use crate::jwt::tests::TEST_RSA_PUB_KEY;
    use crate::{
        AuthenticationContextClass, ClientConfigUrl, ClientContactEmail, ClientName, ClientUrl,
        JsonWebKeySet, JsonWebKeySetUrl, LanguageTag, LogoUrl, PolicyUrl, RequestUrl,
        ResponseTypes, SectorIdentifierUrl, ToSUrl,
    };

    #[test]
    fn test_metadata_serialization() {
        // `jwks_uri` and `jwks` aren't supposed to be used together, but this test is just for
        // serialization/deserialization.
        let json_response = format!("{{
            \"redirect_uris\": [\"https://example.com/redirect-1\", \"https://example.com/redirect-2\"],
            \"response_types\": [\"code\", \"code token id_token\"],
            \"grant_types\": [\"authorization_code\", \"client_credentials\", \"implicit\", \
                \"password\", \"refresh_token\"],
            \"application_type\": \"web\",
            \"contacts\": [\"user@example.com\", \"admin@openidconnect.local\"],
            \"client_name\": \"Example\",
            \"client_name#es\": \"Ejemplo\",
            \"logo_uri\": \"https://example.com/logo.png\",
            \"logo_uri#fr\": \"https://example.com/logo-fr.png\",
            \"client_uri\": \"https://example.com/client-app\",
            \"client_uri#de\": \"https://example.com/client-app-de\",
            \"policy_uri\": \"https://example.com/policy\",
            \"policy_uri#sr-Latn\": \"https://example.com/policy-sr-latin\",
            \"tos_uri\": \"https://example.com/tos\",
            \"tos_uri#sr-Cyrl\": \"https://example.com/tos-sr-cyrl\",
            \"jwks_uri\": \"https://example.com/jwks\",
            \"jwks\": {{\"keys\": [{}]}},
            \"sector_identifier_uri\": \"https://example.com/sector\",
            \"subject_type\": \"pairwise\",
            \"id_token_signed_response_alg\": \"HS256\",
            \"id_token_encrypted_response_alg\": \"RSA1_5\",
            \"id_token_encrypted_response_enc\": \"A128CBC-HS256\",
            \"userinfo_signed_response_alg\": \"RS384\",
            \"userinfo_encrypted_response_alg\": \"RSA-OAEP\",
            \"userinfo_encrypted_response_enc\": \"A256CBC-HS512\",
            \"request_object_signing_alg\": \"ES512\",
            \"request_object_encryption_alg\": \"ECDH-ES+A128KW\",
            \"request_object_encryption_enc\": \"A256GCM\",
            \"token_endpoint_auth_method\": \"client_secret_basic\",
            \"token_endpoint_auth_signing_alg\": \"PS512\",
            \"default_max_age\": 3600,
            \"require_auth_time\": true,
            \"default_acr_values\": [\"0\", \"urn:mace:incommon:iap:silver\", \
                \"urn:mace:incommon:iap:bronze\"],
            \"initiate_login_uri\": \"https://example.com/login\",
            \"request_uris\": [\"https://example.com/request-1\", \"https://example.com/request-2\"]
        }}", TEST_RSA_PUB_KEY);

        let client_metadata: CoreClientMetadata = serde_json::from_str(&json_response).unwrap();

        assert_eq!(
            *client_metadata.redirect_uris(),
            vec![
                RedirectUrl::new("https://example.com/redirect-1".to_string()).unwrap(),
                RedirectUrl::new("https://example.com/redirect-2".to_string()).unwrap(),
            ]
        );
        assert_eq!(
            *client_metadata.response_types().unwrap(),
            vec![
                ResponseTypes::new(vec![CoreResponseType::Code]),
                ResponseTypes::new(vec![
                    CoreResponseType::Code,
                    CoreResponseType::Token,
                    CoreResponseType::IdToken,
                ]),
            ]
        );
        assert_eq!(
            client_metadata.grant_types().unwrap(),
            &vec![
                CoreGrantType::AuthorizationCode,
                CoreGrantType::ClientCredentials,
                CoreGrantType::Implicit,
                CoreGrantType::Password,
                CoreGrantType::RefreshToken,
            ]
        );
        assert_eq!(
            *client_metadata.application_type().unwrap(),
            CoreApplicationType::Web
        );
        assert_eq!(
            *client_metadata.contacts().unwrap(),
            vec![
                ClientContactEmail::new("user@example.com".to_string()),
                ClientContactEmail::new("admin@openidconnect.local".to_string()),
            ]
        );
        assert_eq!(
            sorted(client_metadata.client_name().unwrap().clone())
                .collect::<Vec<(Option<LanguageTag>, ClientName)>>(),
            vec![
                (None, ClientName::new("Example".to_string())),
                (
                    Some(LanguageTag::new("es".to_string())),
                    ClientName::new("Ejemplo".to_string()),
                ),
            ]
        );
        assert_eq!(
            sorted(client_metadata.logo_uri().unwrap().clone())
                .collect::<Vec<(Option<LanguageTag>, LogoUrl)>>(),
            vec![
                (
                    None,
                    LogoUrl::new("https://example.com/logo.png".to_string()).unwrap(),
                ),
                (
                    Some(LanguageTag::new("fr".to_string())),
                    LogoUrl::new("https://example.com/logo-fr.png".to_string()).unwrap(),
                ),
            ]
        );
        assert_eq!(
            sorted(client_metadata.client_uri().unwrap().clone())
                .collect::<Vec<(Option<LanguageTag>, ClientUrl)>>(),
            vec![
                (
                    None,
                    ClientUrl::new("https://example.com/client-app".to_string()).unwrap(),
                ),
                (
                    Some(LanguageTag::new("de".to_string())),
                    ClientUrl::new("https://example.com/client-app-de".to_string()).unwrap(),
                ),
            ]
        );
        assert_eq!(
            sorted(client_metadata.policy_uri().unwrap().clone())
                .collect::<Vec<(Option<LanguageTag>, PolicyUrl)>>(),
            vec![
                (
                    None,
                    PolicyUrl::new("https://example.com/policy".to_string()).unwrap(),
                ),
                (
                    Some(LanguageTag::new("sr-Latn".to_string())),
                    PolicyUrl::new("https://example.com/policy-sr-latin".to_string()).unwrap(),
                ),
            ]
        );
        assert_eq!(
            sorted(client_metadata.tos_uri().unwrap().clone())
                .collect::<Vec<(Option<LanguageTag>, ToSUrl)>>(),
            vec![
                (
                    None,
                    ToSUrl::new("https://example.com/tos".to_string()).unwrap(),
                ),
                (
                    Some(LanguageTag::new("sr-Cyrl".to_string())),
                    ToSUrl::new("https://example.com/tos-sr-cyrl".to_string()).unwrap(),
                ),
            ]
        );
        assert_eq!(
            *client_metadata.jwks_uri().unwrap(),
            JsonWebKeySetUrl::new("https://example.com/jwks".to_string()).unwrap()
        );
        assert_eq!(
            client_metadata.jwks(),
            Some(&JsonWebKeySet::new(vec![serde_json::from_str(
                TEST_RSA_PUB_KEY
            )
            .unwrap()],))
        );
        assert_eq!(
            *client_metadata.sector_identifier_uri().unwrap(),
            SectorIdentifierUrl::new("https://example.com/sector".to_string()).unwrap()
        );
        assert_eq!(
            *client_metadata.subject_type().unwrap(),
            CoreSubjectIdentifierType::Pairwise
        );
        assert_eq!(
            *client_metadata.id_token_signed_response_alg().unwrap(),
            CoreJwsSigningAlgorithm::HmacSha256
        );
        assert_eq!(
            *client_metadata.id_token_encrypted_response_alg().unwrap(),
            CoreJweKeyManagementAlgorithm::RsaPkcs1V15
        );
        assert_eq!(
            *client_metadata.id_token_encrypted_response_enc().unwrap(),
            CoreJweContentEncryptionAlgorithm::Aes128CbcHmacSha256
        );
        assert_eq!(
            *client_metadata.userinfo_signed_response_alg().unwrap(),
            CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha384
        );
        assert_eq!(
            *client_metadata.userinfo_encrypted_response_alg().unwrap(),
            CoreJweKeyManagementAlgorithm::RsaOaep
        );
        assert_eq!(
            *client_metadata.userinfo_encrypted_response_enc().unwrap(),
            CoreJweContentEncryptionAlgorithm::Aes256CbcHmacSha512
        );
        assert_eq!(
            *client_metadata.request_object_signing_alg().unwrap(),
            CoreJwsSigningAlgorithm::EcdsaP521Sha512
        );
        assert_eq!(
            *client_metadata.request_object_encryption_alg().unwrap(),
            CoreJweKeyManagementAlgorithm::EcdhEsAesKeyWrap128
        );
        assert_eq!(
            *client_metadata.request_object_encryption_enc().unwrap(),
            CoreJweContentEncryptionAlgorithm::Aes256Gcm
        );
        assert_eq!(
            *client_metadata.token_endpoint_auth_method().unwrap(),
            CoreClientAuthMethod::ClientSecretBasic
        );
        assert_eq!(
            *client_metadata.token_endpoint_auth_signing_alg().unwrap(),
            CoreJwsSigningAlgorithm::RsaSsaPssSha512
        );
        assert_eq!(
            *client_metadata.default_max_age().unwrap(),
            Duration::from_secs(3600)
        );
        assert!(client_metadata.require_auth_time().unwrap());
        assert_eq!(
            *client_metadata.default_acr_values().unwrap(),
            vec![
                AuthenticationContextClass::new("0".to_string()),
                AuthenticationContextClass::new("urn:mace:incommon:iap:silver".to_string()),
                AuthenticationContextClass::new("urn:mace:incommon:iap:bronze".to_string()),
            ]
        );
        assert_eq!(
            *client_metadata.sector_identifier_uri().unwrap(),
            SectorIdentifierUrl::new("https://example.com/sector".to_string()).unwrap()
        );
        assert_eq!(
            *client_metadata.request_uris().unwrap(),
            vec![
                RequestUrl::new("https://example.com/request-1".to_string()).unwrap(),
                RequestUrl::new("https://example.com/request-2".to_string()).unwrap(),
            ]
        );
        let serialized_json = serde_json::to_string(&client_metadata).unwrap();

        assert_eq!(
            client_metadata,
            serde_json::from_str(&serialized_json).unwrap()
        );
    }

    #[test]
    fn test_metadata_serialization_minimal() {
        let json_response = "{\"redirect_uris\": [\"https://example.com/redirect-1\"]}";

        let client_metadata: CoreClientMetadata = serde_json::from_str(json_response).unwrap();

        assert_eq!(
            *client_metadata.redirect_uris(),
            vec![RedirectUrl::new("https://example.com/redirect-1".to_string()).unwrap(),]
        );
        assert_eq!(client_metadata.response_types(), None);
        assert_eq!(client_metadata.grant_types(), None);
        assert_eq!(client_metadata.application_type(), None);
        assert_eq!(client_metadata.contacts(), None);
        assert_eq!(client_metadata.client_name(), None);
        assert_eq!(client_metadata.logo_uri(), None);
        assert_eq!(client_metadata.client_uri(), None);
        assert_eq!(client_metadata.policy_uri(), None);
        assert_eq!(client_metadata.tos_uri(), None);
        assert_eq!(client_metadata.jwks_uri(), None);
        assert_eq!(client_metadata.jwks(), None);
        assert_eq!(client_metadata.sector_identifier_uri(), None);
        assert_eq!(client_metadata.subject_type(), None);
        assert_eq!(client_metadata.id_token_signed_response_alg(), None);
        assert_eq!(client_metadata.id_token_encrypted_response_alg(), None);
        assert_eq!(client_metadata.id_token_encrypted_response_enc(), None);
        assert_eq!(client_metadata.userinfo_signed_response_alg(), None);
        assert_eq!(client_metadata.userinfo_encrypted_response_alg(), None);
        assert_eq!(client_metadata.userinfo_encrypted_response_enc(), None);
        assert_eq!(client_metadata.request_object_signing_alg(), None);
        assert_eq!(client_metadata.request_object_encryption_alg(), None);
        assert_eq!(client_metadata.request_object_encryption_enc(), None);
        assert_eq!(client_metadata.token_endpoint_auth_method(), None);
        assert_eq!(client_metadata.token_endpoint_auth_signing_alg(), None);
        assert_eq!(client_metadata.default_max_age(), None);
        assert_eq!(client_metadata.require_auth_time(), None);
        assert_eq!(client_metadata.default_acr_values(), None);
        assert_eq!(client_metadata.sector_identifier_uri(), None);
        assert_eq!(client_metadata.request_uris(), None);

        let serialized_json = serde_json::to_string(&client_metadata).unwrap();

        assert_eq!(
            client_metadata,
            serde_json::from_str(&serialized_json).unwrap()
        );
    }

    #[test]
    fn test_response_serialization() {
        let json_response = format!("{{
            \"client_id\": \"abcdefgh\",
            \"client_secret\": \"shhhh\",
            \"registration_access_token\": \"use_me_to_update_registration\",
            \"registration_client_uri\": \"https://example-provider.com/registration\",
            \"client_id_issued_at\": 1523953306,
            \"client_secret_expires_at\": 1526545306,
            \"redirect_uris\": [\"https://example.com/redirect-1\", \"https://example.com/redirect-2\"],
            \"response_types\": [\"code\", \"code token id_token\"],
            \"grant_types\": [\"authorization_code\", \"client_credentials\", \"implicit\", \
                \"password\", \"refresh_token\"],
            \"application_type\": \"web\",
            \"contacts\": [\"user@example.com\", \"admin@openidconnect.local\"],
            \"client_name\": \"Example\",
            \"client_name#es\": \"Ejemplo\",
            \"logo_uri\": \"https://example.com/logo.png\",
            \"logo_uri#fr\": \"https://example.com/logo-fr.png\",
            \"client_uri\": \"https://example.com/client-app\",
            \"client_uri#de\": \"https://example.com/client-app-de\",
            \"policy_uri\": \"https://example.com/policy\",
            \"policy_uri#sr-Latn\": \"https://example.com/policy-sr-latin\",
            \"tos_uri\": \"https://example.com/tos\",
            \"tos_uri#sr-Cyrl\": \"https://example.com/tos-sr-cyrl\",
            \"jwks_uri\": \"https://example.com/jwks\",
            \"jwks\": {{\"keys\": [{}]}},
            \"sector_identifier_uri\": \"https://example.com/sector\",
            \"subject_type\": \"pairwise\",
            \"id_token_signed_response_alg\": \"HS256\",
            \"id_token_encrypted_response_alg\": \"RSA1_5\",
            \"id_token_encrypted_response_enc\": \"A128CBC-HS256\",
            \"userinfo_signed_response_alg\": \"RS384\",
            \"userinfo_encrypted_response_alg\": \"RSA-OAEP\",
            \"userinfo_encrypted_response_enc\": \"A256CBC-HS512\",
            \"request_object_signing_alg\": \"ES512\",
            \"request_object_encryption_alg\": \"ECDH-ES+A128KW\",
            \"request_object_encryption_enc\": \"A256GCM\",
            \"token_endpoint_auth_method\": \"client_secret_basic\",
            \"token_endpoint_auth_signing_alg\": \"PS512\",
            \"default_max_age\": 3600,
            \"require_auth_time\": true,
            \"default_acr_values\": [\"0\", \"urn:mace:incommon:iap:silver\", \
                \"urn:mace:incommon:iap:bronze\"],
            \"initiate_login_uri\": \"https://example.com/login\",
            \"request_uris\": [\"https://example.com/request-1\", \"https://example.com/request-2\"]
        }}", TEST_RSA_PUB_KEY);

        let registration_response: CoreClientRegistrationResponse =
            serde_json::from_str(&json_response).unwrap();

        assert_eq!(
            *registration_response.client_id(),
            ClientId::new("abcdefgh".to_string())
        );
        assert_eq!(
            *registration_response.client_secret().unwrap().secret(),
            "shhhh"
        );
        assert_eq!(
            *registration_response
                .registration_access_token()
                .unwrap()
                .secret(),
            "use_me_to_update_registration",
        );
        assert_eq!(
            *registration_response.registration_client_uri().unwrap(),
            ClientConfigUrl::new("https://example-provider.com/registration".to_string()).unwrap()
        );
        assert_eq!(
            registration_response.client_id_issued_at().unwrap(),
            Utc.timestamp(1523953306, 0)
        );
        assert_eq!(
            registration_response.client_secret_expires_at().unwrap(),
            Utc.timestamp(1526545306, 0)
        );
        assert_eq!(
            *registration_response.redirect_uris(),
            vec![
                RedirectUrl::new("https://example.com/redirect-1".to_string()).unwrap(),
                RedirectUrl::new("https://example.com/redirect-2".to_string()).unwrap(),
            ]
        );
        assert_eq!(
            *registration_response.response_types().unwrap(),
            vec![
                ResponseTypes::new(vec![CoreResponseType::Code]),
                ResponseTypes::new(vec![
                    CoreResponseType::Code,
                    CoreResponseType::Token,
                    CoreResponseType::IdToken,
                ]),
            ]
        );
        assert_eq!(
            registration_response.grant_types().unwrap(),
            &vec![
                CoreGrantType::AuthorizationCode,
                CoreGrantType::ClientCredentials,
                CoreGrantType::Implicit,
                CoreGrantType::Password,
                CoreGrantType::RefreshToken,
            ]
        );
        assert_eq!(
            *registration_response.application_type().unwrap(),
            CoreApplicationType::Web
        );
        assert_eq!(
            *registration_response.contacts().unwrap(),
            vec![
                ClientContactEmail::new("user@example.com".to_string()),
                ClientContactEmail::new("admin@openidconnect.local".to_string()),
            ]
        );
        assert_eq!(
            sorted(registration_response.client_name().unwrap().clone())
                .collect::<Vec<(Option<LanguageTag>, ClientName)>>(),
            vec![
                (None, ClientName::new("Example".to_string())),
                (
                    Some(LanguageTag::new("es".to_string())),
                    ClientName::new("Ejemplo".to_string()),
                ),
            ]
        );
        assert_eq!(
            sorted(registration_response.logo_uri().unwrap().clone())
                .collect::<Vec<(Option<LanguageTag>, LogoUrl)>>(),
            vec![
                (
                    None,
                    LogoUrl::new("https://example.com/logo.png".to_string()).unwrap(),
                ),
                (
                    Some(LanguageTag::new("fr".to_string())),
                    LogoUrl::new("https://example.com/logo-fr.png".to_string()).unwrap(),
                ),
            ]
        );
        assert_eq!(
            sorted(registration_response.client_uri().unwrap().clone())
                .collect::<Vec<(Option<LanguageTag>, ClientUrl)>>(),
            vec![
                (
                    None,
                    ClientUrl::new("https://example.com/client-app".to_string()).unwrap(),
                ),
                (
                    Some(LanguageTag::new("de".to_string())),
                    ClientUrl::new("https://example.com/client-app-de".to_string()).unwrap(),
                ),
            ]
        );
        assert_eq!(
            sorted(registration_response.policy_uri().unwrap().clone())
                .collect::<Vec<(Option<LanguageTag>, PolicyUrl)>>(),
            vec![
                (
                    None,
                    PolicyUrl::new("https://example.com/policy".to_string()).unwrap(),
                ),
                (
                    Some(LanguageTag::new("sr-Latn".to_string())),
                    PolicyUrl::new("https://example.com/policy-sr-latin".to_string()).unwrap(),
                ),
            ]
        );
        assert_eq!(
            sorted(registration_response.tos_uri().unwrap().clone())
                .collect::<Vec<(Option<LanguageTag>, ToSUrl)>>(),
            vec![
                (
                    None,
                    ToSUrl::new("https://example.com/tos".to_string()).unwrap(),
                ),
                (
                    Some(LanguageTag::new("sr-Cyrl".to_string())),
                    ToSUrl::new("https://example.com/tos-sr-cyrl".to_string()).unwrap(),
                ),
            ]
        );
        assert_eq!(
            *registration_response.jwks_uri().unwrap(),
            JsonWebKeySetUrl::new("https://example.com/jwks".to_string()).unwrap()
        );
        assert_eq!(
            registration_response.jwks(),
            Some(&JsonWebKeySet::new(vec![serde_json::from_str(
                TEST_RSA_PUB_KEY
            )
            .unwrap()],)),
        );
        assert_eq!(
            *registration_response.sector_identifier_uri().unwrap(),
            SectorIdentifierUrl::new("https://example.com/sector".to_string()).unwrap()
        );
        assert_eq!(
            *registration_response.subject_type().unwrap(),
            CoreSubjectIdentifierType::Pairwise
        );
        assert_eq!(
            *registration_response
                .id_token_signed_response_alg()
                .unwrap(),
            CoreJwsSigningAlgorithm::HmacSha256
        );
        assert_eq!(
            *registration_response
                .id_token_encrypted_response_alg()
                .unwrap(),
            CoreJweKeyManagementAlgorithm::RsaPkcs1V15
        );
        assert_eq!(
            *registration_response
                .id_token_encrypted_response_enc()
                .unwrap(),
            CoreJweContentEncryptionAlgorithm::Aes128CbcHmacSha256
        );
        assert_eq!(
            *registration_response
                .userinfo_signed_response_alg()
                .unwrap(),
            CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha384
        );
        assert_eq!(
            *registration_response
                .userinfo_encrypted_response_alg()
                .unwrap(),
            CoreJweKeyManagementAlgorithm::RsaOaep
        );
        assert_eq!(
            *registration_response
                .userinfo_encrypted_response_enc()
                .unwrap(),
            CoreJweContentEncryptionAlgorithm::Aes256CbcHmacSha512
        );
        assert_eq!(
            *registration_response.request_object_signing_alg().unwrap(),
            CoreJwsSigningAlgorithm::EcdsaP521Sha512
        );
        assert_eq!(
            *registration_response
                .request_object_encryption_alg()
                .unwrap(),
            CoreJweKeyManagementAlgorithm::EcdhEsAesKeyWrap128
        );
        assert_eq!(
            *registration_response
                .request_object_encryption_enc()
                .unwrap(),
            CoreJweContentEncryptionAlgorithm::Aes256Gcm
        );
        assert_eq!(
            *registration_response.token_endpoint_auth_method().unwrap(),
            CoreClientAuthMethod::ClientSecretBasic
        );
        assert_eq!(
            *registration_response
                .token_endpoint_auth_signing_alg()
                .unwrap(),
            CoreJwsSigningAlgorithm::RsaSsaPssSha512
        );
        assert_eq!(
            *registration_response.default_max_age().unwrap(),
            Duration::from_secs(3600)
        );
        assert!(registration_response.require_auth_time().unwrap());
        assert_eq!(
            *registration_response.default_acr_values().unwrap(),
            vec![
                AuthenticationContextClass::new("0".to_string()),
                AuthenticationContextClass::new("urn:mace:incommon:iap:silver".to_string()),
                AuthenticationContextClass::new("urn:mace:incommon:iap:bronze".to_string()),
            ]
        );
        assert_eq!(
            *registration_response.sector_identifier_uri().unwrap(),
            SectorIdentifierUrl::new("https://example.com/sector".to_string()).unwrap()
        );
        assert_eq!(
            *registration_response.request_uris().unwrap(),
            vec![
                RequestUrl::new("https://example.com/request-1".to_string()).unwrap(),
                RequestUrl::new("https://example.com/request-2".to_string()).unwrap(),
            ]
        );
        let serialized_json = serde_json::to_string(&registration_response).unwrap();

        let deserialized: CoreClientRegistrationResponse =
            serde_json::from_str(&serialized_json).unwrap();
        assert_eq!(registration_response.client_id, deserialized.client_id);
        assert_eq!(
            registration_response.client_secret.unwrap().secret(),
            deserialized.client_secret.unwrap().secret(),
        );
        assert_eq!(
            registration_response
                .registration_access_token
                .unwrap()
                .secret(),
            deserialized.registration_access_token.unwrap().secret(),
        );
        assert_eq!(
            registration_response.registration_client_uri,
            deserialized.registration_client_uri,
        );
        assert_eq!(
            registration_response.client_id_issued_at,
            deserialized.client_id_issued_at,
        );
        assert_eq!(
            registration_response.client_secret_expires_at,
            deserialized.client_secret_expires_at,
        );
        assert_eq!(
            registration_response.client_metadata,
            deserialized.client_metadata,
        );
        assert_eq!(
            registration_response.additional_response,
            deserialized.additional_response,
        );
    }
}
