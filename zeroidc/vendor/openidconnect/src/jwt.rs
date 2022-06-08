use std::fmt::{Debug, Formatter, Result as FormatterResult};
use std::marker::PhantomData;
use std::ops::Deref;
use std::str;

use serde::de::{DeserializeOwned, Error as _, Visitor};
use serde::{Deserialize, Deserializer, Serialize, Serializer};
use thiserror::Error;

use super::{
    JsonWebKey, JsonWebKeyId, JsonWebKeyType, JsonWebKeyUse, JweContentEncryptionAlgorithm,
    JwsSigningAlgorithm, PrivateSigningKey, SignatureVerificationError, SigningError,
};

new_type![
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    JsonWebTokenContentType(String)
];

new_type![
    #[derive(Deserialize, Eq, Hash, Ord, PartialOrd, Serialize)]
    JsonWebTokenType(String)
];

#[derive(Clone, Debug, PartialEq)]
#[non_exhaustive]
pub enum JsonWebTokenAlgorithm<JE, JS, JT>
where
    JE: JweContentEncryptionAlgorithm<JT>,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
{
    Encryption(JE),
    // This is ugly, but we don't expose this module via the public API, so it's fine.
    Signature(JS, PhantomData<JT>),
    ///
    /// No digital signature or MAC performed.
    ///
    /// # Security Warning
    ///
    /// This algorithm provides no security over the integrity of the JSON Web Token. Clients
    /// should be careful not to rely on unsigned JWT's for security purposes. See
    /// [Critical vulnerabilities in JSON Web Token libraries](
    ///     https://auth0.com/blog/critical-vulnerabilities-in-json-web-token-libraries/) for
    /// further discussion.
    ///
    None,
}
impl<'de, JE, JS, JT> Deserialize<'de> for JsonWebTokenAlgorithm<JE, JS, JT>
where
    JE: JweContentEncryptionAlgorithm<JT>,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
{
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        let value: serde_json::Value = Deserialize::deserialize(deserializer)?;
        // TODO: get rid of this clone() (see below)
        let s: String = serde_json::from_value(value.clone()).map_err(D::Error::custom)?;

        // NB: These comparisons are case sensitive. Section 4.1.1 of RFC 7515 states: "The "alg"
        // value is a case-sensitive ASCII string containing a StringOrURI value."
        if s == "none" {
            Ok(JsonWebTokenAlgorithm::None)
        // TODO: Figure out a way to deserialize the enums without giving up ownership
        } else if let Ok(val) = serde_json::from_value::<JE>(value.clone()) {
            Ok(JsonWebTokenAlgorithm::Encryption(val))
        } else if let Ok(val) = serde_json::from_value::<JS>(value) {
            Ok(JsonWebTokenAlgorithm::Signature(val, PhantomData))
        } else {
            Err(D::Error::custom(format!(
                "unrecognized JSON Web Algorithm `{}`",
                s
            )))
        }
    }
}
impl<JE, JS, JT> Serialize for JsonWebTokenAlgorithm<JE, JS, JT>
where
    JE: JweContentEncryptionAlgorithm<JT>,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
{
    fn serialize<SE>(&self, serializer: SE) -> Result<SE::Ok, SE::Error>
    where
        SE: Serializer,
    {
        match self {
            JsonWebTokenAlgorithm::Encryption(ref enc) => enc.serialize(serializer),
            JsonWebTokenAlgorithm::Signature(ref sig, _) => sig.serialize(serializer),
            JsonWebTokenAlgorithm::None => serializer.serialize_str("none"),
        }
    }
}

#[derive(Clone, Debug, Deserialize, PartialEq, Serialize)]
pub struct JsonWebTokenHeader<JE, JS, JT>
where
    JE: JweContentEncryptionAlgorithm<JT>,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
{
    #[serde(
        bound = "JE: JweContentEncryptionAlgorithm<JT>, JS: JwsSigningAlgorithm<JT>, JT: JsonWebKeyType"
    )]
    pub alg: JsonWebTokenAlgorithm<JE, JS, JT>,
    // Additional critical header parameters that must be understood by this implementation. Since
    // we don't understand any such extensions, we reject any JWT with this value present (the
    // spec specifically prohibits including public (standard) headers in this field).
    // See https://tools.ietf.org/html/rfc7515#section-4.1.11.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub crit: Option<Vec<String>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub cty: Option<JsonWebTokenContentType>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub kid: Option<JsonWebKeyId>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub typ: Option<JsonWebTokenType>,
    // Other JOSE header fields are omitted since the OpenID Connect spec specifically says that
    // the "x5u", "x5c", "jku", "jwk" header parameter fields SHOULD NOT be used.
    // See http://openid.net/specs/openid-connect-core-1_0-final.html#IDToken.
    #[serde(skip)]
    _phantom_jt: PhantomData<JT>,
}

