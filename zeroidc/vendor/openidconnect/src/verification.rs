use std::collections::HashSet;
use std::fmt::Debug;
use std::marker::PhantomData;
use std::ops::Deref;
use std::sync::Arc;

use chrono::{DateTime, Utc};
use oauth2::helpers::variant_name;
use oauth2::{ClientId, ClientSecret};
use ring::constant_time::verify_slices_are_equal;
use serde::de::DeserializeOwned;
use serde::Serialize;
use thiserror::Error;

use crate::jwt::{JsonWebToken, JsonWebTokenJsonPayloadSerde};
use crate::user_info::UserInfoClaimsImpl;
use crate::{
    AdditionalClaims, Audience, AuthenticationContextClass, GenderClaim, IdTokenClaims, IssuerUrl,
    JsonWebKey, JsonWebKeySet, JsonWebKeyType, JsonWebKeyUse, JsonWebTokenAccess,
    JsonWebTokenAlgorithm, JsonWebTokenHeader, JweContentEncryptionAlgorithm, JwsSigningAlgorithm,
    Nonce, SubjectIdentifier,
};

pub(crate) trait AudiencesClaim {
    fn audiences(&self) -> Option<&Vec<Audience>>;
}

pub(crate) trait IssuerClaim {
    fn issuer(&self) -> Option<&IssuerUrl>;
}

