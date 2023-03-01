use std::fmt::Debug;
use std::marker::PhantomData;
use std::str::FromStr;

use chrono::{DateTime, Utc};
use oauth2::ClientId;
use serde::Serialize;
use serde_json::Value;

use crate::helpers::FilteredFlatten;
use crate::jwt::JsonWebTokenAccess;
use crate::jwt::{JsonWebTokenError, JsonWebTokenJsonPayloadSerde};
use crate::types::helpers::{deserialize_string_or_vec, serde_utc_seconds, serde_utc_seconds_opt};
use crate::types::LocalizedClaim;
use crate::{
    AccessToken, AccessTokenHash, AdditionalClaims, AddressClaim, Audience, AudiencesClaim,
    AuthenticationContextClass, AuthenticationMethodReference, AuthorizationCode,
    AuthorizationCodeHash, ClaimsVerificationError, EndUserBirthday, EndUserEmail,
    EndUserFamilyName, EndUserGivenName, EndUserMiddleName, EndUserName, EndUserNickname,
    EndUserPhoneNumber, EndUserPictureUrl, EndUserProfileUrl, EndUserTimezone, EndUserUsername,
    EndUserWebsiteUrl, ExtraTokenFields, GenderClaim, IdTokenVerifier, IssuerClaim, IssuerUrl,
    JsonWebKey, JsonWebKeyType, JsonWebKeyUse, JsonWebToken, JsonWebTokenAlgorithm,
    JweContentEncryptionAlgorithm, JwsSigningAlgorithm, LanguageTag, Nonce, NonceVerifier,
    PrivateSigningKey, SigningError, StandardClaims, SubjectIdentifier,
};

// This wrapper layer exists instead of directly verifying the JWT and returning the claims so that
// we can pass it around and easily access a serialized JWT representation of it (e.g., for passing
// to the authorization endpoint as an id_token_hint).
///
/// OpenID Connect ID token.
///
#[derive(Clone, Debug, Deserialize, PartialEq, Serialize)]
pub struct IdToken<
    AC: AdditionalClaims,
    GC: GenderClaim,
    JE: JweContentEncryptionAlgorithm<JT>,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
>(
    #[serde(bound = "AC: AdditionalClaims")]
    JsonWebToken<JE, JS, JT, IdTokenClaims<AC, GC>, JsonWebTokenJsonPayloadSerde>,
);

impl<AC, GC, JE, JS, JT> FromStr for IdToken<AC, GC, JE, JS, JT>
where
    AC: AdditionalClaims,
    GC: GenderClaim,
    JE: JweContentEncryptionAlgorithm<JT>,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
{
    type Err = serde_json::Error;
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        serde_json::from_value(Value::String(s.to_string()))
    }
}

impl<AC, GC, JE, JS, JT> IdToken<AC, GC, JE, JS, JT>
where
    AC: AdditionalClaims,
    GC: GenderClaim,
    JE: JweContentEncryptionAlgorithm<JT>,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
{
    ///
    /// Initializes an ID token with the specified claims, signed using the given signing key and
    /// algorithm.
    ///
    /// If an `access_token` and/or `code` are provided, this method sets the `at_hash` and/or
    /// `c_hash` claims using the given signing algorithm, respectively. Otherwise, those claims are
    /// unchanged from the values specified in `claims`.
    ///
    pub fn new<JU, K, S>(
        claims: IdTokenClaims<AC, GC>,
        signing_key: &S,
        alg: JS,
        access_token: Option<&AccessToken>,
        code: Option<&AuthorizationCode>,
    ) -> Result<Self, JsonWebTokenError>
    where
        JU: JsonWebKeyUse,
        K: JsonWebKey<JS, JT, JU>,
        S: PrivateSigningKey<JS, JT, JU, K>,
    {
        let at_hash = access_token
            .map(|at| {
                AccessTokenHash::from_token(at, &alg).map_err(JsonWebTokenError::SigningError)
            })
            .transpose()?
            .or_else(|| claims.access_token_hash.clone());
        let c_hash = code
            .map(|c| {
                AuthorizationCodeHash::from_code(c, &alg).map_err(JsonWebTokenError::SigningError)
            })
            .transpose()?
            .or_else(|| claims.code_hash.clone());

        JsonWebToken::new(
            IdTokenClaims {
                access_token_hash: at_hash,
                code_hash: c_hash,
                ..claims
            },
            signing_key,
            &alg,
        )
        .map(Self)
    }

    ///
    /// Verifies and returns a reference to the ID token claims.
    ///
    pub fn claims<'a, JU, K, N>(
        &'a self,
        verifier: &IdTokenVerifier<JS, JT, JU, K>,
        nonce_verifier: N,
    ) -> Result<&'a IdTokenClaims<AC, GC>, ClaimsVerificationError>
    where
        JU: JsonWebKeyUse,
        K: JsonWebKey<JS, JT, JU>,
        N: NonceVerifier,
    {
        verifier.verified_claims(&self.0, nonce_verifier)
    }

    ///
    /// Verifies and returns the ID token claims.
    ///
    pub fn into_claims<JU, K, N>(
        self,
        verifier: &IdTokenVerifier<JS, JT, JU, K>,
        nonce_verifier: N,
    ) -> Result<IdTokenClaims<AC, GC>, ClaimsVerificationError>
    where
        JU: JsonWebKeyUse,
        K: JsonWebKey<JS, JT, JU>,
        N: NonceVerifier,
    {
        verifier.verified_claims_owned(self.0, nonce_verifier)
    }

    ///
    /// Returns the [`JwsSigningAlgorithm`] used to sign this ID token.
    ///
    /// This function returns an error if the token is unsigned or utilizes JSON Web Encryption
    /// (JWE).
    ///
    pub fn signing_alg(&self) -> Result<JS, SigningError> {
        match self.0.unverified_header().alg {
            JsonWebTokenAlgorithm::Signature(ref signing_alg, _) => Ok(signing_alg.clone()),
            JsonWebTokenAlgorithm::Encryption(ref other) => Err(SigningError::UnsupportedAlg(
                serde_plain::to_string(other).unwrap_or_else(|err| {
                    panic!(format!(
                        "encryption alg {:?} failed to serialize to a string: {}",
                        other, err
                    ))
                }),
            )),
            JsonWebTokenAlgorithm::None => Err(SigningError::UnsupportedAlg("none".to_string())),
        }
    }
}
impl<AC, GC, JE, JS, JT> ToString for IdToken<AC, GC, JE, JS, JT>
where
    AC: AdditionalClaims,
    GC: GenderClaim,
    JE: JweContentEncryptionAlgorithm<JT>,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
{
    fn to_string(&self) -> String {
        serde_json::to_value(&self)
            // This should never arise, since we're just asking serde_json to serialize the
            // signing input concatenated with the signature, both of which are precomputed.
            .expect("ID token serialization failed")
            .as_str()
            // This should also never arise, since our IdToken serializer always calls serialize_str
            .expect("ID token serializer did not produce a str")
            .to_owned()
    }
}

