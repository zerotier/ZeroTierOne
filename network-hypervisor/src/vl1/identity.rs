// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::cmp::Ordering;
use std::convert::TryInto;
use std::hash::{Hash, Hasher};
use std::io::{Read, Write};
use std::str::FromStr;

use serde::{Deserialize, Deserializer, Serialize, Serializer};

use zerotier_crypto::hash::*;
use zerotier_crypto::p384::*;
use zerotier_crypto::salsa::Salsa;
use zerotier_crypto::secret::Secret;
use zerotier_crypto::x25519::*;

use zerotier_utils::hex;
use zerotier_utils::memory::{as_byte_array, as_flat_object};

use crate::error::{InvalidFormatError, InvalidParameterError};
use crate::vl1::protocol::{ADDRESS_SIZE, ADDRESS_SIZE_STRING, IDENTITY_FINGERPRINT_SIZE, IDENTITY_POW_THRESHOLD};
use crate::vl1::Address;

/// Secret keys associated with NIST P-384 public keys.
#[derive(Clone)]
pub struct IdentityP384Secret {
    pub ecdh: P384KeyPair,
    pub ecdsa: P384KeyPair,
}

/// NIST P-384 public keys and signatures binding them bidirectionally to V0 c25519 keys.
#[derive(Clone)]
pub struct IdentityP384Public {
    pub ecdh: P384PublicKey,
    pub ecdsa: P384PublicKey,
    pub ecdsa_self_signature: [u8; P384_ECDSA_SIGNATURE_SIZE],
    pub ed25519_self_signature: [u8; ED25519_SIGNATURE_SIZE],
}

/// Secret keys associated with an identity.
#[derive(Clone)]
pub struct IdentitySecret {
    pub x25519: X25519KeyPair,
    pub ed25519: Ed25519KeyPair,
    pub p384: Option<IdentityP384Secret>,
}

/// A unique identity on the global VL1 network.
///
/// Identity implements serde Serialize and Deserialize. Identities are serialized as strings
/// for human-readable formats and binary otherwise.
///
/// SECURITY NOTE: for security reasons secret keys are NOT exported by default by to_string()
/// or the default marshal() in Marshalable. You must use to_string_with_options() and
/// marshal_with_options() to get secrets. The clone() method on the other hand does duplicate
/// secrets so as not to violate the contract of creating an exact duplicate of the object.
/// There is a clone_without_secrets() if this isn't wanted.
#[derive(Clone)]
pub struct Identity {
    pub address: Address,
    pub x25519: [u8; C25519_PUBLIC_KEY_SIZE],
    pub ed25519: [u8; ED25519_PUBLIC_KEY_SIZE],
    pub p384: Option<IdentityP384Public>,
    pub secret: Option<IdentitySecret>,
    pub fingerprint: [u8; IDENTITY_FINGERPRINT_SIZE],
}

#[inline(always)]
fn concat_arrays_2<const A: usize, const B: usize, const S: usize>(a: &[u8; A], b: &[u8; B]) -> [u8; S] {
    assert_eq!(A + B, S);
    let mut tmp = [0_u8; S];
    tmp[..A].copy_from_slice(a);
    tmp[A..].copy_from_slice(b);
    tmp
}

#[inline(always)]
fn concat_arrays_4<const A: usize, const B: usize, const C: usize, const D: usize, const S: usize>(
    a: &[u8; A],
    b: &[u8; B],
    c: &[u8; C],
    d: &[u8; D],
) -> [u8; S] {
    assert_eq!(A + B + C + D, S);
    let mut tmp = [0_u8; S];
    tmp[..A].copy_from_slice(a);
    tmp[A..(A + B)].copy_from_slice(b);
    tmp[(A + B)..(A + B + C)].copy_from_slice(c);
    tmp[(A + B + C)..].copy_from_slice(d);
    tmp
}

fn zt_address_derivation_work_function(digest: &mut [u8; 64]) {
    const ADDRESS_DERIVATION_HASH_MEMORY_SIZE: usize = 2097152;
    unsafe {
        let genmem_layout = std::alloc::Layout::from_size_align(ADDRESS_DERIVATION_HASH_MEMORY_SIZE, 16).unwrap(); // aligned for access as u64 or u8
        let genmem: *mut u8 = std::alloc::alloc(genmem_layout);
        assert!(!genmem.is_null());

        let mut salsa: Salsa<20> = Salsa::new(&digest[..32], &digest[32..40]);
        salsa.crypt(&[0_u8; 64], &mut *genmem.cast::<[u8; 64]>());
        let mut k = 0;
        while k < (ADDRESS_DERIVATION_HASH_MEMORY_SIZE - 64) {
            let i = k + 64;
            salsa.crypt(&*genmem.add(k).cast::<[u8; 64]>(), &mut *genmem.add(i).cast::<[u8; 64]>());
            k = i;
        }

        #[cfg(any(target_arch = "x86", target_arch = "x86_64", target_arch = "aarch64", target_arch = "powerpc64"))]
        let digest_buf = &mut *digest.as_mut_ptr().cast::<[u64; 8]>();

        #[cfg(not(any(target_arch = "x86", target_arch = "x86_64", target_arch = "aarch64", target_arch = "powerpc64")))]
        let mut digest_buf: [u64; 8] = std::mem::MaybeUninit::uninit().assume_init();
        #[cfg(not(any(target_arch = "x86", target_arch = "x86_64", target_arch = "aarch64", target_arch = "powerpc64")))]
        std::ptr::copy_nonoverlapping(digest.as_ptr(), digest_buf.as_mut_ptr().cast(), 64);

        let mut i = 0;
        while i < ADDRESS_DERIVATION_HASH_MEMORY_SIZE {
            let idx1 = *genmem.add(i + 7) % 8; // same as: u64::from_be(*genmem.add(i).cast::<u64>()) % 8;
            let idx2 = (u64::from_be(*genmem.add(i + 8).cast::<u64>()) % ((ADDRESS_DERIVATION_HASH_MEMORY_SIZE / 8) as u64)) * 8;
            i += 16;
            let genmem_idx2 = genmem.add(idx2 as usize).cast::<u64>();
            let digest_idx1 = digest_buf.as_mut_ptr().add(idx1 as usize);
            let tmp = *genmem_idx2;
            *genmem_idx2 = *digest_idx1;
            *digest_idx1 = tmp;
            salsa.crypt_in_place(&mut *digest_buf.as_mut_ptr().cast::<[u8; 64]>());
        }

        #[cfg(not(any(target_arch = "x86", target_arch = "x86_64", target_arch = "aarch64", target_arch = "powerpc64")))]
        std::ptr::copy_nonoverlapping(digest_buf.as_ptr().cast(), digest.as_mut_ptr(), 64);

        std::alloc::dealloc(genmem, genmem_layout);
    }
}

impl Identity {
    /// Length of an x25519-only public identity in byte array form.
    pub const BYTE_LENGTH_X25519_PUBLIC: usize = ADDRESS_SIZE + 1 + C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + 1 + 1 + 2;

