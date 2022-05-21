// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::alloc::{alloc, dealloc, Layout};
use std::cmp::Ordering;
use std::convert::TryInto;
use std::hash::{Hash, Hasher};
use std::io::Write;
use std::mem::MaybeUninit;
use std::ptr::{slice_from_raw_parts, slice_from_raw_parts_mut};
use std::str::FromStr;

use lazy_static::lazy_static;

use serde::{Deserialize, Deserializer, Serialize, Serializer};

use zerotier_core_crypto::hash::*;
use zerotier_core_crypto::hex;
use zerotier_core_crypto::p384::*;
use zerotier_core_crypto::salsa::Salsa;
use zerotier_core_crypto::secret::Secret;
use zerotier_core_crypto::x25519::*;

use crate::error::{InvalidFormatError, InvalidParameterError};
use crate::util::buffer::Buffer;
use crate::util::marshalable::Marshalable;
use crate::util::pool::{Pool, PoolFactory, Pooled};
use crate::vl1::protocol::{ADDRESS_SIZE, ADDRESS_SIZE_STRING, IDENTITY_POW_THRESHOLD};
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
    pub c25519: C25519KeyPair,
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
    pub c25519: [u8; C25519_PUBLIC_KEY_SIZE],
    pub ed25519: [u8; ED25519_PUBLIC_KEY_SIZE],
    pub p384: Option<IdentityP384Public>,
    pub secret: Option<IdentitySecret>,
    pub fingerprint: [u8; SHA512_HASH_SIZE],
}

#[inline(always)]
fn concat_arrays_2<const A: usize, const B: usize, const S: usize>(a: &[u8; A], b: &[u8; B]) -> [u8; S] {
    assert_eq!(A + B, S);
    let mut tmp: [u8; S] = unsafe { MaybeUninit::uninit().assume_init() };
    tmp[..A].copy_from_slice(a);
    tmp[A..].copy_from_slice(b);
    tmp
}

#[inline(always)]
fn concat_arrays_4<const A: usize, const B: usize, const C: usize, const D: usize, const S: usize>(a: &[u8; A], b: &[u8; B], c: &[u8; C], d: &[u8; D]) -> [u8; S] {
    assert_eq!(A + B + C + D, S);
    let mut tmp: [u8; S] = unsafe { MaybeUninit::uninit().assume_init() };
    tmp[..A].copy_from_slice(a);
    tmp[A..(A + B)].copy_from_slice(b);
    tmp[(A + B)..(A + B + C)].copy_from_slice(c);
    tmp[(A + B + C)..].copy_from_slice(d);
    tmp
}

impl Identity {
    /// Curve25519 and Ed25519
    pub const ALGORITHM_X25519: u8 = 0x01;

    /// NIST P-384 ECDH and ECDSA
    pub const ALGORITHM_EC_NIST_P384: u8 = 0x02;

    /// Bit mask to include all algorithms.
    pub const ALGORITHM_ALL: u8 = 0xff;