///
/// OpenID Connect ID token claims.
///
#[derive(Clone, Debug, Deserialize, PartialEq, Serialize)]
pub struct IdTokenClaims<AC, GC>
where
    AC: AdditionalClaims,
    GC: GenderClaim,
{
    #[serde(rename = "iss")]
    issuer: IssuerUrl,
    // We always serialize as an array, which is valid according to the spec. This sets the
    // 'default' attribute to be compatible with non-spec compliant OIDC providers that omit this
    // field.
    #[serde(
        default,
        rename = "aud",
        deserialize_with = "deserialize_string_or_vec"
    )]
    audiences: Vec<Audience>,
    #[serde(rename = "exp", with = "serde_utc_seconds")]
    expiration: DateTime<Utc>,
    #[serde(rename = "iat", with = "serde_utc_seconds")]
    issue_time: DateTime<Utc>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        with = "serde_utc_seconds_opt"
    )]
    auth_time: Option<DateTime<Utc>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    nonce: Option<Nonce>,
    #[serde(rename = "acr", skip_serializing_if = "Option::is_none")]
    auth_context_ref: Option<AuthenticationContextClass>,
    #[serde(rename = "amr", skip_serializing_if = "Option::is_none")]
    auth_method_refs: Option<Vec<AuthenticationMethodReference>>,
    #[serde(rename = "azp", skip_serializing_if = "Option::is_none")]
    authorized_party: Option<ClientId>,
    #[serde(rename = "at_hash", skip_serializing_if = "Option::is_none")]
    access_token_hash: Option<AccessTokenHash>,
    #[serde(rename = "c_hash", skip_serializing_if = "Option::is_none")]
    code_hash: Option<AuthorizationCodeHash>,

    #[serde(bound = "GC: GenderClaim")]
    #[serde(flatten)]
    standard_claims: StandardClaims<GC>,

    #[serde(bound = "AC: AdditionalClaims")]
    #[serde(flatten)]
    additional_claims: FilteredFlatten<StandardClaims<GC>, AC>,
}
impl<AC, GC> IdTokenClaims<AC, GC>
where
    AC: AdditionalClaims,
    GC: GenderClaim,
{
    ///
    /// Initializes new ID token claims.
    ///
    pub fn new(
        issuer: IssuerUrl,
        audiences: Vec<Audience>,
        expiration: DateTime<Utc>,
        issue_time: DateTime<Utc>,
        standard_claims: StandardClaims<GC>,
        additional_claims: AC,
    ) -> Self {
        Self {
            issuer,
            audiences,
            expiration,
            issue_time,
            auth_time: None,
            nonce: None,
            auth_context_ref: None,
            auth_method_refs: None,
            authorized_party: None,
            access_token_hash: None,
            code_hash: None,
            standard_claims,
            additional_claims: additional_claims.into(),
        }
    }

    field_getters_setters![
        pub self [self] ["claim"] {
            set_issuer -> issuer[IssuerUrl] ["iss"],
            set_audiences -> audiences[Vec<Audience>] ["aud"],
            set_expiration -> expiration[DateTime<Utc>] ["exp"],
            set_issue_time -> issue_time[DateTime<Utc>] ["iat"],
            set_auth_time -> auth_time[Option<DateTime<Utc>>],
            set_nonce -> nonce[Option<Nonce>],
            set_auth_context_ref -> auth_context_ref[Option<AuthenticationContextClass>] ["acr"],
            set_auth_method_refs -> auth_method_refs[Option<Vec<AuthenticationMethodReference>>] ["amr"],
            set_authorized_party -> authorized_party[Option<ClientId>] ["azp"],
            set_access_token_hash -> access_token_hash[Option<AccessTokenHash>] ["at_hash"],
            set_code_hash -> code_hash[Option<AuthorizationCodeHash>] ["c_hash"],
        }
    ];

    ///
    /// Returns the `sub` claim.
    ///
    pub fn subject(&self) -> &SubjectIdentifier {
        &self.standard_claims.sub
    }
    ///
    /// Sets the `sub` claim.
    ///
    pub fn set_subject(mut self, subject: SubjectIdentifier) -> Self {
        self.standard_claims.sub = subject;
        self
    }

    field_getters_setters![
        pub self [self.standard_claims] ["claim"] {
            set_name -> name[Option<LocalizedClaim<EndUserName>>],
            set_given_name -> given_name[Option<LocalizedClaim<EndUserGivenName>>],
            set_family_name ->
                family_name[Option<LocalizedClaim<EndUserFamilyName>>],
            set_middle_name ->
                middle_name[Option<LocalizedClaim<EndUserMiddleName>>],
            set_nickname -> nickname[Option<LocalizedClaim<EndUserNickname>>],
            set_preferred_username -> preferred_username[Option<EndUserUsername>],
            set_profile -> profile[Option<LocalizedClaim<EndUserProfileUrl>>],
            set_picture -> picture[Option<LocalizedClaim<EndUserPictureUrl>>],
            set_website -> website[Option<LocalizedClaim<EndUserWebsiteUrl>>],
            set_email -> email[Option<EndUserEmail>],
            set_email_verified -> email_verified[Option<bool>],
            set_gender -> gender[Option<GC>],
            set_birthday -> birthday[Option<EndUserBirthday>],
            set_zoneinfo -> zoneinfo[Option<EndUserTimezone>],
            set_locale -> locale[Option<LanguageTag>],
            set_phone_number -> phone_number[Option<EndUserPhoneNumber>],
            set_phone_number_verified -> phone_number_verified[Option<bool>],
            set_address -> address[Option<AddressClaim>],
            set_updated_at -> updated_at[Option<DateTime<Utc>>],
        }
    ];

    ///
    /// Returns additional ID token claims.
    ///
    pub fn additional_claims(&self) -> &AC {
        self.additional_claims.as_ref()
    }
    ///
    /// Returns mutable additional ID token claims.
    ///
    pub fn additional_claims_mut(&mut self) -> &mut AC {
        self.additional_claims.as_mut()
    }
}
impl<AC, GC> AudiencesClaim for IdTokenClaims<AC, GC>
where
    AC: AdditionalClaims,
    GC: GenderClaim,
{
    fn audiences(&self) -> Option<&Vec<Audience>> {
        Some(IdTokenClaims::audiences(self))
    }
}
impl<'a, AC, GC> AudiencesClaim for &'a IdTokenClaims<AC, GC>
where
    AC: AdditionalClaims,
    GC: GenderClaim,
{
    fn audiences(&self) -> Option<&Vec<Audience>> {
        Some(IdTokenClaims::audiences(self))
    }
}
impl<AC, GC> IssuerClaim for IdTokenClaims<AC, GC>
where
    AC: AdditionalClaims,
    GC: GenderClaim,
{
    fn issuer(&self) -> Option<&IssuerUrl> {
        Some(IdTokenClaims::issuer(self))
    }
}
impl<'a, AC, GC> IssuerClaim for &'a IdTokenClaims<AC, GC>
where
    AC: AdditionalClaims,
    GC: GenderClaim,
{
    fn issuer(&self) -> Option<&IssuerUrl> {
        Some(IdTokenClaims::issuer(self))
    }
}