///
/// Error verifying claims.
///
#[derive(Clone, Debug, Error, PartialEq)]
#[non_exhaustive]
pub enum ClaimsVerificationError {
    /// Claims have expired.
    #[error("Expired: {0}")]
    Expired(String),
    /// Audience claim is invalid.
    #[error("Invalid audiences: {0}")]
    InvalidAudience(String),
    /// Authorization context class reference (`acr`) claim is invalid.
    #[error("Invalid authorization context class reference: {0}")]
    InvalidAuthContext(String),
    /// User authenticated too long ago.
    #[error("Invalid authentication time: {0}")]
    InvalidAuthTime(String),
    /// Issuer claim is invalid.
    #[error("Invalid issuer: {0}")]
    InvalidIssuer(String),
    /// Nonce is invalid.
    #[error("Invalid nonce: {0}")]
    InvalidNonce(String),
    /// Subject claim is invalid.
    #[error("Invalid subject: {0}")]
    InvalidSubject(String),
    /// No signature present but claims must be signed.
    #[error("Claims must be signed")]
    NoSignature,
    /// An unexpected error occurred.
    #[error("{0}")]
    Other(String),
    /// Failed to verify the claims signature.
    #[error("Signature verification failed")]
    SignatureVerification(#[source] SignatureVerificationError),
    /// Unsupported argument or value.
    #[error("Unsupported: {0}")]
    Unsupported(String),
}

///
/// Error verifying claims signature.
///
#[derive(Clone, Debug, Error, PartialEq)]
#[non_exhaustive]
pub enum SignatureVerificationError {
    /// More than one key matches the supplied key constraints (e.g., key ID).
    #[error("Ambiguous key identification: {0}")]
    AmbiguousKeyId(String),
    /// Invalid signature for the supplied claims and signing key.
    #[error("Crypto error: {0}")]
    CryptoError(String),
    /// The supplied signature algorithm is disallowed by the verifier.
    #[error("Disallowed signature algorithm: {0}")]
    DisallowedAlg(String),
    /// The supplied key cannot be used in this context. This may occur if the key type does not
    /// match the signature type (e.g., an RSA key used to validate an HMAC) or the JWK usage
    /// disallows signatures.
    #[error("Invalid cryptographic key: {0}")]
    InvalidKey(String),
    /// The signing key needed for verifying the
    /// [JSON Web Token](https://tools.ietf.org/html/rfc7519)'s signature/MAC could not be found.
    /// This error can occur if the key ID (`kid`) specified in the JWT's
    /// [JOSE header](https://tools.ietf.org/html/rfc7519#section-5) does not match the ID of any
    /// key in the OpenID Connect provider's JSON Web Key Set (JWKS), typically retrieved from
    /// the provider's [JWKS document](
    /// http://openid.net/specs/openid-connect-discovery-1_0.html#ProviderMetadata). To support
    /// [rotation of asymmetric signing keys](
    /// http://openid.net/specs/openid-connect-core-1_0.html#RotateSigKeys), client applications
    /// should consider refreshing the JWKS document (via
    /// [`JsonWebKeySet::fetch`][crate::JsonWebKeySet::fetch]).
    ///
    /// This error can also occur if the identified
    /// [JSON Web Key](https://tools.ietf.org/html/rfc7517) is of the wrong type (e.g., an RSA key
    /// when the JOSE header specifies an ECDSA algorithm) or does not support signing.
    #[error("No matching key found")]
    NoMatchingKey,
    /// Unsupported signature algorithm.
    #[error("Unsupported signature algorithm: {0}")]
    UnsupportedAlg(String),
    /// An unexpected error occurred.
    #[error("Other error: {0}")]
    Other(String),
}

// This struct is intentionally private.
#[derive(Clone)]
struct JwtClaimsVerifier<'a, JS, JT, JU, K>
where
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    JU: JsonWebKeyUse,
    K: JsonWebKey<JS, JT, JU>,
{
    allowed_algs: Option<HashSet<JS>>,
    aud_match_required: bool,
    client_id: ClientId,
    client_secret: Option<ClientSecret>,
    iss_required: bool,
    issuer: IssuerUrl,
    is_signature_check_enabled: bool,
    other_aud_verifier_fn: Arc<dyn Fn(&Audience) -> bool + 'a + Send + Sync>,
    signature_keys: JsonWebKeySet<JS, JT, JU, K>,
}
impl<'a, JS, JT, JU, K> JwtClaimsVerifier<'a, JS, JT, JU, K>
where
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    JU: JsonWebKeyUse,
    K: JsonWebKey<JS, JT, JU>,
{
    pub fn new(
        client_id: ClientId,
        issuer: IssuerUrl,
        signature_keys: JsonWebKeySet<JS, JT, JU, K>,
    ) -> Self {
        JwtClaimsVerifier {
            allowed_algs: Some([JS::rsa_sha_256()].iter().cloned().collect()),
            aud_match_required: true,
            client_id,
            client_secret: None,
            iss_required: true,
            issuer,
            is_signature_check_enabled: true,
            // Secure default: reject all other audiences as untrusted, since any other audience
            // can potentially impersonate the user when by sending its copy of these claims
            // to this relying party.
            other_aud_verifier_fn: Arc::new(|_| false),
            signature_keys,
        }
    }

    pub fn require_audience_match(mut self, aud_required: bool) -> Self {
        self.aud_match_required = aud_required;
        self
    }

    pub fn require_issuer_match(mut self, iss_required: bool) -> Self {
        self.iss_required = iss_required;
        self
    }

    pub fn require_signature_check(mut self, sig_required: bool) -> Self {
        self.is_signature_check_enabled = sig_required;
        self
    }

    pub fn set_allowed_algs<I>(mut self, algs: I) -> Self
    where
        I: IntoIterator<Item = JS>,
    {
        self.allowed_algs = Some(algs.into_iter().collect());
        self
    }
    pub fn allow_any_alg(mut self) -> Self {
        self.allowed_algs = None;
        self
    }

    pub fn set_client_secret(mut self, client_secret: ClientSecret) -> Self {
        self.client_secret = Some(client_secret);
        self
    }

    pub fn set_other_audience_verifier_fn<T>(mut self, other_aud_verifier_fn: T) -> Self
    where
        T: Fn(&Audience) -> bool + 'a + Send + Sync,
    {
        self.other_aud_verifier_fn = Arc::new(other_aud_verifier_fn);
        self
    }

    fn validate_jose_header<JE>(
        jose_header: &JsonWebTokenHeader<JE, JS, JT>,
    ) -> Result<(), ClaimsVerificationError>
    where
        JE: JweContentEncryptionAlgorithm<JT>,
    {
        // The 'typ' header field must either be omitted or have the canonicalized value JWT.
        if let Some(ref jwt_type) = jose_header.typ {
            if jwt_type.to_uppercase() != "JWT" {
                return Err(ClaimsVerificationError::Unsupported(format!(
                    "unexpected or unsupported JWT type `{}`",
                    **jwt_type
                )));
            }
        }
        // The 'cty' header field must be omitted, since it's only used for JWTs that contain
        // content types other than JSON-encoded claims. This may include nested JWTs, such as if
        // JWE encryption is used. This is currently unsupported.
        if let Some(ref content_type) = jose_header.cty {
            if content_type.to_uppercase() == "JWT" {
                return Err(ClaimsVerificationError::Unsupported(
                    "nested JWT's are not currently supported".to_string(),
                ));
            } else {
                return Err(ClaimsVerificationError::Unsupported(format!(
                    "unexpected or unsupported JWT content type `{}`",
                    **content_type
                )));
            }
        }

        // If 'crit' fields are specified, we must reject any we do not understand. Since this
        // implementation doesn't understand any of them, unconditionally reject the JWT. Note that
        // the spec prohibits this field from containing any of the standard headers or being empty.
        if jose_header.crit.is_some() {
            // https://tools.ietf.org/html/rfc7515#appendix-E
            return Err(ClaimsVerificationError::Unsupported(
                "critical JWT header fields are unsupported".to_string(),
            ));
        }
        Ok(())
    }

    pub fn verified_claims<A, C, JE, T>(&self, jwt: A) -> Result<T, ClaimsVerificationError>
    where
        A: JsonWebTokenAccess<JE, JS, JT, C, ReturnType = T>,
        C: AudiencesClaim + Debug + DeserializeOwned + IssuerClaim + Serialize,
        JE: JweContentEncryptionAlgorithm<JT>,
        T: AudiencesClaim + IssuerClaim,
    {
        {
            let jose_header = jwt.unverified_header();
            Self::validate_jose_header(jose_header)?;

            // The code below roughly follows the validation steps described in
            // https://openid.net/specs/openid-connect-core-1_0.html#IDTokenValidation

            // 1. If the ID Token is encrypted, decrypt it using the keys and algorithms that the Client
            //    specified during Registration that the OP was to use to encrypt the ID Token. If
            //    encryption was negotiated with the OP at Registration time and the ID Token is not
            //    encrypted, the RP SHOULD reject it.

            if let JsonWebTokenAlgorithm::Encryption(ref encryption_alg) = jose_header.alg {
                return Err(ClaimsVerificationError::Unsupported(format!(
                    "JWE encryption is not currently supported (found algorithm `{}`)",
                    variant_name(encryption_alg),
                )));
            }
        }

        // TODO: Add encryption (JWE) support
        {
            // 2. The Issuer Identifier for the OpenID Provider (which is typically obtained during
            //    Discovery) MUST exactly match the value of the iss (issuer) Claim.
            let unverified_claims = jwt.unverified_payload_ref();
            if self.iss_required {
                if let Some(issuer) = unverified_claims.issuer() {
                    if *issuer != self.issuer {
                        return Err(ClaimsVerificationError::InvalidIssuer(format!(
                            "expected `{}` (found `{}`)",
                            *self.issuer, **issuer
                        )));
                    }
                } else {
                    return Err(ClaimsVerificationError::InvalidIssuer(
                        "missing issuer claim".to_string(),
                    ));
                }
            }

            // 3. The Client MUST validate that the aud (audience) Claim contains its client_id value
            //    registered at the Issuer identified by the iss (issuer) Claim as an audience. The aud
            //    (audience) Claim MAY contain an array with more than one element. The ID Token MUST be
            //    rejected if the ID Token does not list the Client as a valid audience, or if it
            //    contains additional audiences not trusted by the Client.
            if self.aud_match_required {
                if let Some(audiences) = unverified_claims.audiences() {
                    if !audiences
                        .iter()
                        .any(|aud| (**aud).deref() == self.client_id.deref())
                    {
                        return Err(ClaimsVerificationError::InvalidAudience(format!(
                            "must contain `{}` (found audiences: {})",
                            *self.client_id,
                            audiences
                                .iter()
                                .map(|aud| format!("`{}`", Deref::deref(aud)))
                                .collect::<Vec<_>>()
                                .join(", ")
                        )));
                    } else if audiences.len() > 1 {
                        audiences
                            .iter()
                            .filter(|aud| (**aud).deref() != self.client_id.deref())
                            .find(|aud| !(self.other_aud_verifier_fn)(aud))
                            .map(|aud| {
                                Err(ClaimsVerificationError::InvalidAudience(format!(
                                    "`{}` is not a trusted audience",
                                    **aud,
                                )))
                            })
                            .unwrap_or(Ok(()))?;
                    }
                } else {
                    return Err(ClaimsVerificationError::InvalidAudience(
                        "missing audiences claim".to_string(),
                    ));
                }
            }
        }
        // Steps 4--5 (azp claim validation) are specific to the ID token.

        // 6. If the ID Token is received via direct communication between the Client and the Token
        //    Endpoint (which it is in this flow), the TLS server validation MAY be used to validate
        //    the issuer in place of checking the token signature. The Client MUST validate the
        //    signature of all other ID Tokens according to JWS [JWS] using the algorithm specified
        //    in the JWT alg Header Parameter. The Client MUST use the keys provided by the Issuer.
        if !self.is_signature_check_enabled {
            return Ok(jwt.unverified_payload());
        }

        // Borrow the header again. We had to drop the reference above to allow for the
        // early exit calling jwt.unverified_claims(), which takes ownership of the JWT.
        let signature_alg = match jwt.unverified_header().alg {
            // Encryption is handled above.
            JsonWebTokenAlgorithm::Encryption(_) => unreachable!(),
            JsonWebTokenAlgorithm::Signature(ref signature_alg, _) => signature_alg,
            // Section 2 of OpenID Connect Core 1.0 specifies that "ID Tokens MUST NOT use
            // none as the alg value unless the Response Type used returns no ID Token from
            // the Authorization Endpoint (such as when using the Authorization Code Flow)
            // and the Client explicitly requested the use of none at Registration time."
            //
            // While there's technically a use case where this is ok, we choose not to
            // support it for now to protect against accidental misuse. If demand arises,
            // we can figure out a API that mitigates the risk.
            JsonWebTokenAlgorithm::None => return Err(ClaimsVerificationError::NoSignature),
        }
        .clone();

        // 7. The alg value SHOULD be the default of RS256 or the algorithm sent by the Client
        //    in the id_token_signed_response_alg parameter during Registration.
        if let Some(ref allowed_algs) = self.allowed_algs {
            if !allowed_algs.contains(&signature_alg) {
                return Err(ClaimsVerificationError::SignatureVerification(
                    SignatureVerificationError::DisallowedAlg(format!(
                        "algorithm `{}` is not one of: {}",
                        variant_name(&signature_alg),
                        allowed_algs
                            .iter()
                            .map(variant_name)
                            .collect::<Vec<_>>()
                            .join(", "),
                    )),
                ));
            }
        }

        // NB: We must *not* trust the 'kid' (key ID) or 'alg' (algorithm) fields present in the
        // JOSE header, as an attacker could manipulate these while forging the JWT. The code
        // below must be secure regardless of how these fields are manipulated.

        if signature_alg.uses_shared_secret() {
            // 8. If the JWT alg Header Parameter uses a MAC based algorithm such as HS256,
            //    HS384, or HS512, the octets of the UTF-8 representation of the client_secret
            //    corresponding to the client_id contained in the aud (audience) Claim are used
            //    as the key to validate the signature. For MAC based algorithms, the behavior
            //    is unspecified if the aud is multi-valued or if an azp value is present that
            //    is different than the aud value.
            if let Some(ref client_secret) = self.client_secret {
                let key = K::new_symmetric(client_secret.secret().clone().into_bytes());
                return jwt
                    .payload(&signature_alg, &key)
                    .map_err(ClaimsVerificationError::SignatureVerification);
            } else {
                // The client secret isn't confidential for public clients, so anyone can forge a
                // JWT with a valid signature.
                return Err(ClaimsVerificationError::SignatureVerification(
                    SignatureVerificationError::DisallowedAlg(
                        "symmetric signatures are disallowed for public clients".to_string(),
                    ),
                ));
            }
        }

        // Section 10.1 of OpenID Connect Core 1.0 states that the JWT must include a key ID
        // if the JWK set contains more than one public key.

        // See if any key has a matching key ID (if supplied) and compatible type.
        let public_keys = {
            let jose_header = jwt.unverified_header();
            self.signature_keys
                .keys()
                .iter()
                .filter(|key|
                    // The key must be of the type expected for this signature algorithm.
                    Some(key.key_type()) == signature_alg.key_type().as_ref() &&
                        // Either the key hasn't specified it's allowed usage (in which case
                        // any usage is acceptable), or the key supports signing.
                        (key.key_use().is_none() ||
                            key.key_use().iter().any(
                                |key_use| key_use.allows_signature()
                            )) &&
                        // Either the JWT doesn't include a 'kid' (in which case any 'kid'
                        // is acceptable), or the 'kid' matches the key's ID.
                        (jose_header.kid.is_none() ||
                            jose_header.kid.as_ref() == key.key_id()))
                .collect::<Vec<&K>>()
        };
        if public_keys.is_empty() {
            return Err(ClaimsVerificationError::SignatureVerification(
                SignatureVerificationError::NoMatchingKey,
            ));
        } else if public_keys.len() != 1 {
            return Err(ClaimsVerificationError::SignatureVerification(
                SignatureVerificationError::AmbiguousKeyId(format!(
                    "JWK set must only contain one eligible public key \
                     ({} eligible keys: {})",
                    public_keys.len(),
                    public_keys
                        .iter()
                        .map(|key| format!(
                            "{} ({})",
                            key.key_id()
                                .map(|kid| format!("`{}`", **kid))
                                .unwrap_or_else(|| "null ID".to_string()),
                            variant_name(key.key_type())
                        ))
                        .collect::<Vec<_>>()
                        .join(", ")
                )),
            ));
        }

        jwt.payload(
            &signature_alg.clone(),
            *public_keys.first().expect("unreachable"),
        )
        .map_err(ClaimsVerificationError::SignatureVerification)

        // Steps 9--13 are specific to the ID token.
    }
}

///
/// Trait for verifying ID token nonces.
///
pub trait NonceVerifier {
    ///
    /// Verifies the nonce.
    ///
    /// Returns `Ok(())` if the nonce is valid, or a string describing the error otherwise.
    ///
    fn verify(self, nonce: Option<&Nonce>) -> Result<(), String>;
}

impl NonceVerifier for &Nonce {
    fn verify(self, nonce: Option<&Nonce>) -> Result<(), String> {
        if let Some(claims_nonce) = nonce {
            if verify_slices_are_equal(claims_nonce.secret().as_bytes(), self.secret().as_bytes())
                .is_err()
            {
                return Err("nonce mismatch".to_string());
            }
        } else {
            return Err("missing nonce claim".to_string());
        }
        Ok(())
    }
}

impl<F> NonceVerifier for F
where
    F: FnOnce(Option<&Nonce>) -> Result<(), String>,
{
    fn verify(self, nonce: Option<&Nonce>) -> Result<(), String> {
        self(nonce)
    }
}

///
/// ID token verifier.
///
#[derive(Clone)]
pub struct IdTokenVerifier<'a, JS, JT, JU, K>
where
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    JU: JsonWebKeyUse,
    K: JsonWebKey<JS, JT, JU>,
{
    acr_verifier_fn:
        Arc<dyn Fn(Option<&AuthenticationContextClass>) -> Result<(), String> + 'a + Send + Sync>,
    #[allow(clippy::type_complexity)]
    auth_time_verifier_fn:
        Arc<dyn Fn(Option<DateTime<Utc>>) -> Result<(), String> + 'a + Send + Sync>,
    iat_verifier_fn: Arc<dyn Fn(DateTime<Utc>) -> Result<(), String> + 'a + Send + Sync>,
    jwt_verifier: JwtClaimsVerifier<'a, JS, JT, JU, K>,
    time_fn: Arc<dyn Fn() -> DateTime<Utc> + 'a + Send + Sync>,
}
impl<'a, JS, JT, JU, K> IdTokenVerifier<'a, JS, JT, JU, K>
where
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    JU: JsonWebKeyUse,
    K: JsonWebKey<JS, JT, JU>,
{
    fn new(jwt_verifier: JwtClaimsVerifier<'a, JS, JT, JU, K>) -> Self {
        IdTokenVerifier {
            // By default, accept authorization context reference (acr claim).
            acr_verifier_fn: Arc::new(|_| Ok(())),
            auth_time_verifier_fn: Arc::new(|_| Ok(())),
            // By default, accept any issued time (iat claim).
            iat_verifier_fn: Arc::new(|_| Ok(())),
            jwt_verifier,
            // By default, use the current system time.
            time_fn: Arc::new(Utc::now),
        }
    }

    ///
    /// Initializes a new verifier for a public client (i.e., one without a client secret).
    ///
    pub fn new_public_client(
        client_id: ClientId,
        issuer: IssuerUrl,
        signature_keys: JsonWebKeySet<JS, JT, JU, K>,
    ) -> Self {
        Self::new(JwtClaimsVerifier::new(client_id, issuer, signature_keys))
    }

    ///
    /// Initializes a no-op verifier that performs no signature, audience, or issuer verification.
    /// The token's expiration time is still checked, and the token is otherwise required to conform to the expected format.
    ///
    pub fn new_insecure_without_verification() -> Self {
        let empty_issuer = IssuerUrl::new("https://0.0.0.0".to_owned())
            .expect("Creating empty issuer url mustn't fail");
        Self::new_public_client(
            ClientId::new(String::new()),
            empty_issuer,
            JsonWebKeySet::new(vec![]),
        )
        .insecure_disable_signature_check()
        .require_audience_match(false)
        .require_issuer_match(false)
    }

    ///
    /// Initializes a new verifier for a confidential client (i.e., one with a client secret).
    ///
    /// A confidential client verifier is required in order to verify ID tokens signed using a
    /// shared secret algorithm such as `HS256`, `HS384`, or `HS512`. For these algorithms, the
    /// client secret is the shared secret.
    ///
    pub fn new_confidential_client(
        client_id: ClientId,
        client_secret: ClientSecret,
        issuer: IssuerUrl,
        signature_keys: JsonWebKeySet<JS, JT, JU, K>,
    ) -> Self {
        Self::new(
            JwtClaimsVerifier::new(client_id, issuer, signature_keys)
                .set_client_secret(client_secret),
        )
    }

    ///
    /// Specifies which JSON Web Signature algorithms are supported.
    ///
    pub fn set_allowed_algs<I>(mut self, algs: I) -> Self
    where
        I: IntoIterator<Item = JS>,
    {
        self.jwt_verifier = self.jwt_verifier.set_allowed_algs(algs);
        self
    }

    ///
    /// Specifies that any signature algorithm is supported.
    ///
    pub fn allow_any_alg(mut self) -> Self {
        self.jwt_verifier = self.jwt_verifier.allow_any_alg();
        self
    }

    ///
    /// Specifies a function for verifying the `acr` claim.
    ///
    /// The function should return `Ok(())` if the claim is valid, or a string describing the error
    /// otherwise.
    ///
    pub fn set_auth_context_verifier_fn<T>(mut self, acr_verifier_fn: T) -> Self
    where
        T: Fn(Option<&AuthenticationContextClass>) -> Result<(), String> + 'a + Send + Sync,
    {
        self.acr_verifier_fn = Arc::new(acr_verifier_fn);
        self
    }

    ///
    /// Specifies a function for verifying the `auth_time` claim.
    ///
    /// The function should return `Ok(())` if the claim is valid, or a string describing the error
    /// otherwise.
    ///
    pub fn set_auth_time_verifier_fn<T>(mut self, auth_time_verifier_fn: T) -> Self
    where
        T: Fn(Option<DateTime<Utc>>) -> Result<(), String> + 'a + Send + Sync,
    {
        self.auth_time_verifier_fn = Arc::new(auth_time_verifier_fn);
        self
    }

    ///
    /// Enables signature verification.
    ///
    /// Signature verification is enabled by default, so this function is only useful if
    /// [`IdTokenVerifier::insecure_disable_signature_check`] was previously invoked.
    ///
    pub fn enable_signature_check(mut self) -> Self {
        self.jwt_verifier = self.jwt_verifier.require_signature_check(true);
        self
    }

    ///
    /// Disables signature verification.
    ///
    /// # Security Warning
    ///
    /// Unverified ID tokens may be subject to forgery. See [Section 16.3](
    /// https://openid.net/specs/openid-connect-core-1_0.html#TokenManufacture) for more
    /// information.
    ///
    pub fn insecure_disable_signature_check(mut self) -> Self {
        self.jwt_verifier = self.jwt_verifier.require_signature_check(false);
        self
    }

    ///
    /// Specifies whether the issuer claim must match the expected issuer URL for the provider.
    ///
    pub fn require_issuer_match(mut self, iss_required: bool) -> Self {
        self.jwt_verifier = self.jwt_verifier.require_issuer_match(iss_required);
        self
    }

    ///
    /// Specifies whether the audience claim must match this client's client ID.
    ///
    pub fn require_audience_match(mut self, aud_required: bool) -> Self {
        self.jwt_verifier = self.jwt_verifier.require_audience_match(aud_required);
        self
    }

    ///
    /// Specifies a function for returning the current time.
    ///
    /// This function is used for verifying the ID token expiration time.
    ///
    pub fn set_time_fn<T>(mut self, time_fn: T) -> Self
    where
        T: Fn() -> DateTime<Utc> + 'a + Send + Sync,
    {
        self.time_fn = Arc::new(time_fn);
        self
    }

    ///
    /// Specifies a function for verifying the ID token issue time.
    ///
    /// The function should return `Ok(())` if the claim is valid, or a string describing the error
    /// otherwise.
    ///
    pub fn set_issue_time_verifier_fn<T>(mut self, iat_verifier_fn: T) -> Self
    where
        T: Fn(DateTime<Utc>) -> Result<(), String> + 'a + Send + Sync,
    {
        self.iat_verifier_fn = Arc::new(iat_verifier_fn);
        self
    }

    ///
    /// Specifies a function for verifying audiences included in the `aud` claim that differ from
    /// this client's client ID.
    ///
    /// The function should return `true` if the audience is trusted, or `false` otherwise.
    ///
    /// [Section 3.1.3.7](https://openid.net/specs/openid-connect-core-1_0.html#IDTokenValidation)
    /// states that *"The ID Token MUST be rejected if the ID Token does not list the Client as a
    /// valid audience, or if it contains additional audiences not trusted by the Client."*
    ///
    pub fn set_other_audience_verifier_fn<T>(mut self, other_aud_verifier_fn: T) -> Self
    where
        T: Fn(&Audience) -> bool + 'a + Send + Sync,
    {
        self.jwt_verifier = self
            .jwt_verifier
            .set_other_audience_verifier_fn(other_aud_verifier_fn);
        self
    }

    pub(super) fn verified_claims<'b, AC, GC, JE, N>(
        &self,
        jwt: &'b JsonWebToken<JE, JS, JT, IdTokenClaims<AC, GC>, JsonWebTokenJsonPayloadSerde>,
        nonce_verifier: N,
    ) -> Result<&'b IdTokenClaims<AC, GC>, ClaimsVerificationError>
    where
        AC: AdditionalClaims,
        GC: GenderClaim,
        JE: JweContentEncryptionAlgorithm<JT>,
        N: NonceVerifier,
    {
        // The code below roughly follows the validation steps described in
        // https://openid.net/specs/openid-connect-core-1_0.html#IDTokenValidation

        // Steps 1--3 are handled by the generic JwtClaimsVerifier.
        let partially_verified_claims = self.jwt_verifier.verified_claims(jwt)?;

        self.verify_claims(partially_verified_claims, nonce_verifier)?;
        Ok(partially_verified_claims)
    }

