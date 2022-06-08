use crate::algorithm::store::Store;
use crate::algorithm::VerifyingAlgorithm;
use crate::error::Error;
use crate::header::{Header, JoseHeader};
use crate::token::{Unverified, Verified};
use crate::{FromBase64, Token, SEPARATOR};

/// Allow objects to be verified with a key.
pub trait VerifyWithKey<T> {
    fn verify_with_key(self, key: &impl VerifyingAlgorithm) -> Result<T, Error>;
}

/// Allow objects to be verified with a store.
pub trait VerifyWithStore<T> {
    fn verify_with_store<S, A>(self, store: &S) -> Result<T, Error>
    where
        S: Store<Algorithm = A>,
        A: VerifyingAlgorithm;
}

impl<'a, H: JoseHeader, C> VerifyWithKey<Token<H, C, Verified>> for Token<H, C, Unverified<'a>> {
    fn verify_with_key(
        self,
        key: &impl VerifyingAlgorithm,
    ) -> Result<Token<H, C, Verified>, Error> {
        let header = self.header();
        let header_algorithm = header.algorithm_type();
        let key_algorithm = key.algorithm_type();
        if header_algorithm != key_algorithm {
            return Err(Error::AlgorithmMismatch(header_algorithm, key_algorithm));
        }

        let Unverified {
            header_str,
            claims_str,
            signature_str,
        } = self.signature;

        if key.verify(header_str, claims_str, signature_str)? {
            Ok(Token {
                header: self.header,
                claims: self.claims,
                signature: Verified,
            })
        } else {
            Err(Error::InvalidSignature)
        }
    }
}

impl<'a, H: JoseHeader, C> VerifyWithStore<Token<H, C, Verified>> for Token<H, C, Unverified<'a>> {
    fn verify_with_store<S, A>(self, store: &S) -> Result<Token<H, C, Verified>, Error>
    where
        S: Store<Algorithm = A>,
        A: VerifyingAlgorithm,
    {
        let header = self.header();
        let key_id = header.key_id().ok_or(Error::NoKeyId)?;
        let key = store
            .get(key_id)
            .ok_or_else(|| Error::NoKeyWithKeyId(key_id.to_owned()))?;

        self.verify_with_key(key)
    }
}

impl<'a, H, C> VerifyWithKey<Token<H, C, Verified>> for &'a str
where
    H: FromBase64 + JoseHeader,
    C: FromBase64,
{
    fn verify_with_key(
        self,
        key: &impl VerifyingAlgorithm,
    ) -> Result<Token<H, C, Verified>, Error> {
        let unverified = Token::parse_unverified(self)?;
        unverified.verify_with_key(key)
    }
}

impl<'a, H, C> VerifyWithStore<Token<H, C, Verified>> for &'a str
where
    H: FromBase64 + JoseHeader,
    C: FromBase64,
{
    fn verify_with_store<S, A>(self, store: &S) -> Result<Token<H, C, Verified>, Error>
    where
        S: Store<Algorithm = A>,
        A: VerifyingAlgorithm,
    {
        let unverified: Token<H, C, _> = Token::parse_unverified(self)?;
        unverified.verify_with_store(store)
    }
}

impl<'a, C: FromBase64> VerifyWithKey<C> for &'a str {
    fn verify_with_key(self, key: &impl VerifyingAlgorithm) -> Result<C, Error> {
        let token: Token<Header, C, _> = self.verify_with_key(key)?;
        Ok(token.claims)
    }
}

impl<'a, C: FromBase64> VerifyWithStore<C> for &'a str {
    fn verify_with_store<S, A>(self, store: &S) -> Result<C, Error>
    where
        S: Store<Algorithm = A>,
        A: VerifyingAlgorithm,
    {
        let token: Token<Header, C, _> = self.verify_with_store(store)?;
        Ok(token.claims)
    }
}

impl<'a, H: FromBase64, C: FromBase64> Token<H, C, Unverified<'a>> {
    /// Not recommended. Parse the header and claims without checking the validity of the signature.
    pub fn parse_unverified(token_str: &str) -> Result<Token<H, C, Unverified>, Error> {
        let [header_str, claims_str, signature_str] = split_components(token_str)?;
        let header = H::from_base64(header_str)?;
        let claims = C::from_base64(claims_str)?;
        let signature = Unverified {
            header_str,
            claims_str,
            signature_str,
        };

        Ok(Token {
            header,
            claims,
            signature,
        })
    }
}

pub(crate) fn split_components(token: &str) -> Result<[&str; 3], Error> {
    let mut components = token.split(SEPARATOR);
    let header = components.next().ok_or(Error::NoHeaderComponent)?;
    let claims = components.next().ok_or(Error::NoClaimsComponent)?;
    let signature = components.next().ok_or(Error::NoSignatureComponent)?;

    if components.next().is_some() {
        return Err(Error::TooManyComponents);
    }

    Ok([header, claims, signature])
}

#[cfg(test)]
mod tests {
    use std::collections::{BTreeMap, HashMap};
    use std::iter::FromIterator;

    use hmac::{Hmac, Mac};
    use serde::Deserialize;
    use sha2::{Sha256, Sha512};

    use crate::algorithm::VerifyingAlgorithm;
    use crate::error::Error;
    use crate::token::verified::{VerifyWithKey, VerifyWithStore};

    #[derive(Debug, Deserialize)]
    struct Claims {
        name: String,
    }