    /// Generate a new identity.
    pub fn generate() -> Self {
        // First generate an identity with just x25519 keys and derive its address.
        let mut sha = SHA512::new();
        let ed25519 = Ed25519KeyPair::generate();
        let ed25519_pub = ed25519.public_bytes();
        let address;
        let mut c25519;
        let mut c25519_pub;
        let mut genmem_pool_obj = ADDRESS_DERVIATION_MEMORY_POOL.get();
        loop {
            c25519 = C25519KeyPair::generate();
            c25519_pub = c25519.public_bytes();

            sha.update(&c25519_pub);
            sha.update(&ed25519_pub);
            let mut digest = sha.finish();
            zt_address_derivation_memory_intensive_hash(&mut digest, &mut genmem_pool_obj);

            if digest[0] < IDENTITY_POW_THRESHOLD {
                let addr = Address::from_bytes(&digest[59..64]);
                if addr.is_some() {
                    address = addr.unwrap();
                    break;
                }
            }

            sha.reset();
        }
        drop(genmem_pool_obj);
        let mut id = Self {
            address,
            c25519: c25519_pub,
            ed25519: ed25519_pub,
            p384: None,
            secret: Some(IdentitySecret { c25519, ed25519, p384: None }),
            fingerprint: [0_u8; 64], // replaced in upgrade()
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
    pub fn upgrade(&mut self) -> Result<bool, InvalidParameterError> {
        if self.secret.is_none() {
            return Err(InvalidParameterError("an identity can only be upgraded if it includes its private key"));
        }
        if self.p384.is_none() {
            let p384_ecdh = P384KeyPair::generate();
            let p384_ecdsa = P384KeyPair::generate();

            let mut self_sign_buf: Vec<u8> = Vec::with_capacity(ADDRESS_SIZE + C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P384_PUBLIC_KEY_SIZE + P384_PUBLIC_KEY_SIZE + P384_ECDSA_SIGNATURE_SIZE + 4);
            let _ = self_sign_buf.write_all(&self.address.to_bytes());
            let _ = self_sign_buf.write_all(&self.c25519);
            let _ = self_sign_buf.write_all(&self.ed25519);
            self_sign_buf.push(Self::ALGORITHM_EC_NIST_P384);
            let _ = self_sign_buf.write_all(p384_ecdh.public_key_bytes());
            let _ = self_sign_buf.write_all(p384_ecdsa.public_key_bytes());

            // Fingerprint includes only the above fields, so calc before appending the ECDSA signature.
            self.fingerprint = SHA512::hash(self_sign_buf.as_slice());

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
            let _ = self.secret.as_mut().unwrap().p384.insert(IdentityP384Secret { ecdh: p384_ecdh, ecdsa: p384_ecdsa });

            return Ok(true);
        }
        return Ok(false);
    }

    /// Create a clone minus any secret key it holds.
    pub fn clone_without_secret(&self) -> Identity {
        Self {
            address: self.address,
            c25519: self.c25519.clone(),
            ed25519: self.ed25519.clone(),
            p384: self.p384.clone(),
            secret: None,
            fingerprint: self.fingerprint.clone(),
        }
    }

    /// Get a bit mask of algorithms present in this identity.
    pub fn algorithms(&self) -> u8 {
        if self.p384.is_some() {
            Self::ALGORITHM_X25519 | Self::ALGORITHM_EC_NIST_P384
        } else {
            Self::ALGORITHM_X25519
        }
    }

    /// Locally check the validity of this identity.
    ///
    /// This is somewhat time consuming due to the memory-intensive work algorithm.
    pub fn validate_identity(&self) -> bool {
        if self.p384.is_some() {
            let p384 = self.p384.as_ref().unwrap();

            let mut self_sign_buf: Vec<u8> = Vec::with_capacity(ADDRESS_SIZE + 4 + C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P384_PUBLIC_KEY_SIZE + P384_PUBLIC_KEY_SIZE);
            let _ = self_sign_buf.write_all(&self.address.to_bytes());
            let _ = self_sign_buf.write_all(&self.c25519);
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
        sha.update(&self.c25519);
        sha.update(&self.ed25519);
        let mut digest = sha.finish();
        let mut genmem_pool_obj = ADDRESS_DERVIATION_MEMORY_POOL.get();
        zt_address_derivation_memory_intensive_hash(&mut digest, &mut genmem_pool_obj);
        drop(genmem_pool_obj);

        return digest[0] < IDENTITY_POW_THRESHOLD && Address::from_bytes(&digest[59..64]).map_or(false, |a| a == self.address);
    }

    /// Perform ECDH key agreement, returning a shared secret or None on error.
    ///
    /// An error can occur if this identity does not hold its secret portion or if either key is invalid.
    ///
    /// If both sides have NIST P-384 keys then key agreement is performed using both Curve25519 and
    /// NIST P-384 and the result is HMAC(Curve25519 secret, NIST P-384 secret).
    ///
    /// Nothing actually uses a 512-bit secret directly, but if the base secret is 512 bits then
    /// no entropy is lost when deriving smaller secrets with a KDF.
    pub fn agree(&self, other: &Identity) -> Option<Secret<64>> {
        self.secret.as_ref().and_then(|secret| {
            let c25519_secret = Secret(SHA512::hash(&secret.c25519.agree(&other.c25519).0));

            // FIPS note: FIPS-compliant exchange algorithms must be the last algorithms in any HKDF chain
            // for the final result to be technically FIPS compliant. Non-FIPS algorithm secrets are considered
            // a salt in the HMAC(salt, key) HKDF construction.
            if secret.p384.is_some() && other.p384.is_some() {
                secret.p384.as_ref().unwrap().ecdh.agree(&other.p384.as_ref().unwrap().ecdh).map(|p384_secret| Secret(hmac_sha512(&c25519_secret.0, &p384_secret.0)))
            } else {
                Some(c25519_secret)
            }
        })
    }

    /// Sign a message with this identity.
    ///
    /// If legacy_compatibility is true this generates only an ed25519 signature and uses the old
    /// format that also includes part of the plaintext hash at the end. The include_algorithms mask
    /// will be ignored. Otherwise it will generate a signature for every algorithm with a secret
    /// in this identity and that is specified in the include_algorithms bit mask.
    ///
    /// A return of None happens if we don't have our secret key(s) or some other error occurs.
    pub fn sign(&self, msg: &[u8], include_algorithms: u8, legacy_compatibility: bool) -> Option<Vec<u8>> {
        if self.secret.is_some() {
            let secret = self.secret.as_ref().unwrap();
            if legacy_compatibility {
                Some(secret.ed25519.sign_zt(msg).to_vec())
            } else {
                let mut tmp: Vec<u8> = Vec::with_capacity(1 + P384_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE);
                tmp.push(0);
                if secret.p384.is_some() && (include_algorithms & Self::ALGORITHM_EC_NIST_P384) != 0 {
                    *tmp.first_mut().unwrap() |= Self::ALGORITHM_EC_NIST_P384;
                    let _ = tmp.write_all(&secret.p384.as_ref().unwrap().ecdsa.sign(msg));
                }
                if (include_algorithms & Self::ALGORITHM_X25519) != 0 {
                    *tmp.first_mut().unwrap() |= Self::ALGORITHM_X25519;
                    let _ = tmp.write_all(&secret.ed25519.sign(msg));
                }
                if tmp.len() > 1 {
                    Some(tmp)
                } else {
                    None
                }
            }
        } else {
            None
        }
    }

    /// Verify a signature against this identity.
    pub fn verify(&self, msg: &[u8], mut signature: &[u8]) -> bool {
        if signature.len() == 96 {
            // legacy ed25519-only signature with hash included detected by their unique size.
            return ed25519_verify(&self.ed25519, signature, msg);
        } else if signature.len() > 1 {
            // Otherwise we support compound signatures. Note that it's possible for there to be
            // unknown algorithms here if we ever add e.g. a PQ signature scheme and older nodes
            // don't support it, and therefore it's valid if all algorithms that are present and
            // understood pass signature check. The 'passed' variable makes sure we can't pass without
            // verifying at least one signature. If any present and understood algorithm fails the
            // whole check fails, so you can't have one good and one bad signature.
            let algorithms = signature[0];
            signature = &signature[1..];
            let mut passed = false; // makes sure we can't pass with an empty signature!
            if (algorithms & Self::ALGORITHM_EC_NIST_P384) != 0 && signature.len() >= P384_ECDSA_SIGNATURE_SIZE && self.p384.is_some() {
                if !self.p384.as_ref().unwrap().ecdsa.verify(msg, &signature[..P384_ECDSA_SIGNATURE_SIZE]) {
                    return false;
                }
                signature = &signature[P384_ECDSA_SIGNATURE_SIZE..];
                passed = true;
            }
            if (algorithms & Self::ALGORITHM_X25519) != 0 && signature.len() >= ED25519_SIGNATURE_SIZE {
                if !ed25519_verify(&self.ed25519, &signature[..ED25519_SIGNATURE_SIZE], msg) {
                    return false;
                }
                //signature = &signature[ED25519_SIGNATURE_SIZE..];
                passed = true;
            }
            return passed;
        } else {
            return false;
        }
    }

    pub fn to_buffer_with_options(&self, include_algorithms: u8, include_private: bool) -> Buffer<{ Self::MAX_MARSHAL_SIZE }> {
        let mut b: Buffer<{ Self::MAX_MARSHAL_SIZE }> = Buffer::new();
        assert!(self.marshal_with_options(&mut b, include_algorithms, include_private).is_ok());
        b
    }

    pub fn marshal_with_options<const BL: usize>(&self, buf: &mut Buffer<BL>, include_algorithms: u8, include_private: bool) -> std::io::Result<()> {
        let algorithms = self.algorithms() & include_algorithms;
        let secret = self.secret.as_ref();

        buf.append_bytes_fixed(&self.address.to_bytes())?;

        if (algorithms & Self::ALGORITHM_X25519) != 0 {
            buf.append_u8(0x00)?; // 0x00 is used for X25519 for backward compatibility with v0 identities
            buf.append_bytes_fixed(&self.c25519)?;
            buf.append_bytes_fixed(&self.ed25519)?;
            if include_private && secret.is_some() {
                let secret = secret.unwrap();
                buf.append_u8((C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE) as u8)?;
                buf.append_bytes_fixed(&secret.c25519.secret_bytes().0)?;
                buf.append_bytes_fixed(&secret.ed25519.secret_bytes().0)?;
            } else {
                buf.append_u8(0)?;
            }
        }

        /*
         * For legacy backward compatibility, any key pairs and other material after the x25519
         * keys are prefixed by 0x03 followed by the number of remaining bytes. This allows old nodes
         * to parse HELLO normally and ignore the rest of the extended identity. It's ignored by
         * newer nodes.
         */
        buf.append_u8(0x03)?;
        let remaining_data_size_field_at = buf.len();
        buf.append_padding(0, 2)?;

        if (algorithms & Self::ALGORITHM_EC_NIST_P384) != 0 && self.p384.is_some() {
            let p384 = self.p384.as_ref().unwrap();
            let p384s = if include_private { secret.clone().map_or(None, |s| s.p384.as_ref()) } else { None };

            buf.append_u8(Self::ALGORITHM_EC_NIST_P384)?;
            buf.append_varint(if p384s.is_some() {
                ((P384_PUBLIC_KEY_SIZE * 2) + P384_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE + (P384_SECRET_KEY_SIZE * 2)) as u64
            } else {
                ((P384_PUBLIC_KEY_SIZE * 2) + P384_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE) as u64
            })?;
            buf.append_bytes_fixed(p384.ecdh.as_bytes())?;
            buf.append_bytes_fixed(p384.ecdsa.as_bytes())?;
            buf.append_bytes_fixed(&p384.ecdsa_self_signature)?;
            buf.append_bytes_fixed(&p384.ed25519_self_signature)?;
            if let Some(p384s) = p384s {
                buf.append_bytes_fixed(&p384s.ecdh.secret_key_bytes().0)?;
                buf.append_bytes_fixed(&p384s.ecdsa.secret_key_bytes().0)?;
            }
        }

        buf.append_u8(0xff)?;

        // Fill in the remaining data field earmarked above.
        *buf.bytes_fixed_mut_at(remaining_data_size_field_at).unwrap() = ((buf.len() - remaining_data_size_field_at) as u16).to_be_bytes();

        Ok(())
    }

    /// Marshal this identity as a string.
    ///
    /// The include_algorithms bitmap controls which algorithms will be included, provided we have them.
    /// If include_private is true private keys will be included, again if we have them.
    pub fn to_string_with_options(&self, include_algorithms: u8, include_private: bool) -> String {
        let include_p384 = self.p384.is_some() && ((include_algorithms & Self::ALGORITHM_EC_NIST_P384) != 0);

        let mut s = String::with_capacity(Self::MAX_MARSHAL_SIZE * 2);
        s.push_str(self.address.to_string().as_str());

        if (include_algorithms & Self::ALGORITHM_X25519) != 0 {
            s.push_str(":0:"); // 0 used for x25519 for legacy reasons just like in marshal()
            s.push_str(hex::to_string(&self.c25519).as_str());
            s.push_str(hex::to_string(&self.ed25519).as_str());
            if self.secret.is_some() && include_private {
                let secret = self.secret.as_ref().unwrap();
                s.push(':');
                s.push_str(hex::to_string(secret.c25519.secret_bytes().as_bytes()).as_str());
                s.push_str(hex::to_string(secret.ed25519.secret_bytes().as_bytes()).as_str());
            } else if include_p384 {
                s.push(':');
            }
        }

        if include_p384 {
            let p384 = self.p384.as_ref().unwrap();

            s.push_str(":2:"); // 2 == IDENTITY_ALGORITHM_EC_NIST_P384
            let p384_joined: [u8; P384_PUBLIC_KEY_SIZE + P384_PUBLIC_KEY_SIZE + P384_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE] = concat_arrays_4(p384.ecdh.as_bytes(), p384.ecdsa.as_bytes(), &p384.ecdsa_self_signature, &p384.ed25519_self_signature);
            s.push_str(base64::encode_config(p384_joined, base64::URL_SAFE_NO_PAD).as_str());
            if self.secret.is_some() && include_private {
                let secret = self.secret.as_ref().unwrap();
                if secret.p384.is_some() {
                    let p384_secret = secret.p384.as_ref().unwrap();
                    let p384_secret_joined: [u8; P384_SECRET_KEY_SIZE + P384_SECRET_KEY_SIZE] = concat_arrays_2(p384_secret.ecdh.secret_key_bytes().as_bytes(), p384_secret.ecdsa.secret_key_bytes().as_bytes());
                    s.push(':');
                    s.push_str(base64::encode_config(p384_secret_joined, base64::URL_SAFE_NO_PAD).as_str());
                }
            }
        }

        s
    }

    /// Get this identity in string form with all ciphers and with secrets (if present)
    pub fn to_secret_string(&self) -> String {
        self.to_string_with_options(Self::ALGORITHM_ALL, true)
    }
}

impl ToString for Identity {
    /// Get only the public portion of this identity as a string, including all cipher suites.
    #[inline(always)]
    fn to_string(&self) -> String {
        self.to_string_with_options(Self::ALGORITHM_ALL, false)
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
        if !keys[2].is_empty() && keys[2].len() != P384_PUBLIC_KEY_SIZE + P384_PUBLIC_KEY_SIZE + P384_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE {
            return Err(InvalidFormatError);
        }
        if !keys[3].is_empty() && keys[3].len() != P384_SECRET_KEY_SIZE + P384_SECRET_KEY_SIZE {
            return Err(InvalidFormatError);
        }

        let mut sha = SHA512::new();
        sha.update(&address.to_bytes());
        sha.update(&keys[0].as_slice()[0..64]);
        if !keys[2].is_empty() {
            sha.update(&[Self::ALGORITHM_EC_NIST_P384]);
            sha.update(&keys[2].as_slice()[0..(P384_PUBLIC_KEY_SIZE * 2)]);
        }

        Ok(Identity {
            address,
            c25519: keys[0].as_slice()[0..32].try_into().unwrap(),
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
                    ecdsa_self_signature: keys[2].as_slice()[(P384_PUBLIC_KEY_SIZE * 2)..((P384_PUBLIC_KEY_SIZE * 2) + P384_ECDSA_SIGNATURE_SIZE)].try_into().unwrap(),
                    ed25519_self_signature: keys[2].as_slice()[((P384_PUBLIC_KEY_SIZE * 2) + P384_ECDSA_SIGNATURE_SIZE)..].try_into().unwrap(),
                })
            },
            secret: if keys[1].is_empty() {
                None
            } else {
                if keys[1].len() != C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE {
                    return Err(InvalidFormatError);
                }
                Some(IdentitySecret {
                    c25519: {
                        let tmp = C25519KeyPair::from_bytes(&keys[0].as_slice()[0..32], &keys[1].as_slice()[0..32]);
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
                                let tmp = P384KeyPair::from_bytes(&keys[2].as_slice()[..P384_PUBLIC_KEY_SIZE], &keys[3].as_slice()[..P384_SECRET_KEY_SIZE]);
                                if tmp.is_none() {
                                    return Err(InvalidFormatError);
                                }
                                tmp.unwrap()
                            },
                            ecdsa: {
                                let tmp = P384KeyPair::from_bytes(&keys[2].as_slice()[P384_PUBLIC_KEY_SIZE..(P384_PUBLIC_KEY_SIZE * 2)], &keys[3].as_slice()[P384_SECRET_KEY_SIZE..]);
                                if tmp.is_none() {
                                    return Err(InvalidFormatError);
                                }
                                tmp.unwrap()
                            },
                        })
                    },
                })
            },
            fingerprint: sha.finish(),
        })
    }
}