    /// Length of an x25519-only secret identity in byte array form.
    pub const BYTE_LENGTH_X25519_SECRET: usize = Self::BYTE_LENGTH_X25519_PUBLIC + C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE;

    /// Length of a new dual-key public identity in byte array form.
    pub const BYTE_LENGTH_X25519P384_PUBLIC: usize = Self::BYTE_LENGTH_X25519_PUBLIC
        + 1
        + P384_PUBLIC_KEY_SIZE
        + P384_PUBLIC_KEY_SIZE
        + P384_ECDSA_SIGNATURE_SIZE
        + ED25519_SIGNATURE_SIZE;

    /// Length of a new dual-key secret identity in byte array form.
    pub const BYTE_LENGTH_X25519P384_SECRET: usize = Self::BYTE_LENGTH_X25519P384_PUBLIC
        + C25519_SECRET_KEY_SIZE
        + ED25519_SECRET_KEY_SIZE
        + P384_SECRET_KEY_SIZE
        + P384_SECRET_KEY_SIZE;

    const ALGORITHM_X25519: u8 = 0x01;
    const ALGORITHM_EC_NIST_P384: u8 = 0x02;
    const FLAG_INCLUDES_SECRET: u8 = 0x80;

    /// Generate a new identity.
    pub fn generate() -> Self {
        // First generate an identity with just x25519 keys and derive its address.
        let mut sha = SHA512::new();
        let ed25519 = Ed25519KeyPair::generate();
        let ed25519_pub = ed25519.public_bytes();
        let address;
        let mut x25519;
        let mut x25519_pub;
        loop {
            x25519 = X25519KeyPair::generate();
            x25519_pub = x25519.public_bytes();

            sha.update(&x25519_pub);
            sha.update(&ed25519_pub);
            let mut digest = sha.finish();
            zt_address_derivation_work_function(&mut digest);

            if digest[0] < IDENTITY_POW_THRESHOLD {
                let addr = Address::from_bytes(&digest[59..64]);
                if addr.is_some() {
                    address = addr.unwrap();
                    break;
                }
            }

            sha.reset();
        }
        let mut id = Self {
            address,
            x25519: x25519_pub,
            ed25519: ed25519_pub,
            p384: None,
            secret: Some(IdentitySecret { x25519, ed25519, p384: None }),
            fingerprint: [0_u8; IDENTITY_FINGERPRINT_SIZE], // replaced in upgrade()
        };

        // Then "upgrade" to add NIST P-384 keys and compute fingerprint.
        assert!(id.upgrade().is_ok());
        assert!(id.p384.is_some() && id.secret.as_ref().unwrap().p384.is_some());

        id
    }

    /// Upgrade older x25519-only identities to hybrid identities with both x25519 and NIST P-384 curves.
    ///
    /// The boolean indicates whether or not an upgrade occurred. An error occurs if this identity is
    /// invalid or missing its private key(s). This does nothing if no upgrades are possible.
    ///
    /// NOTE: upgrading is not deterministic. This generates a new set of NIST P-384 keys and the new
    /// identity contains these and a signature by the original keys and by the new keys to bind them
    /// together. However repeated calls to upgrade() will generate different secondary keys. This should
    /// only be used once to upgrade and then save a new identity.
    ///
    /// It would be possible to change this in the future, with care.
    pub fn upgrade(&mut self) -> Result<bool, InvalidParameterError> {
        if self.secret.is_none() {
            return Err(InvalidParameterError(
                "an identity can only be upgraded if it includes its private key",
            ));
        }
        if self.p384.is_none() {
            let p384_ecdh = P384KeyPair::generate();
            let p384_ecdsa = P384KeyPair::generate();

            let mut self_sign_buf: Vec<u8> = Vec::with_capacity(
                ADDRESS_SIZE
                    + C25519_PUBLIC_KEY_SIZE
                    + ED25519_PUBLIC_KEY_SIZE
                    + P384_PUBLIC_KEY_SIZE
                    + P384_PUBLIC_KEY_SIZE
                    + P384_ECDSA_SIGNATURE_SIZE
                    + 4,
            );
            let _ = self_sign_buf.write_all(&self.address.to_bytes());
            let _ = self_sign_buf.write_all(&self.x25519);
            let _ = self_sign_buf.write_all(&self.ed25519);
            self_sign_buf.push(Self::ALGORITHM_EC_NIST_P384);
            let _ = self_sign_buf.write_all(p384_ecdh.public_key_bytes());
            let _ = self_sign_buf.write_all(p384_ecdsa.public_key_bytes());

            // Sign all keys including the x25519 ones with the new P-384 keys.
            let ecdsa_self_signature = p384_ecdsa.sign(self_sign_buf.as_slice());

            // Sign everything with the original ed25519 key to bind the new key pairs. Include ECDSA
            // signature because ECDSA signatures are randomized and we want only this specific one.
            // Identities should be rigid. (Ed25519 signatures are deterministic.)
            let _ = self_sign_buf.write_all(&ecdsa_self_signature);
            let ed25519_self_signature = self.secret.as_ref().unwrap().ed25519.sign(self_sign_buf.as_slice());

            let _ = self.p384.insert(IdentityP384Public {
                ecdh: p384_ecdh.public_key().clone(),
                ecdsa: p384_ecdsa.public_key().clone(),
                ecdsa_self_signature,
                ed25519_self_signature,
            });
            let _ = self
                .secret
                .as_mut()
                .unwrap()
                .p384
                .insert(IdentityP384Secret { ecdh: p384_ecdh, ecdsa: p384_ecdsa });

            self.fingerprint = SHA384::hash(self.to_public_bytes().as_bytes());

            return Ok(true);
        }
        return Ok(false);
    }

    /// Create a clone minus any secret key it holds.
    pub fn clone_without_secret(&self) -> Identity {
        Self {
            address: self.address,
            x25519: self.x25519,
            ed25519: self.ed25519,
            p384: self.p384.clone(),
            secret: None,
            fingerprint: self.fingerprint,
        }
    }

    /// Locally check the validity of this identity.
    ///
    /// This is somewhat time consuming due to the memory-intensive work algorithm.
    pub fn validate_identity(&self) -> bool {
        if let Some(p384) = self.p384.as_ref() {
            let mut self_sign_buf: Vec<u8> = Vec::with_capacity(
                ADDRESS_SIZE + 4 + C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P384_PUBLIC_KEY_SIZE + P384_PUBLIC_KEY_SIZE,
            );
            let _ = self_sign_buf.write_all(&self.address.to_bytes());
            let _ = self_sign_buf.write_all(&self.x25519);
            let _ = self_sign_buf.write_all(&self.ed25519);
            self_sign_buf.push(Self::ALGORITHM_EC_NIST_P384);
            let _ = self_sign_buf.write_all(p384.ecdh.as_bytes());
            let _ = self_sign_buf.write_all(p384.ecdsa.as_bytes());

            if !p384.ecdsa.verify(self_sign_buf.as_slice(), &p384.ecdsa_self_signature) {
                return false;
            }

            let _ = self_sign_buf.write_all(&p384.ecdsa_self_signature);
            if !ed25519_verify(&self.ed25519, &p384.ed25519_self_signature, self_sign_buf.as_slice()) {
                return false;
            }
        }

        // NOTE: fingerprint is always computed on generation or deserialize so no need to check.

        let mut sha = SHA512::new();
        sha.update(&self.x25519);
        sha.update(&self.ed25519);
        let mut digest = sha.finish();
        zt_address_derivation_work_function(&mut digest);

        return digest[0] < IDENTITY_POW_THRESHOLD && Address::from_bytes(&digest[59..64]).map_or(false, |a| a == self.address);
    }