    pub(super) fn verified_claims_owned<AC, GC, JE, N>(
        &self,
        jwt: JsonWebToken<JE, JS, JT, IdTokenClaims<AC, GC>, JsonWebTokenJsonPayloadSerde>,
        nonce_verifier: N,
    ) -> Result<IdTokenClaims<AC, GC>, ClaimsVerificationError>
    where
        AC: AdditionalClaims,
        GC: GenderClaim,
        JE: JweContentEncryptionAlgorithm<JT>,
        N: NonceVerifier,
    {
        // The code below roughly follows the validation steps described in
        // https://openid.net/specs/openid-connect-core-1_0.html#IDTokenValidation

        // Steps 1--3 are handled by the generic JwtClaimsVerifier.
        let partially_verified_claims = self.jwt_verifier.verified_claims(jwt)?;

        self.verify_claims(&partially_verified_claims, nonce_verifier)?;
        Ok(partially_verified_claims)
    }

    fn verify_claims<AC, GC, N>(
        &self,
        partially_verified_claims: &'_ IdTokenClaims<AC, GC>,
        nonce_verifier: N,
    ) -> Result<(), ClaimsVerificationError>
    where
        AC: AdditionalClaims,
        GC: GenderClaim,
        N: NonceVerifier,
    {
        // 4. If the ID Token contains multiple audiences, the Client SHOULD verify that an azp
        //    Claim is present.

        // There is significant confusion and contradiction in the OpenID Connect Core spec around
        // the azp claim. See https://bitbucket.org/openid/connect/issues/973/ for a detailed
        // discussion. Given the lack of clarity around how this claim should be used, we defer
        // any verification of it here until a use case becomes apparent. If such a use case does
        // arise, we most likely want to allow clients to pass in a function for validating the
        // azp claim rather than introducing logic that affects all clients of this library.

        // This naive implementation of the spec would almost certainly not be useful in practice:
        /*
        let azp_required = partially_verified_claims.audiences().len() > 1;

        // 5. If an azp (authorized party) Claim is present, the Client SHOULD verify that its
        //    client_id is the Claim Value.
        if let Some(authorized_party) = partially_verified_claims.authorized_party() {
            if *authorized_party != self.client_id {
                return Err(ClaimsVerificationError::InvalidAudience(format!(
                    "authorized party must match client ID `{}` (found `{}`",
                    *self.client_id, **authorized_party
                )));
            }
        } else if azp_required {
            return Err(ClaimsVerificationError::InvalidAudience(format!(
                "missing authorized party claim but multiple audiences found"
            )));
        }
        */

        // Steps 6--8 are handled by the generic JwtClaimsVerifier.

        // 9. The current time MUST be before the time represented by the exp Claim.
        let cur_time = (*self.time_fn)();
        if cur_time >= partially_verified_claims.expiration() {
            return Err(ClaimsVerificationError::Expired(format!(
                "ID token expired at {} (current time is {})",
                partially_verified_claims.expiration(),
                cur_time
            )));
        }

        // 10. The iat Claim can be used to reject tokens that were issued too far away from the
        //     current time, limiting the amount of time that nonces need to be stored to prevent
        //     attacks. The acceptable range is Client specific.
        (*self.iat_verifier_fn)(partially_verified_claims.issue_time())
            .map_err(ClaimsVerificationError::Expired)?;

        // 11. If a nonce value was sent in the Authentication Request, a nonce Claim MUST be
        //     present and its value checked to verify that it is the same value as the one that was
        //     sent in the Authentication Request. The Client SHOULD check the nonce value for
        //     replay attacks. The precise method for detecting replay attacks is Client specific.
        nonce_verifier
            .verify(partially_verified_claims.nonce())
            .map_err(ClaimsVerificationError::InvalidNonce)?;

        // 12. If the acr Claim was requested, the Client SHOULD check that the asserted Claim Value
        //     is appropriate. The meaning and processing of acr Claim Values is out of scope for
        //     this specification.
        (*self.acr_verifier_fn)(partially_verified_claims.auth_context_ref())
            .map_err(ClaimsVerificationError::InvalidAuthContext)?;

        // 13. If the auth_time Claim was requested, either through a specific request for this
        //     Claim or by using the max_age parameter, the Client SHOULD check the auth_time Claim
        //     value and request re-authentication if it determines too much time has elapsed since
        //     the last End-User authentication.
        (*self.auth_time_verifier_fn)(partially_verified_claims.auth_time())
            .map_err(ClaimsVerificationError::InvalidAuthTime)?;

        Ok(())
    }
}

///
/// User info verifier.
///
#[derive(Clone)]
pub struct UserInfoVerifier<'a, JE, JS, JT, JU, K>
where
    JE: JweContentEncryptionAlgorithm<JT>,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    JU: JsonWebKeyUse,
    K: JsonWebKey<JS, JT, JU>,
{
    jwt_verifier: JwtClaimsVerifier<'a, JS, JT, JU, K>,
    expected_subject: Option<SubjectIdentifier>,
    _phantom: PhantomData<JE>,
}
impl<'a, JE, JS, JT, JU, K> UserInfoVerifier<'a, JE, JS, JT, JU, K>
where
    JE: JweContentEncryptionAlgorithm<JT>,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    JU: JsonWebKeyUse,
    K: JsonWebKey<JS, JT, JU>,
{
    ///
    /// Instantiates a user info verifier.
    ///
    pub fn new(
        client_id: ClientId,
        issuer: IssuerUrl,
        signature_keys: JsonWebKeySet<JS, JT, JU, K>,
        expected_subject: Option<SubjectIdentifier>,
    ) -> Self {
        UserInfoVerifier {
            jwt_verifier: JwtClaimsVerifier::new(client_id, issuer, signature_keys),
            expected_subject,
            _phantom: PhantomData,
        }
    }

    pub(crate) fn expected_subject(&self) -> Option<&SubjectIdentifier> {
        self.expected_subject.as_ref()
    }

    ///
    /// Specifies whether the issuer claim must match the expected issuer URL for the provider.
    ///
    pub fn require_issuer_match(mut self, iss_required: bool) -> Self {
        self.jwt_verifier = self.jwt_verifier.require_issuer_match(iss_required);
        self
    }

    ///
    /// Specifies whether the audience claim must match this client's client ID.
    ///
    pub fn require_audience_match(mut self, aud_required: bool) -> Self {
        self.jwt_verifier = self.jwt_verifier.require_audience_match(aud_required);
        self
    }

    pub(crate) fn verified_claims<AC, GC>(
        &self,
        user_info_jwt: JsonWebToken<
            JE,
            JS,
            JT,
            UserInfoClaimsImpl<AC, GC>,
            JsonWebTokenJsonPayloadSerde,
        >,
    ) -> Result<UserInfoClaimsImpl<AC, GC>, ClaimsVerificationError>
    where
        AC: AdditionalClaims,
        GC: GenderClaim,
    {
        let user_info = self.jwt_verifier.verified_claims(user_info_jwt)?;
        if self
            .expected_subject
            .iter()
            .all(|expected_subject| user_info.standard_claims.sub == *expected_subject)
        {
            Ok(user_info)
        } else {
            Err(ClaimsVerificationError::InvalidSubject(format!(
                "expected `{}` (found `{}`)",
                // This can only happen when self.expected_subject is not None.
                self.expected_subject.as_ref().unwrap().as_str(),
                user_info.standard_claims.sub.as_str()
            )))
        }
    }
}

#[cfg(test)]
mod tests {
    use std::sync::atomic::{AtomicBool, AtomicUsize, Ordering};

    use chrono::{TimeZone, Utc};
    use oauth2::{ClientId, ClientSecret};