pub trait JsonWebTokenPayloadSerde<P>: Debug
where
    P: Debug + DeserializeOwned + Serialize,
{
    fn deserialize<DE: serde::de::Error>(payload: &[u8]) -> Result<P, DE>;
    fn serialize(payload: &P) -> Result<String, serde_json::Error>;
}

#[derive(Clone, Debug, PartialEq)]
pub struct JsonWebTokenJsonPayloadSerde;
impl<P> JsonWebTokenPayloadSerde<P> for JsonWebTokenJsonPayloadSerde
where
    P: Debug + DeserializeOwned + Serialize,
{
    fn deserialize<DE: serde::de::Error>(payload: &[u8]) -> Result<P, DE> {
        serde_json::from_slice(payload)
            .map_err(|err| DE::custom(format!("Failed to parse payload JSON: {:?}", err)))
    }

    fn serialize(payload: &P) -> Result<String, serde_json::Error> {
        serde_json::to_string(payload).map_err(Into::into)
    }
}

// Helper trait so that we can get borrowed payload when we have a reference to the JWT and owned
// payload when we own the JWT.
pub trait JsonWebTokenAccess<JE, JS, JT, P>
where
    JE: JweContentEncryptionAlgorithm<JT>,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    P: Debug + DeserializeOwned + Serialize,
{
    type ReturnType;

    fn unverified_header(&self) -> &JsonWebTokenHeader<JE, JS, JT>;
    fn unverified_payload(self) -> Self::ReturnType;
    fn unverified_payload_ref(&self) -> &P;

    fn payload<JU, JW>(
        self,
        signature_alg: &JS,
        key: &JW,
    ) -> Result<Self::ReturnType, SignatureVerificationError>
    where
        JU: JsonWebKeyUse,
        JW: JsonWebKey<JS, JT, JU>;
}