impl Marshalable for Identity {
    /// Current sanity limit for the size of a marshaled Identity
    /// This is padded just a little up to 512 and can be increased if new key types are ever added.
    const MAX_MARSHAL_SIZE: usize = 25
        + ADDRESS_SIZE
        + C25519_PUBLIC_KEY_SIZE
        + ED25519_PUBLIC_KEY_SIZE
        + C25519_SECRET_KEY_SIZE
        + ED25519_SECRET_KEY_SIZE
        + P384_PUBLIC_KEY_SIZE
        + P384_PUBLIC_KEY_SIZE
        + P384_SECRET_KEY_SIZE
        + P384_SECRET_KEY_SIZE
        + P384_ECDSA_SIGNATURE_SIZE
        + ED25519_SIGNATURE_SIZE;

    #[inline(always)]
    fn marshal<const BL: usize>(&self, buf: &mut Buffer<BL>) -> std::io::Result<()> {
        self.marshal_with_options(buf, Self::ALGORITHM_ALL, false)
    }

    fn unmarshal<const BL: usize>(buf: &Buffer<BL>, cursor: &mut usize) -> std::io::Result<Identity> {
        let address = Address::from_bytes(buf.read_bytes_fixed::<ADDRESS_SIZE>(cursor)?);
        if !address.is_some() {
            return std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid address"));
        }
        let address = address.unwrap();

        let mut x25519_public: Option<(&[u8; C25519_PUBLIC_KEY_SIZE], &[u8; ED25519_PUBLIC_KEY_SIZE])> = None;
        let mut x25519_secret: Option<(&[u8; C25519_SECRET_KEY_SIZE], &[u8; ED25519_SECRET_KEY_SIZE])> = None;
        let mut p384_ecdh_ecdsa_public: Option<(P384PublicKey, P384PublicKey, &[u8; P384_ECDSA_SIGNATURE_SIZE], &[u8; ED25519_SIGNATURE_SIZE])> = None;
        let mut p384_ecdh_ecdsa_secret: Option<(&[u8; P384_SECRET_KEY_SIZE], &[u8; P384_SECRET_KEY_SIZE])> = None;

        loop {
            let mut algorithm = buf.read_u8(cursor)?;
            if algorithm == 0 {
                algorithm = Self::ALGORITHM_X25519;
            }
            match algorithm {
                Self::ALGORITHM_X25519 => {
                    let a = buf.read_bytes_fixed::<C25519_PUBLIC_KEY_SIZE>(cursor)?;
                    let b = buf.read_bytes_fixed::<ED25519_PUBLIC_KEY_SIZE>(cursor)?;
                    x25519_public = Some((a, b));
                    let sec_size = buf.read_u8(cursor)?;
                    if sec_size == (C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE) as u8 {
                        let a = buf.read_bytes_fixed::<C25519_SECRET_KEY_SIZE>(cursor)?;
                        let b = buf.read_bytes_fixed::<ED25519_SECRET_KEY_SIZE>(cursor)?;
                        x25519_secret = Some((a, b));
                    } else if sec_size != 0 {
                        return std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid x25519 secret"));
                    }
                }
                0x03 => {
                    // This isn't an algorithm; each algorithm is identified by just one bit. This
                    // indicates the total size of the section after the x25519 keys for backward
                    // compatibility. See comments in marshal(). New versions can ignore this field.
                    *cursor += 2;
                }
                Self::ALGORITHM_EC_NIST_P384 => {
                    let field_length = buf.read_varint(cursor)?;
                    let has_secret = if field_length == ((P384_PUBLIC_KEY_SIZE * 2) + P384_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE) as u64 {
                        false
                    } else {
                        if field_length == ((P384_PUBLIC_KEY_SIZE * 2) + P384_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE + (P384_SECRET_KEY_SIZE * 2)) as u64 {
                            true
                        } else {
                            return std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid p384 public key"));
                        }
                    };

                    let a = P384PublicKey::from_bytes(buf.read_bytes_fixed::<P384_PUBLIC_KEY_SIZE>(cursor)?);
                    let b = P384PublicKey::from_bytes(buf.read_bytes_fixed::<P384_PUBLIC_KEY_SIZE>(cursor)?);
                    let c = buf.read_bytes_fixed::<P384_ECDSA_SIGNATURE_SIZE>(cursor)?;
                    let d = buf.read_bytes_fixed::<ED25519_SIGNATURE_SIZE>(cursor)?;
                    if a.is_none() || b.is_none() {
                        return std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid p384 public key"));
                    }
                    p384_ecdh_ecdsa_public = Some((a.unwrap(), b.unwrap(), c, d));

                    if has_secret {
                        let a = buf.read_bytes_fixed::<P384_SECRET_KEY_SIZE>(cursor)?;
                        let b = buf.read_bytes_fixed::<P384_SECRET_KEY_SIZE>(cursor)?;
                        p384_ecdh_ecdsa_secret = Some((a, b));
                    }
                }
                0xff => break,
                _ => {
                    *cursor += buf.read_varint(cursor)? as usize;
                }
            }
        }

        if x25519_public.is_none() {
            return std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "x25519 key missing"));
        }
        let x25519_public = x25519_public.unwrap();

        let mut sha = SHA512::new();
        sha.update(&address.to_bytes());
        sha.update(x25519_public.0);
        sha.update(x25519_public.1);
        if p384_ecdh_ecdsa_public.is_some() {
            let p384 = p384_ecdh_ecdsa_public.as_ref().unwrap();
            sha.update(&[Self::ALGORITHM_EC_NIST_P384]);
            sha.update(p384.0.as_bytes());
            sha.update(p384.1.as_bytes());
        }

        Ok(Identity {
            address,
            c25519: x25519_public.0.clone(),
            ed25519: x25519_public.1.clone(),
            p384: if p384_ecdh_ecdsa_public.is_some() {
                let p384_ecdh_ecdsa_public = p384_ecdh_ecdsa_public.as_ref().unwrap();
                Some(IdentityP384Public {
                    ecdh: p384_ecdh_ecdsa_public.0.clone(),
                    ecdsa: p384_ecdh_ecdsa_public.1.clone(),
                    ecdsa_self_signature: p384_ecdh_ecdsa_public.2.clone(),
                    ed25519_self_signature: p384_ecdh_ecdsa_public.3.clone(),
                })
            } else {
                None
            },
            secret: if x25519_secret.is_some() {
                let x25519_secret = x25519_secret.unwrap();
                let c25519_secret = C25519KeyPair::from_bytes(x25519_public.0, x25519_secret.0);
                let ed25519_secret = Ed25519KeyPair::from_bytes(x25519_public.1, x25519_secret.1);
                if c25519_secret.is_none() || ed25519_secret.is_none() {
                    return std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "x25519 public key invalid"));
                }
                Some(IdentitySecret {
                    c25519: c25519_secret.unwrap(),
                    ed25519: ed25519_secret.unwrap(),
                    p384: if p384_ecdh_ecdsa_secret.is_some() && p384_ecdh_ecdsa_public.is_some() {
                        let p384_ecdh_ecdsa_public = p384_ecdh_ecdsa_public.as_ref().unwrap();
                        let p384_ecdh_ecdsa_secret = p384_ecdh_ecdsa_secret.as_ref().unwrap();
                        let p384_ecdh_secret = P384KeyPair::from_bytes(p384_ecdh_ecdsa_public.0.as_bytes(), p384_ecdh_ecdsa_secret.0);
                        let p384_ecdsa_secret = P384KeyPair::from_bytes(p384_ecdh_ecdsa_public.1.as_bytes(), p384_ecdh_ecdsa_secret.1);
                        if p384_ecdh_secret.is_none() || p384_ecdsa_secret.is_none() {
                            return std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "p384 secret key invalid"));
                        }
                        Some(IdentityP384Secret {
                            ecdh: p384_ecdh_secret.unwrap(),
                            ecdsa: p384_ecdsa_secret.unwrap(),
                        })
                    } else {
                        None
                    },
                })
            } else {
                None
            },
            fingerprint: sha.finish(),
        })
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
    fn cmp(&self, other: &Self) -> Ordering {
        self.address.cmp(&other.address).then_with(|| self.fingerprint.cmp(&other.fingerprint))
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
        state.write_u64(self.address.to_u64())
    }
}