    /// Perform ECDH key agreement, returning a shared secret or None on error.
    ///
    /// An error can occur if this identity does not hold its secret portion or if either key is invalid.
    ///
    /// For new identities with P-384 keys a hybrid agreement is performed using both X25519 and NIST P-384 ECDH.
    /// The final key is derived as HMAC(x25519 secret, p-384 secret) to yield a FIPS-compliant key agreement with
    /// the X25519 secret being used as a "salt" as far as FIPS is concerned.
    pub fn agree(&self, other: &Identity) -> Option<Secret<64>> {
        if let Some(secret) = self.secret.as_ref() {
            let c25519_secret: Secret<64> = Secret(SHA512::hash(&secret.x25519.agree(&other.x25519).0));

            // FIPS note: FIPS-compliant exchange algorithms must be the last algorithms in any HKDF chain
            // for the final result to be technically FIPS compliant. Non-FIPS algorithm secrets are considered
            // a salt in the HMAC(salt, key) HKDF construction.
            if secret.p384.is_some() && other.p384.is_some() {
                secret
                    .p384
                    .as_ref()
                    .unwrap()
                    .ecdh
                    .agree(&other.p384.as_ref().unwrap().ecdh)
                    .map(|p384_secret| Secret(hmac_sha512(&c25519_secret.0, &p384_secret.0)))
            } else {
                Some(c25519_secret)
            }
        } else {
            None
        }
    }

    /// Sign a message with this identity.
    ///
    /// Identities with P-384 keys sign with that unless legacy_ed25519_only is selected. If this is
    /// set the old 96-byte signature plus hash format used in ZeroTier v1 is used.
    ///
    /// A return of None happens if we don't have our secret key(s) or some other error occurs.
    pub fn sign(&self, msg: &[u8], legacy_ed25519_only: bool) -> Option<Vec<u8>> {
        if let Some(secret) = self.secret.as_ref() {
            if legacy_ed25519_only {
                Some(secret.ed25519.sign_zt(msg).to_vec())
            } else if let Some(p384s) = secret.p384.as_ref() {
                let mut tmp: Vec<u8> = Vec::with_capacity(1 + P384_ECDSA_SIGNATURE_SIZE);
                tmp.push(Self::ALGORITHM_EC_NIST_P384);
                let _ = tmp.write_all(&p384s.ecdsa.sign(msg));
                Some(tmp)
            } else {
                let mut tmp: Vec<u8> = Vec::with_capacity(1 + ED25519_SIGNATURE_SIZE);
                tmp.push(Self::ALGORITHM_X25519);
                let _ = tmp.write_all(&secret.ed25519.sign(msg));
                Some(tmp)
            }
        } else {
            None
        }
    }

    /// Verify a signature against this identity.
    pub fn verify(&self, msg: &[u8], signature: &[u8]) -> bool {
        if signature.len() == 96 {
            // LEGACY: ed25519-only signature with hash included, detected by having a unique size of 96 bytes
            return ed25519_verify(&self.ed25519, &signature[..64], msg);
        } else if let Some(algorithm) = signature.get(0) {
            if *algorithm == Self::ALGORITHM_EC_NIST_P384 && signature.len() == (1 + P384_ECDSA_SIGNATURE_SIZE) {
                if let Some(p384) = self.p384.as_ref() {
                    return p384.ecdsa.verify(msg, &signature[1..]);
                }
            } else if *algorithm == Self::ALGORITHM_X25519 && signature.len() == (1 + ED25519_SIGNATURE_SIZE) {
                return ed25519_verify(&self.ed25519, &signature[1..], msg);
            }
        }
        return false;
    }

    pub fn to_public_bytes(&self) -> IdentityBytes {
        if let Some(p384) = self.p384.as_ref() {
            IdentityBytes::X25519P384Public(
                as_byte_array(&packed::V1 {
                    v0: packed::V0 {
                        address: self.address.to_bytes(),
                        key_type: 0,
                        x25519: self.x25519,
                        ed25519: self.ed25519,
                        secret_length: 0,
                        reserved: 0x03,
                        ext_len: ((Self::BYTE_LENGTH_X25519P384_PUBLIC - Self::BYTE_LENGTH_X25519_PUBLIC) as u16).to_be_bytes(),
                    },
                    key_type_flags: Self::ALGORITHM_EC_NIST_P384,
                    ecdh: p384.ecdh.as_bytes().clone(),
                    ecdsa: p384.ecdsa.as_bytes().clone(),
                    ecdsa_self_signature: p384.ecdsa_self_signature,
                    ed25519_self_signature: p384.ed25519_self_signature,
                })
                .clone(),
            )
        } else {
            IdentityBytes::X25519Public(
                as_byte_array(&packed::V0 {
                    address: self.address.to_bytes(),
                    key_type: 0,
                    x25519: self.x25519,
                    ed25519: self.ed25519,
                    secret_length: 0,
                    reserved: 0x03,
                    ext_len: [0; 2],
                })
                .clone(),
            )
        }
    }

    pub fn to_secret_bytes(&self) -> Option<IdentityBytes> {
        self.secret.as_ref().map(|s| {
            if let Some(p384) = s.p384.as_ref() {
                IdentityBytes::X25519P384Secret(
                    as_byte_array(&packed::V1S {
                        v0s: packed::V0S {
                            address: self.address.to_bytes(),
                            key_type: 0,
                            x25519: self.x25519,
                            ed25519: self.ed25519,
                            secret_length: (C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE) as u8,
                            x25519_secret: s.x25519.secret_bytes().0.clone(),
                            ed25519_secret: s.ed25519.secret_bytes().0.clone(),
                            reserved: 0x03,
                            ext_len: ((Self::BYTE_LENGTH_X25519P384_SECRET - Self::BYTE_LENGTH_X25519_SECRET) as u16).to_be_bytes(),
                        },
                        key_type_flags: Self::ALGORITHM_EC_NIST_P384 | Self::FLAG_INCLUDES_SECRET,
                        ecdh: p384.ecdh.public_key_bytes().clone(),
                        ecdsa: p384.ecdsa.public_key_bytes().clone(),
                        ecdsa_self_signature: self.p384.as_ref().unwrap().ecdsa_self_signature,
                        ed25519_self_signature: self.p384.as_ref().unwrap().ed25519_self_signature,
                        ecdh_secret: p384.ecdh.secret_key_bytes().0.clone(),
                        ecdsa_secret: p384.ecdsa.secret_key_bytes().0.clone(),
                    })
                    .clone(),
                )
            } else {
                IdentityBytes::X25519Secret(
                    as_byte_array(&packed::V0S {
                        address: self.address.to_bytes(),
                        key_type: 0,
                        x25519: self.x25519,
                        ed25519: self.ed25519,
                        secret_length: (C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE) as u8,
                        x25519_secret: s.x25519.secret_bytes().0.clone(),
                        ed25519_secret: s.ed25519.secret_bytes().0.clone(),
                        reserved: 0x03,
                        ext_len: [0; 2],
                    })
                    .clone(),
                )
            }
        })
    }