///
/// Extends the base OAuth2 token response with an ID token.
///
#[derive(Clone, Debug, Deserialize, PartialEq, Serialize)]
pub struct IdTokenFields<AC, EF, GC, JE, JS, JT>
where
    AC: AdditionalClaims,
    EF: ExtraTokenFields,
    GC: GenderClaim,
    JE: JweContentEncryptionAlgorithm<JT>,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
{
    #[serde(bound = "AC: AdditionalClaims")]
    id_token: Option<IdToken<AC, GC, JE, JS, JT>>,
    #[serde(bound = "EF: ExtraTokenFields", flatten)]
    extra_fields: EF,
    #[serde(skip)]
    _phantom: PhantomData<JT>,
}
impl<AC, EF, GC, JE, JS, JT> IdTokenFields<AC, EF, GC, JE, JS, JT>
where
    AC: AdditionalClaims,
    EF: ExtraTokenFields,
    GC: GenderClaim,
    JE: JweContentEncryptionAlgorithm<JT>,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
{
    ///
    /// Initializes new ID token fields containing the specified [`IdToken`] and extra fields.
    ///
    pub fn new(id_token: Option<IdToken<AC, GC, JE, JS, JT>>, extra_fields: EF) -> Self {
        Self {
            id_token,
            extra_fields,
            _phantom: PhantomData,
        }
    }

    ///
    /// Returns the [`IdToken`] contained in the OAuth2 token response.
    ///
    pub fn id_token(&self) -> Option<&IdToken<AC, GC, JE, JS, JT>> {
        self.id_token.as_ref()
    }
    ///
    /// Returns the extra fields contained in the OAuth2 token response.
    ///
    pub fn extra_fields(&self) -> &EF {
        &self.extra_fields
    }
}
impl<AC, EF, GC, JE, JS, JT> ExtraTokenFields for IdTokenFields<AC, EF, GC, JE, JS, JT>
where
    AC: AdditionalClaims,
    EF: ExtraTokenFields,
    GC: GenderClaim,
    JE: JweContentEncryptionAlgorithm<JT>,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
{
}

#[cfg(test)]
mod tests {
    use std::collections::HashMap;
    use std::str::FromStr;

    use chrono::{TimeZone, Utc};
    use oauth2::basic::BasicTokenType;
    use oauth2::{ClientId, TokenResponse};
    use url::Url;

