//! OpenSSL support through the openssl crate.
//! Note that private keys can only be used for signing and that public keys
//! can only be used for verification.
//! ## Examples
//! ```
//! use jwt::PKeyWithDigest;
//! use openssl::hash::MessageDigest;
//! use openssl::pkey::PKey;
//! let pem = include_bytes!("../../test/rs256-public.pem");
//! let rs256_public_key = PKeyWithDigest {
//!     digest: MessageDigest::sha256(),
//!     key: PKey::public_key_from_pem(pem).unwrap(),
//! };
//! ```

use crate::algorithm::{AlgorithmType, SigningAlgorithm, VerifyingAlgorithm};
use crate::error::Error;
use crate::SEPARATOR;

use openssl::bn::BigNum;
use openssl::ecdsa::EcdsaSig;
use openssl::hash::MessageDigest;
use openssl::nid::Nid;
use openssl::pkey::{Id, PKey, Private, Public};
use openssl::sign::{Signer, Verifier};

/// A wrapper class around [PKey](../../../openssl/pkey/struct.PKey.html) that
/// associates the key with a
/// [MessageDigest](../../../openssl/hash/struct.MessageDigest.html).
pub struct PKeyWithDigest<T> {
    pub digest: MessageDigest,
    pub key: PKey<T>,
}

impl<T> PKeyWithDigest<T> {
    fn algorithm_type(&self) -> AlgorithmType {
        match (self.key.id(), self.digest.type_()) {
            (Id::RSA, Nid::SHA256) => AlgorithmType::Rs256,
            (Id::RSA, Nid::SHA384) => AlgorithmType::Rs384,
            (Id::RSA, Nid::SHA512) => AlgorithmType::Rs512,
            (Id::EC, Nid::SHA256) => AlgorithmType::Es256,
            (Id::EC, Nid::SHA384) => AlgorithmType::Es384,
            (Id::EC, Nid::SHA512) => AlgorithmType::Es512,
            _ => panic!("Invalid algorithm type"),
        }
    }
}

impl SigningAlgorithm for PKeyWithDigest<Private> {
    fn algorithm_type(&self) -> AlgorithmType {
        PKeyWithDigest::algorithm_type(self)
    }

    fn sign(&self, header: &str, claims: &str) -> Result<String, Error> {
        let mut signer = Signer::new(self.digest.clone(), &self.key)?;
        signer.update(header.as_bytes())?;
        signer.update(SEPARATOR.as_bytes())?;
        signer.update(claims.as_bytes())?;
        let signer_signature = signer.sign_to_vec()?;

        let signature = if self.key.id() == Id::EC {
            der_to_jose(&signer_signature)?
        } else {
            signer_signature
        };

        Ok(base64::encode_config(&signature, base64::URL_SAFE_NO_PAD))
    }
}

impl VerifyingAlgorithm for PKeyWithDigest<Public> {
    fn algorithm_type(&self) -> AlgorithmType {
        PKeyWithDigest::algorithm_type(self)
    }

    fn verify_bytes(&self, header: &str, claims: &str, signature: &[u8]) -> Result<bool, Error> {
        let mut verifier = Verifier::new(self.digest.clone(), &self.key)?;
        verifier.update(header.as_bytes())?;
        verifier.update(SEPARATOR.as_bytes())?;
        verifier.update(claims.as_bytes())?;

        let verified = if self.key.id() == Id::EC {
            let der = jose_to_der(signature)?;
            verifier.verify(&der)?
        } else {
            verifier.verify(signature)?
        };

        Ok(verified)
    }
}

/// OpenSSL by default signs ECDSA in DER, but JOSE expects them in a concatenated (R, S) format
fn der_to_jose(der: &[u8]) -> Result<Vec<u8>, Error> {
    let signature = EcdsaSig::from_der(&der)?;
    let r = signature.r().to_vec();
    let s = signature.s().to_vec();
    Ok([r, s].concat())
}

/// OpenSSL by default verifies ECDSA in DER, but JOSE parses out a concatenated (R, S) format
fn jose_to_der(jose: &[u8]) -> Result<Vec<u8>, Error> {
    let (r, s) = jose.split_at(jose.len() / 2);
    let ecdsa_signature =
        EcdsaSig::from_private_components(BigNum::from_slice(r)?, BigNum::from_slice(s)?)?;
    Ok(ecdsa_signature.to_der()?)
}

#[cfg(test)]
mod tests {
    use crate::algorithm::openssl::PKeyWithDigest;
    use crate::algorithm::AlgorithmType::*;
    use crate::algorithm::{SigningAlgorithm, VerifyingAlgorithm};
    use crate::error::Error;
    use crate::header::PrecomputedAlgorithmOnlyHeader as AlgOnly;
    use crate::ToBase64;

    use openssl::hash::MessageDigest;
    use openssl::pkey::PKey;

    // {"sub":"1234567890","name":"John Doe","admin":true}
    const CLAIMS: &'static str =
        "eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiYWRtaW4iOnRydWV9";

    const RS256_SIGNATURE: &'static str =
    "cQsAHF2jHvPGFP5zTD8BgoJrnzEx6JNQCpupebWLFnOc2r_punDDTylI6Ia4JZNkvy2dQP-7W-DEbFQ3oaarHsDndqUgwf9iYlDQxz4Rr2nEZX1FX0-FMEgFPeQpdwveCgjtTYUbVy37ijUySN_rW-xZTrsh_Ug-ica8t-zHRIw";

    #[test]
    fn rs256_sign() -> Result<(), Error> {
        let pem = include_bytes!("../../test/rs256-private.pem");

        let algorithm = PKeyWithDigest {
            digest: MessageDigest::sha256(),
            key: PKey::private_key_from_pem(pem)?,
        };

        let result = algorithm.sign(&AlgOnly(Rs256).to_base64()?, CLAIMS)?;
        assert_eq!(result, RS256_SIGNATURE);
        Ok(())
    }

    #[test]
    fn rs256_verify() -> Result<(), Error> {
        let pem = include_bytes!("../../test/rs256-public.pem");

        let algorithm = PKeyWithDigest {
            digest: MessageDigest::sha256(),
            key: PKey::public_key_from_pem(pem)?,
        };

        let verification_result =
            algorithm.verify(&AlgOnly(Rs256).to_base64()?, CLAIMS, RS256_SIGNATURE)?;
        assert!(verification_result);
        Ok(())
    }

    #[test]
    fn es256() -> Result<(), Error> {
        let private_pem = include_bytes!("../../test/es256-private.pem");
        let private_key = PKeyWithDigest {
            digest: MessageDigest::sha256(),
            key: PKey::private_key_from_pem(private_pem)?,
        };

        let signature = private_key.sign(&AlgOnly(Es256).to_base64()?, CLAIMS)?;

        let public_pem = include_bytes!("../../test/es256-public.pem");

        let public_key = PKeyWithDigest {
            digest: MessageDigest::sha256(),
            key: PKey::public_key_from_pem(public_pem)?,
        };

        let verification_result =
            public_key.verify(&AlgOnly(Es256).to_base64()?, CLAIMS, &*signature)?;
        assert!(verification_result);
        Ok(())
    }
}