    use super::{
        AudiencesClaim, ClaimsVerificationError, IssuerClaim, JsonWebTokenHeader,
        JwtClaimsVerifier, SignatureVerificationError, SubjectIdentifier,
    };
    use crate::core::{
        CoreIdToken, CoreIdTokenClaims, CoreIdTokenVerifier, CoreJsonWebKey, CoreJsonWebKeySet,
        CoreJsonWebKeyType, CoreJsonWebKeyUse, CoreJweContentEncryptionAlgorithm,
        CoreJwsSigningAlgorithm, CoreRsaPrivateSigningKey, CoreUserInfoClaims,
        CoreUserInfoJsonWebToken, CoreUserInfoVerifier,
    };
    use crate::jwt::tests::{TEST_RSA_PRIV_KEY, TEST_RSA_PUB_KEY};
    use crate::jwt::{JsonWebToken, JsonWebTokenJsonPayloadSerde};
    use crate::types::helpers::timestamp_to_utc;
    use crate::types::Base64UrlEncodedBytes;
    use crate::types::Timestamp;
    use crate::{
        AccessToken, Audience, AuthenticationContextClass, AuthorizationCode, EndUserName,
        IssuerUrl, JsonWebKeyId, Nonce, StandardClaims, UserInfoError,
    };

    type CoreJsonWebTokenHeader = JsonWebTokenHeader<
        CoreJweContentEncryptionAlgorithm,
        CoreJwsSigningAlgorithm,
        CoreJsonWebKeyType,
    >;

    type CoreJwtClaimsVerifier<'a> = JwtClaimsVerifier<
        'a,
        CoreJwsSigningAlgorithm,
        CoreJsonWebKeyType,
        CoreJsonWebKeyUse,
        CoreJsonWebKey,
    >;

    fn assert_unsupported<T>(result: Result<T, ClaimsVerificationError>, expected_substr: &str) {
        match result {
            Err(ClaimsVerificationError::Unsupported(msg)) => {
                assert!(msg.contains(expected_substr))
            }
            Err(err) => panic!("unexpected error: {:?}", err),
            Ok(_) => panic!("validation should fail"),
        }
    }

    #[test]
    fn test_jose_header() {
        // Unexpected JWT type.
        assert_unsupported(
            CoreJwtClaimsVerifier::validate_jose_header(
                &serde_json::from_str::<CoreJsonWebTokenHeader>(
                    "{\"alg\":\"RS256\",\"typ\":\"NOT_A_JWT\"}",
                )
                .expect("failed to deserialize"),
            ),
            "unsupported JWT type",
        );

        // Nested JWTs.
        assert_unsupported(
            CoreJwtClaimsVerifier::validate_jose_header(
                &serde_json::from_str::<CoreJsonWebTokenHeader>(
                    "{\"alg\":\"RS256\",\"cty\":\"JWT\"}",
                )
                .expect("failed to deserialize"),
            ),
            "nested JWT",
        );
        assert_unsupported(
            CoreJwtClaimsVerifier::validate_jose_header(
                &serde_json::from_str::<CoreJsonWebTokenHeader>(
                    "{\"alg\":\"RS256\",\"cty\":\"NOT_A_JWT\"}",
                )
                .expect("failed to deserialize"),
            ),
            "unsupported JWT content type",
        );

        // Critical fields. Adapted from https://tools.ietf.org/html/rfc7515#appendix-E
        assert_unsupported(
            CoreJwtClaimsVerifier::validate_jose_header(
                &serde_json::from_str::<CoreJsonWebTokenHeader>(
                    "{\
                     \"alg\":\"RS256\",\
                     \"crit\":[\"http://example.invalid/UNDEFINED\"],\
                     \"http://example.invalid/UNDEFINED\":true\
                     }",
                )
                .expect("failed to deserialize"),
            ),
            "critical JWT header fields are unsupported",
        );
    }

    #[derive(Clone, Debug, Deserialize, PartialEq, Serialize)]
    struct TestClaims {
        aud: Option<Vec<Audience>>,
        iss: Option<IssuerUrl>,
        payload: String,
    }
    impl AudiencesClaim for TestClaims {
        fn audiences(&self) -> Option<&Vec<Audience>> {
            self.aud.as_ref()
        }
    }
    impl IssuerClaim for TestClaims {
        fn issuer(&self) -> Option<&IssuerUrl> {
            self.iss.as_ref()
        }
    }
    type TestClaimsJsonWebToken = JsonWebToken<
        CoreJweContentEncryptionAlgorithm,
        CoreJwsSigningAlgorithm,
        CoreJsonWebKeyType,
        TestClaims,
        JsonWebTokenJsonPayloadSerde,
    >;

