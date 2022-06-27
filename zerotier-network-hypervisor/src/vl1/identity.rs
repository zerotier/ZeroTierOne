// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::cmp::Ordering;
use std::convert::TryInto;
use std::hash::{Hash, Hasher};
use std::io::Write;
use std::str::FromStr;

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
use crate::util::ZEROES;
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
fn concat_arrays_4<const A: usize, const B: usize, const C: usize, const D: usize, const S: usize>(a: &[u8; A], b: &[u8; B], c: &[u8; C], d: &[u8; D]) -> [u8; S] {
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
        salsa.crypt(&ZEROES, &mut *genmem.cast::<[u8; 64]>());
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
        loop {
            c25519 = C25519KeyPair::generate();
            c25519_pub = c25519.public_bytes();

            sha.update(&c25519_pub);
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
            c25519: c25519_pub,
            ed25519: ed25519_pub,
            p384: None,
            secret: Some(IdentitySecret { c25519, ed25519, p384: None }),
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
            self.fingerprint = SHA384::hash(self_sign_buf.as_slice());

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
        if let Some(p384) = self.p384.as_ref() {
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
        zt_address_derivation_work_function(&mut digest);

        return digest[0] < IDENTITY_POW_THRESHOLD && Address::from_bytes(&digest[59..64]).map_or(false, |a| a == self.address);
    }

    /// Perform ECDH key agreement, returning a shared secret or None on error.
    ///
    /// An error can occur if this identity does not hold its secret portion or if either key is invalid.
    ///
    /// If both sides have NIST P-384 keys then key agreement is performed using both Curve25519 and
    /// NIST P-384 and the result is HMAC(Curve25519 secret, NIST P-384 secret).
    pub fn agree(&self, other: &Identity) -> Option<Secret<48>> {
        if let Some(secret) = self.secret.as_ref() {
            let c25519_secret: Secret<48> = Secret((&SHA512::hash(&secret.c25519.agree(&other.c25519).0)[..48]).try_into().unwrap());

            // FIPS note: FIPS-compliant exchange algorithms must be the last algorithms in any HKDF chain
            // for the final result to be technically FIPS compliant. Non-FIPS algorithm secrets are considered
            // a salt in the HMAC(salt, key) HKDF construction.
            if secret.p384.is_some() && other.p384.is_some() {
                secret.p384.as_ref().unwrap().ecdh.agree(&other.p384.as_ref().unwrap().ecdh).map(|p384_secret| Secret(hmac_sha384(&c25519_secret.0, &p384_secret.0)))
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
         * LEGACY:
         *
         * The prefix of 0x03 is for backward compatibility. Older nodes will interpret this as
         * an empty unidentified InetAddress object and will skip the number of bytes following it.
         *
         * For future compatibility the size field here will allow this to be extended, something
         * that should have been in the protocol from the beginning.
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

        // Fill in the remaining data field earmarked above.
        *buf.bytes_fixed_mut_at(remaining_data_size_field_at).unwrap() = (((buf.len() - remaining_data_size_field_at) - 2) as u16).to_be_bytes();

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

        let mut sha = SHA384::new();
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

        let mut eof = buf.len();
        while *cursor < eof {
            let algorithm = buf.read_u8(cursor)?;
            match algorithm {
                0_u8 | Self::ALGORITHM_X25519 => {
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
                    let bytes_remaining = buf.read_u16(cursor)? as usize;
                    eof = *cursor + bytes_remaining;
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
                _ => {
                    *cursor += buf.read_varint(cursor)? as usize;
                }
            }
        }

        if x25519_public.is_none() {
            return std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "x25519 key missing"));
        }
        let x25519_public = x25519_public.unwrap();

        let mut sha = SHA384::new();
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

#[cfg(test)]
mod tests {
    use crate::util::marshalable::Marshalable;
    use crate::vl1::identity::*;
    use std::str::FromStr;
    use zerotier_core_crypto::hex;

    #[test]
    fn v0_identity() {
        let self_agree_expected = hex::from_string("de904fc90ff3a2b96b739b926e623113f5334c80841b654509b77916c4c4a6eb0ca69ec6ed01a7f04aee17c546b30ba4");

        // Test self-agree with a known good x25519-only (v0) identity.
        let id = Identity::from_str("728efdb79d:0:3077ed0084d8d48a3ac628af6b45d9351e823bff34bc4376cddfc77a3d73a966c7d347bdcc1244d0e99e1b9c961ff5e963092e90ca43b47ff58c114d2d699664:2afaefcd1dca336ed59957eb61919b55009850b0b7088af3ee142672b637d1d49cc882b30a006f9eee42f2211ef8fe1cbe99a16a4436737fc158ce2243c15f12").unwrap();
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
        "174cd00112:0:fd7e144befe03a8bca114094f576a6848224f35ef2c764f73d4b6f51ce54392127163722755be3e1de4375bec6d704e823acfa40180a39b7d76600c7776483b6::2:A9WKCt_BhL9EnKAb8SnPisFbCIXNDxFbtTxZiTjki9t5cu1xqEIOjk94s4r2CEaR5gOdpDyUGcoY0e1JjRRFK3CzVivW35eUhMKS1qQorcts35bYblMASQGp9ek047ROlKuzq4M5a-2Ymqb_fo1lhPxhaxLTmgsjmtllJJNknSOwGGYvQXukwzH4Vf0E-OWmkGrSWo6n7FkbuHwvKe7oPbhKwRQU03ya3-kM5vHGBglTOnN1NceXZvKwUhSiQ0zynt4OHrM5eJFlX3rE9mal5ml0l6CYs0Wh52byTHcav8A6tiSGNxxqU-BJ1EYEBG_kWB94gvGwImmBpaAR0xKfndIJ",
        "4a23204ebb:0:ff61cddca9062501edc4390a8f218728fd58876ecfe2d757f611b9895d8e4e3a9cf8c6b49e18d5112f15c1a04715f0a579c2c43d7af0f3bb81de9a05135dcfcb::2:A4C5X0VLw98-g2Nc0ksfF50rt3-G-K5GGwmam2jUOGuz0IUH6cbryz3p4QjxFVEh9wM8VJUHF6yN7dpTwTWp6UMTVG5pFH6jSel-Z8GTSOJi3sybB-PFp0huY9oc0OOg1yuGbzKJFodkRmSCTku935hWVcV01HtNpNIScouXNVLIwDnxMd7iJ5J2pLJLS79Ofbf5VJWNXSIm4Ykse1BIv6vzbPZToQg_PXuIx8LFY1pGm8kb2Hx6FRUEDtP6jc3q5BDwVO8qVqatvpCTyjXWdBBURynH2FnmHeH_8u0mYse__wJoK5ICDRTxDKWWBOAOcS9pEQpjLuHnQex5cz9VI-gM",
        "47e4f45e96:0:8a159c1ae1a81fb9f6d12027533e98178fca97c02edcb3b6f08c59578d54651cde7c596f9b7a8bb001b5ca7337ae99321a046bfcb8d5bdfd68a184a918000e7f::2:Aq0WCQ2jorXo2hlxKV8lQ0GX46DuisOSIRR8V3kg9oFbvnfa1c5W2lLMFtkqVqccAQPNPfU-Qfxh9lFUDKWNeS_bCx9Akt5Kw2yBi28eeySQP1DcHek4nVez9DG5-aTqnyyKnU3fNX34LuqMdTxZws4id33VXicY-sqKRobpyqFosC1U1SIiwqgSy290WBj_RKeZFU7QQF26cmbVF6bZ-OdYpcmNhdu_yzZ42Hh2W4duBHmJG1gDKGD-dXByaJVcnLIDwUMne8KSDZ7VaM_YfxWBmBBUqWUIpeeHnKLIlgF2ZJKgdUb8xzfUk0WzPkJfu2dcYduYGtVAG7r7jMatffMH",
        "f172595d9d:0:739b4146fc7fff9d0234bd37b6d8c846b52bbad6f286bfc725580214b3a14149bf013dc23080d844a8cb1a0a32a6bb7caa455eeedc356660bee6cf80a7b586c1::2:A1_KXkfl5aHO6r7KmEf39Y5enSxWdiiVy6B5ZBWbhsT4sZV1Aqh4c_K2dH7s_QAydAIi-jrgvKshER7Dfn__xArPU_zWNvcW0-rCY3D0_y-Lr6b8_OjeZeN3Ry9cFkMhAyJE0x_n4uKGgFvtYFDjAtnGIzzzfnNf_h0dOFHZMRwcFRxtvo-e2hz-PuDsEQ_s350phi9N1Djtcy6aj-Cn_A0KzNpA5HFlWoOHTcNUDjZ64Eus47wemNl4lt3PBgQdcsk7MPsq122Da_LalAk_26CnbNzwifsfRnXHbHn7mf14r7YBb-8r-88LzL_Rtlnvcvhm6ib4pGFpv7TVkmkLvBoD",
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
}
