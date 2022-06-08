use crate::algorithm::store::Store;
use crate::algorithm::SigningAlgorithm;
use crate::error::Error;
use crate::header::{BorrowedKeyHeader, Header, JoseHeader};
use crate::token::{Signed, Unsigned};
use crate::{ToBase64, Token, SEPARATOR};

/// Allow objects to be signed with a key.
pub trait SignWithKey<T> {
    fn sign_with_key(self, key: &impl SigningAlgorithm) -> Result<T, Error>;
}

/// Allow objects to be signed with a store.
pub trait SignWithStore<T> {
    fn sign_with_store<S, A>(self, store: &S) -> Result<T, Error>
    where
        S: Store<Algorithm = A>,
        A: SigningAlgorithm;
}

impl<H, C> Token<H, C, Unsigned> {
    /// Create a new unsigned token, with mutable headers and claims.
    pub fn new(header: H, claims: C) -> Self {
        Token {
            header,
            claims,
            signature: Unsigned,
        }
    }

    pub fn header_mut(&mut self) -> &mut H {
        &mut self.header
    }

    pub fn claims_mut(&mut self) -> &mut C {
        &mut self.claims
    }
}

impl<H, C> Default for Token<H, C, Unsigned>
where
    H: Default,
    C: Default,
{
    fn default() -> Self {
        Token::new(H::default(), C::default())
    }
}

impl<C: ToBase64> SignWithKey<String> for C {
    fn sign_with_key(self, key: &impl SigningAlgorithm) -> Result<String, Error> {
        let header = Header {
            algorithm: key.algorithm_type(),
            ..Default::default()
        };

        let token = Token::new(header, self).sign_with_key(key)?;
        Ok(token.signature.token_string)
    }
}

impl<'a, C: ToBase64> SignWithStore<String> for (&'a str, C) {
    fn sign_with_store<S, A>(self, store: &S) -> Result<String, Error>
    where
        S: Store<Algorithm = A>,
        A: SigningAlgorithm,
    {
        let (key_id, claims) = self;
        let key = store
            .get(key_id)
            .ok_or_else(|| Error::NoKeyWithKeyId(key_id.to_owned()))?;

        let header = BorrowedKeyHeader {
            algorithm: key.algorithm_type(),
            key_id,
        };

        let token = Token::new(header, claims).sign_with_key(key)?;
        Ok(token.signature.token_string)
    }
}

impl<H, C> SignWithKey<Token<H, C, Signed>> for Token<H, C, Unsigned>
where
    H: ToBase64 + JoseHeader,
    C: ToBase64,
{
    fn sign_with_key(self, key: &impl SigningAlgorithm) -> Result<Token<H, C, Signed>, Error> {
        let header_algorithm = self.header.algorithm_type();
        let key_algorithm = key.algorithm_type();
        if header_algorithm != key_algorithm {
            return Err(Error::AlgorithmMismatch(header_algorithm, key_algorithm));
        }

        let header = self.header.to_base64()?;
        let claims = self.claims.to_base64()?;
        let signature = key.sign(&header, &claims)?;

        let token_string = [&*header, &*claims, &signature].join(SEPARATOR);

        Ok(Token {
            header: self.header,
            claims: self.claims,
            signature: Signed { token_string },
        })
    }
}

impl<H, C> SignWithStore<Token<H, C, Signed>> for Token<H, C, Unsigned>
where
    H: ToBase64 + JoseHeader,
    C: ToBase64,
{
    fn sign_with_store<S, A>(self, store: &S) -> Result<Token<H, C, Signed>, Error>
    where
        S: Store<Algorithm = A>,
        A: SigningAlgorithm,
    {
        let key_id = self.header.key_id().ok_or(Error::NoKeyId)?;
        let key = store
            .get(key_id)
            .ok_or_else(|| Error::NoKeyWithKeyId(key_id.to_owned()))?;
        self.sign_with_key(key)
    }
}

impl<'a, H, C> Token<H, C, Signed> {
    /// Get the string representation of the token.
    pub fn as_str(&self) -> &str {
        &self.signature.token_string
    }
}

impl<H, C> From<Token<H, C, Signed>> for String {
    fn from(token: Token<H, C, Signed>) -> Self {
        token.signature.token_string
    }
}

#[cfg(test)]
mod tests {
    use std::collections::BTreeMap;

    use hmac::{Hmac, Mac};
    use serde::Serialize;
    use sha2::{Sha256, Sha512};

    use crate::algorithm::AlgorithmType;
    use crate::error::Error;
    use crate::header::Header;
    use crate::token::signed::{SignWithKey, SignWithStore};
    use crate::Token;

    #[derive(Serialize)]
    struct Claims<'a> {
        name: &'a str,
    }

    #[test]
    pub fn sign_claims() -> Result<(), Error> {
        let claims = Claims { name: "John Doe" };
        let key: Hmac<Sha256> = Hmac::new_from_slice(b"secret")?;

        let signed_token = claims.sign_with_key(&key)?;

        assert_eq!(signed_token, "eyJhbGciOiJIUzI1NiJ9.eyJuYW1lIjoiSm9obiBEb2UifQ.LlTGHPZRXbci-y349jXXN0byQniQQqwKGybzQCFIgY0");
        Ok(())
    }

    #[test]
    pub fn sign_unsigned_with_store() -> Result<(), Error> {
        let mut key_store = BTreeMap::new();
        let key1: Hmac<Sha512> = Hmac::new_from_slice(b"first")?;
        let key2: Hmac<Sha512> = Hmac::new_from_slice(b"second")?;
        key_store.insert("first_key".to_owned(), key1);
        key_store.insert("second_key".to_owned(), key2);

        let header = Header {
            algorithm: AlgorithmType::Hs512,
            key_id: Some(String::from("second_key")),
            ..Default::default()
        };
        let claims = Claims { name: "Jane Doe" };
        let token = Token::new(header, claims);
        let signed_token = token.sign_with_store(&key_store)?;

        assert_eq!(signed_token.as_str(), "eyJhbGciOiJIUzUxMiIsImtpZCI6InNlY29uZF9rZXkifQ.eyJuYW1lIjoiSmFuZSBEb2UifQ.t2ON5s8DDb2hefBIWAe0jaEcp-T7b2Wevmj0kKJ8BFxKNQURHpdh4IA-wbmBmqtiCnqTGoRdqK45hhW0AOtz0A");
        Ok(())
    }
}