    #[test]
    fn test_jwt_verified_claims() {
        let rsa_key = serde_json::from_str::<CoreJsonWebKey>(TEST_RSA_PUB_KEY)
            .expect("deserialization failed");

        let client_id = ClientId::new("my_client".to_string());
        let issuer = IssuerUrl::new("https://example.com".to_string()).unwrap();
        let verifier = CoreJwtClaimsVerifier::new(
            client_id.clone(),
            issuer.clone(),
            CoreJsonWebKeySet::new(vec![rsa_key.clone()]),
        );

        // Invalid JOSE header.
        assert_unsupported(
            verifier.verified_claims(
                serde_json::from_value::<TestClaimsJsonWebToken>(serde_json::Value::String(
                    "eyJhbGciOiJBMjU2R0NNIiwiY3R5IjoiSldUIn0.eyJhdWQiOlsibXlfY2xpZW50Il0sImlzcyI6Im\
                     h0dHBzOi8vZXhhbXBsZS5jb20iLCJwYXlsb2FkIjoiaGVsbG8gd29ybGQifQ.YmFkX2hhc2g"
                        .to_string(),
                )).expect("failed to deserialize"),
            ),
            "nested JWT",
        );

        // JWE-encrypted JWT.
        assert_unsupported(
            verifier.verified_claims(
                serde_json::from_value::<TestClaimsJsonWebToken>(serde_json::Value::String(
                    "eyJhbGciOiJBMjU2R0NNIn0.eyJhdWQiOlsibXlfY2xpZW50Il0sImlzcyI6Imh0dHBzOi8vZXhhbX\
                     BsZS5jb20iLCJwYXlsb2FkIjoiaGVsbG8gd29ybGQifQ.YmFkX2hhc2g"
                        .to_string(),
                )).expect("failed to deserialize"),
            ),
            "JWE encryption",
        );

        // Wrong issuer.
        match verifier.verified_claims(
            serde_json::from_value::<TestClaimsJsonWebToken>(serde_json::Value::String(
                "eyJhbGciOiJSUzI1NiJ9.eyJhdWQiOlsibXlfY2xpZW50Il0sImlzcyI6Imh0dHBzOi8vYXR0YWNrZXIuY\
                 29tIiwicGF5bG9hZCI6ImhlbGxvIHdvcmxkIn0.YmFkX2hhc2g"
                    .to_string(),
            )).expect("failed to deserialize"),
        ) {
            Err(ClaimsVerificationError::InvalidIssuer(_)) => {},
            other => panic!("unexpected result: {:?}", other),
        }

        // Missing issuer.
        match verifier.verified_claims(
            serde_json::from_value::<TestClaimsJsonWebToken>(serde_json::Value::String(
                "eyJhbGciOiJSUzI1NiJ9.eyJhdWQiOlsibXlfY2xpZW50Il0sInBheWxvYWQiOiJoZWxsbyB3b3JsZCJ9.\
                 YmFkX2hhc2g"
                    .to_string(),
            )).expect("failed to deserialize"),
        ) {
            Err(ClaimsVerificationError::InvalidIssuer(_)) => {},
            other => panic!("unexpected result: {:?}", other),
        }

        // Ignore missing issuer.
        verifier
            .clone()
            .require_issuer_match(false)
            .verified_claims(
            serde_json::from_value::<TestClaimsJsonWebToken>(serde_json::Value::String(
                "eyJhbGciOiJSUzI1NiJ9.eyJhdWQiOlsibXlfY2xpZW50Il0sInBheWxvYWQiOiJoZWxsbyB3b3JsZCJ9.\
                 nv09al63NNDfb8cF3IozegXKbPaUC08zknRPKmQ5qKgXv80hjVxknkpRz7BxocB3JYTBjhYd0gyN9wAuJj\
                 byZ1QaUC14HOB83awAGbehy5yFLkLadTfPT7-siBCvE2V7AF73a_21YvwdkKmJ-RaKWHzFnG8CDmioma3X\
                 cWyrsdRLgvUkrWllajLRo8DCIXQ8OuZo1_o4n17PSlPxSkhKIrgaWCvG6tan40Y_1DZOFv47bx4hQUGd-J\
                 h2aEjiwn65WV3M_Xb2vQMP7VgYNVaNlfxzpL4yDASItbPMWaXBt3ZUa_IOGoSx2GMnPkrQ4xp56qUth6U7\
                 esWPqRSqqolnHg"
                    .to_string(),
            )).expect("failed to deserialize"),
        ).expect("verification should succeed");

        // Wrong audience.
        match verifier.verified_claims(
            serde_json::from_value::<TestClaimsJsonWebToken>(serde_json::Value::String(
                "eyJhbGciOiJSUzI1NiJ9.eyJhdWQiOlsib3RoZXJfY2xpZW50Il0sImlzcyI6Imh0dHBzOi8vZXhhbXBsZ\
                 S5jb20iLCJwYXlsb2FkIjoiaGVsbG8gd29ybGQifQ.YmFkX2hhc2g"
                    .to_string(),
            )).expect("failed to deserialize"),
        ) {
            Err(ClaimsVerificationError::InvalidAudience(_)) => {},
            other => panic!("unexpected result: {:?}", other),
        }

        // Missing audience.
        match verifier.verified_claims(
            serde_json::from_value::<TestClaimsJsonWebToken>(serde_json::Value::String(
                "eyJhbGciOiJSUzI1NiJ9.eyJpc3MiOiJodHRwczovL2V4YW1wbGUuY29tIiwicGF5bG9hZCI6ImhlbGxvI\
                 HdvcmxkIn0.YmFkX2hhc2g"
                    .to_string(),
            )).expect("failed to deserialize"),
        ) {
            Err(ClaimsVerificationError::InvalidAudience(_)) => {},
            other => panic!("unexpected result: {:?}", other),
        }

        // Ignore missing audience.
        verifier
            .clone()
            .require_audience_match(false)
            .verified_claims(
                serde_json::from_value::<TestClaimsJsonWebToken>(serde_json::Value::String(
                    "eyJhbGciOiJSUzI1NiJ9.eyJpc3MiOiJodHRwczovL2V4YW1wbGUuY29tIiwicGF5bG9hZCI6Imhlb\
                     GxvIHdvcmxkIn0.lP-Z_zGPNoKIbLQsnrZc2LAc5qJrKyb7t07ZtJUKVhcwHiCUou4bBhq5RHlElCh\
                     0ElRRP6I25lp6UszkRvIC46UV3GVze0x73kVkHSvCVI7MO75LbL9BRqrm5b4CN2zCiFBY8-EwTXnJd\
                     Ri0d_U8K29TV24L2I-Z5ZILebwUue1N59AGDjx2yYLFx5NOw3TUsPyscG62aZAT321pL_jcYwTWTWw\
                     2FYm07zguwx-PUTZwGXlJiOgXQqRIbY_1bS3I_D8UWsmEB3DmV0f9z-iklgIPFawa4wHaE-hpzBAEx\
                     pSieyOavA5pl0Se3XRYA-CkdDVgzG0Pt4IdnxFanfUXTw"
                        .to_string(),
                )).expect("failed to deserialize"),
            ).expect("verification should succeed");

        // Multiple audiences, where one is a match (default = reject)
        match verifier.verified_claims(
            serde_json::from_value::<TestClaimsJsonWebToken>(serde_json::Value::String(
                "eyJhbGciOiJSUzI1NiJ9.eyJhdWQiOlsiYXVkMSIsIm15X2NsaWVudCIsImF1ZDIiXSwiaXNzIjoia\
                 HR0cHM6Ly9leGFtcGxlLmNvbSIsInBheWxvYWQiOiJoZWxsbyB3b3JsZCJ9.N9ibisEe0kKLe1GDWM\
                 ON3PmYqbL73dag-loM8pjKJNinF9SB7n4JuSu4FrNkeW4F1Cz8MIbLuWfKvDa_4v_3FstMA3GODZWH\
                 BVIiuNFay2ovCfGFyykwe47dF_47g_OM5AkJc_teE5MN8lPh9V5zYCy3ON3zZ3acFPJMOPTdbU56xD\
                 eFe7lil6DmV4JU9A52t5ZkJILFaIuxxXJUIDmqpPTvHkggh_QOj9C2US9bgg5b543JwT4j-HbDp51L\
                 dDB4k3azOssT1ddtoAuuDOctnraMKUtqffJXexxfwA1uM6EIofSrK5v11xwgTciL9xDXAvav_G2buP\
                 ol1bjGLa2t0Q"
                    .to_string(),
            ))
            .expect("failed to deserialize"),
        ) {
            Err(ClaimsVerificationError::InvalidAudience(_)) => {}
            other => panic!("unexpected result: {:?}", other),
        }

        // Multiple audiences, where one is a match (allowed)
        verifier
            .clone()
            .set_other_audience_verifier_fn(|aud| **aud == "aud1" || **aud == "aud2")
            .verified_claims(
            serde_json::from_value::<TestClaimsJsonWebToken>(serde_json::Value::String(
                "eyJhbGciOiJSUzI1NiJ9.eyJhdWQiOlsiYXVkMSIsIm15X2NsaWVudCIsImF1ZDIiXSwiaXNzIjoia\
                 HR0cHM6Ly9leGFtcGxlLmNvbSIsInBheWxvYWQiOiJoZWxsbyB3b3JsZCJ9.N9ibisEe0kKLe1GDWM\
                 ON3PmYqbL73dag-loM8pjKJNinF9SB7n4JuSu4FrNkeW4F1Cz8MIbLuWfKvDa_4v_3FstMA3GODZWH\
                 BVIiuNFay2ovCfGFyykwe47dF_47g_OM5AkJc_teE5MN8lPh9V5zYCy3ON3zZ3acFPJMOPTdbU56xD\
                 eFe7lil6DmV4JU9A52t5ZkJILFaIuxxXJUIDmqpPTvHkggh_QOj9C2US9bgg5b543JwT4j-HbDp51L\
                 dDB4k3azOssT1ddtoAuuDOctnraMKUtqffJXexxfwA1uM6EIofSrK5v11xwgTciL9xDXAvav_G2buP\
                 ol1bjGLa2t0Q"
                    .to_string(),
            )).expect("failed to deserialize"),
        ).expect("verification should succeed");

        // Multiple audiences, where none is a match
        match verifier.verified_claims(
            serde_json::from_value::<TestClaimsJsonWebToken>(serde_json::Value::String(
                "eyJhbGciOiJSUzI1NiJ9.eyJhdWQiOlsiYXVkMSIsImF1ZDIiXSwiaXNzIjoiaHR0cHM6Ly9leGFtcGxlL\
                 mNvbSIsInBheWxvYWQiOiJoZWxsbyB3b3JsZCJ9.YmFkX2hhc2g"
                    .to_string(),
            )).expect("failed to deserialize"),
        ) {
            Err(ClaimsVerificationError::InvalidAudience(_)) => {},
            other => panic!("unexpected result: {:?}", other),
        }

        // Disable signature check.
        verifier
            .clone()
            .require_signature_check(false)
            .verified_claims(
                serde_json::from_value::<TestClaimsJsonWebToken>(serde_json::Value::String(
                    "eyJhbGciOiJSUzI1NiJ9.eyJhdWQiOlsibXlfY2xpZW50Il0sImlzcyI6Imh0dHBzOi8vZXhhbXBsZ\
                     S5jb20iLCJwYXlsb2FkIjoiaGVsbG8gd29ybGQifQ.YmFkX2hhc2g"
                        .to_string(),
                )).expect("failed to deserialize"),
            ).expect("verification should succeed");

        // "none" algorithm (unsigned JWT).
        match verifier.verified_claims(
            serde_json::from_value::<TestClaimsJsonWebToken>(serde_json::Value::String(
                "eyJhbGciOiJub25lIn0.eyJhdWQiOlsibXlfY2xpZW50Il0sImlzcyI6Imh0dHBzOi8vZXhhbXBsZ\
                 S5jb20iLCJwYXlsb2FkIjoiaGVsbG8gd29ybGQifQ."
                    .to_string(),
            ))
            .expect("failed to deserialize"),
        ) {
            Err(ClaimsVerificationError::NoSignature) => {}
            other => panic!("unexpected result: {:?}", other),
        }

        let valid_rs256_jwt =
            serde_json::from_value::<TestClaimsJsonWebToken>(serde_json::Value::String(
                "eyJhbGciOiJSUzI1NiJ9.eyJhdWQiOlsibXlfY2xpZW50Il0sImlzcyI6Imh0dHBzOi8vZXhhbXBsZ\
                 S5jb20iLCJwYXlsb2FkIjoiaGVsbG8gd29ybGQifQ.UZ7vmAsDmOBzeB6e2_0POUfyhMRZKM6WSKz3\
                 jB2QdmO-eZ9605EzhkJufJQ8515ryWnHv-gUHtZHQi3zilrzhBwvE2cVP83Gv2XIL1EKaMMmfISeEB\
                 ShWez_FvqxN_bamh5yTROhWmoZTmof-MweBCHgINcsEd7K4e_BHHgq3aaRBpvSFlL_z4l_1NwNcTBo\
                 kqjNScKZITk42AbsSuGR39L94BWLhz6WXQZ_Sn6R1Ro6roOm1b7E82jJiQEtlseQiCCvPR2JJ6LgW6\
                 XTMzQ0vCqSh1A7U_IBDsjY_yag8_X3xxFh2URCtHJ47ZSjqfv6hq7OAq8tmVecOVgfIvABOg"
                    .to_string(),
            ))
            .expect("failed to deserialize");
        // Default algs + RS256 -> allowed
        verifier
            .verified_claims(valid_rs256_jwt.clone())
            .expect("verification should succeed");

        let verifier_with_client_secret = CoreJwtClaimsVerifier::new(
            client_id.clone(),
            issuer.clone(),
            CoreJsonWebKeySet::new(vec![]),
        )
        .set_client_secret(ClientSecret::new("my_secret".to_string()));
        let valid_hs256_jwt =
            serde_json::from_value::<TestClaimsJsonWebToken>(serde_json::Value::String(
                "eyJhbGciOiJIUzI1NiJ9.eyJhdWQiOlsibXlfY2xpZW50Il0sImlzcyI6Imh0dHBzOi8vZXhhbXBsZ\
                 S5jb20iLCJwYXlsb2FkIjoiaGVsbG8gd29ybGQifQ.dTXvSWen74_rC4oiWw0ziLZNe4KZk8Jw2VZe\
                 N6vLCDo"
                    .to_string(),
            ))
            .expect("failed to deserialize");

        // Default algs + HS256 -> disallowed
        match verifier_with_client_secret.verified_claims(valid_hs256_jwt.clone()) {
            Err(ClaimsVerificationError::SignatureVerification(
                SignatureVerificationError::DisallowedAlg(_),
            )) => {}
            other => panic!("unexpected result: {:?}", other),
        }

        // none algs + RS256 -> allowed
        verifier
            .clone()
            .allow_any_alg()
            .verified_claims(valid_rs256_jwt.clone())
            .expect("verification should succeed");

        // none algs + HS256 -> allowed
        verifier_with_client_secret
            .clone()
            .allow_any_alg()
            .verified_claims(valid_hs256_jwt.clone())
            .expect("verification should succeed");

        // none algs + none -> disallowed
        match verifier.clone().allow_any_alg().verified_claims(
            serde_json::from_value::<TestClaimsJsonWebToken>(serde_json::Value::String(
                "eyJhbGciOiJub25lIn0.eyJhdWQiOlsibXlfY2xpZW50Il0sImlzcyI6Imh0dHBzOi8vZXhhbXBsZ\
                 S5jb20iLCJwYXlsb2FkIjoiaGVsbG8gd29ybGQifQ."
                    .to_string(),
            ))
            .expect("failed to deserialize"),
        ) {
            Err(ClaimsVerificationError::NoSignature) => {}
            other => panic!("unexpected result: {:?}", other),
        }

        // HS256 + no client secret -> disallowed
        match verifier
            .clone()
            .allow_any_alg()
            .verified_claims(valid_hs256_jwt.clone())
        {
            Err(ClaimsVerificationError::SignatureVerification(
                SignatureVerificationError::DisallowedAlg(_),
            )) => {}
            other => panic!("unexpected result: {:?}", other),
        }

        // HS256 + valid signature
        verifier_with_client_secret
            .clone()
            .set_allowed_algs(vec![CoreJwsSigningAlgorithm::HmacSha256])
            .verified_claims(valid_hs256_jwt)
            .expect("verification should succeed");

        // HS256 + invalid signature
        match verifier_with_client_secret
            .clone()
            .set_allowed_algs(vec![CoreJwsSigningAlgorithm::HmacSha256])
            .verified_claims(
                serde_json::from_value::<TestClaimsJsonWebToken>(serde_json::Value::String(
                    "eyJhbGciOiJIUzI1NiJ9.eyJhdWQiOlsibXlfY2xpZW50Il0sImlzcyI6Imh0dHBzOi8vZXhhbXBsZ\
                     S5jb20iLCJwYXlsb2FkIjoiaGVsbG8gd29ybGQifQ.dTXvSWen74_rC4oiWw0ziLZNe4KZk8Jw2VZe\
                     N6vLCEo"
                        .to_string(),
                )).expect("failed to deserialize")
            )
        {
            Err(ClaimsVerificationError::SignatureVerification(
                    SignatureVerificationError::CryptoError(_),
                )) => {}
            other => panic!("unexpected result: {:?}", other),
        }

        // No public keys
        match CoreJwtClaimsVerifier::new(
            client_id.clone(),
            issuer.clone(),
            CoreJsonWebKeySet::new(vec![]),
        )
        .verified_claims(valid_rs256_jwt.clone())
        {
            Err(ClaimsVerificationError::SignatureVerification(
                SignatureVerificationError::NoMatchingKey,
            )) => {}
            other => panic!("unexpected result: {:?}", other),
        }

        let kid = JsonWebKeyId::new("bilbo.baggins@hobbiton.example".to_string());
        let n = Base64UrlEncodedBytes::new(vec![
            159, 129, 15, 180, 3, 130, 115, 208, 37, 145, 228, 7, 63, 49, 210, 182, 0, 27, 130,
            206, 219, 77, 146, 240, 80, 22, 93, 71, 207, 202, 184, 163, 196, 28, 183, 120, 172,
            117, 83, 121, 63, 142, 249, 117, 118, 141, 26, 35, 116, 216, 113, 37, 100, 195, 188,
            215, 123, 158, 164, 52, 84, 72, 153, 64, 124, 255, 0, 153, 146, 10, 147, 26, 36, 196,
            65, 72, 82, 171, 41, 189, 176, 169, 92, 6, 83, 243, 108, 96, 230, 11, 249, 11, 98, 88,
            221, 165, 111, 55, 4, 123, 165, 194, 209, 208, 41, 175, 156, 157, 64, 186, 199, 170,
            65, 199, 138, 13, 209, 6, 138, 221, 105, 158, 128, 143, 234, 1, 30, 161, 68, 29, 138,
            79, 123, 180, 233, 123, 227, 159, 85, 241, 221, 212, 78, 156, 75, 163, 53, 21, 151, 3,
            212, 211, 75, 96, 62, 101, 20, 122, 79, 35, 214, 211, 192, 153, 108, 117, 237, 238,
            132, 106, 130, 209, 144, 174, 16, 120, 60, 150, 28, 240, 56, 122, 237, 33, 6, 210, 208,
            85, 91, 111, 217, 55, 250, 213, 83, 83, 135, 224, 255, 114, 255, 190, 120, 148, 20, 2,
            176, 184, 34, 234, 42, 116, 182, 5, 140, 29, 171, 249, 179, 74, 118, 203, 99, 184, 127,
            170, 44, 104, 71, 184, 226, 131, 127, 255, 145, 24, 110, 107, 28, 20, 145, 28, 249,
            137, 168, 144, 146, 168, 28, 230, 1, 221, 172, 211, 249, 207,
        ]);
        let e = Base64UrlEncodedBytes::new(vec![1, 0, 1]);

        // Wrong key type (symmetric key)
        match CoreJwtClaimsVerifier::new(
            client_id.clone(),
            issuer.clone(),
            CoreJsonWebKeySet::new(vec![CoreJsonWebKey {
                kty: CoreJsonWebKeyType::Symmetric,
                use_: Some(CoreJsonWebKeyUse::Signature),
                kid: Some(kid.clone()),
                n: None,
                e: None,
                k: Some(Base64UrlEncodedBytes::new(vec![1, 2, 3, 4])),
                crv: None,
                x: None,
                y: None,
                d: None,
            }]),
        )
        .verified_claims(valid_rs256_jwt.clone())
        {
            Err(ClaimsVerificationError::SignatureVerification(
                SignatureVerificationError::NoMatchingKey,
            )) => {}
            other => panic!("unexpected result: {:?}", other),
        }

        // Correct public key, but with signing disallowed
        match CoreJwtClaimsVerifier::new(
            client_id.clone(),
            issuer.clone(),
            CoreJsonWebKeySet::new(vec![CoreJsonWebKey {
                kty: CoreJsonWebKeyType::RSA,
                use_: Some(CoreJsonWebKeyUse::Encryption),
                kid: Some(kid),
                n: Some(n),
                e: Some(e),
                k: None,
                crv: None,
                x: None,
                y: None,
                d: None,
            }]),
        )
        .verified_claims(valid_rs256_jwt.clone())
        {
            Err(ClaimsVerificationError::SignatureVerification(
                SignatureVerificationError::NoMatchingKey,
            )) => {}
            other => panic!("unexpected result: {:?}", other),
        }

        // Wrong key ID
        match verifier.verified_claims(
            serde_json::from_value::<TestClaimsJsonWebToken>(serde_json::Value::String(
                "eyJhbGciOiJSUzI1NiIsImtpZCI6Indyb25nX2tleSJ9.eyJhdWQiOlsibXlfY2xpZW50Il0sImlzcyI6I\
                 mh0dHBzOi8vZXhhbXBsZS5jb20iLCJwYXlsb2FkIjoiaGVsbG8gd29ybGQifQ.lVLomyIyO8WmyS1VZWPu\
                 cGhRTUyK9RCw90fJC5CfDWUCgt1CBn-aP_ieWWBGfjb4ccR4dl57OYxdLl0Day8QN5pTCBud9QKpQ0rKQX\
                 K8eBlOW8uSosx8q5pwU_bRyy-XuKJiPlDCOwTEHOp_hOgZFGjoN27MH3Xm8kc0iT3PgyqQ46-wsqHY9S02\
                 hdJORX7vqYwQLZF8_k_L8K0IG_dC-1Co0g5oAf37oVSdl8hE-ScQ9K-AiSpS-cGYyldbMhyKNDL3ry2cuI\
                 EUgYSIznkVFuM7RrEdNK222z5PF11ijYx-TM7BIDggbcIyJm-UqpmvVaJImmj5FNkMzuHYznLtdg"
                    .to_string(),
            )).expect("failed to deserialize")
        ) {
            Err(ClaimsVerificationError::SignatureVerification(
                    SignatureVerificationError::NoMatchingKey,
                )) => {}
            other => panic!("unexpected result: {:?}", other),
        }

        // Client secret + public key
        verifier
            .clone()
            .set_client_secret(ClientSecret::new("my_secret".to_string()))
            .verified_claims(valid_rs256_jwt.clone())
            .expect("verification should succeed");

        // Multiple matching public keys: no KID specified
        match CoreJwtClaimsVerifier::new(
            client_id.clone(),
            issuer.clone(),
            CoreJsonWebKeySet::new(vec![rsa_key.clone(), rsa_key.clone()]),
        )
        .verified_claims(valid_rs256_jwt.clone())
        {
            Err(ClaimsVerificationError::SignatureVerification(
                SignatureVerificationError::AmbiguousKeyId(_),
            )) => {}
            other => panic!("unexpected result: {:?}", other),
        }

        // Multiple matching public keys: KID specified
        match CoreJwtClaimsVerifier::new(
            client_id,
            issuer,
            CoreJsonWebKeySet::new(vec![rsa_key.clone(), rsa_key]),
        ).verified_claims(
            serde_json::from_value::<TestClaimsJsonWebToken>(serde_json::Value::String(
                "eyJhbGciOiJSUzI1NiIsImtpZCI6ImJpbGJvLmJhZ2dpbnNAaG9iYml0b24uZXhhbXBsZSJ9.eyJhdWQiO\
                 lsibXlfY2xpZW50Il0sImlzcyI6Imh0dHBzOi8vZXhhbXBsZS5jb20iLCJwYXlsb2FkIjoiaGVsbG8gd29\
                 ybGQifQ.jH0v2fQGvH2MD0jn5pQP6W6AF5rJlizyofdyRUIt7E3GraGA1LYDiLAVIfhST3uwJopP-TgtBk\
                 zc-zyJSvgTR63S8iI1YlHypItpx7r4I9ydzo8GSN5RrZudcU2esY4uEnLbVl17ZVNu4IyTExeKJ0sPM0Hj\
                 qkOA4XaP2cJwsK-bookNHSA8NRE6adRMrHAKJbor5jrGjpkZAKHbnQFK-wu-nEV_OjS9jpN_FboRZVcDTZ\
                 GFzeFbqFqHdRn6UWPFnVpVnUhih16UjNH1om6gwc0uFoPWTDxJlXQCFbHMhZtgCbUkXQBH7twPMc4YUziw\
                 S8GIRKCcXjdrP5oyxmcitQ"
                    .to_string(),
            )).expect("failed to deserialize")
        ) {
            Err(ClaimsVerificationError::SignatureVerification(
                    SignatureVerificationError::AmbiguousKeyId(_),
                )) => {}
            other => panic!("unexpected result: {:?}", other),
        }

        // RS256 + valid signature
        verifier
            .verified_claims(valid_rs256_jwt)
            .expect("verification should succeed");

        // RS256 + invalid signature
        match verifier.verified_claims(
            serde_json::from_value::<TestClaimsJsonWebToken>(serde_json::Value::String(
                "eyJhbGciOiJSUzI1NiJ9.eyJhdWQiOlsibXlfY2xpZW50Il0sImlzcyI6Imh0dHBzOi8vZXhhbXBsZS5jb\
                 20iLCJwYXlsb2FkIjoiaGVsbG8gd29ybGQifQ.YmFkX2hhc2g"
                    .to_string(),
            )).expect("failed to deserialize"),
        ) {
            Err(ClaimsVerificationError::SignatureVerification(
                    SignatureVerificationError::CryptoError(_),
                )) => {}
            other => panic!("unexpected result: {:?}", other),
        }
    }