///
/// Error creating a JSON Web Token.
///
#[derive(Debug, Error)]
#[non_exhaustive]
pub enum JsonWebTokenError {
    ///
    /// Failed to serialize JWT.
    ///
    #[error("Failed to serialize JWT")]
    SerializationError(#[source] serde_json::Error),
    ///
    /// Failed to sign JWT.
    ///
    #[error("Failed to sign JWT")]
    SigningError(#[source] SigningError),
}

#[derive(Clone, Debug, PartialEq)]
pub struct JsonWebToken<JE, JS, JT, P, S>
where
    JE: JweContentEncryptionAlgorithm<JT>,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    P: Debug + DeserializeOwned + Serialize,
    S: JsonWebTokenPayloadSerde<P>,
{
    header: JsonWebTokenHeader<JE, JS, JT>,
    payload: P,
    signature: Vec<u8>,
    signing_input: String,
    _phantom: PhantomData<S>,
}
impl<JE, JS, JT, P, S> JsonWebToken<JE, JS, JT, P, S>
where
    JE: JweContentEncryptionAlgorithm<JT>,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    P: Debug + DeserializeOwned + Serialize,
    S: JsonWebTokenPayloadSerde<P>,
{
    pub fn new<JU, K, SK>(payload: P, signing_key: &SK, alg: &JS) -> Result<Self, JsonWebTokenError>
    where
        JU: JsonWebKeyUse,
        K: JsonWebKey<JS, JT, JU>,
        SK: PrivateSigningKey<JS, JT, JU, K>,
    {
        let header = JsonWebTokenHeader::<JE, _, _> {
            alg: JsonWebTokenAlgorithm::Signature(alg.clone(), PhantomData),
            crit: None,
            cty: None,
            kid: signing_key.as_verification_key().key_id().cloned(),
            typ: None,
            _phantom_jt: PhantomData,
        };

        let header_json =
            serde_json::to_string(&header).map_err(JsonWebTokenError::SerializationError)?;
        let header_base64 = base64::encode_config(&header_json, base64::URL_SAFE_NO_PAD);

        let serialized_payload =
            S::serialize(&payload).map_err(JsonWebTokenError::SerializationError)?;
        let payload_base64 = base64::encode_config(&serialized_payload, base64::URL_SAFE_NO_PAD);

        let signing_input = format!("{}.{}", header_base64, payload_base64);

        let signature = signing_key
            .sign(alg, signing_input.as_bytes())
            .map_err(JsonWebTokenError::SigningError)?;

        Ok(JsonWebToken {
            header,
            payload,
            signature,
            signing_input,
            _phantom: PhantomData,
        })
    }
}
// Owned JWT.
impl<JE, JS, JT, P, S> JsonWebTokenAccess<JE, JS, JT, P> for JsonWebToken<JE, JS, JT, P, S>
where
    JE: JweContentEncryptionAlgorithm<JT>,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    P: Debug + DeserializeOwned + Serialize,
    S: JsonWebTokenPayloadSerde<P>,
{
    type ReturnType = P;
    fn unverified_header(&self) -> &JsonWebTokenHeader<JE, JS, JT> {
        &self.header
    }
    fn unverified_payload(self) -> Self::ReturnType {
        self.payload
    }
    fn unverified_payload_ref(&self) -> &P {
        &self.payload
    }
    fn payload<JU, JW>(
        self,
        signature_alg: &JS,
        key: &JW,
    ) -> Result<Self::ReturnType, SignatureVerificationError>
    where
        JU: JsonWebKeyUse,
        JW: JsonWebKey<JS, JT, JU>,
    {
        key.verify_signature(
            signature_alg,
            self.signing_input.as_bytes(),
            &self.signature,
        )?;
        Ok(self.payload)
    }
}
// Borrowed JWT.
impl<'a, JE, JS, JT, P, S> JsonWebTokenAccess<JE, JS, JT, P> for &'a JsonWebToken<JE, JS, JT, P, S>
where
    JE: JweContentEncryptionAlgorithm<JT>,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    P: Debug + DeserializeOwned + Serialize,
    S: JsonWebTokenPayloadSerde<P>,
{
    type ReturnType = &'a P;
    fn unverified_header(&self) -> &JsonWebTokenHeader<JE, JS, JT> {
        &self.header
    }
    fn unverified_payload(self) -> Self::ReturnType {
        &self.payload
    }
    fn unverified_payload_ref(&self) -> &P {
        &self.payload
    }
    fn payload<JU, JW>(
        self,
        signature_alg: &JS,
        key: &JW,
    ) -> Result<Self::ReturnType, SignatureVerificationError>
    where
        JU: JsonWebKeyUse,
        JW: JsonWebKey<JS, JT, JU>,
    {
        key.verify_signature(
            signature_alg,
            self.signing_input.as_bytes(),
            &self.signature,
        )?;
        Ok(&self.payload)
    }
}
impl<'de, JE, JS, JT, P, S> Deserialize<'de> for JsonWebToken<JE, JS, JT, P, S>
where
    JE: JweContentEncryptionAlgorithm<JT>,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    P: Debug + DeserializeOwned + Serialize,
    S: JsonWebTokenPayloadSerde<P>,
{
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct JsonWebTokenVisitor<
            JE: JweContentEncryptionAlgorithm<JT>,
            JS: JwsSigningAlgorithm<JT>,
            JT: JsonWebKeyType,
            P: Debug + DeserializeOwned + Serialize,
            S: JsonWebTokenPayloadSerde<P>,
        >(
            PhantomData<JE>,
            PhantomData<JS>,
            PhantomData<JT>,
            PhantomData<P>,
            PhantomData<S>,
        );
        impl<'de, JE, JS, JT, P, S> Visitor<'de> for JsonWebTokenVisitor<JE, JS, JT, P, S>
        where
            JE: JweContentEncryptionAlgorithm<JT>,
            JS: JwsSigningAlgorithm<JT>,
            JT: JsonWebKeyType,
            P: Debug + DeserializeOwned + Serialize,
            S: JsonWebTokenPayloadSerde<P>,
        {
            type Value = JsonWebToken<JE, JS, JT, P, S>;

            fn expecting(&self, formatter: &mut Formatter) -> FormatterResult {
                formatter.write_str("JsonWebToken")
            }

            fn visit_str<DE>(self, v: &str) -> Result<Self::Value, DE>
            where
                DE: serde::de::Error,
            {
                let raw_token = v.to_string();
                let header: JsonWebTokenHeader<JE, JS, JT>;
                let payload: P;
                let signature;
                let signing_input;

                {
                    let parts = raw_token.split('.').collect::<Vec<_>>();

                    // NB: We avoid including the full payload encoding in the error output to avoid
                    // clients potentially logging sensitive values.
                    if parts.len() != 3 {
                        return Err(DE::custom(format!(
                            "Invalid JSON web token: found {} parts (expected 3)",
                            parts.len()
                        )));
                    }

                    let header_json =
                        base64::decode_config(parts[0], crate::core::base64_url_safe_no_pad())
                            .map_err(|err| {
                                DE::custom(format!("Invalid base64url header encoding: {:?}", err))
                            })?;
                    header = serde_json::from_slice(&header_json).map_err(|err| {
                        DE::custom(format!("Failed to parse header JSON: {:?}", err))
                    })?;

                    let raw_payload =
                        base64::decode_config(parts[1], crate::core::base64_url_safe_no_pad())
                            .map_err(|err| {
                                DE::custom(format!("Invalid base64url payload encoding: {:?}", err))
                            })?;
                    payload = S::deserialize::<DE>(&raw_payload)?;

                    signature =
                        base64::decode_config(parts[2], crate::core::base64_url_safe_no_pad())
                            .map_err(|err| {
                                DE::custom(format!(
                                    "Invalid base64url signature encoding: {:?}",
                                    err
                                ))
                            })?;

                    signing_input = format!("{}.{}", parts[0], parts[1]);
                }

                Ok(JsonWebToken {
                    header,
                    payload,
                    signature,
                    signing_input,
                    _phantom: PhantomData,
                })
            }
        }
        deserializer.deserialize_str(JsonWebTokenVisitor(
            PhantomData,
            PhantomData,
            PhantomData,
            PhantomData,
            PhantomData,
        ))
    }
}
impl<JE, JS, JT, P, S> Serialize for JsonWebToken<JE, JS, JT, P, S>
where
    JE: JweContentEncryptionAlgorithm<JT>,
    JS: JwsSigningAlgorithm<JT>,
    JT: JsonWebKeyType,
    P: Debug + DeserializeOwned + Serialize,
    S: JsonWebTokenPayloadSerde<P>,
{
    fn serialize<SE>(&self, serializer: SE) -> Result<SE::Ok, SE::Error>
    where
        SE: Serializer,
    {
        let signature_base64 = base64::encode_config(&self.signature, base64::URL_SAFE_NO_PAD);
        serializer.serialize_str(&format!("{}.{}", self.signing_input, signature_base64))
    }
}

#[cfg(test)]
pub mod tests {
    use std::marker::PhantomData;
    use std::string::ToString;

