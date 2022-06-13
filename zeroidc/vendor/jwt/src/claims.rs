//! Convenience structs for commonly defined fields in claims.

use std::collections::BTreeMap;

use serde::{Deserialize, Serialize};

/// Generic [JWT claims](https://tools.ietf.org/html/rfc7519#page-8) with
/// defined fields for registered and private claims.
#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct Claims {
    #[serde(flatten)]
    pub registered: RegisteredClaims,
    #[serde(flatten)]
    pub private: BTreeMap<String, serde_json::Value>,
}

impl Claims {
    pub fn new(registered: RegisteredClaims) -> Self {
        Claims {
            registered,
            private: BTreeMap::new(),
        }
    }
}

pub type SecondsSinceEpoch = u64;

/// Registered claims according to the
/// [JWT specification](https://tools.ietf.org/html/rfc7519#page-9).
#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct RegisteredClaims {
    #[serde(rename = "iss", skip_serializing_if = "Option::is_none")]
    pub issuer: Option<String>,

    #[serde(rename = "sub", skip_serializing_if = "Option::is_none")]
    pub subject: Option<String>,

    #[serde(rename = "aud", skip_serializing_if = "Option::is_none")]
    pub audience: Option<String>,

    #[serde(rename = "exp", skip_serializing_if = "Option::is_none")]
    pub expiration: Option<SecondsSinceEpoch>,

    #[serde(rename = "nbf", skip_serializing_if = "Option::is_none")]
    pub not_before: Option<SecondsSinceEpoch>,

    #[serde(rename = "iat", skip_serializing_if = "Option::is_none")]
    pub issued_at: Option<SecondsSinceEpoch>,

    #[serde(rename = "jti", skip_serializing_if = "Option::is_none")]
    pub json_web_token_id: Option<String>,
}

#[cfg(test)]
mod tests {
    use crate::claims::Claims;
    use crate::error::Error;
    use crate::{FromBase64, ToBase64};
    use serde_json::Value;
    use std::default::Default;

    // {"iss":"mikkyang.com","exp":1302319100,"custom_claim":true}
    const ENCODED_PAYLOAD: &str =
        "eyJpc3MiOiJtaWtreWFuZy5jb20iLCJleHAiOjEzMDIzMTkxMDAsImN1c3RvbV9jbGFpbSI6dHJ1ZX0K";

    #[test]
    fn registered_claims() -> Result<(), Error> {
        let claims = Claims::from_base64(ENCODED_PAYLOAD)?;

        assert_eq!(claims.registered.issuer.unwrap(), "mikkyang.com");
        assert_eq!(claims.registered.expiration.unwrap(), 1302319100);
        Ok(())
    }

    #[test]
    fn private_claims() -> Result<(), Error> {
        let claims = Claims::from_base64(ENCODED_PAYLOAD)?;

        assert_eq!(claims.private["custom_claim"], Value::Bool(true));
        Ok(())
    }

    #[test]
    fn roundtrip() -> Result<(), Error> {
        let mut claims: Claims = Default::default();
        claims.registered.issuer = Some("mikkyang.com".into());
        claims.registered.expiration = Some(1302319100);
        let enc = claims.to_base64()?;
        assert_eq!(claims, Claims::from_base64(&*enc)?);
        Ok(())
    }
}