    /// Convert a byte respresentation into an identity.
    ///
    /// WARNING: this performs basic sanity checking but does NOT perform a full validation of address derivation or self-signatures.
    pub fn from_bytes(bytes: &IdentityBytes) -> Option<Self> {
        let mut id = match bytes {
            IdentityBytes::X25519Public(b) => {
                let b: &packed::V0 = as_flat_object(b);
                if b.key_type == 0 && b.secret_length == 0 && b.reserved == 0x03 && u16::from_be_bytes(b.ext_len) == 0 {
                    Some(Self {
                        address: Address::from_bytes_fixed(&b.address)?,
                        x25519: b.x25519,
                        ed25519: b.ed25519,
                        p384: None,
                        secret: None,
                        fingerprint: [0; 48],
                    })
                } else {
                    None
                }
            }
            IdentityBytes::X25519Secret(b) => {
                let b: &packed::V0S = as_flat_object(b);
                if b.key_type == 0
                    && b.secret_length == (C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE) as u8
                    && b.reserved == 0x03
                    && u16::from_be_bytes(b.ext_len) == 0
                {
                    Some(Self {
                        address: Address::from_bytes_fixed(&b.address)?,
                        x25519: b.x25519,
                        ed25519: b.ed25519,
                        p384: None,
                        secret: Some(IdentitySecret {
                            x25519: X25519KeyPair::from_bytes(&b.x25519, &b.x25519_secret)?,
                            ed25519: Ed25519KeyPair::from_bytes(&b.ed25519, &b.ed25519_secret)?,
                            p384: None,
                        }),
                        fingerprint: [0; 48],
                    })
                } else {
                    None
                }
            }
            IdentityBytes::X25519P384Public(b) => {
                let b: &packed::V1 = as_flat_object(b);
                if b.v0.key_type == 0
                    && b.v0.secret_length == 0
                    && b.v0.reserved == 0x03
                    && u16::from_be_bytes(b.v0.ext_len) == (Self::BYTE_LENGTH_X25519P384_PUBLIC - Self::BYTE_LENGTH_X25519_PUBLIC) as u16
                    && b.key_type_flags == Self::ALGORITHM_EC_NIST_P384
                {
                    Some(Self {
                        address: Address::from_bytes_fixed(&b.v0.address)?,
                        x25519: b.v0.x25519,
                        ed25519: b.v0.ed25519,
                        p384: Some(IdentityP384Public {
                            ecdh: P384PublicKey::from_bytes(&b.ecdh)?,
                            ecdsa: P384PublicKey::from_bytes(&b.ecdsa)?,
                            ecdsa_self_signature: b.ecdsa_self_signature,
                            ed25519_self_signature: b.ed25519_self_signature,
                        }),
                        secret: None,
                        fingerprint: [0; 48],
                    })
                } else {
                    None
                }
            }
            IdentityBytes::X25519P384Secret(b) => {
                let b: &packed::V1S = as_flat_object(b);
                if b.v0s.key_type == 0
                    && b.v0s.secret_length == (C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE) as u8
                    && b.v0s.reserved == 0x03
                    && u16::from_be_bytes(b.v0s.ext_len) == (Self::BYTE_LENGTH_X25519P384_SECRET - Self::BYTE_LENGTH_X25519_SECRET) as u16
                    && b.key_type_flags == (Self::ALGORITHM_EC_NIST_P384 | Self::FLAG_INCLUDES_SECRET)
                {
                    Some(Self {
                        address: Address::from_bytes_fixed(&b.v0s.address)?,
                        x25519: b.v0s.x25519,
                        ed25519: b.v0s.ed25519,
                        p384: Some(IdentityP384Public {
                            ecdh: P384PublicKey::from_bytes(&b.ecdh)?,
                            ecdsa: P384PublicKey::from_bytes(&b.ecdsa)?,
                            ecdsa_self_signature: b.ecdsa_self_signature,
                            ed25519_self_signature: b.ed25519_self_signature,
                        }),
                        secret: Some(IdentitySecret {
                            x25519: X25519KeyPair::from_bytes(&b.v0s.x25519, &b.v0s.x25519_secret)?,
                            ed25519: Ed25519KeyPair::from_bytes(&b.v0s.ed25519, &b.v0s.ed25519_secret)?,
                            p384: Some(IdentityP384Secret {
                                ecdh: P384KeyPair::from_bytes(&b.ecdh, &b.ecdh_secret)?,
                                ecdsa: P384KeyPair::from_bytes(&b.ecdsa, &b.ecdsa_secret)?,
                            }),
                        }),
                        fingerprint: [0; 48],
                    })
                } else {
                    None
                }
            }
        };
        let fingerprint = SHA384::hash(id.as_ref().unwrap().to_public_bytes().as_bytes());
        id.as_mut().unwrap().fingerprint = fingerprint;
        id
    }

    /// Read an identity from a reader, inferring its total length from the stream.
    pub fn read_bytes<R: Read>(r: &mut R) -> std::io::Result<Self> {
        let mut buf = [0_u8; 512];
        r.read_exact(&mut buf[..Self::BYTE_LENGTH_X25519_PUBLIC])?;
        let x25519_public: &packed::V0 = as_flat_object(&buf);
        let ext_len = u16::from_be_bytes(x25519_public.ext_len) as usize;
        let obj_len = if x25519_public.secret_length == 0 {
            let obj_len = ext_len + Self::BYTE_LENGTH_X25519_PUBLIC;
            if ext_len > 0 {
                r.read_exact(&mut buf[Self::BYTE_LENGTH_X25519_PUBLIC..obj_len])?;
            }
            obj_len
        } else {
            let obj_len = ext_len + Self::BYTE_LENGTH_X25519_SECRET;
            if ext_len > 0 {
                r.read_exact(&mut buf[Self::BYTE_LENGTH_X25519_PUBLIC..obj_len])?;
            }
            obj_len
        };
        IdentityBytes::try_from(&buf[..obj_len]).map_or_else(
            |_| Err(std::io::Error::new(std::io::ErrorKind::Other, "invalid identity")),
            |b| {
                Identity::from_bytes(&b).map_or_else(
                    || Err(std::io::Error::new(std::io::ErrorKind::Other, "invalid identity")),
                    |id| Ok(id),
                )
            },
        )
    }

