/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::alloc::{alloc, dealloc, Layout};
use std::cmp::Ordering;
use std::convert::TryInto;
use std::hash::{Hash, Hasher};
use std::io::Write;
use std::mem::MaybeUninit;
use std::ptr::{slice_from_raw_parts, slice_from_raw_parts_mut};
use std::str::FromStr;

use lazy_static::lazy_static;

use serde_derive::{Deserialize, Serialize};

use zerotier_core_crypto::c25519::*;
use zerotier_core_crypto::hash::*;
use zerotier_core_crypto::hex;
use zerotier_core_crypto::p384::*;
use zerotier_core_crypto::salsa::Salsa;
use zerotier_core_crypto::secret::Secret;

use crate::error::{InvalidFormatError, InvalidParameterError};
use crate::util::buffer::Buffer;
use crate::util::pool::{Pool, PoolFactory, Pooled};
use crate::vl1::protocol::{ADDRESS_SIZE, ADDRESS_SIZE_STRING, IDENTITY_POW_THRESHOLD};
use crate::vl1::Address;

/// Curve25519 and Ed25519
pub const IDENTITY_ALGORITHM_X25519: u8 = 0x01;

/// NIST P-384 ECDH and ECDSA
pub const IDENTITY_ALGORITHM_EC_NIST_P384: u8 = 0x02;

/// Bit mask to include all algorithms.
pub const IDENTITY_ALGORITHM_ALL: u8 = 0xff;

/// Current sanity limit for the size of a marshaled Identity (can be increased if needed).
pub const MAX_MARSHAL_SIZE: usize =
    ADDRESS_SIZE + C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE + P384_PUBLIC_KEY_SIZE + P384_PUBLIC_KEY_SIZE + P384_SECRET_KEY_SIZE + P384_SECRET_KEY_SIZE + P384_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE + 16;

#[derive(Clone)]
pub struct IdentityP384Secret {
    pub ecdh: P384KeyPair,
    pub ecdsa: P384KeyPair,
}

#[derive(Clone)]
pub struct IdentityP384Public {
    pub ecdh: P384PublicKey,
    pub ecdsa: P384PublicKey,
    pub ecdsa_self_signature: [u8; P384_ECDSA_SIGNATURE_SIZE],
    pub ed25519_self_signature: [u8; ED25519_SIGNATURE_SIZE],
}

#[derive(Clone)]
pub struct IdentitySecret {
    pub c25519: C25519KeyPair,
    pub ed25519: Ed25519KeyPair,
    pub p384: Option<IdentityP384Secret>,
}

#[derive(Clone)]
pub struct Identity {
    pub address: Address,
    pub c25519: [u8; C25519_PUBLIC_KEY_SIZE],
    pub ed25519: [u8; ED25519_PUBLIC_KEY_SIZE],
    pub p384: Option<IdentityP384Public>,
    pub secret: Option<IdentitySecret>,
    pub fingerprint: [u8; SHA512_HASH_SIZE],
}

