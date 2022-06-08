use num_bigint::{BigInt, Sign};
use ring::hmac;
use ring::rand::SecureRandom;
use ring::signature as ring_signature;

use crate::types::Base64UrlEncodedBytes;
use crate::{JsonWebKey, SignatureVerificationError, SigningError};

use super::{jwk::CoreJsonCurveType, CoreJsonWebKey, CoreJsonWebKeyType};

use std::ops::Deref;

pub fn sign_hmac(key: &[u8], hmac_alg: hmac::Algorithm, msg: &[u8]) -> hmac::Tag {
    let signing_key = hmac::Key::new(hmac_alg, key);
    hmac::sign(&signing_key, msg)
}

pub fn verify_hmac(
    key: &CoreJsonWebKey,
    hmac_alg: hmac::Algorithm,
    msg: &[u8],
    signature: &[u8],
) -> Result<(), SignatureVerificationError> {
    let k = key.k.as_ref().ok_or_else(|| {
        SignatureVerificationError::InvalidKey("Symmetric key `k` is missing".to_string())
    })?;
    let verification_key = hmac::Key::new(hmac_alg, k);
    hmac::verify(&verification_key, msg, signature)
        .map_err(|_| SignatureVerificationError::CryptoError("bad HMAC".to_string()))
}

pub fn sign_rsa(
    key: &ring_signature::RsaKeyPair,
    padding_alg: &'static dyn ring_signature::RsaEncoding,
    rng: &dyn SecureRandom,
    msg: &[u8],
) -> Result<Vec<u8>, SigningError> {
    let sig_len = key.public_modulus_len();
    let mut sig = vec![0; sig_len];
    key.sign(padding_alg, rng, msg, &mut sig)
        .map_err(|_| SigningError::CryptoError)?;
    Ok(sig)
}

fn rsa_public_key(
    key: &CoreJsonWebKey,
) -> Result<(&Base64UrlEncodedBytes, &Base64UrlEncodedBytes), String> {
    if *key.key_type() != CoreJsonWebKeyType::RSA {
        Err("RSA key required".to_string())
    } else {
        let n = key
            .n
            .as_ref()
            .ok_or_else(|| "RSA modulus `n` is missing".to_string())?;
        let e = key
            .e
            .as_ref()
            .ok_or_else(|| "RSA exponent `e` is missing".to_string())?;
        Ok((n, e))
    }
}

fn ec_public_key(
    key: &CoreJsonWebKey,
) -> Result<
    (
        &Base64UrlEncodedBytes,
        &Base64UrlEncodedBytes,
        &CoreJsonCurveType,
    ),
    String,
> {
    if *key.key_type() != CoreJsonWebKeyType::EllipticCurve {
        Err("EC key required".to_string())
    } else {
        let x = key
            .x
            .as_ref()
            .ok_or_else(|| "EC `x` part is missing".to_string())?;
        let y = key
            .y
            .as_ref()
            .ok_or_else(|| "EC `y` part is missing".to_string())?;
        let crv = key
            .crv
            .as_ref()
            .ok_or_else(|| "EC `crv` part is missing".to_string())?;
        Ok((x, y, crv))
    }
}

pub fn verify_rsa_signature(
    key: &CoreJsonWebKey,
    params: &ring_signature::RsaParameters,
    msg: &[u8],
    signature: &[u8],
) -> Result<(), SignatureVerificationError> {
    let (n, e) = rsa_public_key(key).map_err(SignatureVerificationError::InvalidKey)?;
    // let's n and e as a big integers to prevent issues with leading zeros
    // according to https://datatracker.ietf.org/doc/html/rfc7518#section-6.3.1.1
    // `n` is alwasy unsigned (hence has sign plus)

    let n_bigint = BigInt::from_bytes_be(Sign::Plus, n.deref());
    let e_bigint = BigInt::from_bytes_be(Sign::Plus, e.deref());
    let public_key = ring_signature::RsaPublicKeyComponents {
        n: &n_bigint.to_bytes_be().1,
        e: &e_bigint.to_bytes_be().1,
    };

    public_key
        .verify(params, msg, signature)
        .map_err(|_| SignatureVerificationError::CryptoError("bad signature".to_string()))
}
/// According to RFC5480, Section-2.2 implementations of Elliptic Curve Cryptography MUST support the uncompressed form.
/// The first octet of the octet string indicates whether the uncompressed or compressed form is used. For the uncompressed
/// form, the first octet has to be 0x04.
/// According to https://briansmith.org/rustdoc/ring/signature/index.html#ecdsa__fixed-details-fixed-length-pkcs11-style-ecdsa-signatures,
/// to recover the X and Y coordinates from an octet string, the Octet-String-To-Elliptic-Curve-Point Conversion
/// is used (Section 2.3.4 of https://www.secg.org/sec1-v2.pdf).