    use crate::core::{
        CoreJsonWebKey, CoreJsonWebKeyType, CoreJweContentEncryptionAlgorithm,
        CoreJwsSigningAlgorithm, CoreRsaPrivateSigningKey,
    };
    use crate::JsonWebKeyId;

    use super::{
        JsonWebToken, JsonWebTokenAccess, JsonWebTokenAlgorithm, JsonWebTokenJsonPayloadSerde,
        JsonWebTokenPayloadSerde,
    };

    type CoreAlgorithm = JsonWebTokenAlgorithm<
        CoreJweContentEncryptionAlgorithm,
        CoreJwsSigningAlgorithm,
        CoreJsonWebKeyType,
    >;

    pub const TEST_JWT: &str =
        "eyJhbGciOiJSUzI1NiIsImtpZCI6ImJpbGJvLmJhZ2dpbnNAaG9iYml0b24uZXhhbXBsZSJ9.SXTigJlzIGEgZ\
         GFuZ2Vyb3VzIGJ1c2luZXNzLCBGcm9kbywgZ29pbmcgb3V0IHlvdXIgZG9vci4gWW91IHN0ZXAgb250byB0aGU\
         gcm9hZCwgYW5kIGlmIHlvdSBkb24ndCBrZWVwIHlvdXIgZmVldCwgdGhlcmXigJlzIG5vIGtub3dpbmcgd2hlc\
         mUgeW91IG1pZ2h0IGJlIHN3ZXB0IG9mZiB0by4.MRjdkly7_-oTPTS3AXP41iQIGKa80A0ZmTuV5MEaHoxnW2e\
         5CZ5NlKtainoFmKZopdHM1O2U4mwzJdQx996ivp83xuglII7PNDi84wnB-BDkoBwA78185hX-Es4JIwmDLJK3l\
         fWRa-XtL0RnltuYv746iYTh_qHRD68BNt1uSNCrUCTJDt5aAE6x8wW1Kt9eRo4QPocSadnHXFxnt8Is9UzpERV\
         0ePPQdLuW3IS_de3xyIrDaLGdjluPxUAhb6L2aXic1U12podGU0KLUQSE_oI-ZnmKJ3F4uOZDnd6QZWJushZ41\
         Axf_fcIe8u9ipH84ogoree7vjbU5y18kDquDg";