    type CoreIdTokenJwt = JsonWebToken<
        CoreJweContentEncryptionAlgorithm,
        CoreJwsSigningAlgorithm,
        CoreJsonWebKeyType,
        CoreIdTokenClaims,
        JsonWebTokenJsonPayloadSerde,
    >;

    #[test]
    fn test_id_token_verified_claims() {
        let rsa_key = serde_json::from_str::<CoreJsonWebKey>(TEST_RSA_PUB_KEY)
            .expect("deserialization failed");

        let client_id = ClientId::new("my_client".to_string());
        let issuer = IssuerUrl::new("https://example.com".to_string()).unwrap();
        let mock_current_time = AtomicUsize::new(1544932149);
        let mock_is_valid_issue_time = AtomicBool::new(true);
        // Extra scope needed to ensure closures are destroyed before the values they borrow.
        {
            let public_client_verifier = CoreIdTokenVerifier::new_public_client(
                client_id.clone(),
                issuer.clone(),
                CoreJsonWebKeySet::new(vec![rsa_key.clone()]),
            )
            .set_time_fn(|| {
                timestamp_to_utc(&Timestamp::Seconds(
                    mock_current_time.load(Ordering::Relaxed).into(),
                ))
                .unwrap()
            })
            .set_issue_time_verifier_fn(|_| {
                if mock_is_valid_issue_time.load(Ordering::Relaxed) {
                    Ok(())
                } else {
                    Err("Invalid iat claim".to_string())
                }
            });

            let insecure_verifier = CoreIdTokenVerifier::new_insecure_without_verification()
                .set_time_fn(|| {
                    timestamp_to_utc(&Timestamp::Seconds(
                        mock_current_time.load(Ordering::Relaxed).into(),
                    ))
                    .unwrap()
                });

            // This JWTs below have an issue time of 1544928549 and an expiration time of 1544932149.

            let test_jwt_without_nonce =
                serde_json::from_value::<CoreIdTokenJwt>(serde_json::Value::String(
                    "eyJhbGciOiJSUzI1NiJ9.eyJhdWQiOlsibXlfY2xpZW50Il0sImlzcyI6Imh0dHBzOi8vZXhhbXBsZ\
                     S5jb20iLCJzdWIiOiJzdWJqZWN0IiwiZXhwIjoxNTQ0OTMyMTQ5LCJpYXQiOjE1NDQ5Mjg1NDl9.nN\
                     aTxNwclnTHd1Q9POkddm5wB1w3wJ-gwQWHomhimttk3SWQTLhxI0SSjWrHahGxlfkjufJlSyt-t_VO\
                     SdcROvIYZTDznDfFZz3oSOev-p9XiZ-EZTS-U6N11Y923sDQjbTMeukz1F3ZFEfn5Mv2xjdEoJccCe\
                     7SaGuDmVqMqTLXMtsw9NCE_KDd0oKSwDzbJIBBPEfG3JjbKg0Dln7ENHg9wzoNFQzPXrkKzjneBgD3\
                     vuwFCV5y-e8xUBdLaLZF1kdkDZJIA48uRROLlWjsM8pEptosA5QK07luQCZNqcaZWEczoGXeQs8PyA\
                     zkNV7JEmti3bJnWSN-ud4cFU0LiQ"
                            .to_string(),
                ))
                .expect("failed to deserialize");

            // Invalid JWT claims
            match public_client_verifier.verified_claims(
                &serde_json::from_value::<CoreIdTokenJwt>(serde_json::Value::String(
                    "eyJhbGciOiJSUzI1NiJ9.eyJhdWQiOlsibXlfY2xpZW50Il0sImlzcyI6Imh0dHBzOi8vYXR0YWNrZ\
                     XIuY29tIiwic3ViIjoic3ViamVjdCIsImV4cCI6MTU0NDkzMjE0OSwiaWF0IjoxNTQ0OTI4NTQ5LCJ\
                     ub25jZSI6InRoZV9ub25jZSIsImFjciI6InRoZV9hY3IifQ.Pkicxk0dTU5BkSxgqTON6lE7A7ir3l\
                     aADRyoeRoCNDX3AOx7BXCbfzbda6HJiPskN2nu56w0q-0OdkDSIHls-2xTUlLEJv2Bv0BLYwV5ZVJ8\
                     hoc-rTd0_oLUb5NzyD80RyVByjVMK8bh6cwysTnr8QDxsEiFZbFo3mVJob2yjPZnNOdcNJWPcVVueP\
                     8vqMJnx5kHih1gKZpWj_dMN9b2AW6zVLOInW3Ox__gx6fsFFz7rjxItG-PTY_OQMzthqeHUyq4o9y7\
                     Jv8mB_jFkTZGVKHTPpObHV-qptJ_rnlwvF_mP5GARBLng-4Yd7nmSr31onYL48QDjGOrwPqQ-IyaCQ"
                        .to_string(),
                ))
                    .expect("failed to deserialize"), |_: Option<&Nonce>| Ok(())) {
                Err(ClaimsVerificationError::InvalidIssuer(_)) => {}
                other => panic!("unexpected result: {:?}", other),
            }

            // TODO: disallowed algs

            // Expired token
            mock_current_time.store(1544928549 + 3600, Ordering::Relaxed);
            match public_client_verifier
                .verified_claims(&test_jwt_without_nonce, |_: Option<&Nonce>| Ok(()))
            {
                Err(ClaimsVerificationError::Expired(_)) => {}
                other => panic!("unexpected result: {:?}", other),
            }
            mock_current_time.store(1544928549 + 1, Ordering::Relaxed);

            // Invalid issue time
            mock_is_valid_issue_time.store(false, Ordering::Relaxed);
            match public_client_verifier
                .verified_claims(&test_jwt_without_nonce, |_: Option<&Nonce>| Ok(()))
            {
                Err(ClaimsVerificationError::Expired(_)) => {}
                other => panic!("unexpected result: {:?}", other),
            }
            mock_is_valid_issue_time.store(true, Ordering::Relaxed);

            let valid_nonce = Nonce::new("the_nonce".to_string());

            // Successful verification w/o checking nonce
            public_client_verifier
                .verified_claims(&test_jwt_without_nonce, |_: Option<&Nonce>| Ok(()))
                .expect("verification should succeed");

            // Missing nonce
            match public_client_verifier.verified_claims(&test_jwt_without_nonce, &valid_nonce) {
                Err(ClaimsVerificationError::InvalidNonce(_)) => {}
                other => panic!("unexpected result: {:?}", other),
            }

            // Missing nonce w/ closure
            match public_client_verifier.verified_claims(
                &test_jwt_without_nonce,
                |nonce: Option<&Nonce>| {
                    if nonce.iter().any(|n| n.secret() == valid_nonce.secret()) {
                        Ok(())
                    } else {
                        Err("invalid nonce".to_string())
                    }
                },
            ) {
                Err(ClaimsVerificationError::InvalidNonce(_)) => {}
                other => panic!("unexpected result: {:?}", other),
            }

            let test_jwt_with_nonce =
                serde_json::from_value::<CoreIdTokenJwt>(serde_json::Value::String(
                    "eyJhbGciOiJSUzI1NiJ9.eyJhdWQiOlsibXlfY2xpZW50Il0sImlzcyI6Imh0dHBzOi8vZXhhbXBsZ\
                     S5jb20iLCJzdWIiOiJzdWJqZWN0IiwiZXhwIjoxNTQ0OTMyMTQ5LCJpYXQiOjE1NDQ5Mjg1NDksIm5\
                     vbmNlIjoidGhlX25vbmNlIiwiYWNyIjoidGhlX2FjciIsImF1dGhfdGltZSI6MTU0NDkyODU0OH0.W\
                     XA7SS9aMh_6rvBEgQce5D2J84OqphmmnCLGgEKRTN5G-UuQTNOBp8VS5_4f3xgzMEEMvGJJauJoALk\
                     muUeHB-N_ESrkmB3tgDzBSYBa7kuYPHUPYpdjZM2UVolqI9RYyHaWwKjL_Io5YyAazB5lH5ibPaiBl\
                     UNKGs3cmVsEB22UGMFKM6cek7GinrHQe_aJQsMU839-c2zzlEyFSeI8QBphQtG6AN82IPkNRv8QWmw\
                     ZjUiB5a-W73Z3gURYMNs7f32BjAUNoJzW0Qj34vzD2djoSHhltE0wHKBzPqGhUM1Y3A-a3q-LS2g1h\
                     6qgXb_KQ_Mmok8v8ld0cW_aYRLfNg"
                        .to_string(),
                ))
                .expect("failed to deserialize");

            // Invalid nonce
            match public_client_verifier.verified_claims(
                &test_jwt_with_nonce,
                &Nonce::new("different_nonce".to_string()),
            ) {
                Err(ClaimsVerificationError::InvalidNonce(_)) => {}
                other => panic!("unexpected result: {:?}", other),
            }

            // Invalid AuthenticationContextClass reference
            match public_client_verifier
                .clone()
                .set_auth_context_verifier_fn(|acr| {
                    assert_eq!(**acr.unwrap(), "the_acr");
                    Err("Invalid acr claim".to_string())
                })
                .verified_claims(&test_jwt_with_nonce, &valid_nonce)
            {
                Err(ClaimsVerificationError::InvalidAuthContext(_)) => {}
                other => panic!("unexpected result: {:?}", other),
            }

            let test_jwt_without_auth_time =
                serde_json::from_value::<CoreIdTokenJwt>(serde_json::Value::String(
                    "eyJhbGciOiJSUzI1NiJ9.eyJhdWQiOlsibXlfY2xpZW50Il0sImlzcyI6Imh0dHBzOi8vZXhhbXBsZ\
                     S5jb20iLCJzdWIiOiJzdWJqZWN0IiwiZXhwIjoxNTQ0OTMyMTQ5LCJpYXQiOjE1NDQ5Mjg1NDksIm5\
                     vbmNlIjoidGhlX25vbmNlIiwiYWNyIjoidGhlX2FjciJ9.c_lU1VRasTg0mB4lwdOzbzvFS_XShMLN\
                     lAPUpHBaMtCSPtI71L2x3hIByfkqIrAED-Qc_am2gNJ20bifidlkTOO6nyaBrJuaSjwT8aqajEbXon\
                     5JFswwPvqCIWjd0eV5dXC1MZunpd7ANXSC7Qw16v3m_crc9wcI_fLFCzuAKrWYokGvNy0gr1CxcgVg\
                     aE9qR0eqaatetzCuaOJhYOq4njrRlGZWtbj5Q56q3zhxJ_yS8K8gv1QcB4sHjUyXIj21jzjUD87zVG\
                     dJsn8E-nFJSltBdQhEaLksTBH6ZZhkeGicQ8cEPnNeS4L1vfVyAd_cjl64JHLmzw8RUp8XuoF9nA"
                        .to_string(),
                ))
                .expect("failed to deserialize");

            // Missing auth_time (ok)
            public_client_verifier
                .verified_claims(&test_jwt_without_auth_time, |_: Option<&Nonce>| Ok(()))
                .expect("verification should succeed");

            // Missing auth_time (error)
            match public_client_verifier
                .clone()
                .set_auth_time_verifier_fn(|auth_time| {
                    assert!(auth_time.is_none());
                    Err("Invalid auth_time claim".to_string())
                })
                .verified_claims(&test_jwt_without_auth_time, |_: Option<&Nonce>| Ok(()))
            {
                Err(ClaimsVerificationError::InvalidAuthTime(_)) => {}
                other => panic!("unexpected result: {:?}", other),
            }

            // Invalid auth_time
            match public_client_verifier
                .clone()
                .set_auth_time_verifier_fn(|auth_time| {
                    assert_eq!(
                        auth_time.unwrap(),
                        timestamp_to_utc(&Timestamp::Seconds(1544928548.into())).unwrap(),
                    );
                    Err("Invalid auth_time claim".to_string())
                })
                .verified_claims(&test_jwt_with_nonce, &valid_nonce)
            {
                Err(ClaimsVerificationError::InvalidAuthTime(_)) => {}
                other => panic!("unexpected result: {:?}", other),
            }

            // Successful verification with nonce, acr, and auth_time specified (no expected Nonce)
            public_client_verifier
                .verified_claims(&test_jwt_with_nonce, |_: Option<&Nonce>| Ok(()))
                .expect("verification should succeed");
            insecure_verifier
                .verified_claims(&test_jwt_with_nonce, |_: Option<&Nonce>| Ok(()))
                .expect("verification should succeed");

            // Successful verification with nonce, acr, and auth_time specified (w/ expected Nonce)
            public_client_verifier
                .verified_claims(&test_jwt_with_nonce, &valid_nonce)
                .expect("verification should succeed");
            insecure_verifier
                .verified_claims(&test_jwt_with_nonce, &valid_nonce)
                .expect("verification should succeed");

            // Successful verification with nonce, acr, and auth_time specified (w/ closure)
            public_client_verifier
                .verified_claims(&test_jwt_with_nonce, |nonce: Option<&Nonce>| {
                    if nonce.iter().any(|n| n.secret() == valid_nonce.secret()) {
                        Ok(())
                    } else {
                        Err("invalid nonce".to_string())
                    }
                })
                .expect("verification should succeed");
            insecure_verifier
                .verified_claims(&test_jwt_with_nonce, |nonce: Option<&Nonce>| {
                    if nonce.iter().any(|n| n.secret() == valid_nonce.secret()) {
                        Ok(())
                    } else {
                        Err("invalid nonce".to_string())
                    }
                })
                .expect("verification should succeed");

            // HS256 w/ default algs
            let test_jwt_hs256 =
                serde_json::from_value::<CoreIdTokenJwt>(serde_json::Value::String(
                    "eyJhbGciOiJIUzI1NiJ9.eyJhdWQiOlsibXlfY2xpZW50Il0sImlzcyI6Imh0dHBzOi8vZXhhbXBsZ\
                     S5jb20iLCJzdWIiOiJzdWJqZWN0IiwiZXhwIjoxNTQ0OTMyMTQ5LCJpYXQiOjE1NDQ5Mjg1NDksIm5\
                     vbmNlIjoidGhlX25vbmNlIn0.xUnSwSbcHsHWyJxwKGg69BIo_CktcyN5BVulGDb_QzE"
                        .to_string(),
                ))
                .expect("failed to deserialize");
            let private_client_verifier = CoreIdTokenVerifier::new_confidential_client(
                client_id.clone(),
                ClientSecret::new("my_secret".to_string()),
                issuer.clone(),
                CoreJsonWebKeySet::new(vec![rsa_key.clone()]),
            )
            .set_time_fn(|| {
                timestamp_to_utc(&Timestamp::Seconds(
                    mock_current_time.load(Ordering::Relaxed).into(),
                ))
                .unwrap()
            });
            match private_client_verifier.verified_claims(&test_jwt_hs256, &valid_nonce) {
                Err(ClaimsVerificationError::SignatureVerification(_)) => {}
                other => panic!("unexpected result: {:?}", other),
            }
            insecure_verifier
                .clone()
                .verified_claims(&test_jwt_hs256, &valid_nonce)
                .expect("verification should succeed");

            // HS256 w/ set_allowed_algs
            private_client_verifier
                .clone()
                .set_allowed_algs(vec![CoreJwsSigningAlgorithm::HmacSha256])
                .verified_claims(&test_jwt_hs256, &valid_nonce)
                .expect("verification should succeed");

            // HS256 w/ allow_any_alg
            private_client_verifier
                .clone()
                .allow_any_alg()
                .verified_claims(&test_jwt_hs256, &valid_nonce)
                .expect("verification should succeed");

            // Invalid signature
            let private_client_verifier_with_other_secret =
                CoreIdTokenVerifier::new_confidential_client(
                    client_id,
                    ClientSecret::new("other_secret".to_string()),
                    issuer,
                    CoreJsonWebKeySet::new(vec![rsa_key]),
                )
                .allow_any_alg()
                .set_time_fn(|| {
                    timestamp_to_utc(&Timestamp::Seconds(
                        mock_current_time.load(Ordering::Relaxed).into(),
                    ))
                    .unwrap()
                });
            match private_client_verifier_with_other_secret
                .verified_claims(&test_jwt_hs256, &valid_nonce)
            {
                Err(ClaimsVerificationError::SignatureVerification(_)) => {}
                other => panic!("unexpected result: {:?}", other),
            }

            // Invalid signature w/ signature check disabled
            private_client_verifier_with_other_secret
                .clone()
                .insecure_disable_signature_check()
                .verified_claims(&test_jwt_hs256, &valid_nonce)
                .expect("verification should succeed");
        };
    }