    use crate::claims::{AdditionalClaims, EmptyAdditionalClaims, StandardClaims};
    use crate::core::{CoreGenderClaim, CoreIdToken, CoreIdTokenClaims, CoreTokenResponse};
    use crate::jwt::JsonWebTokenAccess;
    use crate::{
        AccessTokenHash, AddressClaim, AddressCountry, AddressLocality, AddressPostalCode,
        AddressRegion, Audience, AuthenticationContextClass, AuthenticationMethodReference,
        AuthorizationCodeHash, EndUserBirthday, EndUserEmail, EndUserFamilyName, EndUserGivenName,
        EndUserMiddleName, EndUserName, EndUserNickname, EndUserPhoneNumber, EndUserPictureUrl,
        EndUserProfileUrl, EndUserTimezone, EndUserUsername, EndUserWebsiteUrl, FormattedAddress,
        IssuerUrl, LanguageTag, Nonce, StreetAddress, SubjectIdentifier,
    };

    use super::{AudiencesClaim, IdTokenClaims, IssuerClaim};

    #[test]
    fn test_id_token() {
        static ID_TOKEN: &str = concat!(
            "eyJhbGciOiJSUzI1NiJ9.",
            "eyJpc3MiOiJodHRwczovL3NlcnZlci5leGFtcGxlLmNvbSIsImF1ZCI6WyJzNkJoZ",
            "FJrcXQzIl0sImV4cCI6MTMxMTI4MTk3MCwiaWF0IjoxMzExMjgwOTcwLCJzdWIiOi",
            "IyNDQwMDMyMCIsInRmYV9tZXRob2QiOiJ1MmYifQ.",
            "aW52YWxpZF9zaWduYXR1cmU"
        );

        // `serde::Deserialize` implementation is tested within the `FromStr` implementation
        let id_token = CoreIdToken::from_str(ID_TOKEN).expect("failed to parse id_token");

        let claims = id_token.0.unverified_payload_ref();

        assert_eq!(
            *claims.issuer().url(),
            Url::parse("https://server.example.com").unwrap()
        );
        assert_eq!(
            *claims.audiences(),
            vec![Audience::new("s6BhdRkqt3".to_string())]
        );
        assert_eq!(claims.expiration(), Utc.timestamp(1311281970, 0));
        assert_eq!(claims.issue_time(), Utc.timestamp(1311280970, 0));
        assert_eq!(
            *claims.subject(),
            SubjectIdentifier::new("24400320".to_string())
        );

        // test `ToString` implementation
        assert_eq!(&id_token.to_string(), ID_TOKEN);

        // test `serde::Serialize` implementation too
        let de = serde_json::to_string(&id_token).expect("failed to deserializee id token");
        assert_eq!(de, format!("\"{}\"", ID_TOKEN));
    }

    #[test]
    fn test_oauth2_response() {
        let response_str = "{\
            \"access_token\":\"foobar\",\
            \"token_type\":\"bearer\",\
            \"id_token\":\"eyJhbGciOiJSUzI1NiJ9.eyJpc3MiOiJodHRwczovL3NlcnZlci5leGFtcGxlLmNvbSIsImF\
            1ZCI6WyJzNkJoZFJrcXQzIl0sImV4cCI6MTMxMTI4MTk3MCwiaWF0IjoxMzExMjgwOTcwLCJzdWIiOiIyNDQwMD\
            MyMCIsInRmYV9tZXRob2QiOiJ1MmYifQ.aW52YWxpZF9zaWduYXR1cmU\"\
        }";
        let response =
            serde_json::from_str::<CoreTokenResponse>(response_str).expect("failed to deserialize");

        assert_eq!(*response.access_token().secret(), "foobar");
        assert_eq!(*response.token_type(), BasicTokenType::Bearer);

        let id_token = response.extra_fields().id_token();
        let claims = id_token.unwrap().0.unverified_payload_ref();

        assert_eq!(
            *claims.issuer().url(),
            Url::parse("https://server.example.com").unwrap()
        );
        assert_eq!(
            *claims.audiences(),
            vec![Audience::new("s6BhdRkqt3".to_string())]
        );
        assert_eq!(claims.expiration(), Utc.timestamp(1311281970, 0));
        assert_eq!(claims.issue_time(), Utc.timestamp(1311280970, 0));
        assert_eq!(
            *claims.subject(),
            SubjectIdentifier::new("24400320".to_string())
        );

        assert_eq!(
            serde_json::to_string(&response).expect("failed to serialize"),
            response_str
        );
    }