    fn to_string_internal(&self, include_private: bool) -> String {
        let mut s = String::with_capacity(1024);
        s.push_str(self.address.to_string().as_str());

        s.push_str(":0:"); // 0 used for x25519 for legacy reasons just like in marshal()
        s.push_str(hex::to_string(&self.x25519).as_str());
        s.push_str(hex::to_string(&self.ed25519).as_str());
        if self.secret.is_some() && include_private {
            let secret = self.secret.as_ref().unwrap();
            s.push(':');
            s.push_str(hex::to_string(secret.x25519.secret_bytes().as_bytes()).as_str());
            s.push_str(hex::to_string(secret.ed25519.secret_bytes().as_bytes()).as_str());
        }

        if let Some(p384) = self.p384.as_ref() {
            if self.secret.is_none() || !include_private {
                s.push(':');
            }
            s.push_str(":2:"); // 2 == IDENTITY_ALGORITHM_EC_NIST_P384
            let p384_joined: [u8; P384_PUBLIC_KEY_SIZE + P384_PUBLIC_KEY_SIZE + P384_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE] =
                concat_arrays_4(
                    p384.ecdh.as_bytes(),
                    p384.ecdsa.as_bytes(),
                    &p384.ecdsa_self_signature,
                    &p384.ed25519_self_signature,
                );
            s.push_str(base64::encode_config(p384_joined, base64::URL_SAFE_NO_PAD).as_str());
            if self.secret.is_some() && include_private {
                let secret = self.secret.as_ref().unwrap();
                if secret.p384.is_some() {
                    let p384_secret = secret.p384.as_ref().unwrap();
                    let p384_secret_joined: [u8; P384_SECRET_KEY_SIZE + P384_SECRET_KEY_SIZE] = concat_arrays_2(
                        p384_secret.ecdh.secret_key_bytes().as_bytes(),
                        p384_secret.ecdsa.secret_key_bytes().as_bytes(),
                    );
                    s.push(':');
                    s.push_str(base64::encode_config(p384_secret_joined, base64::URL_SAFE_NO_PAD).as_str());
                }
            }
        }

        s
    }

    #[inline(always)]
    pub fn to_public_string(&self) -> String {
        self.to_string_internal(false)
    }

    #[inline(always)]
    pub fn to_secret_string(&self) -> String {
        self.to_string_internal(true)
    }
}

impl ToString for Identity {
    #[inline(always)]
    fn to_string(&self) -> String {
        self.to_string_internal(false)
    }
}

impl FromStr for Identity {
    type Err = InvalidFormatError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let fields_v: Vec<&str> = s.split(':').collect();
        let fields = fields_v.as_slice();

        if fields.len() < 3 || fields[0].len() != ADDRESS_SIZE_STRING {
            return Err(InvalidFormatError);
        }
        let address = Address::from_str(fields[0]).map_err(|_| InvalidFormatError)?;

        // x25519 public, x25519 secret, p384 public, p384 secret
        let mut keys: [Option<&str>; 4] = [None, None, None, None];

        let mut ptr = 1;
        let mut state = 0;
        let mut key_ptr = 0;
        while ptr < fields.len() {
            match state {
                0 => {
                    if fields[ptr] == "0" || fields[ptr] == "1" {
                        key_ptr = 0;
                    } else if fields[ptr] == "2" {
                        key_ptr = 2;
                    } else {
                        return Err(InvalidFormatError);
                    }
                    state = 1;
                }
                1 | 2 => {
                    let _ = keys[key_ptr].replace(fields[ptr]);
                    key_ptr += 1;
                    state = (state + 1) % 3;
                }
                _ => {
                    return Err(InvalidFormatError);
                }
            }
            ptr += 1;
        }

        let keys = [
            hex::from_string(keys[0].unwrap_or("")),
            hex::from_string(keys[1].unwrap_or("")),
            base64::decode_config(keys[2].unwrap_or(""), base64::URL_SAFE_NO_PAD).unwrap_or_else(|_| Vec::new()),
            base64::decode_config(keys[3].unwrap_or(""), base64::URL_SAFE_NO_PAD).unwrap_or_else(|_| Vec::new()),
        ];
        if keys[0].len() != C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE {
            return Err(InvalidFormatError);
        }
        if !keys[2].is_empty()
            && keys[2].len() != P384_PUBLIC_KEY_SIZE + P384_PUBLIC_KEY_SIZE + P384_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE
        {
            return Err(InvalidFormatError);
        }
        if !keys[3].is_empty() && keys[3].len() != P384_SECRET_KEY_SIZE + P384_SECRET_KEY_SIZE {
            return Err(InvalidFormatError);
        }

        let mut sha = SHA384::new();
        sha.update(&address.to_bytes());
        sha.update(&keys[0].as_slice()[0..64]);
        if !keys[2].is_empty() {
            sha.update(&[Self::ALGORITHM_EC_NIST_P384]);
            sha.update(&keys[2].as_slice()[0..(P384_PUBLIC_KEY_SIZE * 2)]);
        }

        let mut id = Ok(Identity {
            address,
            x25519: keys[0].as_slice()[0..32].try_into().unwrap(),
            ed25519: keys[0].as_slice()[32..64].try_into().unwrap(),
            p384: if keys[2].is_empty() {
                None
            } else {
                let ecdh = P384PublicKey::from_bytes(&keys[2].as_slice()[..P384_PUBLIC_KEY_SIZE]);
                let ecdsa = P384PublicKey::from_bytes(&keys[2].as_slice()[P384_PUBLIC_KEY_SIZE..(P384_PUBLIC_KEY_SIZE * 2)]);
                if ecdh.is_none() || ecdsa.is_none() {
                    return Err(InvalidFormatError);
                }
                Some(IdentityP384Public {
                    ecdh: ecdh.unwrap(),
                    ecdsa: ecdsa.unwrap(),
                    ecdsa_self_signature: keys[2].as_slice()
                        [(P384_PUBLIC_KEY_SIZE * 2)..((P384_PUBLIC_KEY_SIZE * 2) + P384_ECDSA_SIGNATURE_SIZE)]
                        .try_into()
                        .unwrap(),
                    ed25519_self_signature: keys[2].as_slice()[((P384_PUBLIC_KEY_SIZE * 2) + P384_ECDSA_SIGNATURE_SIZE)..]
                        .try_into()
                        .unwrap(),
                })
            },
            secret: if keys[1].is_empty() {
                None
            } else {
                if keys[1].len() != C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE {
                    return Err(InvalidFormatError);
                }
                Some(IdentitySecret {
                    x25519: {
                        let tmp = X25519KeyPair::from_bytes(&keys[0].as_slice()[0..32], &keys[1].as_slice()[0..32]);
                        if tmp.is_none() {
                            return Err(InvalidFormatError);
                        }
                        tmp.unwrap()
                    },
                    ed25519: {
                        let tmp = Ed25519KeyPair::from_bytes(&keys[0].as_slice()[32..64], &keys[1].as_slice()[32..64]);
                        if tmp.is_none() {
                            return Err(InvalidFormatError);
                        }
                        tmp.unwrap()
                    },
                    p384: if keys[3].is_empty() {
                        None
                    } else {
                        Some(IdentityP384Secret {
                            ecdh: {
                                let tmp = P384KeyPair::from_bytes(
                                    &keys[2].as_slice()[..P384_PUBLIC_KEY_SIZE],
                                    &keys[3].as_slice()[..P384_SECRET_KEY_SIZE],
                                );
                                if tmp.is_none() {
                                    return Err(InvalidFormatError);
                                }
                                tmp.unwrap()
                            },
                            ecdsa: {
                                let tmp = P384KeyPair::from_bytes(
                                    &keys[2].as_slice()[P384_PUBLIC_KEY_SIZE..(P384_PUBLIC_KEY_SIZE * 2)],
                                    &keys[3].as_slice()[P384_SECRET_KEY_SIZE..],
                                );
                                if tmp.is_none() {
                                    return Err(InvalidFormatError);
                                }
                                tmp.unwrap()
                            },
                        })
                    },
                })
            },
            fingerprint: [0; 48],
        });