    #[test]
    fn test_new_id_token() {
        let client_id = ClientId::new("my_client".to_string());
        let issuer = IssuerUrl::new("https://example.com".to_string()).unwrap();
        let nonce = Nonce::new("the_nonce".to_string());
        let rsa_priv_key = CoreRsaPrivateSigningKey::from_pem(TEST_RSA_PRIV_KEY, None).unwrap();

        let id_token = CoreIdToken::new(
            CoreIdTokenClaims::new(
                issuer.clone(),
                vec![Audience::new((*client_id).clone())],
                Utc.timestamp(1544932149, 0),
                Utc.timestamp(1544928549, 0),
                StandardClaims::new(SubjectIdentifier::new("subject".to_string())),
                Default::default(),
            )
            .set_nonce(Some(nonce.clone()))
            .set_auth_context_ref(Some(AuthenticationContextClass::new("the_acr".to_string())))
            .set_auth_time(Some(Utc.timestamp(1544928548, 0))),
            &rsa_priv_key,
            CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha256,
            Some(&AccessToken::new("the_access_token".to_string())),
            Some(&AuthorizationCode::new(
                "the_authorization_code".to_string(),
            )),
        )
        .unwrap();

        let serialized_jwt: serde_json::Value = serde_json::to_value(&id_token).unwrap();
        let expected_serialized_jwt =
            "eyJhbGciOiJSUzI1NiJ9.eyJpc3MiOiJodHRwczovL2V4YW1wbGUuY29tIiwiYXVkIjpbIm15X2NsaWVudCJdL\
             CJleHAiOjE1NDQ5MzIxNDksImlhdCI6MTU0NDkyODU0OSwiYXV0aF90aW1lIjoxNTQ0OTI4NTQ4LCJub25jZSI\
             6InRoZV9ub25jZSIsImFjciI6InRoZV9hY3IiLCJhdF9oYXNoIjoiWjNJQUNVR00tbXhIV3lZUXZpSzhFUSIsI\
             mNfaGFzaCI6Imo2OW1CZmFIbmRMM1Y1RmNoak9LVXciLCJzdWIiOiJzdWJqZWN0In0.CHCWFcIqbCZhZwZH4oY\
             _mlcRy5aUQQtlNI0VHNYxiILn9ppRHLL4Bn_LMn9VP8tGXkfZWxCgP25ZTyBXXKfk0fQvnukVdyM0bCOpQbiBg\
             5gB9c46l_f-ZznDoHWonpnKky2Gmzk3ocb3TCUQ9GSeRXAzRdRNWTT0ElWNBsLWU4j2IIdnghM78gkXwOC76Rk\
             pshgB73ubtuHGdIf5L9Ec3hifHlVjzKuvedAM4SIOjdBOelgtBlF3463ufX_Ut91CjP5TzLMsuK3Lh_vyo8ttn\
             S41rBDuetR2ENvR0yj5RjkX_SPY3V0yCW8_NPPu1CHu_1oL0Nma0ohCbF3vnUJcwg";
        assert_eq!(expected_serialized_jwt, serialized_jwt.as_str().unwrap());

        let rsa_pub_key = serde_json::from_str::<CoreJsonWebKey>(TEST_RSA_PUB_KEY)
            .expect("deserialization failed");

        let mock_current_time = AtomicUsize::new(1544932148);
        let time_fn = || {
            timestamp_to_utc(&Timestamp::Seconds(
                mock_current_time.load(Ordering::Relaxed).into(),
            ))
            .unwrap()
        };
        let verifier = CoreIdTokenVerifier::new_public_client(
            client_id,
            issuer,
            CoreJsonWebKeySet::new(vec![rsa_pub_key]),
        )
        .set_time_fn(time_fn);
        let claims = id_token.claims(&verifier, &nonce).unwrap();
        let unverified = id_token
            .claims(
                &CoreIdTokenVerifier::new_insecure_without_verification().set_time_fn(time_fn),
                &nonce,
            )
            .unwrap();
        assert_eq!(claims, unverified);
    }