    #[test]
    fn test_minimal_claims_serde() {
        let new_claims = CoreIdTokenClaims::new(
            IssuerUrl::new("https://server.example.com".to_string()).unwrap(),
            vec![Audience::new("s6BhdRkqt3".to_string())],
            Utc.timestamp(1311281970, 0),
            Utc.timestamp(1311280970, 0),
            StandardClaims::new(SubjectIdentifier::new("24400320".to_string())),
            EmptyAdditionalClaims {},
        );
        let expected_serialized_claims = "\
                                          {\
                                          \"iss\":\"https://server.example.com\",\
                                          \"aud\":[\"s6BhdRkqt3\"],\
                                          \"exp\":1311281970,\
                                          \"iat\":1311280970,\
                                          \"sub\":\"24400320\"\
                                          }";

        let new_serialized_claims =
            serde_json::to_string(&new_claims).expect("failed to serialize");
        assert_eq!(new_serialized_claims, expected_serialized_claims);

        let claims: CoreIdTokenClaims = serde_json::from_str(
            "{
            \"iss\": \"https://server.example.com\",
            \"sub\": \"24400320\",
            \"aud\": \"s6BhdRkqt3\",
            \"exp\": 1311281970,
            \"iat\": 1311280970
            }",
        )
        .expect("failed to deserialize");
        assert_eq!(claims, new_claims);
        assert_eq!(claims.issuer().url(), new_claims.issuer().url());
        assert_eq!(claims.audiences(), new_claims.audiences());
        assert_eq!(claims.expiration(), new_claims.expiration());
        assert_eq!(claims.issue_time(), new_claims.issue_time());
        assert_eq!(claims.auth_time(), None);
        assert!(claims.nonce().is_none());
        assert_eq!(claims.auth_context_ref(), None);
        assert_eq!(claims.auth_method_refs(), None);
        assert_eq!(claims.authorized_party(), None);
        assert_eq!(claims.access_token_hash(), None);
        assert_eq!(claims.code_hash(), None);
        assert_eq!(*claims.additional_claims(), EmptyAdditionalClaims {});
        assert_eq!(claims.subject(), new_claims.subject());
        assert_eq!(claims.name(), None);
        assert_eq!(claims.given_name(), None);
        assert_eq!(claims.family_name(), None);
        assert_eq!(claims.middle_name(), None);
        assert_eq!(claims.nickname(), None);
        assert_eq!(claims.preferred_username(), None);
        assert_eq!(claims.profile(), None);
        assert_eq!(claims.picture(), None);
        assert_eq!(claims.website(), None);
        assert_eq!(claims.email(), None);
        assert_eq!(claims.email_verified(), None);
        assert_eq!(claims.gender(), None);
        assert_eq!(claims.birthday(), None);
        assert_eq!(claims.zoneinfo(), None);
        assert_eq!(claims.locale(), None);
        assert_eq!(claims.phone_number(), None);
        assert_eq!(claims.phone_number_verified(), None);
        assert_eq!(claims.address(), None);
        assert_eq!(claims.updated_at(), None);

        let serialized_claims = serde_json::to_string(&claims).expect("failed to serialize");
        assert_eq!(serialized_claims, expected_serialized_claims);