    #[test]
    #[cfg(feature = "openssl")]
    pub fn token_can_not_be_verified_with_a_wrong_key() -> Result<(), Error> {
        use crate::{token::signed::SignWithKey, AlgorithmType, Header, PKeyWithDigest, Token};
        use openssl::{hash::MessageDigest, pkey::PKey};

        let private_pem = include_bytes!("../../test/rs256-private.pem");
        let public_pem = include_bytes!("../../test/rs256-public-2.pem");

        let rs256_private_key = PKeyWithDigest {
            digest: MessageDigest::sha256(),
            key: PKey::private_key_from_pem(private_pem).unwrap(),
        };
        let rs256_public_key = PKeyWithDigest {
            digest: MessageDigest::sha256(),
            key: PKey::public_key_from_pem(public_pem).unwrap(),
        };

        let header = Header {
            algorithm: AlgorithmType::Rs256,
            ..Default::default()
        };
        let mut claims = BTreeMap::new();
        claims.insert("sub", "someone");

        let signed_token = Token::new(header, claims).sign_with_key(&rs256_private_key)?;
        let token_str = signed_token.as_str();
        let unverified_token: Token<Header, BTreeMap<String, String>, _> =
            Token::parse_unverified(token_str)?;
        let verified_token_result = unverified_token.verify_with_key(&rs256_public_key);
        assert!(verified_token_result.is_err());
        match verified_token_result.err().unwrap() {
            Error::InvalidSignature => Ok(()),
            other => panic!("Wrong error type: {:?}", other),
        }
    }

    #[test]
    pub fn component_errors() {
        let key: Hmac<Sha256> = Hmac::new_from_slice(b"first").unwrap();

        let no_claims = "header";
        match VerifyWithKey::<String>::verify_with_key(no_claims, &key) {
            Err(Error::NoClaimsComponent) => (),
            Ok(s) => panic!("Verify should not have succeeded with output {:?}", s),
            x => panic!("Incorrect error type {:?}", x),
        }

        let no_signature = "header.claims";
        match VerifyWithKey::<String>::verify_with_key(no_signature, &key) {
            Err(Error::NoSignatureComponent) => (),
            Ok(s) => panic!("Verify should not have succeeded with output {:?}", s),
            x => panic!("Incorrect error type {:?}", x),
        }

        let too_many = "header.claims.signature.";
        match VerifyWithKey::<String>::verify_with_key(too_many, &key) {
            Err(Error::TooManyComponents) => (),
            Ok(s) => panic!("Verify should not have succeeded with output {:?}", s),
            x => panic!("Incorrect error type {:?}", x),
        }
    }

    // Test stores

    fn create_test_data<T>() -> Result<T, Error>
    where
        T: FromIterator<(&'static str, Box<dyn VerifyingAlgorithm>)>,
    {
        // Test two different algorithms in the same store
        let key1: Hmac<Sha256> = Hmac::new_from_slice(b"first")?;
        let key2: Hmac<Sha512> = Hmac::new_from_slice(b"second")?;

        let name_to_key_tuples = vec![
            ("first_key", Box::new(key1) as Box<dyn VerifyingAlgorithm>),
            ("second_key", Box::new(key2) as Box<dyn VerifyingAlgorithm>),
        ]
        .into_iter()
        .collect();

        Ok(name_to_key_tuples)
    }

    // Header   {"alg":"HS512","kid":"second_key"}
    // Claims   {"name":"Jane Doe"}
    const JANE_DOE_SECOND_KEY_TOKEN: &str = "eyJhbGciOiJIUzUxMiIsImtpZCI6InNlY29uZF9rZXkifQ.eyJuYW1lIjoiSmFuZSBEb2UifQ.t2ON5s8DDb2hefBIWAe0jaEcp-T7b2Wevmj0kKJ8BFxKNQURHpdh4IA-wbmBmqtiCnqTGoRdqK45hhW0AOtz0A";

    #[test]
    pub fn verify_claims_with_b_tree_map() -> Result<(), Error> {
        let key_store: BTreeMap<_, _> = create_test_data()?;

        let claims: Claims = JANE_DOE_SECOND_KEY_TOKEN.verify_with_store(&key_store)?;

        assert_eq!(claims.name, "Jane Doe");
        Ok(())
    }

    #[test]
    pub fn verify_claims_with_hash_map() -> Result<(), Error> {
        let key_store: HashMap<_, _> = create_test_data()?;

        let claims: Claims = JANE_DOE_SECOND_KEY_TOKEN.verify_with_store(&key_store)?;

        assert_eq!(claims.name, "Jane Doe");
        Ok(())
    }

    #[test]
    pub fn verify_claims_with_missing_key() -> Result<(), Error> {
        let key_store: BTreeMap<_, _> = create_test_data()?;
        let missing_key_token = "eyJhbGciOiJIUzUxMiIsImtpZCI6Im1pc3Npbmdfa2V5In0.eyJuYW1lIjoiSmFuZSBEb2UifQ.MC9hmBjv9OABdv5bsjVdwUgPOhvpe6a924KU-U7PjVWF2N-f_HXa1PVWtDVJ-dqt1GKutVwixrz7hgVvE_G5_w";

        let should_fail_claims: Result<Claims, _> = missing_key_token.verify_with_store(&key_store);

        match should_fail_claims {
            Err(Error::NoKeyWithKeyId(key_id)) => assert_eq!(key_id, "missing_key"),
            _ => panic!(
                "Missing key should have triggered specific error but returned {:?}",
                should_fail_claims
            ),
        }

        Ok(())
    }
}