#[derive(Eq, PartialEq, Clone, Debug, Ord, PartialOrd, Deserialize, Serialize)]
pub struct NetworkId(pub u64);

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
    /// Generate a new identity.
    pub fn generate() -> Self {
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
            self_sign_buf.push(IDENTITY_ALGORITHM_EC_NIST_P384);
            let _ = self_sign_buf.write_all(p384_ecdh.public_key_bytes());
            let _ = self_sign_buf.write_all(p384_ecdsa.public_key_bytes());

            // Sign all keys including the x25519 ones with the new P-384 keys.
            let ecdsa_self_signature = p384_ecdsa.sign(self_sign_buf.as_slice());

            // Sign everything with the original ed25519 key to bind the new key pairs. Include the ECDSA
            // signature because these signatures are not deterministic. We don't want the ability to
            // make a new identity with the same address but a different fingerprint by mangling the
            // ECDSA signature in some way.
            let ed25519_self_signature = self.secret.as_ref().unwrap().ed25519.sign(self_sign_buf.as_slice());

            let _ = self.p384.insert(IdentityP384Public {
                ecdh: p384_ecdh.public_key().clone(),
                ecdsa: p384_ecdsa.public_key().clone(),
                ecdsa_self_signature,
                ed25519_self_signature,
            });
            let _ = self.secret.as_mut().unwrap().p384.insert(IdentityP384Secret { ecdh: p384_ecdh, ecdsa: p384_ecdsa });
            self.fingerprint = SHA512::hash(self_sign_buf.as_slice());

            return Ok(true);
        }
        return Ok(false);
    }

    #[inline(always)]
    pub fn algorithms(&self) -> u8 {
        if self.p384.is_some() {
            IDENTITY_ALGORITHM_X25519 | IDENTITY_ALGORITHM_EC_NIST_P384
        } else {
            IDENTITY_ALGORITHM_X25519
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
            self_sign_buf.push(IDENTITY_ALGORITHM_EC_NIST_P384);
            let _ = self_sign_buf.write_all(p384.ecdh.as_bytes());
            let _ = self_sign_buf.write_all(p384.ecdsa.as_bytes());

            if !p384.ecdsa.verify(self_sign_buf.as_slice(), &p384.ecdsa_self_signature) {
                return false;
            }

            if !ed25519_verify(&self.ed25519, &p384.ed25519_self_signature, self_sign_buf.as_slice()) {
                return false;
            }
        }

        // NOTE: fingerprint is always computed locally, so no need to check it.

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
    /// no entropy is lost when deriving secrets with a KDF. Ciphers like AES use the first 256 bits
    /// of these keys.
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
    /// If legacy_compatibility is true this generates only an ed25519 signature. Otherwise it
    /// will generate a signature using both the ed25519 key and the P-384 key if the latter
    /// is present in the identity.
    ///
    /// A return of None happens if we don't have our secret key(s) or some other error occurs.
    pub fn sign(&self, msg: &[u8], legacy_compatibility: bool) -> Option<Vec<u8>> {
        if self.secret.is_some() {
            let secret = self.secret.as_ref().unwrap();
            if legacy_compatibility {
                Some(secret.ed25519.sign_zt(msg).to_vec())
            } else if secret.p384.is_some() {
                let mut tmp: Vec<u8> = Vec::with_capacity(1 + P384_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE);
                tmp.push(IDENTITY_ALGORITHM_X25519 | IDENTITY_ALGORITHM_EC_NIST_P384);
                let _ = tmp.write_all(&secret.p384.as_ref().unwrap().ecdsa.sign(msg));
                let _ = tmp.write_all(&secret.ed25519.sign(msg));
                Some(tmp)
            } else {
                let mut tmp: Vec<u8> = Vec::with_capacity(1 + ED25519_SIGNATURE_SIZE);
                tmp.push(IDENTITY_ALGORITHM_X25519);
                let _ = tmp.write_all(&secret.ed25519.sign(msg));
                Some(tmp)
            }
        } else {
            None
        }
    }

    /// Verify a signature against this identity.
    pub fn verify(&self, msg: &[u8], mut signature: &[u8]) -> bool {
        if signature.len() == 96 {
            // legacy ed25519-only signature with hash included
            ed25519_verify(&self.ed25519, signature, msg)
        } else if signature.len() > 1 {
            let algorithms = signature[0];
            signature = &signature[1..];
            let mut ok = true;
            let mut checked = false;
            if ok && (algorithms & IDENTITY_ALGORITHM_EC_NIST_P384) != 0 && signature.len() >= P384_ECDSA_SIGNATURE_SIZE && self.p384.is_some() {
                ok = self.p384.as_ref().unwrap().ecdsa.verify(msg, &signature[..P384_ECDSA_SIGNATURE_SIZE]);
                signature = &signature[P384_ECDSA_SIGNATURE_SIZE..];
                checked = true;
            }
            if ok && (algorithms & IDENTITY_ALGORITHM_X25519) != 0 && signature.len() >= ED25519_SIGNATURE_SIZE {
                ok = ed25519_verify(&self.ed25519, &signature[..ED25519_SIGNATURE_SIZE], msg);
                signature = &signature[ED25519_SIGNATURE_SIZE..];
                checked = true;
            }
            checked && ok
        } else {
            false
        }
    }

    #[inline(always)]
    pub fn to_bytes(&self, include_algorithms: u8, include_private: bool) -> Buffer<MAX_MARSHAL_SIZE> {
        let mut b: Buffer<MAX_MARSHAL_SIZE> = Buffer::new();
        self.marshal(&mut b, include_algorithms, include_private).expect("internal error marshaling Identity");
        b
    }

    const P384_PUBLIC_AND_PRIVATE_BUNDLE_SIZE: u16 = (P384_PUBLIC_KEY_SIZE + P384_PUBLIC_KEY_SIZE + P384_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE + P384_SECRET_KEY_SIZE + P384_SECRET_KEY_SIZE) as u16;
    const P384_PUBLIC_ONLY_BUNDLE_SIZE: u16 = (P384_PUBLIC_KEY_SIZE + P384_PUBLIC_KEY_SIZE + P384_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE) as u16;

    pub fn marshal<const BL: usize>(&self, buf: &mut Buffer<BL>, include_algorithms: u8, include_private: bool) -> std::io::Result<()> {
        let algorithms = self.algorithms() & include_algorithms;
        let secret = self.secret.as_ref();

        buf.append_bytes_fixed(&self.address.to_bytes())?;
        buf.append_u8(0x00)?; // LEGACY: 0x00 here for backward compatibility
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

        if (algorithms & IDENTITY_ALGORITHM_EC_NIST_P384) != 0 && self.p384.is_some() {
            let p384 = self.p384.as_ref().unwrap();

            /*
             * For legacy backward compatibility, any key pairs and other material after the x25519
             * keys are prefixed by 0x03 followed by the total size of this section. This lets us parsimoniously
             * maintain backward compatibility with old versions' parsing of HELLO.
             *
             * In old HELLO the identity was followed by an InetAddress. The InetAddress encoding does support
             * a variable length encoding for unknown "future use" address types. This consists of 0x03 followed
             * by a 16-bit size.
             *
             * By mimicking this we can create a HELLO containing a new format identity and cleverly skip the
             * InetAddress after it and old nodes will parse this as an old x25519 only identity followed by
             * an unrecognized type InetAddress that will be ignored.
             *
             * Key agreement can then proceed using only x25519 keys.
             */
            buf.append_u8(0x03)?;
            let p384_has_private = if include_private && secret.map_or(false, |s| s.p384.is_some()) {
                buf.append_u16(Self::P384_PUBLIC_AND_PRIVATE_BUNDLE_SIZE + 1 + 2)?;
                true
            } else {
                buf.append_u16(Self::P384_PUBLIC_ONLY_BUNDLE_SIZE + 1 + 2)?;
                false
            };

            buf.append_u8(IDENTITY_ALGORITHM_EC_NIST_P384)?;
            if p384_has_private {
                buf.append_u16(Self::P384_PUBLIC_AND_PRIVATE_BUNDLE_SIZE)?;
            } else {
                buf.append_u16(Self::P384_PUBLIC_ONLY_BUNDLE_SIZE)?;
            }
            buf.append_bytes_fixed(p384.ecdh.as_bytes())?;
            buf.append_bytes_fixed(p384.ecdsa.as_bytes())?;
            buf.append_bytes_fixed(&p384.ecdsa_self_signature)?;
            buf.append_bytes_fixed(&p384.ed25519_self_signature)?;
            if p384_has_private {
                let p384s = secret.unwrap().p384.as_ref().unwrap();
                buf.append_bytes_fixed(&p384s.ecdh.secret_key_bytes().0)?;
                buf.append_bytes_fixed(&p384s.ecdsa.secret_key_bytes().0)?;
            }
        }

        Ok(())
    }

    pub fn unmarshal<const BL: usize>(buf: &Buffer<BL>, cursor: &mut usize) -> std::io::Result<Identity> {
        let address = Address::from_bytes(buf.read_bytes_fixed::<ADDRESS_SIZE>(cursor)?);
        if !address.is_some() {
            return std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid address"));
        }
        let address = address.unwrap();

        let mut x25519_public: Option<([u8; C25519_PUBLIC_KEY_SIZE], [u8; ED25519_PUBLIC_KEY_SIZE])> = None;
        let mut x25519_secret: Option<([u8; C25519_SECRET_KEY_SIZE], [u8; ED25519_SECRET_KEY_SIZE])> = None;
        let mut p384_ecdh_ecdsa_public: Option<(P384PublicKey, P384PublicKey, [u8; P384_ECDSA_SIGNATURE_SIZE], [u8; ED25519_SIGNATURE_SIZE])> = None;
        let mut p384_ecdh_ecdsa_secret: Option<([u8; P384_SECRET_KEY_SIZE], [u8; P384_SECRET_KEY_SIZE])> = None;

        loop {
            let algorithm = buf.read_u8(cursor);
            if algorithm.is_err() {
                break;
            }
            match algorithm.unwrap() {
                0x00 | IDENTITY_ALGORITHM_X25519 => {
                    let a = buf.read_bytes_fixed::<C25519_PUBLIC_KEY_SIZE>(cursor)?;
                    let b = buf.read_bytes_fixed::<ED25519_PUBLIC_KEY_SIZE>(cursor)?;
                    x25519_public = Some((a.clone(), b.clone()));
                    let sec_size = buf.read_u8(cursor)?;
                    if sec_size == (C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE) as u8 {
                        let a = buf.read_bytes_fixed::<C25519_SECRET_KEY_SIZE>(cursor)?;
                        let b = buf.read_bytes_fixed::<ED25519_SECRET_KEY_SIZE>(cursor)?;
                        x25519_secret = Some((a.clone(), b.clone()));
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
                IDENTITY_ALGORITHM_EC_NIST_P384 => {
                    let size = buf.read_u16(cursor)?;
                    if size < Self::P384_PUBLIC_ONLY_BUNDLE_SIZE {
                        return std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid p384 public key"));
                    }
                    let a = buf.read_bytes_fixed::<P384_PUBLIC_KEY_SIZE>(cursor)?;
                    let b = buf.read_bytes_fixed::<P384_PUBLIC_KEY_SIZE>(cursor)?;
                    let c = buf.read_bytes_fixed::<P384_ECDSA_SIGNATURE_SIZE>(cursor)?;
                    let d = buf.read_bytes_fixed::<ED25519_SIGNATURE_SIZE>(cursor)?;
                    let a = P384PublicKey::from_bytes(a);
                    let b = P384PublicKey::from_bytes(b);
                    if a.is_none() || b.is_none() {
                        return std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid p384 public key"));
                    }
                    p384_ecdh_ecdsa_public = Some((a.unwrap(), b.unwrap(), c.clone(), d.clone()));
                    if size > Self::P384_PUBLIC_ONLY_BUNDLE_SIZE {
                        if size != Self::P384_PUBLIC_AND_PRIVATE_BUNDLE_SIZE {
                            return std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid p384 secret key"));
                        }
                        let a = buf.read_bytes_fixed::<P384_SECRET_KEY_SIZE>(cursor)?;
                        let b = buf.read_bytes_fixed::<P384_SECRET_KEY_SIZE>(cursor)?;
                        p384_ecdh_ecdsa_secret = Some((a.clone(), b.clone()));
                    }
                }
                _ => {
                    // Skip any unrecognized cipher suites, all of which will be prefixed by a size.
                    *cursor += buf.read_u16(cursor)? as usize;
                    if *cursor > buf.len() {
                        return std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid field length"));
                    }
                }
            }
        }

        if x25519_public.is_none() {
            return std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "x25519 key missing"));
        }
        let x25519_public = x25519_public.unwrap();

        let mut sha = SHA512::new();
        sha.update(&address.to_bytes());
        sha.update(&x25519_public.0);
        sha.update(&x25519_public.1);
        if p384_ecdh_ecdsa_public.is_some() {
            let p384 = p384_ecdh_ecdsa_public.as_ref().unwrap();
            sha.update(&[IDENTITY_ALGORITHM_EC_NIST_P384]);
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
                let c25519_secret = C25519KeyPair::from_bytes(&x25519_public.0, &x25519_secret.0);
                let ed25519_secret = Ed25519KeyPair::from_bytes(&x25519_public.1, &x25519_secret.1);
                if c25519_secret.is_none() || ed25519_secret.is_none() {
                    return std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "x25519 public key invalid"));
                }
                Some(IdentitySecret {
                    c25519: c25519_secret.unwrap(),
                    ed25519: ed25519_secret.unwrap(),
                    p384: if p384_ecdh_ecdsa_secret.is_some() && p384_ecdh_ecdsa_public.is_some() {
                        let p384_ecdh_ecdsa_public = p384_ecdh_ecdsa_public.as_ref().unwrap();
                        let p384_ecdh_ecdsa_secret = p384_ecdh_ecdsa_secret.as_ref().unwrap();
                        let p384_ecdh_secret = P384KeyPair::from_bytes(p384_ecdh_ecdsa_public.0.as_bytes(), &p384_ecdh_ecdsa_secret.0);
                        let p384_ecdsa_secret = P384KeyPair::from_bytes(p384_ecdh_ecdsa_public.1.as_bytes(), &p384_ecdh_ecdsa_secret.1);
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

    /// Marshal this identity as a string with options to control which ciphers are included and whether private keys are included.
    pub fn to_string_with_options(&self, include_algorithms: u8, include_private: bool) -> String {
        if include_private && self.secret.is_some() {
            let secret = self.secret.as_ref().unwrap();
            if (include_algorithms & IDENTITY_ALGORITHM_EC_NIST_P384) == IDENTITY_ALGORITHM_EC_NIST_P384 && secret.p384.is_some() && self.p384.is_some() {
                let p384_secret = secret.p384.as_ref().unwrap();
                let p384 = self.p384.as_ref().unwrap();
                let p384_secret_joined: [u8; P384_SECRET_KEY_SIZE + P384_SECRET_KEY_SIZE] = concat_arrays_2(p384_secret.ecdh.secret_key_bytes().as_bytes(), p384_secret.ecdsa.secret_key_bytes().as_bytes());
                let p384_joined: [u8; P384_PUBLIC_KEY_SIZE + P384_PUBLIC_KEY_SIZE + P384_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE] = concat_arrays_4(p384.ecdh.as_bytes(), p384.ecdsa.as_bytes(), &p384.ecdsa_self_signature, &p384.ed25519_self_signature);
                format!(
                    "{}:0:{}{}:{}{}:2:{}:{}",
                    self.address.to_string(),
                    hex::to_string(&self.c25519),
                    hex::to_string(&self.ed25519),
                    hex::to_string(&secret.c25519.secret_bytes().0),
                    hex::to_string(&secret.ed25519.secret_bytes().0),
                    base64::encode_config(p384_joined, base64::URL_SAFE_NO_PAD),
                    base64::encode_config(p384_secret_joined, base64::URL_SAFE_NO_PAD)
                )
            } else {
                format!("{}:0:{}{}:{}{}", self.address.to_string(), hex::to_string(&self.c25519), hex::to_string(&self.ed25519), hex::to_string(&secret.c25519.secret_bytes().0), hex::to_string(&secret.ed25519.secret_bytes().0))
            }
        } else {
            self.p384.as_ref().map_or_else(
                || format!("{}:0:{}{}", self.address.to_string(), hex::to_string(&self.c25519), hex::to_string(&self.ed25519)),
                |p384| {
                    let p384_joined: [u8; P384_PUBLIC_KEY_SIZE + P384_PUBLIC_KEY_SIZE + P384_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE] = concat_arrays_4(p384.ecdh.as_bytes(), p384.ecdsa.as_bytes(), &p384.ecdsa_self_signature, &p384.ed25519_self_signature);
                    format!("{}:0:{}{}::2:{}", self.address.to_string(), hex::to_string(&self.c25519), hex::to_string(&self.ed25519), base64::encode_config(p384_joined, base64::URL_SAFE_NO_PAD))
                },
            )
        }
    }

    /// Get this identity in string form with all ciphers and with secrets (if present)
    pub fn to_secret_string(&self) -> String {
        self.to_string_with_options(IDENTITY_ALGORITHM_ALL, true)
    }
}

impl ToString for Identity {
    /// Get only the public portion of this identity as a string, including all cipher suites.
    #[inline(always)]
    fn to_string(&self) -> String {
        self.to_string_with_options(IDENTITY_ALGORITHM_ALL, false)
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

        let keys =
            [hex::from_string(keys[0].unwrap_or("")), hex::from_string(keys[1].unwrap_or("")), base64::decode_config(keys[2].unwrap_or(""), base64::URL_SAFE_NO_PAD).unwrap_or_else(|_| Vec::new()), base64::decode_config(keys[3].unwrap_or(""), base64::URL_SAFE_NO_PAD).unwrap_or_else(|_| Vec::new())];
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
            sha.update(&[IDENTITY_ALGORITHM_EC_NIST_P384]);
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
#[inline(always)]
pub(crate) fn purge_verification_memory_pool() {
    ADDRESS_DERVIATION_MEMORY_POOL.purge();
}

#[cfg(test)]
mod tests {
    use crate::vl1::identity::{Identity, IDENTITY_ALGORITHM_ALL};
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
        "a8f6e0566e:0:a13a6394de205384eb75eb62179ef11423295c5ecdccfeed7f2eff6c7a74f8059c99eed164c5dfaf2a4cf395ec7b72b68ee1c3c31916de4bc57c07abfe77f9c2::2:A1Cj2O0hKLlhDQ6guCCv5H1UgzbegZwse0iqTaaZov9LpKifyKH0e1VzmHrPmoKcvgJyzI-BAqRQzBiUjScXIjojneNKOywc0Gvq-zeDCYPcXN393xi3q25mB3ud9iEN-GN6wiXPWFjHy-CBD9tGDJzr-G3ZJZvrdiLGT5rZ5W2cZtx8ORYnp9L9HJJOeb8qgdfVr67B5pT9jPsxSsw8P4qlzFFOlX2WN9Hvvu0TO6S_N4yq173deyr-f-ehcBFiBXsSG96p44oU4uRRBEDZWhzHDuD22Vw8PhsB8mko9IRqVXCGbvlaKJ0vyAZ_PyVRM9n_Z-HAEvLveAT-f61mh4YP",
        "d913346e57:0:afa3bb56d4f8aae1f91205e3629fa80db32f8a67698a0ba4e5a948105572b91f7f5368d4bcbf99424e359d8ac461ebd075d336a91651968d31c625d2c6dcd7ca::2:A_ApZGZgN_ImMs7FawasAsthZ5NIqcjp1vAj0Gjcrl8ugwY2CXbHIKFCertcdPNEuQM5HIYmfIB3iTXN3-CtjgBgI5KpJ_Jzm-BoTQKgSHlhVfiSbUQkgmlntXK8yG165rYDdAyA4U7vAipOT9kdeolr4WddCso11M0B_V7O72u0Nmquaw0KRF-GpS_LaM96LHJBKoTSwKrymmgelXV8VYnO-tY_YJPWtdtfXjoI9nz6q4cTHgQDw3NlUDMt1Zd7c9d7jrKI-FOca2GycVtDEHGq2yxZl7dPHWrpP96yr9NXrziXJo9UWaxJW0nMka7eTRwzzhhdIsM9Ra-2k4pZmvEH",
        "ad52ee12b3:0:e8cafd1e4794f259a481b466009b3d1a267653246d21ba9c5feb803ff63cc829f6e45070585f069440cb5ad46b8f17ab90894be7efce7d3d1de6ee00ce7d3fe5::2:A_DC0dhns3PEzcdo6HKr83BSZ-Jc7R25HTkAq9r7HHAhC7Fg49ijzTJ9FT5p0_t-7AKZHxLpCOf8h4bwTCSqUDl5UT5_5m4X3aHCEyUwZR0CjbNaXL2lZVlPn3EnuBEq5w3zKHO8LeYPIXPgmsG0KkUk3eRXDjgZF24YNY4Iu10IMVLgiSAMWRveh5Dg_ShkEWWsbBK5IHkWm9tqkthm61sg9FgLbyWRsj3D-GF_X6g3Tz3a9iOKiTzTX2roR4OERbxnWV5lFggrlkoJFEUm0MsKTCX8ul_gO_bhYUIXSqibVn7XlIF_fj69frWgDgBB4ehCuBDuoMopRgbTZYWw9BEP",
        "2e7864c663:0:6564af3f6feb9a8239ec9d7e4e8f1ded503dbac95d5f197c73103b4f9e325c322f82284a22e4eefa9a4077d7b25d335845858fdae85f2b0eef1fca27f45015e9::2:AyvRLX_lyzC6DTtK6JSd-Gxk954kHWDIuUwY8PFCUv-xrzmWKIJqNM_FWkFhzlon9wKaqWbTzHm3Frb39zzvC-24-AkYVnnvkoOlUaifnk8a0ndfvTHruYoBZ_ojJqsonErET-cZrRZ_QjeShHYmlggz1W_Ipkcut7R0vjLev5jHGQSZh7CYNo7FsGc7POjxUKgUD7F1DNnbb623Ecq9A7uZoTDcER5wjNhjR9ov1cVUExPHD_BLiszRMCFvTxqPDZB-_1HUw3KhRjUed0k_pF2HQkbfggiQ9ZPcGTNur17Y2fc8-P1hcH5LIZA6bt1AjXAM4R1bfTHUpiA01WJHe5QL",
        "3ab7e39024:0:44f07b42dad081542e3c24f39b39fe6ca6b168236578a8360600682cd98bb904636a2f90ef5e3f09bb690362383e9a5fcd84a6b4cd74ac5fe73e2514eb701471::2:Ar7x1arjl5HnkLCufDF-QpD2n9UqfaMvZXCcxVGVyfDfWnwCzEkzPFJwmrRYXoRW2ANnkjijRfJI4aADvd0rxthp4NLkkXdWIyzifnLVgPO0ieRTv6wveAPYGHFHpi6wnKiUEq40zcAXe4XEdY1YHeMyTaArg33vQSw9lUvHmvxIsh1BErYe1_TSxkxqogVu_X_9QXiIu6fn1BXTuYcLDOKOQzPurl_1g2uILhow9wV_jlMsmEIIA6LNgcmQtR-Qyvl4j9--cmQyZ3GNw1Qx5EnTwCkiLyoLGnrynqxVFBriJTU6wYvZEHBgPGBy9AtkI9B7J4IMTNi6RM1fcu6Kv4QH",
        "d29e7b84cf:0:b1d15bfd75bedfafe92b556deee8c9c820d6ef7688d4020f7032bee59a1d3a5f6afa74d06c6391f0c7dba12eb2501b737abc5a64c16dae54fe4b63b575930363::2:Apad_7rAJvntlpC1-ikl174cAHI7iHUOWiTk3XOMmAUVkmwWO-qUoDkpsYd6jrSCzAIyopjlJIlK0Fw8O8Y4n4njLzVqXsWTGE61CBouoYcMwm0qNVbu6R8RiYnWTg247dvfttQCzLu66ZKwsMTl7_iS52FnUdNC-nznYvqSHRQYOVTamVTWfMxIc0tsYcyRumkLn0q9bzvOarHCuxB2oV2Wu1sLpPQEqe3qNThtxObfFcTVsDcMCSq8gcii0jLJxSrK7Bv7ZkoIq4Q0lD_JH9bp-v_52l7CRye8gZaa-3wScenW9VnxAEWly7YiHgRIF2oNZMbavXyM2PJuv4CvAnUJ",
        "2e9dc1b7fc:0:4bb7eda550d7fe1f50b9bf473d9141d086728f773755abb66c45ba35e134264e80e26d77944a1a7e2aecb3000117735f0b74a2efa4fb2e4f3ca8d2346ea39485::2:A6B4txc97pW0JR3bXm-90_OuOJBs54p0gD9crTVnHi6mMTAujELLh7_E3aNh6XKviwNi-UXZgI5ip21RayLH3EiUOErSWO2sxQWi-lUY5XyeSwp-sOqDuRtgZVXDdBUsWf4X5lW2K_Oly5gD7InS29QVu2kkHxnd_FEKvC0-EF_jCr_UTiR_Mki0MZIGoecP_5a_CRTsLuVg7i0c5_pEypNcys6flXtNSpzev0ramywTkIN7IU3b-9lL4nsxwMlTHS2qXR-NQP-Ic9yIZ-NR-wr6tW7IcsTR9D0jB5H8SZyBMzXF90zaRLL_YgUZKRJ6TFlvHu2moNHdqSjvnklf1UoE",
        "37cab19425:0:d9c9e16a56dc1a7e51b575b9a2d63a39c4d2d4d3b7b6ec9120a434aff0f3d54e09ac0fc5452b78613154611944d4d0372a8cab42e29dd2c062375f21f750f12f::2:A9ACtecFby8mpN9nAGc_UE7AMBS035_z28KiXHfbqL7T2uoI2ig0HRYjAB_E_WYhzgJb6AykhZtQ6wUInzSZCttpxSgnbnvJQ56bkkKa-QIlE68KlG8DBt0rIvf9T30q3z_Yd9NVsqJhpJJyQMin1juqpQ7rKNNQLe95XXv28lWrELYgXroDnnF2RWJF4BBDcISx4mNLaXJgojAaT6EqoletIA-9_71rl0SeKDh6cV8leMGYkdkFsqPXhvf3-mx9_seuTOFmQvohXf3pGjWjK0jJGEiQHblo2h7KEMnuwDSsZNjx7UsiqyXEH98jP-haXmuLyWgOkSIj6FeaYBn8ir4D",
        "d9b3ef808f:0:fff56156ccbb67cfbcb22f8e11360e1f72049c607b0d037ef0502d6332c1dc4ac52caad347fd1a797272b1afe1cdd78203e0672d50af3d349e1db3c8f73f815e::2:A2XJ4QNrEQC9sh_dNrhDre2YkX4IWesb9H6c466Tg99sWEpDPOxsSLkflPj0uLL3wQJHlCgBwpd6zQJRaxABCaQzri-ODlN_z58M1casR47Ez_E3ZoznKG3I3Dh4cTo0fchudJiPF2sE66cRI_in20-95LpbbQeP6_PquiG0z_C9zEbIOopfllM4GlsXx_KKV6pYHooWNpai6QEGU9KRyJJqOVY3wh3v0lYJNNN8BG00rj_TQBR0ZuPhEE5pM2KiOWkhpgC-P3AChn1oQp78fH08FVoW9TXyQMArsGp7Cj-j-jvG6PyRfLUxI90h_taqPrLPnGHYZ5wqkFlsRDs86pIE",
        "637cb047d7:0:fe11b6f03dc441f6070b631d2f948d74381b7b6da83cbfbdc859bbc7c7ee6675963576322830f52d01929b47703151cb9462f82103781a172a5542aec3d69f88::2:AzLuKAy1qkieKWN6fYDduBzkK7m330NJoLByI6S8-LeyfsWNXxn6XLO1mFklAPwvnAOdatyaEUYE5S2zGIdRFkWOBiTXNnzCTNBVhVQC0VXua4tqIL8BlumH_unK4oAEIRGWlt5gyU9wmpFJtA27b0YUUwUzR9V1cSnze5oxwheWlsT5befZQbR8sNOP89Jh9fkYxoHCWTiaBT7LtmD_kGCahXZXt6EK5nypJkyN_Pkj7Csyk2Y7gCFjo4kz86UFa-wM88zDFzrxrM4cnQT2unatIoiTATTZBOg7hRM5elvtWt5P-4sUMHHmsQwukF6bgYuQkpgD2AZ3o6y7pIYWrUIE"
    ];

    #[test]
    fn marshal_unmarshal_sign_verify_agree() {
        let gen = Identity::generate();
        assert!(gen.agree(&gen).is_some());
        assert!(gen.validate_identity());
        let bytes = gen.to_bytes(IDENTITY_ALGORITHM_ALL, true);
        let string = gen.to_string_with_options(IDENTITY_ALGORITHM_ALL, true);
        assert!(Identity::from_str(string.as_str()).unwrap().eq(&gen));
        let mut cursor = 0_usize;
        assert!(Identity::unmarshal(&bytes, &mut cursor).unwrap().eq(&gen));
        cursor = 0;
        assert!(Identity::unmarshal(&bytes, &mut cursor).unwrap().secret.is_some());
        assert!(Identity::from_str(string.as_str()).unwrap().secret.is_some());

        let gen2 = Identity::generate();
        assert!(gen2.validate_identity());
        assert!(gen2.agree(&gen).unwrap().eq(&gen.agree(&gen2).unwrap()));

        for id_str in GOOD_V0_IDENTITIES {
            let mut id = Identity::from_str(id_str).unwrap();

            assert!(id.validate_identity());
            assert!(id.p384.is_none());

            let idb = id.to_bytes(IDENTITY_ALGORITHM_ALL, true);
            let mut cursor = 0;
            let id_unmarshal = Identity::unmarshal(&idb, &mut cursor).unwrap();
            assert!(id == id_unmarshal);
            assert!(id_unmarshal.secret.is_some());

            let idb2 = id_unmarshal.to_bytes(IDENTITY_ALGORITHM_ALL, false);
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

            assert!(id.validate_identity());
            assert!(id.p384.is_some());

            let idb = id.to_bytes(IDENTITY_ALGORITHM_ALL, true);
            let mut cursor = 0;
            let id_unmarshal = Identity::unmarshal(&idb, &mut cursor).unwrap();
            assert!(id == id_unmarshal);

            cursor = 0;
            let idb2 = id_unmarshal.to_bytes(IDENTITY_ALGORITHM_ALL, false);
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