        let claims_round_trip: CoreIdTokenClaims =
            serde_json::from_str(&serialized_claims).expect("failed to deserialize");
        assert_eq!(claims, claims_round_trip);
    }

    #[test]
    fn test_complete_claims_serde() {
        let claims_json = "{\
                           \"iss\":\"https://server.example.com\",\
                           \"aud\":[\"s6BhdRkqt3\"],\
                           \"exp\":1311281970,\
                           \"iat\":1311280970,\
                           \"auth_time\":1311282970,\
                           \"nonce\":\"Zm9vYmFy\",\
                           \"acr\":\"urn:mace:incommon:iap:silver\",\
                           \"amr\":[\"password\",\"totp\"],\
                           \"azp\":\"dGhpc19jbGllbnQ\",\
                           \"at_hash\":\"_JPLB-GtkomFJxAOWKHPHQ\",\
                           \"c_hash\":\"VpTQii5T_8rgwxA-Wtb2Bw\",\
                           \"sub\":\"24400320\",\
                           \"name\":\"Homer Simpson\",\
                           \"name#es\":\"Jomer Simpson\",\
                           \"given_name\":\"Homer\",\
                           \"given_name#es\":\"Jomer\",\
                           \"family_name\":\"Simpson\",\
                           \"family_name#es\":\"Simpson\",\
                           \"middle_name\":\"Jay\",\
                           \"middle_name#es\":\"Jay\",\
                           \"nickname\":\"Homer\",\
                           \"nickname#es\":\"Jomer\",\
                           \"preferred_username\":\"homersimpson\",\
                           \"profile\":\"https://example.com/profile?id=12345\",\
                           \"profile#es\":\"https://example.com/profile?id=12345&lang=es\",\
                           \"picture\":\"https://example.com/avatar?id=12345\",\
                           \"picture#es\":\"https://example.com/avatar?id=12345&lang=es\",\
                           \"website\":\"https://homersimpson.me\",\
                           \"website#es\":\"https://homersimpson.me/?lang=es\",\
                           \"email\":\"homer@homersimpson.me\",\
                           \"email_verified\":true,\
                           \"gender\":\"male\",\
                           \"birthday\":\"1956-05-12\",\
                           \"zoneinfo\":\"America/Los_Angeles\",\
                           \"locale\":\"en-US\",\
                           \"phone_number\":\"+1 (555) 555-5555\",\
                           \"phone_number_verified\":false,\
                           \"address\":{\
                           \"formatted\":\"1234 Hollywood Blvd., Los Angeles, CA 90210\",\
                           \"street_address\":\"1234 Hollywood Blvd.\",\
                           \"locality\":\"Los Angeles\",\
                           \"region\":\"CA\",\
                           \"postal_code\":\"90210\",\
                           \"country\":\"US\"\
                           },\
                           \"updated_at\":1311283970\
                           }";

        let new_claims = CoreIdTokenClaims::new(
            IssuerUrl::new("https://server.example.com".to_string()).unwrap(),
            vec![Audience::new("s6BhdRkqt3".to_string())],
            Utc.timestamp(1311281970, 0),
            Utc.timestamp(1311280970, 0),
            StandardClaims {
                sub: SubjectIdentifier::new("24400320".to_string()),
                name: Some(
                    vec![
                        (None, EndUserName::new("Homer Simpson".to_string())),
                        (
                            Some(LanguageTag::new("es".to_string())),
                            EndUserName::new("Jomer Simpson".to_string()),
                        ),
                    ]
                    .into_iter()
                    .collect(),
                ),
                given_name: Some(
                    vec![
                        (None, EndUserGivenName::new("Homer".to_string())),
                        (
                            Some(LanguageTag::new("es".to_string())),
                            EndUserGivenName::new("Jomer".to_string()),
                        ),
                    ]
                    .into_iter()
                    .collect(),
                ),
                family_name: Some(
                    vec![
                        (None, EndUserFamilyName::new("Simpson".to_string())),
                        (
                            Some(LanguageTag::new("es".to_string())),
                            EndUserFamilyName::new("Simpson".to_string()),
                        ),
                    ]
                    .into_iter()
                    .collect(),
                ),
                middle_name: Some(
                    vec![
                        (None, EndUserMiddleName::new("Jay".to_string())),
                        (
                            Some(LanguageTag::new("es".to_string())),
                            EndUserMiddleName::new("Jay".to_string()),
                        ),
                    ]
                    .into_iter()
                    .collect(),
                ),
                nickname: Some(
                    vec![
                        (None, EndUserNickname::new("Homer".to_string())),
                        (
                            Some(LanguageTag::new("es".to_string())),
                            EndUserNickname::new("Jomer".to_string()),
                        ),
                    ]
                    .into_iter()
                    .collect(),
                ),
                preferred_username: Some(EndUserUsername::new("homersimpson".to_string())),
                profile: Some(
                    vec![
                        (
                            None,
                            EndUserProfileUrl::new(
                                "https://example.com/profile?id=12345".to_string(),
                            ),
                        ),
                        (
                            Some(LanguageTag::new("es".to_string())),
                            EndUserProfileUrl::new(
                                "https://example.com/profile?id=12345&lang=es".to_string(),
                            ),
                        ),
                    ]
                    .into_iter()
                    .collect(),
                ),
                picture: Some(
                    vec![
                        (
                            None,
                            EndUserPictureUrl::new(
                                "https://example.com/avatar?id=12345".to_string(),
                            ),
                        ),
                        (
                            Some(LanguageTag::new("es".to_string())),
                            EndUserPictureUrl::new(
                                "https://example.com/avatar?id=12345&lang=es".to_string(),
                            ),
                        ),
                    ]
                    .into_iter()
                    .collect(),
                ),
                website: Some(
                    vec![
                        (
                            None,
                            EndUserWebsiteUrl::new("https://homersimpson.me".to_string()),
                        ),
                        (
                            Some(LanguageTag::new("es".to_string())),
                            EndUserWebsiteUrl::new("https://homersimpson.me/?lang=es".to_string()),
                        ),
                    ]
                    .into_iter()
                    .collect(),
                ),
                email: Some(EndUserEmail::new("homer@homersimpson.me".to_string())),
                email_verified: Some(true),
                gender: Some(CoreGenderClaim::new("male".to_string())),
                birthday: Some(EndUserBirthday::new("1956-05-12".to_string())),
                zoneinfo: Some(EndUserTimezone::new("America/Los_Angeles".to_string())),
                locale: Some(LanguageTag::new("en-US".to_string())),
                phone_number: Some(EndUserPhoneNumber::new("+1 (555) 555-5555".to_string())),
                phone_number_verified: Some(false),
                address: Some(AddressClaim {
                    formatted: Some(FormattedAddress::new(
                        "1234 Hollywood Blvd., Los Angeles, CA 90210".to_string(),
                    )),
                    street_address: Some(StreetAddress::new("1234 Hollywood Blvd.".to_string())),
                    locality: Some(AddressLocality::new("Los Angeles".to_string())),
                    region: Some(AddressRegion::new("CA".to_string())),
                    postal_code: Some(AddressPostalCode::new("90210".to_string())),
                    country: Some(AddressCountry::new("US".to_string())),
                }),
                updated_at: Some(Utc.timestamp(1311283970, 0)),
            },
            EmptyAdditionalClaims {},
        )
        .set_auth_time(Some(Utc.timestamp(1311282970, 0)))
        .set_nonce(Some(Nonce::new("Zm9vYmFy".to_string())))
        .set_auth_context_ref(Some(AuthenticationContextClass::new(
            "urn:mace:incommon:iap:silver".to_string(),
        )))
        .set_auth_method_refs(Some(vec![
            AuthenticationMethodReference::new("password".to_string()),
            AuthenticationMethodReference::new("totp".to_string()),
        ]))
        .set_authorized_party(Some(ClientId::new("dGhpc19jbGllbnQ".to_string())))
        .set_access_token_hash(Some(AccessTokenHash::new(
            "_JPLB-GtkomFJxAOWKHPHQ".to_string(),
        )))
        .set_code_hash(Some(AuthorizationCodeHash::new(
            "VpTQii5T_8rgwxA-Wtb2Bw".to_string(),
        )));

        let claims: CoreIdTokenClaims =
            serde_json::from_str(claims_json).expect("failed to deserialize");
        assert_eq!(claims, new_claims);
        assert_eq!(claims.issuer(), new_claims.issuer());
        assert_eq!(claims.issuer().url(), new_claims.issuer().url());
        assert_eq!(claims.audiences(), new_claims.audiences());
        assert_eq!(claims.expiration(), new_claims.expiration());
        assert_eq!(claims.issue_time(), new_claims.issue_time());
        assert_eq!(claims.auth_time(), new_claims.auth_time());
        assert_eq!(
            claims.nonce().unwrap().secret(),
            new_claims.nonce().unwrap().secret()
        );
        assert_eq!(claims.auth_context_ref(), new_claims.auth_context_ref());
        assert_eq!(claims.auth_method_refs(), new_claims.auth_method_refs());
        assert_eq!(claims.authorized_party(), new_claims.authorized_party());
        assert_eq!(claims.access_token_hash(), new_claims.access_token_hash());
        assert_eq!(claims.code_hash(), new_claims.code_hash());
        assert_eq!(*claims.additional_claims(), EmptyAdditionalClaims {});
        assert_eq!(claims.subject(), new_claims.subject());
        assert_eq!(claims.name(), new_claims.name());
        assert_eq!(claims.given_name(), new_claims.given_name());
        assert_eq!(claims.family_name(), new_claims.family_name());
        assert_eq!(claims.middle_name(), new_claims.middle_name());
        assert_eq!(claims.nickname(), new_claims.nickname());
        assert_eq!(claims.preferred_username(), new_claims.preferred_username());
        assert_eq!(claims.preferred_username(), new_claims.preferred_username());
        assert_eq!(claims.profile(), new_claims.profile());
        assert_eq!(claims.picture(), new_claims.picture());
        assert_eq!(claims.website(), new_claims.website());
        assert_eq!(claims.email(), new_claims.email());
        assert_eq!(claims.email_verified(), new_claims.email_verified());
        assert_eq!(claims.gender(), new_claims.gender());
        assert_eq!(claims.birthday(), new_claims.birthday());
        assert_eq!(claims.zoneinfo(), new_claims.zoneinfo());
        assert_eq!(claims.locale(), new_claims.locale());
        assert_eq!(claims.phone_number(), new_claims.phone_number(),);
        assert_eq!(
            claims.phone_number_verified(),
            new_claims.phone_number_verified()
        );
        assert_eq!(claims.address(), new_claims.address());
        assert_eq!(claims.updated_at(), new_claims.updated_at());

        let serialized_claims = serde_json::to_string(&claims).expect("failed to serialize");
        let claims_round_trip: CoreIdTokenClaims =
            serde_json::from_str(&serialized_claims).expect("failed to deserialize");
        assert_eq!(claims, claims_round_trip);

        let serialized_new_claims =
            serde_json::to_string(&new_claims).expect("failed to serialize");
        assert_eq!(serialized_new_claims, claims_json);
    }

    // See https://github.com/ramosbugs/openidconnect-rs/issues/23
    #[test]
    #[cfg(feature = "accept-rfc3339-timestamps")]
    fn test_accept_rfc3339_timestamp() {
        let claims: CoreIdTokenClaims = serde_json::from_str(
            "{
            \"iss\": \"https://server.example.com\",
            \"sub\": \"24400320\",
            \"aud\": \"s6BhdRkqt3\",
            \"exp\": 1311281970,
            \"iat\": 1311280970,
            \"updated_at\": \"2021-12-22T02:10:37.000Z\"
            }",
        )
        .expect("failed to deserialize");
        assert_eq!(claims.updated_at(), Some(Utc.timestamp(1640139037, 0)));
    }

    #[test]
    fn test_unknown_claims_serde() {
        let expected_serialized_claims = "{\
                                          \"iss\":\"https://server.example.com\",\
                                          \"aud\":[\"s6BhdRkqt3\"],\
                                          \"exp\":1311281970,\
                                          \"iat\":1311280970,\
                                          \"sub\":\"24400320\"\
                                          }";

        let claims: CoreIdTokenClaims = serde_json::from_str(
            "{
            \"iss\": \"https://server.example.com\",
            \"sub\": \"24400320\",
            \"aud\": \"s6BhdRkqt3\",
            \"exp\": 1311281970,
            \"iat\": 1311280970,
            \"some_other_field\":\"some_other_value\"\
            }",
        )
        .expect("failed to deserialize");

        let serialized_claims = serde_json::to_string(&claims).expect("failed to serialize");
        assert_eq!(serialized_claims, expected_serialized_claims);

        let claims_round_trip: CoreIdTokenClaims =
            serde_json::from_str(&serialized_claims).expect("failed to deserialize");
        assert_eq!(claims, claims_round_trip);
    }

    #[test]
    fn test_audience() {
        let single_aud_str_claims = serde_json::from_str::<CoreIdTokenClaims>(
            "{
                    \"iss\": \"https://server.example.com\",
                    \"sub\": \"24400320\",
                    \"aud\": \"s6BhdRkqt3\",
                    \"exp\": 1311281970,
                    \"iat\": 1311280970
                }",
        )
        .expect("failed to deserialize");
        assert_eq!(
            *single_aud_str_claims.audiences(),
            vec![Audience::new("s6BhdRkqt3".to_string())],
        );

        // We always serialize aud as an array, which is valid according to the spec.
        assert_eq!(
            serde_json::to_string(&single_aud_str_claims).expect("failed to serialize"),
            "{\
             \"iss\":\"https://server.example.com\",\
             \"aud\":[\"s6BhdRkqt3\"],\
             \"exp\":1311281970,\
             \"iat\":1311280970,\
             \"sub\":\"24400320\"\
             }",
        );

        let single_aud_vec_claims = serde_json::from_str::<CoreIdTokenClaims>(
            "{
                    \"iss\": \"https://server.example.com\",
                    \"sub\": \"24400320\",
                    \"aud\": [\"s6BhdRkqt3\"],
                    \"exp\": 1311281970,
                    \"iat\": 1311280970
                }",
        )
        .expect("failed to deserialize");
        assert_eq!(
            *single_aud_vec_claims.audiences(),
            vec![Audience::new("s6BhdRkqt3".to_string())],
        );
        assert_eq!(
            serde_json::to_string(&single_aud_vec_claims).expect("failed to serialize"),
            "{\
             \"iss\":\"https://server.example.com\",\
             \"aud\":[\"s6BhdRkqt3\"],\
             \"exp\":1311281970,\
             \"iat\":1311280970,\
             \"sub\":\"24400320\"\
             }",
        );

        let multi_aud_claims = serde_json::from_str::<CoreIdTokenClaims>(
            "{\
                    \"iss\": \"https://server.example.com\",
                    \"sub\": \"24400320\",
                    \"aud\": [\"s6BhdRkqt3\", \"aud2\"],
                    \"exp\": 1311281970,
                    \"iat\": 1311280970
                }",
        )
        .expect("failed to deserialize");
        assert_eq!(
            *multi_aud_claims.audiences(),
            vec![
                Audience::new("s6BhdRkqt3".to_string()),
                Audience::new("aud2".to_string())
            ],
        );
        assert_eq!(
            serde_json::to_string(&multi_aud_claims).expect("failed to serialize"),
            "{\
             \"iss\":\"https://server.example.com\",\
             \"aud\":[\"s6BhdRkqt3\",\"aud2\"],\
             \"exp\":1311281970,\
             \"iat\":1311280970,\
             \"sub\":\"24400320\"\
             }",
        );
    }

    #[derive(Clone, Debug, Deserialize, PartialEq, Serialize)]
    struct TestClaims {
        pub tfa_method: String,
    }
    impl AdditionalClaims for TestClaims {}

    #[test]
    fn test_additional_claims() {
        let claims = serde_json::from_str::<IdTokenClaims<TestClaims, CoreGenderClaim>>(
            "{
                \"iss\": \"https://server.example.com\",
                \"sub\": \"24400320\",
                \"aud\": [\"s6BhdRkqt3\"],
                \"exp\": 1311281970,
                \"iat\": 1311280970,
                \"tfa_method\": \"u2f\"
            }",
        )
        .expect("failed to deserialize");
        assert_eq!(claims.additional_claims().tfa_method, "u2f");
        assert_eq!(
            serde_json::to_string(&claims).expect("failed to serialize"),
            "{\
             \"iss\":\"https://server.example.com\",\
             \"aud\":[\"s6BhdRkqt3\"],\
             \"exp\":1311281970,\
             \"iat\":1311280970,\
             \"sub\":\"24400320\",\
             \"tfa_method\":\"u2f\"\
             }",
        );

        serde_json::from_str::<IdTokenClaims<TestClaims, CoreGenderClaim>>(
            "{
                \"iss\": \"https://server.example.com\",
                \"sub\": \"24400320\",
                \"aud\": [\"s6BhdRkqt3\"],
                \"exp\": 1311281970,
                \"iat\": 1311280970
            }",
        )
        .expect_err("missing claim should fail to deserialize");
    }

    #[derive(Debug, Deserialize, Serialize)]
    struct AllOtherClaims(HashMap<String, serde_json::Value>);
    impl AdditionalClaims for AllOtherClaims {}

    #[test]
    fn test_catch_all_additional_claims() {
        let claims = serde_json::from_str::<IdTokenClaims<AllOtherClaims, CoreGenderClaim>>(
            "{
                \"iss\": \"https://server.example.com\",
                \"sub\": \"24400320\",
                \"aud\": [\"s6BhdRkqt3\"],
                \"exp\": 1311281970,
                \"iat\": 1311280970,
                \"tfa_method\": \"u2f\",
                \"updated_at\": 1000
            }",
        )
        .expect("failed to deserialize");

        assert_eq!(claims.additional_claims().0.len(), 1);
        assert_eq!(claims.additional_claims().0["tfa_method"], "u2f");
    }

    #[test]
    fn test_audiences_claim() {
        let claims = serde_json::from_str::<CoreIdTokenClaims>(
            "{
                    \"iss\": \"https://server.example.com\",
                    \"sub\": \"24400320\",
                    \"aud\": \"s6BhdRkqt3\",
                    \"exp\": 1311281970,
                    \"iat\": 1311280970
                }",
        )
        .expect("failed to deserialize");

        fn verify_audiences<A: AudiencesClaim>(audiences_claim: &A) {
            assert_eq!(
                (*audiences_claim).audiences(),
                Some(&vec![Audience::new("s6BhdRkqt3".to_string())]),
            )
        }
        verify_audiences(&claims);
        verify_audiences(&&claims);
    }

    #[test]
    fn test_issuer_claim() {
        let claims = serde_json::from_str::<CoreIdTokenClaims>(
            "{
                    \"iss\": \"https://server.example.com\",
                    \"sub\": \"24400320\",
                    \"aud\": \"s6BhdRkqt3\",
                    \"exp\": 1311281970,
                    \"iat\": 1311280970
                }",
        )
        .expect("failed to deserialize");

        fn verify_issuer<I: IssuerClaim>(issuer_claim: &I) {
            assert_eq!(
                (*issuer_claim).issuer(),
                Some(&IssuerUrl::new("https://server.example.com".to_string()).unwrap()),
            )
        }
        verify_issuer(&claims);
        verify_issuer(&&claims);
    }
}