    const TEST_JWT_PAYLOAD: &str = "It\u{2019}s a dangerous business, Frodo, going out your \
                                    door. You step onto the road, and if you don't keep your feet, \
                                    there\u{2019}s no knowing where you might be swept off \
                                    to.";

    pub const TEST_RSA_PUB_KEY: &str = "{
            \"kty\": \"RSA\",
            \"kid\": \"bilbo.baggins@hobbiton.example\",
            \"use\": \"sig\",
            \"n\": \"n4EPtAOCc9AlkeQHPzHStgAbgs7bTZLwUBZdR8_KuKPEHLd4rHVTeT\
                     -O-XV2jRojdNhxJWTDvNd7nqQ0VEiZQHz_AJmSCpMaJMRBSFKrKb2wqV\
                     wGU_NsYOYL-QtiWN2lbzcEe6XC0dApr5ydQLrHqkHHig3RBordaZ6Aj-\
                     oBHqFEHYpPe7Tpe-OfVfHd1E6cS6M1FZcD1NNLYD5lFHpPI9bTwJlsde\
                     3uhGqC0ZCuEHg8lhzwOHrtIQbS0FVbb9k3-tVTU4fg_3L_vniUFAKwuC\
                     LqKnS2BYwdq_mzSnbLY7h_qixoR7jig3__kRhuaxwUkRz5iaiQkqgc5g\
                     HdrNP5zw\",
            \"e\": \"AQAB\"
        }";