pub fn verify_ec_signature(
    key: &CoreJsonWebKey,
    params: &'static ring_signature::EcdsaVerificationAlgorithm,
    msg: &[u8],
    signature: &[u8],
) -> Result<(), SignatureVerificationError> {
    let (x, y, crv) = ec_public_key(key).map_err(SignatureVerificationError::InvalidKey)?;
    if *crv == CoreJsonCurveType::P521 {
        return Err(SignatureVerificationError::UnsupportedAlg(
            "P521".to_string(),
        ));
    }
    let mut pk = vec![0x04];
    pk.extend(x.deref());
    pk.extend(y.deref());
    let public_key = ring_signature::UnparsedPublicKey::new(params, pk);
    public_key
        .verify(msg, signature)
        .map_err(|_| SignatureVerificationError::CryptoError("EC Signature was wrong".to_string()))
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::ops::Deref;

    use crate::{
        core::{crypto::rsa_public_key, CoreJsonWebKey},
        SignatureVerificationError,
    };

    #[test]
    fn test_leading_zeros_are_parsed_correctly() {
        // The message we signed
        let msg = "THIS IS A SIGNATURE TEST";
        let signature = base64::decode_config("bg0ohqKwYHAiODeG6qkJ-6IhodN7LGPxAh4hbWeIoBdSXrXMt8Ft8U0BV7vANPvF56h20XB9C0021x2kt7iAbMgPNcZ7LCuXMPPq04DrBpMHafH5BXBwnyDKJKrzDm5sfr6OgEkcxSLHaSJ6gTWQ3waPt6_SeH2-Fi74rg13MHyX-0iqz7bZveoBbGIs5yQCwvXgrDS9zW5LUwUHozHfE6FuSi_Z92ioXeu7FHHDg1KFfg3hs8ZLx4wAX15Vw2GCQOzvyNdbItxXRLnrN1NPqxFquVNo5RGlx6ihR1Jfe7y_n0NSR2q2TuU4cIwR0LRwEaANy5SDqtleQPrTEn8nGQ", base64::URL_SAFE_NO_PAD).unwrap();
        // RSA pub key with leading 0
        let key : CoreJsonWebKey = serde_json::from_value(serde_json::json!(
            {
            "kty": "RSA",
            "e": "AQAB",
            "use": "sig",
            "kid": "TEST_KEY_ID",
            "alg": "RS256",
            "n": "AN0M6Y760b9Ok2PxDOps1TgSmiOaR9mLIfUHtZ_o-6JypOckGcl1CxrteyokOb3WyDsfIAN9fFNrycv5YoLKO7sh0IcfzNEXFgzK84HTBcGuqhN8NV98Z6N9EryUrgJYsJeVoPYm0MzkDe4NyWHhnq-9OyNCQzVELH0NhhViQqRyM92OPrJcQlk8s3ZvcgRmkd-rEtRua8SbS3GEvfvgweVy5-qcJCGoziKfx-IteMOm6yKoHvqisKb91N-qw_kSS4YQUx-DZVDo2g24F7VIbcYzJGUOU674HUF1j-wJyXzG3VV8lAXD8hABs5Lh87gr8_hIZD5gbYBJRObJk9XZbfk"
            }
        )).unwrap();

        // Old way of verifying the jwt, take the modulus directly form the JWK
        let (n, e) = rsa_public_key(&key)
            .map_err(SignatureVerificationError::InvalidKey)
            .unwrap();

        let public_key = ring_signature::RsaPublicKeyComponents {
            n: n.deref(),
            e: e.deref(),
        };
        // This fails, since ring expects the keys to have no leading zeros
        assert! {
            public_key
                .verify(
                    &ring_signature::RSA_PKCS1_2048_8192_SHA256,
                    msg.as_bytes(),
                    &signature,
                ).is_err()
        };
        // This should succeed as the function uses big-integers to actually harmonize parsing
        assert! {
            verify_rsa_signature(
                &key,
                &ring_signature::RSA_PKCS1_2048_8192_SHA256,
                msg.as_bytes(),
                &signature,
            ).is_ok()
        }
    }
}