        let fingerprint = SHA384::hash(id.as_ref().unwrap().to_public_bytes().as_bytes());
        id.as_mut().unwrap().fingerprint = fingerprint;

        id
    }
}

impl PartialEq for Identity {
    #[inline(always)]
    fn eq(&self, other: &Self) -> bool {
        self.fingerprint == other.fingerprint
    }
}

impl Eq for Identity {}

impl Ord for Identity {
    #[inline(always)]
    fn cmp(&self, other: &Self) -> Ordering {
        self.address
            .cmp(&other.address)
            .then_with(|| self.fingerprint.cmp(&other.fingerprint))
    }
}

impl PartialOrd for Identity {
    #[inline(always)]
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Hash for Identity {
    #[inline(always)]
    fn hash<H: Hasher>(&self, state: &mut H) {
        state.write_u64(self.address.into())
    }
}

mod packed {
    use super::*;

    #[derive(Copy, Clone)]
    #[repr(C, packed)]
    pub(super) struct V0 {
        pub address: [u8; ADDRESS_SIZE],
        pub key_type: u8,
        pub x25519: [u8; C25519_PUBLIC_KEY_SIZE],
        pub ed25519: [u8; ED25519_PUBLIC_KEY_SIZE],
        pub secret_length: u8,
        pub reserved: u8,
        pub ext_len: [u8; 2],
    }

    #[derive(Copy, Clone)]
    #[repr(C, packed)]
    pub(super) struct V0S {
        pub address: [u8; ADDRESS_SIZE],
        pub key_type: u8,
        pub x25519: [u8; C25519_PUBLIC_KEY_SIZE],
        pub ed25519: [u8; ED25519_PUBLIC_KEY_SIZE],
        pub secret_length: u8,
        pub x25519_secret: [u8; C25519_SECRET_KEY_SIZE],
        pub ed25519_secret: [u8; ED25519_SECRET_KEY_SIZE],
        pub reserved: u8,
        pub ext_len: [u8; 2],
    }

    #[derive(Copy, Clone)]
    #[repr(C, packed)]
    pub(super) struct V1 {
        pub v0: V0,
        pub key_type_flags: u8,
        pub ecdh: [u8; P384_PUBLIC_KEY_SIZE],
        pub ecdsa: [u8; P384_PUBLIC_KEY_SIZE],
        pub ecdsa_self_signature: [u8; P384_ECDSA_SIGNATURE_SIZE],
        pub ed25519_self_signature: [u8; ED25519_SIGNATURE_SIZE],
    }

    #[derive(Copy, Clone)]
    #[repr(C, packed)]
    pub(super) struct V1S {
        pub v0s: V0S,
        pub key_type_flags: u8,
        pub ecdh: [u8; P384_PUBLIC_KEY_SIZE],
        pub ecdsa: [u8; P384_PUBLIC_KEY_SIZE],
        pub ecdsa_self_signature: [u8; P384_ECDSA_SIGNATURE_SIZE],
        pub ed25519_self_signature: [u8; ED25519_SIGNATURE_SIZE],
        pub ecdh_secret: [u8; P384_SECRET_KEY_SIZE],
        pub ecdsa_secret: [u8; P384_SECRET_KEY_SIZE],
    }
}

/// Identity rendered as a flat byte array.
///
/// Note that try_from() and into() here perform a straight conversion or cast between this and
/// byte slices. They don't check the actual format of the contained identity data.
#[derive(Clone, PartialEq, Eq)]
pub enum IdentityBytes {
    X25519Public([u8; Identity::BYTE_LENGTH_X25519_PUBLIC]),
    X25519Secret([u8; Identity::BYTE_LENGTH_X25519_SECRET]),
    X25519P384Public([u8; Identity::BYTE_LENGTH_X25519P384_PUBLIC]),
    X25519P384Secret([u8; Identity::BYTE_LENGTH_X25519P384_SECRET]),
}

impl IdentityBytes {
    #[inline(always)]
    pub fn as_bytes(&self) -> &[u8] {
        self.into()
    }
}

impl<'a> From<&'a IdentityBytes> for &'a [u8] {
    fn from(b: &'a IdentityBytes) -> Self {
        match b {
            IdentityBytes::X25519Public(b) => b,
            IdentityBytes::X25519Secret(b) => b,
            IdentityBytes::X25519P384Public(b) => b,
            IdentityBytes::X25519P384Secret(b) => b,
        }
    }
}

impl TryFrom<&[u8]> for IdentityBytes {
    type Error = std::array::TryFromSliceError;

    fn try_from(b: &[u8]) -> Result<Self, Self::Error> {
        match b.len() {
            Identity::BYTE_LENGTH_X25519_PUBLIC => Ok(Self::X25519Public(b.try_into()?)),
            Identity::BYTE_LENGTH_X25519_SECRET => Ok(Self::X25519Secret(b.try_into()?)),
            Identity::BYTE_LENGTH_X25519P384_PUBLIC => Ok(Self::X25519P384Public(b.try_into()?)),
            _ => Ok(Self::X25519P384Secret(b.try_into()?)),
        }
    }
}

impl Drop for IdentityBytes {
    fn drop(&mut self) {
        // This can contain secrets, so zero it like Secret<> in the crypto core.
        unsafe {
            for i in 0..std::mem::size_of::<Self>() {
                std::ptr::write_volatile((self as *mut Self).cast::<u8>().add(i), 0);
            }
        }
    }
}

impl Serialize for Identity {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        if serializer.is_human_readable() {
            serializer.serialize_str(self.to_public_string().as_str())
        } else {
            serializer.serialize_bytes((&self.to_public_bytes()).into())
        }
    }
}