    pub const TEST_EC_PUB_KEY_P256: &str = r#"{
        "kty": "EC",
        "kid": "bilbo.baggins@hobbiton.example",
        "use": "sig",
        "crv": "P-256",
        "x": "t6PHivOTggpaX9lkMkis2p8kMhy-CktJAFTz6atReZw",
        "y": "ODobXupKlD0DeM1yRd7bX4XFNBO1HOgCT1UCu0KY3lc"
    }"#;
    pub const TEST_EC_PUB_KEY_P384: &str = r#"{
        "kty": "EC",
        "kid": "bilbo.baggins@hobbiton.example",
        "use": "sig",
        "crv" : "P-384",
        "x": "9ywsUbxX59kJXFRiWHcx97wRKNiF8Hc9F5wI08n8h2ek_qAl0veEc36k1Qz6KLiL",
        "y": "6PWlqjRbaV7V8ohDscM243IneuLZmxDGLiGNA1w69fQhEDsvZtKLUQ5KiHLgR3op"
    }"#;

    // This is the PEM form of the test private key from:
    // https://tools.ietf.org/html/rfc7520#section-3.4
    pub const TEST_RSA_PRIV_KEY: &str = "-----BEGIN RSA PRIVATE KEY-----\n\
         MIIEowIBAAKCAQEAn4EPtAOCc9AlkeQHPzHStgAbgs7bTZLwUBZdR8/KuKPEHLd4\n\
         rHVTeT+O+XV2jRojdNhxJWTDvNd7nqQ0VEiZQHz/AJmSCpMaJMRBSFKrKb2wqVwG\n\
         U/NsYOYL+QtiWN2lbzcEe6XC0dApr5ydQLrHqkHHig3RBordaZ6Aj+oBHqFEHYpP\n\
         e7Tpe+OfVfHd1E6cS6M1FZcD1NNLYD5lFHpPI9bTwJlsde3uhGqC0ZCuEHg8lhzw\n\
         OHrtIQbS0FVbb9k3+tVTU4fg/3L/vniUFAKwuCLqKnS2BYwdq/mzSnbLY7h/qixo\n\
         R7jig3//kRhuaxwUkRz5iaiQkqgc5gHdrNP5zwIDAQABAoIBAG1lAvQfhBUSKPJK\n\
         Rn4dGbshj7zDSr2FjbQf4pIh/ZNtHk/jtavyO/HomZKV8V0NFExLNi7DUUvvLiW7\n\
         0PgNYq5MDEjJCtSd10xoHa4QpLvYEZXWO7DQPwCmRofkOutf+NqyDS0QnvFvp2d+\n\
         Lov6jn5C5yvUFgw6qWiLAPmzMFlkgxbtjFAWMJB0zBMy2BqjntOJ6KnqtYRMQUxw\n\
         TgXZDF4rhYVKtQVOpfg6hIlsaoPNrF7dofizJ099OOgDmCaEYqM++bUlEHxgrIVk\n\
         wZz+bg43dfJCocr9O5YX0iXaz3TOT5cpdtYbBX+C/5hwrqBWru4HbD3xz8cY1TnD\n\
         qQa0M8ECgYEA3Slxg/DwTXJcb6095RoXygQCAZ5RnAvZlno1yhHtnUex/fp7AZ/9\n\
         nRaO7HX/+SFfGQeutao2TDjDAWU4Vupk8rw9JR0AzZ0N2fvuIAmr/WCsmGpeNqQn\n\
         ev1T7IyEsnh8UMt+n5CafhkikzhEsrmndH6LxOrvRJlsPp6Zv8bUq0kCgYEAuKE2\n\
         dh+cTf6ERF4k4e/jy78GfPYUIaUyoSSJuBzp3Cubk3OCqs6grT8bR/cu0Dm1MZwW\n\
         mtdqDyI95HrUeq3MP15vMMON8lHTeZu2lmKvwqW7anV5UzhM1iZ7z4yMkuUwFWoB\n\
         vyY898EXvRD+hdqRxHlSqAZ192zB3pVFJ0s7pFcCgYAHw9W9eS8muPYv4ZhDu/fL\n\
         2vorDmD1JqFcHCxZTOnX1NWWAj5hXzmrU0hvWvFC0P4ixddHf5Nqd6+5E9G3k4E5\n\
         2IwZCnylu3bqCWNh8pT8T3Gf5FQsfPT5530T2BcsoPhUaeCnP499D+rb2mTnFYeg\n\
         mnTT1B/Ue8KGLFFfn16GKQKBgAiw5gxnbocpXPaO6/OKxFFZ+6c0OjxfN2PogWce\n\
         TU/k6ZzmShdaRKwDFXisxRJeNQ5Rx6qgS0jNFtbDhW8E8WFmQ5urCOqIOYk28EBi\n\
         At4JySm4v+5P7yYBh8B8YD2l9j57z/s8hJAxEbn/q8uHP2ddQqvQKgtsni+pHSk9\n\
         XGBfAoGBANz4qr10DdM8DHhPrAb2YItvPVz/VwkBd1Vqj8zCpyIEKe/07oKOvjWQ\n\
         SgkLDH9x2hBgY01SbP43CvPk0V72invu2TGkI/FXwXWJLLG7tDSgw4YyfhrYrHmg\n\
         1Vre3XB9HH8MYBVB6UIexaAq4xSeoemRKTBesZro7OKjKT8/GmiO\
         -----END RSA PRIVATE KEY-----";

    #[test]
    fn test_jwt_algorithm_deserialization() {
        assert_eq!(
            serde_json::from_str::<CoreAlgorithm>("\"A128CBC-HS256\"")
                .expect("failed to deserialize"),
            JsonWebTokenAlgorithm::Encryption(
                CoreJweContentEncryptionAlgorithm::Aes128CbcHmacSha256
            ),
        );
        assert_eq!(
            serde_json::from_str::<CoreAlgorithm>("\"A128GCM\"").expect("failed to deserialize"),
            JsonWebTokenAlgorithm::Encryption(CoreJweContentEncryptionAlgorithm::Aes128Gcm),
        );
        assert_eq!(
            serde_json::from_str::<CoreAlgorithm>("\"HS256\"").expect("failed to deserialize"),
            JsonWebTokenAlgorithm::Signature(CoreJwsSigningAlgorithm::HmacSha256, PhantomData),
        );
        assert_eq!(
            serde_json::from_str::<CoreAlgorithm>("\"RS256\"").expect("failed to deserialize"),
            JsonWebTokenAlgorithm::Signature(
                CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha256,
                PhantomData,
            ),
        );
        assert_eq!(
            serde_json::from_str::<CoreAlgorithm>("\"none\"").expect("failed to deserialize"),
            JsonWebTokenAlgorithm::None,
        );

        serde_json::from_str::<CoreAlgorithm>("\"invalid\"")
            .expect_err("deserialization should have failed");
    }

    #[test]
    fn test_jwt_algorithm_serialization() {
        assert_eq!(
            serde_json::to_string::<CoreAlgorithm>(&JsonWebTokenAlgorithm::Encryption(
                CoreJweContentEncryptionAlgorithm::Aes128CbcHmacSha256
            ))
            .expect("failed to serialize"),
            "\"A128CBC-HS256\"",
        );
        assert_eq!(
            serde_json::to_string::<CoreAlgorithm>(&JsonWebTokenAlgorithm::Encryption(
                CoreJweContentEncryptionAlgorithm::Aes128Gcm
            ))
            .expect("failed to serialize"),
            "\"A128GCM\"",
        );
        assert_eq!(
            serde_json::to_string::<CoreAlgorithm>(&JsonWebTokenAlgorithm::Signature(
                CoreJwsSigningAlgorithm::HmacSha256,
                PhantomData,
            ))
            .expect("failed to serialize"),
            "\"HS256\"",
        );
        assert_eq!(
            serde_json::to_string::<CoreAlgorithm>(&JsonWebTokenAlgorithm::Signature(
                CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha256,
                PhantomData,
            ))
            .expect("failed to serialize"),
            "\"RS256\"",
        );
        assert_eq!(
            serde_json::to_string::<CoreAlgorithm>(&JsonWebTokenAlgorithm::None)
                .expect("failed to serialize"),
            "\"none\"",
        );
    }

    #[derive(Clone, Debug)]
    pub struct JsonWebTokenStringPayloadSerde;
    impl JsonWebTokenPayloadSerde<String> for JsonWebTokenStringPayloadSerde {
        fn deserialize<DE: serde::de::Error>(payload: &[u8]) -> Result<String, DE> {
            Ok(String::from_utf8(payload.to_owned()).unwrap())
        }
        fn serialize(payload: &String) -> Result<String, serde_json::Error> {
            Ok(payload.to_string())
        }
    }

    #[test]
    fn test_jwt_basic() {
        fn verify_jwt<A>(jwt_access: A, key: &CoreJsonWebKey, expected_payload: &str)
        where
            A: JsonWebTokenAccess<
                CoreJweContentEncryptionAlgorithm,
                CoreJwsSigningAlgorithm,
                CoreJsonWebKeyType,
                String,
            >,
            A::ReturnType: ToString,
        {
            {
                let header = jwt_access.unverified_header();
                assert_eq!(
                    header.alg,
                    JsonWebTokenAlgorithm::Signature(
                        CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha256,
                        PhantomData,
                    )
                );
                assert_eq!(header.crit, None);
                assert_eq!(header.cty, None);
                assert_eq!(
                    header.kid,
                    Some(JsonWebKeyId::new(
                        "bilbo.baggins@hobbiton.example".to_string()
                    ))
                );
                assert_eq!(header.typ, None);
            }
            assert_eq!(jwt_access.unverified_payload_ref(), expected_payload);

            assert_eq!(
                jwt_access
                    .payload(&CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha256, key)
                    .expect("failed to validate payload")
                    .to_string(),
                expected_payload
            );
        }

        let key: CoreJsonWebKey =
            serde_json::from_str(TEST_RSA_PUB_KEY).expect("deserialization failed");

        let jwt: JsonWebToken<
            CoreJweContentEncryptionAlgorithm,
            CoreJwsSigningAlgorithm,
            CoreJsonWebKeyType,
            String,
            JsonWebTokenStringPayloadSerde,
        > = serde_json::from_value(serde_json::Value::String(TEST_JWT.to_string()))
            .expect("failed to deserialize");

        assert_eq!(
            serde_json::to_value(&jwt).expect("failed to serialize"),
            serde_json::Value::String(TEST_JWT.to_string())
        );

        verify_jwt(&jwt, &key, TEST_JWT_PAYLOAD);
        assert_eq!((&jwt).unverified_payload(), TEST_JWT_PAYLOAD);

        verify_jwt(jwt, &key, TEST_JWT_PAYLOAD);
    }

    #[test]
    fn test_new_jwt() {
        let signing_key = CoreRsaPrivateSigningKey::from_pem(
            TEST_RSA_PRIV_KEY,
            Some(JsonWebKeyId::new(
                "bilbo.baggins@hobbiton.example".to_string(),
            )),
        )
        .unwrap();
        let new_jwt = JsonWebToken::<
            CoreJweContentEncryptionAlgorithm,
            _,
            _,
            _,
            JsonWebTokenStringPayloadSerde,
        >::new(
            TEST_JWT_PAYLOAD.to_owned(),
            &signing_key,
            &CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha256,
        )
        .unwrap();
        assert_eq!(
            serde_json::to_value(&new_jwt).expect("failed to serialize"),
            serde_json::Value::String(TEST_JWT.to_string())
        );
    }

    #[test]
    fn test_invalid_signature() {
        let corrupted_jwt_str = TEST_JWT
            .to_string()
            .chars()
            .take(TEST_JWT.len() - 1)
            .collect::<String>()
            + "f";
        let jwt: JsonWebToken<
            CoreJweContentEncryptionAlgorithm,
            CoreJwsSigningAlgorithm,
            CoreJsonWebKeyType,
            String,
            JsonWebTokenStringPayloadSerde,
        > = serde_json::from_value(serde_json::Value::String(corrupted_jwt_str))
            .expect("failed to deserialize");
        let key: CoreJsonWebKey =
            serde_json::from_str(TEST_RSA_PUB_KEY).expect("deserialization failed");

        // JsonWebTokenAccess for reference.
        (&jwt)
            .payload(&CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha256, &key)
            .expect_err("signature verification should have failed");

        // JsonWebTokenAccess for owned value.
        jwt.payload(&CoreJwsSigningAlgorithm::RsaSsaPkcs1V15Sha256, &key)
            .expect_err("signature verification should have failed");
    }

    #[test]
    fn test_invalid_deserialization() {
        #[derive(Debug, Deserialize, Serialize)]
        struct TestPayload {
            foo: String,
        }

        fn expect_deserialization_err<I: Into<String>>(jwt_str: I, pattern: &str) {
            let err = serde_json::from_value::<
                JsonWebToken<
                    CoreJweContentEncryptionAlgorithm,
                    CoreJwsSigningAlgorithm,
                    CoreJsonWebKeyType,
                    TestPayload,
                    JsonWebTokenJsonPayloadSerde,
                >,
            >(serde_json::Value::String(jwt_str.into()))
            .expect_err("deserialization should have failed");

            assert!(
                err.to_string().contains(pattern),
                "Error `{}` must contain string `{}`",
                err,
                pattern,
            );
        }

        // Too many dots
        expect_deserialization_err("a.b.c.d", "found 4 parts (expected 3)");

        // Invalid header base64
        expect_deserialization_err("a!.b.c", "Invalid base64url header encoding");

        // Invalid header utf-8 (after base64 decoding)
        expect_deserialization_err("gA.b.c", "Error(\"expected value\", line: 1, column: 1)");

        // Invalid header JSON
        expect_deserialization_err("bm90X2pzb24.b.c", "Failed to parse header JSON");

        let valid_header =
            "eyJhbGciOiJSUzI1NiIsImtpZCI6ImJpbGJvLmJhZ2dpbnNAaG9iYml0b24uZXhhbXBsZSJ9";

        // Invalid payload base64
        expect_deserialization_err(
            format!("{}.b!.c", valid_header),
            "Invalid base64url payload encoding",
        );

        // Invalid payload utf-8 (after base64 decoding)
        expect_deserialization_err(
            format!("{}.gA.c", valid_header),
            "Error(\"expected value\", line: 1, column: 1)",
        );

        // Invalid payload JSON
        expect_deserialization_err(
            format!("{}.bm90X2pzb24.c", valid_header),
            "Failed to parse payload JSON",
        );

        let valid_body = "eyJmb28iOiAiYmFyIn0";

        // Invalid signature base64
        expect_deserialization_err(
            format!("{}.{}.c!", valid_header, valid_body),
            "Invalid base64url signature encoding",
        );

        let deserialized = serde_json::from_value::<
            JsonWebToken<
                CoreJweContentEncryptionAlgorithm,
                CoreJwsSigningAlgorithm,
                CoreJsonWebKeyType,
                TestPayload,
                JsonWebTokenJsonPayloadSerde,
            >,
        >(serde_json::Value::String(format!(
            "{}.{}.e2FiY30",
            valid_header, valid_body
        )))
        .expect("failed to deserialize");
        assert_eq!(deserialized.unverified_payload().foo, "bar");
    }
}