    #[test]
    fn test_user_info_verified_claims() {
        let rsa_key = serde_json::from_str::<CoreJsonWebKey>(TEST_RSA_PUB_KEY)
            .expect("deserialization failed");

        let client_id = ClientId::new("my_client".to_string());
        let issuer = IssuerUrl::new("https://example.com".to_string()).unwrap();
        let sub = SubjectIdentifier::new("the_subject".to_string());

        let verifier = CoreUserInfoVerifier::new(
            client_id.clone(),
            issuer.clone(),
            CoreJsonWebKeySet::new(vec![rsa_key.clone()]),
            Some(sub.clone()),
        );

        let json_claims = "{\
                           \"sub\": \"the_subject\",\
                           \"name\": \"Jane Doe\"\
                           }";

        // JSON response (default args)
        assert_eq!(
            CoreUserInfoClaims::from_json::<crate::reqwest::HttpClientError>(
                json_claims.as_bytes(),
                Some(&sub)
            )
            .expect("verification should succeed")
            .name()
            .unwrap()
            .iter()
            .collect::<Vec<_>>(),
            vec![(None, &EndUserName::new("Jane Doe".to_string()))],
        );

        // Invalid subject
        match CoreUserInfoClaims::from_json::<crate::reqwest::HttpClientError>(
            json_claims.as_bytes(),
            Some(&SubjectIdentifier::new("wrong_subject".to_string())),
        ) {
            Err(UserInfoError::ClaimsVerification(ClaimsVerificationError::InvalidSubject(_))) => {}
            other => panic!("unexpected result: {:?}", other),
        }

        let jwt_claims =
            serde_json::from_value::<CoreUserInfoJsonWebToken>(serde_json::Value::String(
                "eyJhbGciOiJSUzI1NiJ9.eyJhdWQiOlsibXlfY2xpZW50Il0sImlzcyI6Imh0dHBzOi8vZXhhb\
                 XBsZS5jb20iLCJzdWIiOiJ0aGVfc3ViamVjdCIsIm5hbWUiOiJKYW5lIERvZSJ9.aX7VpexLAd\
                 43HtC1cFTot3jmqsr105rB50mzTcS1TXzWcxLbqYf1K7Kf-S1oP-ZCL_dnL9-nu3iDK_vRa6xT\
                 nGGt3I1JwhoIv6znSS3JOPT1wtekyD-sLcUwqsJHWBBiTSBwlmGG_kVRuGkBtXgVZ9aGlqg9u1\
                 FlxvyGUJ5q1o9gdb8mKql5ojgsThTNo9qdW3lPIVsiDO-n4mMp4HuOp1re4ZDDkHxiExjtLQAV\
                 kR4q3SlhJC2mkr4mw3_0a2AW52ocWDiwY_lPcdmohmwFaB8aHlivYLFnmKGQIatEW-KDaW5fFo\
                 JYreNkplo4FvzXYyxgxAsqHjHMI8MZVEa1IA"
                    .to_string(),
            ))
            .expect("failed to deserialize");

        // Valid JWT response (default args)
        jwt_claims
            .clone()
            .claims(&verifier)
            .expect("verification should succeed");

        // JWT response with invalid signature
        match serde_json::from_value::<CoreUserInfoJsonWebToken>(serde_json::Value::String(
            "eyJhbGciOiJSUzI1NiJ9.eyJhdWQiOlsibXlfY2xpZW50Il0sImlzcyI6Imh0dHBzOi8vZXhhb\
             XBsZS5jb20iLCJzdWIiOiJ0aGVfc3ViamVjdCIsIm5hbWUiOiJKYW5lIERvZSJ9.bX7VpexLAd\
             43HtC1cFTot3jmqsr105rB50mzTcS1TXzWcxLbqYf1K7Kf-S1oP-ZCL_dnL9-nu3iDK_vRa6xT\
             nGGt3I1JwhoIv6znSS3JOPT1wtekyD-sLcUwqsJHWBBiTSBwlmGG_kVRuGkBtXgVZ9aGlqg9u1\
             FlxvyGUJ5q1o9gdb8mKql5ojgsThTNo9qdW3lPIVsiDO-n4mMp4HuOp1re4ZDDkHxiExjtLQAV\
             kR4q3SlhJC2mkr4mw3_0a2AW52ocWDiwY_lPcdmohmwFaB8aHlivYLFnmKGQIatEW-KDaW5fFo\
             JYreNkplo4FvzXYyxgxAsqHjHMI8MZVEa1IA"
                .to_string(),
        ))
        .expect("failed to deserialize")
        .claims(&verifier)
        {
            Err(ClaimsVerificationError::SignatureVerification(
                SignatureVerificationError::CryptoError(_),
            )) => {}
            other => panic!("unexpected result: {:?}", other),
        }

        // JWT response with invalid issuer claim (error)
        match jwt_claims.clone().claims(&CoreUserInfoVerifier::new(
            client_id.clone(),
            IssuerUrl::new("https://attacker.com".to_string()).unwrap(),
            CoreJsonWebKeySet::new(vec![rsa_key.clone()]),
            Some(sub.clone()),
        )) {
            Err(ClaimsVerificationError::InvalidIssuer(_)) => {}
            other => panic!("unexpected result: {:?}", other),
        }

        // JWT response with invalid issuer claim (allowed)
        jwt_claims
            .clone()
            .claims(
                &CoreUserInfoVerifier::new(
                    client_id,
                    IssuerUrl::new("https://attacker.com".to_string()).unwrap(),
                    CoreJsonWebKeySet::new(vec![rsa_key.clone()]),
                    Some(sub.clone()),
                )
                .require_issuer_match(false),
            )
            .expect("verification should succeed");

        // JWT response with invalid audience claim (error)
        match jwt_claims.clone().claims(&CoreUserInfoVerifier::new(
            ClientId::new("wrong_client".to_string()),
            issuer.clone(),
            CoreJsonWebKeySet::new(vec![rsa_key.clone()]),
            Some(sub.clone()),
        )) {
            Err(ClaimsVerificationError::InvalidAudience(_)) => {}
            other => panic!("unexpected result: {:?}", other),
        }

        // JWT response with invalid audience claim (allowed)
        jwt_claims
            .claims(
                &CoreUserInfoVerifier::new(
                    ClientId::new("wrong_client".to_string()),
                    issuer,
                    CoreJsonWebKeySet::new(vec![rsa_key]),
                    Some(sub),
                )
                .require_audience_match(false),
            )
            .expect("verification should succeed");
    }

    #[test]
    fn test_new_user_info_claims() {
        let claims = CoreUserInfoClaims::new(
            StandardClaims {
                sub: SubjectIdentifier::new("the_subject".to_string()),
                name: Some(EndUserName::new("John Doe".to_string()).into()),
                given_name: None,
                family_name: None,
                middle_name: None,
                nickname: None,
                preferred_username: None,
                profile: None,
                picture: None,
                website: None,
                email: None,
                email_verified: None,
                gender: None,
                birthday: None,
                zoneinfo: None,
                locale: None,
                phone_number: None,
                phone_number_verified: None,
                address: None,
                updated_at: Some(Utc.timestamp(1544928548, 0)),
            },
            Default::default(),
        );

        assert_eq!(
            "{\"sub\":\"the_subject\",\"name\":\"John Doe\",\"updated_at\":1544928548}",
            serde_json::to_string(&claims).unwrap()
        );

        let rsa_priv_key = CoreRsaPrivateSigningKey::from_pem(TEST_RSA_PRIV_KEY, None).unwrap();
        let claims_jwt = CoreUserInfoJsonWebToken::new(
            claims,
            &rsa_priv_key,
            CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha256,
        )
        .unwrap();
        assert_eq!(
            "eyJhbGciOiJSUzI1NiJ9.eyJzdWIiOiJ0aGVfc3ViamVjdCIsIm5hbWUiOiJKb2huIERvZSIsInVwZGF0ZWRfY\
             XQiOjE1NDQ5Mjg1NDh9.nJ7Buckt_p_ACXkyVRCQLqyaW8KhDsk5H9Nu7PdNf4daEcEWm-lGjoSTAfAbDPgHAZ\
             78knomgLgDxiGWrj1qdFTIEFep32I3q18VBP_DcMdyuQafipK6T98RgZFWP8YnxlxLPHeJQlRsdMpemHK4vxas\
             ZD4A4aIn0K7z5J9RvrR3L7DWnc3fJQ0VU2v5QLePyqNWnFxks5eyl8Ios8JrZhwr4Q8GES8Q4Iw8Sz6W9vYpHK\
             2r1YdaACMM4g_TTtV91lpjn-Li2-HxW9NERdLvYvF6HwGIwbss26trp2yjNTARlxBUT6LR7y82oPIJKXIKL1GD\
             YeSLeErhb6oTQ0a5gQ",
            serde_json::to_value(&claims_jwt).unwrap().as_str().unwrap()
        );
    }
}