struct IdentityVisitor;

impl<'de> serde::de::Visitor<'de> for IdentityVisitor {
    type Value = Identity;

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("a ZeroTier identity")
    }

    fn visit_bytes<E>(self, v: &[u8]) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        IdentityBytes::try_from(v).map_or_else(
            |e| Err(E::custom(e.to_string())),
            |b| Identity::from_bytes(&b).map_or_else(|| Err(E::custom("invalid identity")), |id| Ok(id)),
        )
    }

    fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        Identity::from_str(v).map_err(|e| E::custom(e.to_string()))
    }
}

impl<'de> Deserialize<'de> for Identity {
    fn deserialize<D>(deserializer: D) -> Result<Identity, D::Error>
    where
        D: Deserializer<'de>,
    {
        if deserializer.is_human_readable() {
            deserializer.deserialize_str(IdentityVisitor)
        } else {
            deserializer.deserialize_bytes(IdentityVisitor)
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::vl1::identity::*;
    use std::str::FromStr;
    use zerotier_utils::hex;

    #[test]
    fn v0_identity() {
        let self_agree_expected =
            hex::from_string("de904fc90ff3a2b96b739b926e623113f5334c80841b654509b77916c4c4a6eb0ca69ec6ed01a7f04aee17c546b30ba4");

        // Test self-agree with a known good x25519-only (v0) identity.
        let id = Identity::from_str(
            "728efdb79d:0:3077ed0084d8d48a3ac628af6b45d9351e823bff34bc4376cddfc77a3d73a966c7d347bdcc1244d0e99e1b9c961ff5e963092e90ca43b47ff58c114d2d699664:2afaefcd1dca336ed59957eb61919b55009850b0b7088af3ee142672b637d1d49cc882b30a006f9eee42f2211ef8fe1cbe99a16a4436737fc158ce2243c15f12",
        )
        .unwrap();
        assert!(id.validate_identity());
        let self_agree = id.agree(&id).unwrap();
        assert!(self_agree_expected.as_slice().eq(&self_agree.as_bytes()[..48]));

        // Identity should be upgradable.
        let mut upgraded = id.clone();
        assert!(upgraded.upgrade().unwrap());

        // Upgraded identity should generate the same result when agreeing with the old non-upgraded identity.
        let self_agree = id.agree(&upgraded).unwrap();
        assert!(self_agree_expected.as_slice().eq(&self_agree.as_bytes()[..48]));
        let self_agree = upgraded.agree(&id).unwrap();
        assert!(self_agree_expected.as_slice().eq(&self_agree.as_bytes()[..48]));
    }

    const GOOD_V0_IDENTITIES: [&'static str; 4] = [
        "8ee1095428:0:3ee30bb0cf66098891a5375aa8b44c4e7d09fabfe6d04e150bc7f17898726f1b1b8dc16f7cc74ed4eeb06e224db4370668766829434faf3da26ecfb151c87c12:69031e4b2354d41010f7b097f4793e99040342ca641938525e3f72a081a75285bea3c399edecda738c772f59412469a8290405e3e327fb30f3654af49ff8de09",
        "77fcbbd875:0:1724aad9ef6af50ab7a67ed975053779ca1a0251832ef6456cff50bf5af3bb1f859885b67c7ff6a64192e795e7dcdc9ce7b13deb9177022a4a83c02026596993:55c3b96396853f41ba898d7099ca118ba3ba1d306af55248dcbd7008e6752b8900e208a251eeda70f778249dab65a5dfbb4beeaf76de40bf3b732536f93fc7f7",
        "91c4e0e1b0:0:5a96fb6bddbc3e845ec30e369b6517dd936e9b9679404001ba81c66dfe38be7a12f5db4f470f4af2ff4aa3e2fe54a3838c80b3a33fe83fe78fef956772c46ed3:7210ce5b7bc4777c7790d225f81e7f2583417a3ac64fd1a5873186ed6bd5b48126c8e1cfd0e82b391a389547bd3c143c672f83e19632aa445cafb2d5aab4c098",
        "ba0c4a4edd:0:4b75790dce1979b4cec38ca1eb81e0f348f757047c4ad5e8a463fe54f32142739ffd8c0bc9c95a45572d96173a11def1e653e6975343e4bc78d5b504e023aab8:28fa6bf3c103186c41575c91ee86887d21e0bdf77cdf4c36c9430c32e83affbee0b04da61312f4c990a18f2acf9031a6a2c4c69362f79f7f6d5621a3c8abf33c",
    ];
    const GOOD_V1_IDENTITIES: [&'static str; 4] = [
        "75a8a7a199:0:6322fe1ca7941571458bb0bd14faff0af915c2ca5ea5856a682de1040c8cbd1f79054a0c052b253316b17abd9cf34609c6ac0e26dd85ad169c26aa980a69d5e0:b8fcd2a25a0708176d81455a7b576b6835d87cc7b6b4701c914d65688f730f6d1a725018472570440e0ba1d6038be81e3415e8ba6dfa685ae2b582b12b90ad67:2:Az1IZD-cuJS11XWhCMEc6ZPwMpM_nQOwCHqHJaOywRxtkU9UC8BfOdGxYet-7fh3lgMxKntz_iqWD-7PAXoTpg2bDo-hKRT4zLkNm-KnUvqjiXl9W7RLYhvVd_qUXxQlY709m9hQ-omy_zRi3ZIysOMDaPwfyuwWUzhB09FiWn-MjiiUrujqqx66VfZ9rx_u63ZUvrTxWu6motVbS6eXMemIGUtU6UyhIXDIk2_WJceF9k7Bs7C7Ay00zuCTEHIH878e7LR4qPNnPPDRxQV3y5rO7WHsutl9wHSEINJAtYz7xJdk7IuccJfnDzhODroATwdSNtG0sASJU9UToUvWioEF:P5zAldkVK9P5uso9iOQ2hqgXS-J326gK-Z3l1_ZZehes8-0OYxUoHfs80ddG1MVGIOb3AFA0vi7S93wNUxRkg4kzzUl5rFAmy85iuiMCRpPY-8SG1500RI4dhWkRTuPR",
        "a311a9d217:0:477c48a712daf785d7d5f2d2d693361047b102dbcd7eea2d6cabcb7149b0806815e4c4d7cd03a9b3ac87c3e27161724a5ad5c52d1c487c5484869e55e34966e5:f06d080800e3529ac2f5229cb1c07c6761fa1f2f1bc2447807f814e38252b24d6faf2da1ab6fb7db2460081dae877b9658714f3a2976f9ffcbe432151f69c3eb:2:A9_zhw9S4Kh-iOfCqEYjKg9Hyd9OabtE3F4FwlX87AEbeGRl0bRTOnqRNLpZPbwtNwJVJwKaY7dFCSc7Z03TJDSnFwz4HXy5KfdLEMHkJUv8ebfgNMuyc52f5ku3eyNE41uduXppdKzDbEJXiyBElMzaFBwK0q1zuDrRM1sjlaUto6bwsB8wPFC08PHsU98-sgJQXsWYYIlzhFRlW5Cjm-4QAfGOQ4BX1M5LLfzwxV0G4s895iLTAShb_fms8ITqiOGL4CjLFX2HYJ2fYanzrEFH3eYSQjZw0iu1iXVnrcaHpeICuFOIbbrR3Uhewa_kwyzHqIa1pUPONYak6Rje5PcD:1tf-W-tMafIY3VprzVNvcBtyC9i0JQCkVwUpTo7ej8wwdbRHntijqxcyZGEwQ0f_c-sucRPxzYIpA9l5V-QnXRNbWNNuA-1lHINJUVD4DCEc9Km2yJiWl2AmXVpN51KE",
        "4a74fb8416:0:a6ce3d09a384e6d0b28c1f18ca63a95e3d15fb6eaa3177b3b6bc9f11c3dd1b312b50aaed5b919bafadd9c1e902e1f1f4d3944d43f289a01f9408974b9fe49455:10f8f8557856274c9d78b111ce2fe5e696e43a3139d6c685c6d620bb4946427873c80272f3573f460e156f8ac9eb4a8cf7aa7e9e7df152491be23405dd0fdf86:2:A55YK8R0MYVQi47wlDZxk7Ja3o_ahA88AxzmzQon79MF8HmMbxExwovGEk2TkuQ4xQLgWsM_vs-N-nlJUtUkxsd3ECHYs2s68LxzvpcmROIxakMr02ibclwJGnA_deivI2so8RjGsv4d18Up9lt3qNkor8N6SBuyhhEYPjjwFYKMn7oCqj_LDxVshYa9YtcHtjTABQPiKJaqJzpyZfquKrrofr18SBqJOPgtsw6Omuqv-IY384a-uT-51ic-RhW5eVqWQdVFj3-mIJEWUXIk9TKaAuh5soLFK9awXogG_cXCHTzQ9_POiJ8KW8VSE7OzC46wzYgllWXUKzKZCmwJ-7YP:IoLCcuTeUPiRv_TSSBkPX9ps0pfpcww9WR39SQsi4Q0H4sDk1q5tIgeLMNvwk2weIZpMo28n0evlyvuXw1P3LEEm2wY97D3ceGgwKgnAHwERLh6pM9de1_nZYAQYZOiS",
        "466906c1fc:0:7e1df36857083f367b0301af07709748d074eef482f500493280abf22a95e3260ef73caf4496d4b847f6b6f0b9e45abc46c3fa4bb46bb35af207dac98a713609:58ff3e8bac5f1960152d4db8f149c426da2c5b29e53a4e22d86e8b10aee9c44453cd4d19103dd63fa920e9a6a7ff52ac326ed13499eea7b05a44911aaff85524:2:A4BWd-ehtSraBUI7NY_XgI-WHMbN6yVlAJQDiRH8vTk4824OCyNjQ19K5jGuLeEfawJtXG5njz03bvezpOToESZ0HKxPMz-6nz3fYpXMi-0bM6yyNij37t-Gb1_Ee_7JrLplVNfVUKokeG3I6W2W0pWWuSYeyfSSgXwdc3MkfIbLU7xPcR-7UU1FXHzSRRq9OxO3QMe15kVYpSFoxNGCyaMzDwgSLiXxqD3exi9zlKnlZl743vZuZmCjwq0bA7LT20GwT7Zr4qSnC7_XWokj8vofNlUKUziF5zfB5uM3Ml9_zz0HRc-oj42zrdWkXuJbJ7zXjXiFy82kNUllKykoTDoJ:bgOVQS8FrzIrIq_pNvwQfYDJ9HdxAPeAvfN_J8XxDFO2gvV-LGZmR_WWsiNiXVUhF9Y0mU_yZjw7kbenObxYua0OpTXpn-9TAQS1eYDCvR015eAGzpwFjlT85HJWbYfO",
    ];

    #[test]
    fn marshal_unmarshal_sign_verify_agree() {
        let gen = Identity::generate();
        assert!(gen.agree(&gen).is_some());
        assert!(gen.validate_identity());
        let bytes = gen.to_secret_bytes().unwrap();
        let string = gen.to_secret_string();
        assert!(Identity::from_str(string.as_str()).unwrap().eq(&gen));

        let gen_unmarshaled = Identity::from_bytes(&bytes).unwrap();
        assert!(gen_unmarshaled.secret.is_some());
        if !gen_unmarshaled.eq(&gen) {
            println!(
                "{} != {}",
                hex::to_string(&gen_unmarshaled.fingerprint),
                hex::to_string(&gen.fingerprint)
            );
        }

        assert!(Identity::from_str(string.as_str()).unwrap().secret.is_some());

        let gen2 = Identity::generate();
        assert!(gen2.validate_identity());
        assert!(gen2.agree(&gen).unwrap().eq(&gen.agree(&gen2).unwrap()));

        for id_str in GOOD_V0_IDENTITIES {
            let mut id = Identity::from_str(id_str).unwrap();
            assert_eq!(id.to_secret_string().as_str(), id_str);

            assert!(id.validate_identity());
            assert!(id.p384.is_none());

            let idb = id.to_secret_bytes().unwrap();
            let id_unmarshal = Identity::from_bytes(&idb).unwrap();
            assert!(id == id_unmarshal);
            assert!(id_unmarshal.secret.is_some());

            let idb2 = id_unmarshal.to_public_bytes();
            let id_unmarshal2 = Identity::from_bytes(&idb2).unwrap();
            assert!(id_unmarshal2 == id_unmarshal);
            assert!(id_unmarshal2 == id);
            assert!(id_unmarshal2.secret.is_none());

            let ids = id.to_string();
            assert!(Identity::from_str(ids.as_str()).unwrap() == id);

            assert!(id.upgrade().is_ok());
            assert!(id.validate_identity());
            assert!(id.p384.is_some());
            assert!(id.secret.as_ref().unwrap().p384.is_some());

            let ids = id.to_string();
            assert!(Identity::from_str(ids.as_str()).unwrap() == id);
        }
        for id_str in GOOD_V1_IDENTITIES {
            let id = Identity::from_str(id_str).unwrap();
            assert_eq!(id.to_secret_string().as_str(), id_str);

            assert!(id.validate_identity());
            assert!(id.p384.is_some());
            assert!(id.secret.as_ref().unwrap().p384.is_some());

            let idb = id.to_secret_bytes().unwrap();
            let id_unmarshal = Identity::from_bytes(&idb).unwrap();
            assert!(id == id_unmarshal);

            let idb2 = id_unmarshal.to_public_bytes();
            let id_unmarshal2 = Identity::from_bytes(&idb2).unwrap();
            assert!(id_unmarshal2 == id_unmarshal);
            assert!(id_unmarshal2 == id);

            let ids = id.to_string();
            assert!(Identity::from_str(ids.as_str()).unwrap() == id);
        }
    }
}