impl Serialize for Identity {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        if serializer.is_human_readable() {
            serializer.serialize_str(self.to_string_with_options(Self::ALGORITHM_ALL, false).as_str())
        } else {
            let mut tmp: Buffer<{ Self::MAX_MARSHAL_SIZE }> = Buffer::new();
            assert!(self.marshal_with_options(&mut tmp, Self::ALGORITHM_ALL, false).is_ok());
            serializer.serialize_bytes(tmp.as_bytes())
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
        if v.len() <= Identity::MAX_MARSHAL_SIZE {
            let mut tmp: Buffer<{ Identity::MAX_MARSHAL_SIZE }> = Buffer::new();
            let _ = tmp.append_bytes(v);
            let mut cursor = 0;
            Identity::unmarshal(&tmp, &mut cursor).map_err(|e| E::custom(e.to_string()))
        } else {
            Err(E::custom("object too large"))
        }
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

const ADDRESS_DERIVATION_HASH_MEMORY_SIZE: usize = 2097152;

/// This is a compound hasher used for the work function that derives an address.
///
/// FIPS note: addresses are just unique identifiers based on a hash. The actual key is
/// what truly determines node identity. For FIPS purposes this can be considered a
/// non-cryptographic hash. Its memory hardness and use in a work function is a defense
/// in depth feature rather than a primary security feature.
fn zt_address_derivation_memory_intensive_hash(digest: &mut [u8; 64], genmem_pool_obj: &mut Pooled<AddressDerivationMemory, AddressDerivationMemoryFactory>) {
    let genmem_ptr: *mut u8 = genmem_pool_obj.get_memory();
    let (genmem, genmem_alias_hack) = unsafe { (&mut *slice_from_raw_parts_mut(genmem_ptr, ADDRESS_DERIVATION_HASH_MEMORY_SIZE), &*slice_from_raw_parts(genmem_ptr, ADDRESS_DERIVATION_HASH_MEMORY_SIZE)) };
    let genmem_u64_ptr = genmem_ptr.cast::<u64>();

    let mut s20 = Salsa::<20>::new(&digest[0..32], &digest[32..40]);

    s20.crypt(&crate::util::ZEROES[0..64], &mut genmem[0..64]);
    let mut i: usize = 64;
    while i < ADDRESS_DERIVATION_HASH_MEMORY_SIZE {
        let ii = i + 64;
        s20.crypt(&genmem_alias_hack[(i - 64)..i], &mut genmem[i..ii]);
        i = ii;
    }

    i = 0;
    while i < (ADDRESS_DERIVATION_HASH_MEMORY_SIZE / 8) {
        unsafe {
            let idx1 = (((*genmem_u64_ptr.add(i)).to_be() & 7) * 8) as usize;
            let idx2 = ((*genmem_u64_ptr.add(i + 1)).to_be() % (ADDRESS_DERIVATION_HASH_MEMORY_SIZE as u64 / 8)) as usize;
            let genmem_u64_at_idx2_ptr = genmem_u64_ptr.add(idx2);
            let tmp = *genmem_u64_at_idx2_ptr;
            let digest_u64_ptr = digest.as_mut_ptr().add(idx1).cast::<u64>();
            *genmem_u64_at_idx2_ptr = *digest_u64_ptr;
            *digest_u64_ptr = tmp;
        }
        s20.crypt_in_place(digest);
        i += 2;
    }
}

#[repr(transparent)]
struct AddressDerivationMemory(*mut u8);

impl AddressDerivationMemory {
    #[inline(always)]
    fn get_memory(&mut self) -> *mut u8 {
        self.0
    }
}

impl Drop for AddressDerivationMemory {
    #[inline(always)]
    fn drop(&mut self) {
        unsafe { dealloc(self.0, Layout::from_size_align(ADDRESS_DERIVATION_HASH_MEMORY_SIZE, 8).unwrap()) };
    }
}

struct AddressDerivationMemoryFactory;

impl PoolFactory<AddressDerivationMemory> for AddressDerivationMemoryFactory {
    #[inline(always)]
    fn create(&self) -> AddressDerivationMemory {
        AddressDerivationMemory(unsafe { alloc(Layout::from_size_align(ADDRESS_DERIVATION_HASH_MEMORY_SIZE, 8).unwrap()) })
    }

    #[inline(always)]
    fn reset(&self, _: &mut AddressDerivationMemory) {}
}

lazy_static! {
    static ref ADDRESS_DERVIATION_MEMORY_POOL: Pool<AddressDerivationMemory, AddressDerivationMemoryFactory> = Pool::new(0, AddressDerivationMemoryFactory);
}

/// Purge the memory pool used to verify identities. This can be called periodically
/// from the maintenance function to prevent memory buildup from bursts of identity
/// verification.
#[allow(unused)]
#[inline(always)]
pub(crate) fn purge_verification_memory_pool() {
    ADDRESS_DERVIATION_MEMORY_POOL.purge();
}

#[cfg(test)]
mod tests {
    use crate::util::marshalable::Marshalable;
    use crate::vl1::identity::*;
    use std::str::FromStr;
    use std::time::{Duration, SystemTime};
    #[allow(unused_imports)]
    use zerotier_core_crypto::hex;

    const GOOD_V0_IDENTITIES: [&'static str; 10] = [
        "51ef313c3a:0:79fee239cf79833be3a9068565661dc33e04759fa0f7e2218d10f1a51d441f1bf71332eba26dfc3755ce60e14650fe68dede66cf145e429972a7f51e026374de:6d12b1c5e0eae3983a5ee5872fa9061963d9e2f8cdd85adab54bdec4bd67f538cafc91b8b5b93fca658a630aab030ec10d66235f2443ccf362c55c41ae01b46e",
        "9532db97eb:0:86a2c3a7d08be09f794188ef86014f54b699577536db1ded58537c9159020b48c962ff7f25501ada8ef20b604dd29fb1a915966aaffe1ef6a589527525599f10:06ab13d2704583451bb326feb5c3d9bfe7879aa327669ff33150a42c04464aa5435cec79d952e0af970142e9d8c8a0dd26deadf9b9ba2f1cb454bf2ac22e53e6",
        "361d9b8016:0:5935e0d38e690a992d22fdbb587b873e9b6de4de4a45d161c47f249a2dbeed44e917da80736c8c3b61cdcc5a3f0a2c77fc8fa41c1302fa7bb871fe5833f9995f:7cfb67189c36e2588682a065db769a3827f423d099a84c61f30b5ad41c2e51a4c750235820441a524a011facad4555869042750684b01d6eca4b86223e816569",
        "77f925e5e3:0:161678a69aa19d1de096cd9cd7801745f038f74c3680f28da0890c995ecf56408c1f6022a02ab20c68e21b1afc587a0038f1405cbd3167877a69926788e92620:2e1a73ffb750f201451f5c35693179cfa0de14404c8d55e6bb5749787e7e220b292f9193f454b2e97404c5d136cff665874373e9a6d5139efa1b904f19efc7d3",
        "e32e883ac6:0:29e41f935cf419d41103a748938ab0dcc978b6fde9fbb82d6f34ef124538f93dc680c8b26ba03f0c66d15be1a3895ef73dc6879843f3720095fa144d33369195:3654e04cac0beb98a94b97bca2b9a0aea4c7001e13c3ebe813fe8096395ecb69b824d3b6ee2d5b149077abd73cff61dd9ee04811c30b0c7f964b59c67eefa799",
        "6f66865615:0:11443c5c0a6a096245f9790240e15d3b8ea228397447f118bd8b44030b24191f97e11bf704807561cd6d54f627d57d599ca7983547c6d4db52597dbd1c86114b:1d1cb5bbced28b11f2f61ddcbc9693d0233485fc8fe0825c090a7309fe94fd26e8e89d137071ef7567b80cce60672a31da4c1677fa1c37237b0713456788dc81",
        "c5bdb4a6a6:0:e0a8575bc0277ecf59aaa4a2724acc55554151fff510c8211b0b863398a04224ed918c16405552336ad4c4da3b98eb6224574f1cacaa69e19cdfde184fd9292d:0d45f17d73337cc1898f7be6aae54a050b39ed0259b608b80619c3f898caf8a3a48ae56e51c3d7d8426ef295c0628d81b1a99616a3ed28da49bf8f81e1bec863",
        "c622dedbe4:0:0cfec354be26b4b2fa9ea29166b4acaf9476d169d51fd741d7e4cd9de93f321c6b80628c50da566d0a6b07d58d651eba8af63e0edc36202c05c3f97c828788ad:31a75d2b46c1b0f33228d3869bc807b42b371bbcef4c96f7232a27c62f56397568558f115d9cff3d6f7b8efb726a1ea49a591662d9aacd1049e295cbb0cf3197",
        "e28829ab3c:0:8e36c4f6cb524cae6bbea5f26dadb601a76f2a3793961779317365effb17ac6cde4ff4149a1b3480fbdbdbabfe62e1f264e764f95540b63158d1ea8b1eb0df5b:957508a7546df18784cd285da2e6216e4265906c6c7fba9a895f29a724d63a2e0268128c0c9c2cc304c8c3304863cdfe437a7b93b12dc778c0372a116088e9cd",
        "aec623e59d:0:d7b1a715d95490611b8d467bbee442e3c88949f677371d3692da92f5b23d9e01bb916596cc1ddd2d5e0e5ecd6c750bb71ad2ba594b614b771c6f07b39dbe4126:ae4e4759d67158dcc54ede8c8ddb08acac49baf8b816883fc0ac5b6e328d17ced5f05ee0b4cd20b03bc5005471795c29206b835081b873fef26d3941416bd626",
    ];
    const GOOD_V1_IDENTITIES: [&'static str; 10] = [
        "b9553fc08f:0:e5b69b67aba3fbb35fb5b26e3f8d98bd081944c0153350c2fbd0e5d4d68c4d19b0c33c44c7933c9e7d02162a56abad2de4ebe9e2bf606d7021b92e7412e20270:487b70af891cfd47d847e12739623abd67ed1a3554c4bcaf73a7e44178b1c04a9e59f4b4cedc17b6de00f8f7b26880fea6f82fdc67e371f5cfcab7a1f44dd267:2:AjvDSgtrnnkvMheXcH8P42wXPjPNVYedfvFldPUr7Xn_icixyCAoNGkXwTNEMN9xpwNqq9mvfqQa-mOUJUz8yiWTNqnw8T1Esaf-OGZSu-leWOEbmCswVIl94qNsjS5g8Kx4BHzTtkx2t_quKMoelK5EOscwAwEiFS94r1nUau3H9QBQWNr9v44_8_Dbj-V6Eeo70ZwiU2bCUlsvoBS5ae1Aepktv6Usi1Yzl9L-6v22VH0RLLinY_b_r63sdf4LSay9YZ8b4GnMYkXb_0KqDTSZXxsiDUb1lhmE63ARsLo9b9X8oKm_Kog5ZXhnMUUjN6DObiDjJ5pdyjvPEXlxzOoD:IKPkJFOW9NbDYSl2Aagb1OQ4HyDDN6iQpDhECBE84nUQj5KbN-IGU5a-IVvO3K1UyYcDfKnb2PMycSungUWO9LKbWFykXDKDmoWL0Sz7yEmfshDn96KTevLNpCbRNiAj",
        "2f67e50239:0:a79b755302a4fd4805bea5b4e3f12cbf75e9adee50dce2cff48252eeb5e9803b8cf677cc5488851e5c64796b251ac9eca78af01a1825966e3d7d1fc06da4779e:48815d7f81db7709d88067d14fc8cb8a91b24eea70b791603002812619c73f4dabbba724dfb76d188c9150daef9ff4d650a5825752840683027c569fa478ca41:2:AmVWC14Qx4OEERfjBRvECvsbPPgRmuJr2N6ypX-ewfyEsZwQ8kVX2c3mrTsyfnUSuQJrjLh9p7L7G2Fv6LI0T7AXlL8_Hczk_81UlhV9eG9Dj70p4NtPWcAeH-osDJPCDTZmNx4cke6DQ8XwDeU_NFLOm_GKniNj85oO9iVRt40BZLD89S-bloPxptd4vT4QP07YL3VnRyLNUEn1PLnvedBwfU9WSSbGtYVOTYNElr4Jgmt6WK78Yq4bgQogayu8eKCb5AeS0A4vwVf0Ii7nsYOHnEQE-g7MSmV01awR2PC9hL8_uviZQKdNROYx9LKQrXLeszxl0cHawmB8hoOAnxkH:D0lKhr2LBBY8wTlrI2RaxI3bq9fZeJuU6xTbCOfF442YFjTIrqtGokDzirUYpVfMsCyT2nS-xmYDGBmDDAIEdG3x9fWuCjD1trib9ZKCB-1bX21ZOOUzsux9q6EByox9",
        "484d3aabbc:0:0574978881591073ecdfa9e48ae0dce17b285896695e12e7f4f1c76f4abf0a03e22d12d97363542cdb59c0387533f49295a5831de758901328392021e28488fb:58ae228ba918e8558e1f8167b0e3612d9f58e48ff3caaa5c1aafd9cc4e81c7634a7d6b6c5859f539836e28bcd6d7a0d14760134b643d89bdc8d369765501fce8:2:AwTUxHZP6YGmdUzno2_JW6RLRaR8Qr_jyUXcbRVXTismnvzr2ERwSqXMlc3I3qvTBgOUFhl2oNXDNdiuCPsDINJuSiez9sm5nx3yzfYdwP636VjoLzjdGBnh82Sp9jNsYx1ZlK8ZWl2zzlNtdLs30OKvm-xhSmr5Mpkx9VK12j5jm8VDsoTMwAW3kjOBjCIy7kQ7gd61I89W4f7KIntjz1ZW9yuon3XWfFDbskUqbe7sQIldAvOKcggaZQWWljVrnn6hT4k6UwjDPyA_i2CaUTJmn_lVxilsW6UGXeIFyo-opG62zpVeBQcOhblIu9ndkAXpK0KJLjdppvDAwmTk8cEH:grMfFb7ZGfz5-SBX-JLoiY2BW5DJ5rvAsO0Aop1npYMnSHL2VM_56eiYiBlwZ7zdKyYWdA_BRApPl5x0iCQcqXLhgQAVn-iL5edFHTGIF8HLRVcLn6_XIz7_u5TAydXp",
        "210ae3e250:0:d3fb8d2c651a4b5d2cdfe8ac07ed3aa2c303ff5250990d08aa5998fa8017335d2aff2b82bb153cb349c91e4177e16e718bcc74586644a582d8702481e30a813d:f8d31b46693c013f392da1180806edfaa7c389c1040f8e772195c086fd0d44617b9eb809e21e9de0e44b06236a5997b04522dc78218eca76b6c66f6ab8034770:2:Ai6daBUu73oul4o4lpH-xt2jufhIOIeZFR34vrO4MlY8oC258FPdg-e57467BS8pfQKkSh_Qvrarwl4kHgpCfANqWhFS7jdnvRTwWXPNESwsP04y8ZoGtR3-p8eaq9qUYWiZg7qnfvcoqecvySpk3gZIthZOXEtchID0InjYmkbCipHZcyzUDP82hkbrWlZWFySMVwP2sDUc_WT6Lyx4aMP83R1ZMTVw1-Q-peK8Ihevw4yCfOuF_iJOMxsek-NAA4R9n2gYhQJIDOn8Hj7zuNIffjcgoDOSX-pJr0mke_elL0u5Hgnko9qAPIH6PGQJRt5U5tpa5E-beJ0_aGaBWMwK:YC-wYTJu8l_7ZfUoK4xXP6fdVjon4QPQXE92fVgvs7-B2Sw2bsbus9-QGmWcUmJPWvrnpmkT14sgoyf7mWWoJFo93DCHm8dWheC8-cDUMIQ12uHgBKL33r6Ka57kgY71",
        "f947dd72f4:0:5221ae17483f5dfe954a0240ebf0d2b466ff7f7f67cdd702eeadbe2e510dd26a795e15b2c84f85e610becb2ca049c73687dbcc8c6407cfade6a191c6e7f70377:501adf90810f5cd095581dd0f2929184db36278fd6b48909e3a10d80560bf5603d2750143063730932b31f240ea1c30f0349d017a363a75c6c1b95b8d0c659e5:2:AvXfGGmMG2OZRaamtGYJkatdUSHv4Z1PfbdKExOGwWnOewYGcPTvBDChtlAg9IwqOgJ9-XSnSFxgskoQtI8wDFXRhrr659_FFAzM-oyQ9sj3ZrSXYnFNPpfoyiennT2nsQmP2MoahrQCMerQIIO86EXc98BWVZJO79LC5fxhTK1EWzE9APvOBFulv6c-W9dxVlD9CT-oIAjqIBL8hVUgKLdWPmtzGmAd4NnfrfiHDx4zaAkoSmFxyuUOCvfp8AioGXNLrHwIPqquOqQKw2tcIHnDaHA72JpjOV7787Eb_2pTwFhyzJVKixDrESNqhI-35SHrcsaJel1mUOGcweliqEkB:NHS7hGLadxiuFWRqT48kp1NvZ0jf4Po861scAhIFRP4hLMe6Uuk4TGmajBEzZhQjz-60as3nQN5VaHg1NjE5eFUbJuijAagi4dEyvzsQWg4HPUYn8hNe9Z95vz-n5DgO",
        "e11c31a215:0:db25e87625513ba531ac19eff743bf6e769517776e964bec1eaed38c559de3518167075d21ea4d89d611628c559ee52336537583201dcf46f2546e3f8bcdc7ec:40a09a8a1bd5f071262a79ad52c5a6b485318ad58ec38bebf432236bece7624c96b9ef2737eb992ae15c0d63c439c384b63aa35467aee5c686c5201cf7617158:2:A4VDDuVuH4UpctYg5_-rDhLduiHcColHzGAVudbe9ri0kTx7D_paVFYG-YUZaJu4LALW8FgWbjoEYjXepWErZWrZJk-MxrA-8IfeotLDZQbBNC5V_UxpDlPt-gRdyYpz-mN5KLe21shP0JiO7eL-rnGqYS4lmw8Fk_vHDq0AL1rNCIQn6x0k0jVZsXE0SUq_8BfrC3ufI4-D-KuqpdozdR54E4vUxT6WZpoeahMFlT0w87XPbeIp4RerWw4tuuBMfvyvm9cQCqQkAaOPNT4TEzLQwlQasf1pFE3aCguvdU4i2SuIhGIXaukHoXP5nWLibQpILd71bEnwX7wBCgIrBjUM:7IXCtpXjI0RQ2I478353Ab-7iFXv9VNa9SPR18M8ra1CpZWgPD5hkfl0PRyaHkBdzvpb2VkZ0TFTIigh8ZkFUeJxgu84i_AXIaPEsTa9fevCagbR57caRa-xqLRe4Onl",
        "69fc019ddf:0:ebb1b3c67b2c658e60afa1d14fc5586946aeb5f3abb42e3deb684d4eaa4ed04ff758e4af6770184e6aecc52d090a7992279886c88de1ca99ce80bf04d01a48e9:6011b9bed796b82bf2f981a92fdc94b62d6942a25323208ab193227935c429412a239889c8a5c07960706d936d3497db5c5bef80f5ca3f162d96d305374e3dd9:2:AiFYEjbjMSSYf8gXtV-r9YDnl6RVgKmckZi25ss3yLLi5hvdB92I8Vr80QiHcXbGkgO2YBJ1_EUMTQAeiL1VlaY3mSKf0Q892yjeoqe7sOJiWrvx1x2Xl4zD2kHi2pbLy1NA7NUkroJamJ4Z5J__rq8yNv4bsqEr2iaTiADWxKfpLRc29cjA11eiUq_Xjj72gBpr78DmT_-N63OAUhBPSHHrA60NannkvkRKcy7WsCr9pB9L0QSZd3BgoYE78VAfaOx9gJw4vihkel3lFEXEaIyoJhwcFxEBIsnfl0gagQ7NeCqRkEzeiXDVS1pFOY3BCu6zV5Q-Aqtw5yqozSXxa9QN:jmOd2wCLT5z09JXJMXOMPk91I1in7qhq0Rsdsi-xEYenHs3nPX5k___FZ3bpFYqjx914Yy65QrXKkThr3hysgiXztEtILYOr8yXf56O16Io-XB6L6CK0VrKnSb4p40Lf",
        "74152a45b9:0:11a049ebf24d6e9911c68e1c849b4290c8b72bff2954699e54138446573e041346fa34e09b697ae3a9d7c6f94aeb1b21e54745fab10f7a56df22947ef20a5c7d:e8d19530a8ce8685ba39ca69eabfb3d0fad5ed63bf59f025a3658f33e6c629737e49e57b3c91026ef8648d6a473c8ee9f7a27a040c822a0fff44adc0079aa969:2:Ak86tzCx95V6u5t4Ty2OrAWN9PSOjIWQkk0g28VBEwo84HFVVLSl2gBKV2HYDpO2TAJlLNFxhDynea4XdHrbA6Cdv1nsNdWsaizHiClZ3qoBEf9riSimVLpwPI5BMC-1vznYivS-_gMKYmSX5D20UDKZwK3bM-p3wLwcwslurLzsv4KMk2DTzkExdWGjHkZhOC6754YLgdjM6UsYgohPHnqZNUCLo288BFxBhYshmOxrAjnFzapp67dzf5SfV9hiwhWhblEEFYa2FC1lm8WQC3dPVJwg6Bmkw8pMBzN_qoQZYAOVanzu2dUisCx0oLIVW4-AzPJOqHH1DkiUl4opkZEO:rNVgOQsylq8Xi-wnW__VvcHjqert2hl8mLhlHBu9IG4SDelD9c24Bs6skvwxV2QRBQwbvmv11f6MNkhA6PK5W22OMOmHMG8GsnXlORRH5HMfBzVBdffK_91lfBkB67pP",
        "fe63ec5d9a:0:cb64b8f27bc6ed0c2875e98d988e11db3d1f2016d14787ed51fbc0493b58aa3b8329d49142fc9acce421b7d3a9b9908a5afed3367b589bc1b0c94ae04c8a383d:e81cc452f9f616210295f96d2034ac715c3b87ca95dbc1e77bd3dcc798719558a4ca82befa391aaffd8099787a60086b35cc6a2390c55720716ac05d2a7b116f:2:Aor2te5DUyKXFFrvOyegql5i0XOleTu4d1AfEl-SE0KZ_PiQeVsWiWsy57SXVZtFTQL7YNyV5dd3oRUmPqcFzNlfY7RnJnKwf-OfmFzin4VeCqqw-d4eAfgNaav2Y5qVC_uc-VuPdRONqSqd04n6RG9Gyz9XvMqXH8Dk6L2iVkP3y1TjyzapIbNQC8pRnsRJaPafN2QVG6qhu7clFAoOzsgkM8BKaxrTLvbznrTpjGAZk5ocb_gLZiDDwOzUsMoUI4Zi3Gky4TyHFAWX9ifJgeSh_D9gj_oGCvsQhF8xVkgsXDTkHAN4hmIEJM_myzL-XnpROFGcaN6m8y085HS7UCcB:6HVliM67dmXAgYPs-z75TTz3m6tSVX6-EpEsy6N-TsX2a_5czJImDXQ-Qhi3524tDIIktWJcUv8JGA7kTPHkqr4Jrnt6JJXG4DVf6xw-n-GpRje-EwyW4Q378tomPpw5",
        "99ff6d1b10:0:20148e36a3dec6f91537e0bc0e2a852b6cc0fac6c664ef8ce453d7cc404b2b7c5f30bcd244d68ca009f4be0be6ad9a8ea51452d4bb8f7872cb8dacb5d1cdbc25:489fc570d4086ee0bf160d2dc8cc4547b4d3e4336c97b46d45b38064cb19087057adee0ecac1a3fc398627772407f7b814d1292db1ab8eecfdaf6177a7c09870:2:AlLlDYoNRBKH038FnkvZPXfQ1w9T19Df1AW8cav-mrJH524nbtxHI_t-cBorpm8i1wOPnYH96Bkr2LH9UeXaR0xHSNW-dqDTdWbu7uz7sqseULAq7izA8k5VcRQH9YesrcsgASmEOKxJZAc1CkjJpoCj8cwKHZECx5EE3rNFEF4ZxQth5AYg6P8isEpjMw7zBQOGlIRWV5sjVSml_JUKMnB3H9ZOE4UTxTXKVUEUJwHcp_9tXVv_RYuQOarzIRq0-jpod1kamOcLXFsYAiQbg_DwW7F2kQMdqUWJ7tHVwOxfb2CbwD40Yo_-VZ3ZJ8AGm-eY0ngO3PDBbUyKwct_ljwO:29hzAxGQd6oGze7c8XxqueXF_OnZ5WHtpAGQt3RhG2UPEl4uUximYMS25uAqARmZe4VIkwWJ16_IO7nTaizC3feRDWl1BpePoFVuUoFGsK303kUL6IH6xsqdXup2Uahg"
    ];

    #[test]
    fn marshal_unmarshal_sign_verify_agree() {
        let gen = Identity::generate();
        assert!(gen.agree(&gen).is_some());
        assert!(gen.validate_identity());
        let bytes = gen.to_buffer_with_options(Identity::ALGORITHM_ALL, true);
        let string = gen.to_string_with_options(Identity::ALGORITHM_ALL, true);
        assert!(Identity::from_str(string.as_str()).unwrap().eq(&gen));

        let mut cursor = 0_usize;
        let gen_unmarshaled = Identity::unmarshal(&bytes, &mut cursor).unwrap();
        assert!(gen_unmarshaled.secret.is_some());
        if !gen_unmarshaled.eq(&gen) {
            println!("{} != {}", hex::to_string(&gen_unmarshaled.fingerprint), hex::to_string(&gen.fingerprint));
        }

        assert!(Identity::from_str(string.as_str()).unwrap().secret.is_some());

        let gen2 = Identity::generate();
        assert!(gen2.validate_identity());
        assert!(gen2.agree(&gen).unwrap().eq(&gen.agree(&gen2).unwrap()));

        for id_str in GOOD_V0_IDENTITIES {
            let mut id = Identity::from_str(id_str).unwrap();
            assert_eq!(id.to_string_with_options(Identity::ALGORITHM_ALL, true).as_str(), id_str);

            assert!(id.validate_identity());
            assert!(id.p384.is_none());

            let idb = id.to_buffer_with_options(Identity::ALGORITHM_ALL, true);
            let mut cursor = 0;
            let id_unmarshal = Identity::unmarshal(&idb, &mut cursor).unwrap();
            assert!(id == id_unmarshal);
            assert!(id_unmarshal.secret.is_some());

            let idb2 = id_unmarshal.to_buffer_with_options(Identity::ALGORITHM_ALL, false);
            cursor = 0;
            let id_unmarshal2 = Identity::unmarshal(&idb2, &mut cursor).unwrap();
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
            assert_eq!(id.to_string_with_options(Identity::ALGORITHM_ALL, true).as_str(), id_str);

            assert!(id.validate_identity());
            assert!(id.p384.is_some());

            let idb = id.to_buffer_with_options(Identity::ALGORITHM_ALL, true);
            let mut cursor = 0;
            let id_unmarshal = Identity::unmarshal(&idb, &mut cursor).unwrap();
            assert!(id == id_unmarshal);

            cursor = 0;
            let idb2 = id_unmarshal.to_buffer_with_options(Identity::ALGORITHM_ALL, false);
            let id_unmarshal2 = Identity::unmarshal(&idb2, &mut cursor).unwrap();
            assert!(id_unmarshal2 == id_unmarshal);
            assert!(id_unmarshal2 == id);

            let ids = id.to_string();
            assert!(Identity::from_str(ids.as_str()).unwrap() == id);
        }
    }

    #[test]
    fn benchmark_generate() {
        let mut count = 0;
        let run_time = Duration::from_secs(5);
        let start = SystemTime::now();
        let mut end;
        let mut duration;
        loop {
            let _id = Identity::generate();
            //println!("{}", _id.to_string());
            end = SystemTime::now();
            duration = end.duration_since(start).unwrap();
            count += 1;
            if duration >= run_time {
                break;
            }
        }
        println!("benchmark: V1 identity generation: {} ms / identity (average)", (duration.as_millis() as f64) / (count as f64));
    }
}
