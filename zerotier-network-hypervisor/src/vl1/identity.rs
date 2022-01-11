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
use std::ops::Deref;
use std::ptr::{slice_from_raw_parts, slice_from_raw_parts_mut};
use std::str::FromStr;

use lazy_static::lazy_static;

use zerotier_core_crypto::c25519::{C25519_PUBLIC_KEY_SIZE, C25519_SECRET_KEY_SIZE, C25519KeyPair, ED25519_PUBLIC_KEY_SIZE, ED25519_SECRET_KEY_SIZE, ED25519_SIGNATURE_SIZE, ed25519_verify, Ed25519KeyPair};
use zerotier_core_crypto::hash::{SHA384, SHA384_HASH_SIZE, SHA512};
use zerotier_core_crypto::hex;
use zerotier_core_crypto::p521::{P521_ECDSA_SIGNATURE_SIZE, P521_PUBLIC_KEY_SIZE, P521_SECRET_KEY_SIZE, P521KeyPair, P521PublicKey};
use zerotier_core_crypto::salsa::Salsa;
use zerotier_core_crypto::secret::Secret;

use crate::error::InvalidFormatError;
use crate::util::array_range;
use crate::util::buffer::Buffer;
use crate::util::pool::{Pool, Pooled, PoolFactory};
use crate::vl1::Address;
use crate::vl1::protocol::{ADDRESS_SIZE, ADDRESS_SIZE_STRING, IDENTITY_V0_POW_THRESHOLD, IDENTITY_V1_POW_THRESHOLD};

/// X25519 cipher suite (present in all identities, so no actual flag).
pub const IDENTITY_CIPHER_SUITE_X25519: u8 = 0x00;

/// NIST P-521 ECDH/ECDSA cipher suite.
///
/// Sooo.... why 0x03 and not 0x01 or some other value? It's to compensate at the cost of
/// one wasted bit in our bit mask for a short-sighted aspect of the old identity encoding
/// and HELLO packet encoding.
///
/// The old identity encoding contains no provision for skipping data it doesn't understand
/// nor any provision for an upgrade. That's dumb, but there it is on millions of nodes. The
/// place where this matters in terms of identities the most is the HELLO packet.
///
/// In HELLO the identity is sent immediately followed by an endpoint, namely the InetAddress to
/// which the packet was sent. The old InetAddress DOES have a provision for extension. The type
/// byte 0x04 is followed by a 16-bit size for an "unknown address type" so it can be skipped if
/// it is not understood.
///
/// If we preface the x25519 key with 0x00 as normal and then preface the next key with 0x04,
/// we can follow that by what should have been there in the first place: a size for the remaining
/// identity fields.
///
/// When old nodes parse a HELLO they will interpret this as an x25519 identity followed by an
/// unrecognized InetAddress type that will be silently ignored.
///
/// The one wasted bit (0x02) is in a field with only one bit in use anyway. It means we can only
/// add six more cipher suites in the future, and if we're adding that many something is wrong.
/// There will probably only be one more ever, a post-quantum long term key.
pub const IDENTITY_CIPHER_SUITE_EC_NIST_P521: u8 = 0x03;

/// Mask for functions that take a cipher mask to use all available ciphers or the best available cipher.
pub const IDENTITY_CIPHER_SUITE_INCLUDE_ALL: u8 = 0xff;

/// Current sanity limit for the size of a marshaled Identity (can be increased if needed).
pub const MAX_MARSHAL_SIZE: usize = ADDRESS_SIZE + C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE + P521_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE + 16;

#[derive(Clone)]
pub struct IdentityP521Secret {
    pub ecdh: P521KeyPair,
    pub ecdsa: P521KeyPair,
}

#[derive(Clone)]
pub struct IdentityP521Public {
    pub ecdh: [u8; P521_PUBLIC_KEY_SIZE],
    pub ecdsa: [u8; P521_PUBLIC_KEY_SIZE],
    pub ecdsa_self_signature: [u8; P521_ECDSA_SIGNATURE_SIZE],
    pub ed25519_self_signature: [u8; ED25519_SIGNATURE_SIZE],
}

#[derive(Clone)]
pub struct IdentitySecret {
    pub c25519: C25519KeyPair,
    pub ed25519: Ed25519KeyPair,
    pub p521: Option<IdentityP521Secret>,
}

#[derive(Clone)]
pub struct Identity {
    pub address: Address,
    pub c25519: [u8; C25519_PUBLIC_KEY_SIZE],
    pub ed25519: [u8; ED25519_PUBLIC_KEY_SIZE],
    pub p521: Option<IdentityP521Public>,
    pub secret: Option<IdentitySecret>,
}

#[inline(always)]
fn concat_arrays_2<const A: usize, const B: usize, const S: usize>(a: &[u8; A], b: &[u8; B]) -> [u8; S] {
    debug_assert_eq!(A + B, S);
    let mut tmp: [u8; S] = unsafe { MaybeUninit::uninit().assume_init() };
    tmp[..A].copy_from_slice(a);
    tmp[A..].copy_from_slice(b);
    tmp
}

#[inline(always)]
fn concat_arrays_4<const A: usize, const B: usize, const C: usize, const D: usize, const S: usize>(a: &[u8; A], b: &[u8; B], c: &[u8; C], d: &[u8; D]) -> [u8; S] {
    debug_assert_eq!(A + B + C + D, S);
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
        let ed25519 = Ed25519KeyPair::generate(false);
        let ed25519_pub = ed25519.public_bytes();
        let mut address;
        let mut c25519;
        let mut c25519_pub;
        let mut genmem_pool_obj = unsafe { ADDRESS_DERVIATION_MEMORY_POOL.get() };
        loop {
            c25519 = C25519KeyPair::generate(false);
            c25519_pub = c25519.public_bytes();

            sha.update(&c25519_pub);
            sha.update(&ed25519_pub);
            let mut digest = sha.finish();
            zt_address_derivation_memory_intensive_hash(&mut digest, &mut genmem_pool_obj);

            if digest[0] < IDENTITY_V1_POW_THRESHOLD {
                let addr = Address::from_bytes(&digest[59..64]);
                if addr.is_some() {
                    address = addr.unwrap();
                    break;
                }
            }

            sha.reset();
        }
        drop(genmem_pool_obj);

        let p521_ecdh = P521KeyPair::generate(false).unwrap();
        let p521_ecdsa = P521KeyPair::generate(false).unwrap();
        let p521_ecdh_pub = p521_ecdh.public_key_bytes().clone();
        let p521_ecdsa_pub = p521_ecdsa.public_key_bytes().clone();

        let mut self_sign_buf: Vec<u8> = Vec::with_capacity(ADDRESS_SIZE + 4 + C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE);
        let _ = self_sign_buf.write_all(&address.to_bytes());
        self_sign_buf.push(IDENTITY_CIPHER_SUITE_X25519);
        let _ = self_sign_buf.write_all(&c25519_pub);
        let _ = self_sign_buf.write_all(&ed25519_pub);
        self_sign_buf.push(IDENTITY_CIPHER_SUITE_X25519);
        self_sign_buf.push(IDENTITY_CIPHER_SUITE_EC_NIST_P521);
        let _ = self_sign_buf.write_all(&p521_ecdh_pub);
        let _ = self_sign_buf.write_all(&p521_ecdsa_pub);
        self_sign_buf.push(IDENTITY_CIPHER_SUITE_EC_NIST_P521);

        Self {
            address,
            c25519: c25519_pub,
            ed25519: ed25519_pub,
            p521: Some(IdentityP521Public {
                ecdh: p521_ecdh_pub,
                ecdsa: p521_ecdsa_pub,
                ecdsa_self_signature: p521_ecdsa.sign(self_sign_buf.as_slice()).expect("NIST P-521 signature failed in identity generation"),
                ed25519_self_signature: ed25519.sign(self_sign_buf.as_slice()),
            }),
            secret: Some(IdentitySecret {
                c25519,
                ed25519,
                p521: Some(IdentityP521Secret {
                    ecdh: p521_ecdh,
                    ecdsa: p521_ecdsa,
                }),
            }),
        }
    }

    /// Get a bit mask of this identity's available cipher suites.
    #[inline(always)]
    pub fn cipher_suites(&self) -> u8 {
        if self.p521.is_some() {
            IDENTITY_CIPHER_SUITE_X25519 | IDENTITY_CIPHER_SUITE_EC_NIST_P521
        } else {
            IDENTITY_CIPHER_SUITE_X25519
        }
    }

    /// Get a SHA384 hash of this identity's address and public keys.
    /// This provides a globally unique 384-bit fingerprint of this identity.
    pub fn hash(&self) -> [u8; SHA384_HASH_SIZE] {
        let mut sha = SHA384::new();
        sha.update(&self.address.to_bytes());
        // don't prefix x25519 with cipher suite for backward compatibility
        sha.update(&self.c25519);
        sha.update(&self.ed25519);
        let _ = self.p521.as_ref().map(|p521| {
            sha.update(&[IDENTITY_CIPHER_SUITE_EC_NIST_P521]);
            sha.update(&p521.ecdh);
            sha.update(&p521.ecdsa);
            sha.update(&p521.ecdsa_self_signature);
            sha.update(&[IDENTITY_CIPHER_SUITE_EC_NIST_P521]);
        });
        sha.finish()
    }

    /// Locally check the validity of this identity.
    ///
    /// This is somewhat time consuming due to the memory-intensive work algorithm.
    pub fn validate_identity(&self) -> bool {
        let pow_threshold = if self.p521.is_some() {
            let p521 = self.p521.as_ref().unwrap();
            let mut self_sign_buf: Vec<u8> = Vec::with_capacity(ADDRESS_SIZE + 4 + C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE);
            let _ = self_sign_buf.write_all(&self.address.to_bytes());
            self_sign_buf.push(IDENTITY_CIPHER_SUITE_X25519);
            let _ = self_sign_buf.write_all(&self.c25519);
            let _ = self_sign_buf.write_all(&self.ed25519);
            self_sign_buf.push(IDENTITY_CIPHER_SUITE_X25519);
            self_sign_buf.push(IDENTITY_CIPHER_SUITE_EC_NIST_P521);
            let _ = self_sign_buf.write_all(&p521.ecdh);
            let _ = self_sign_buf.write_all(&p521.ecdsa);
            self_sign_buf.push(IDENTITY_CIPHER_SUITE_EC_NIST_P521);

            if !P521PublicKey::from_bytes(&p521.ecdsa).map_or(false, |ecdsa_pub| ecdsa_pub.verify(self_sign_buf.as_slice(), &p521.ecdsa_self_signature)) {
                return false;
            }
            if !ed25519_verify(&self.ed25519, &p521.ed25519_self_signature, self_sign_buf.as_slice()) {
                return false;
            }

            IDENTITY_V1_POW_THRESHOLD
        } else {
            IDENTITY_V0_POW_THRESHOLD
        };

        let mut sha = SHA512::new();
        sha.update(&self.c25519);
        sha.update(&self.ed25519);
        let mut digest = sha.finish();
        let mut genmem_pool_obj = unsafe { ADDRESS_DERVIATION_MEMORY_POOL.get() };
        zt_address_derivation_memory_intensive_hash(&mut digest, &mut genmem_pool_obj);
        drop(genmem_pool_obj);

        return digest[0] < pow_threshold && Address::from_bytes(&digest[59..64]).map_or(false, |a| a == self.address);
    }

    /// Perform ECDH key agreement, returning a shared secret or None on error.
    ///
    /// An error can occur if this identity does not hold its secret portion or if either key is invalid.
    ///
    /// If both sides have NIST P-521 keys then key agreement is performed using both Curve25519 and
    /// NIST P-521 and the result is HMAC(Curve25519 secret, NIST P-521 secret).
    pub fn agree(&self, other: &Identity) -> Option<Secret<48>> {
        self.secret.as_ref().and_then(|secret| {
            let c25519_secret = Secret(SHA512::hash(&secret.c25519.agree(&other.c25519).0));

            // FIPS note: FIPS-compliant exchange algorithms must be the last algorithms in any HKDF chain
            // for the final result to be technically FIPS compliant. Non-FIPS algorithm secrets are considered
            // a salt in the HMAC(salt, key) HKDF construction.
            if secret.p521.is_some() && other.p521.is_some() {
                P521PublicKey::from_bytes(&other.p521.as_ref().unwrap().ecdh).and_then(|other_p521| {
                    secret.p521.as_ref().unwrap().ecdh.agree(&other_p521).map(|p521_secret| {
                        Secret(SHA384::hmac(&c25519_secret.0[0..48], &p521_secret.0))
                    })
                })
            } else {
                Some(Secret(array_range::<u8, 64, 0, 48>(&c25519_secret.0).clone()))
            }
        })
    }

    /// Sign a message with this identity.
    ///
    /// A return of None happens if we don't have our secret key(s) or some other error occurs.
    pub fn sign(&self, msg: &[u8], use_cipher_suites: u8) -> Option<Vec<u8>> {
        self.secret.as_ref().and_then(|secret| {
            if (use_cipher_suites & IDENTITY_CIPHER_SUITE_EC_NIST_P521) != 0 && secret.p521.is_some() {
                secret.p521.as_ref().unwrap().ecdsa.sign(msg).map(|sig| sig.to_vec())
            } else {
                Some(secret.ed25519.sign_zt(msg).to_vec())
            }
        })
    }

    /// Verify a signature against this identity.
    pub fn verify(&self, msg: &[u8], signature: &[u8]) -> bool {
        if signature.len() == 64 || signature.len() == 96 {
            ed25519_verify(&self.ed25519, signature, msg)
        } else if signature.len() == P521_ECDSA_SIGNATURE_SIZE && self.p521.is_some() {
            P521PublicKey::from_bytes(&self.p521.as_ref().unwrap().ecdsa).map_or(false, |p521_public| p521_public.verify(msg, signature))
        } else {
            false
        }
    }

    #[inline(always)]
    pub fn to_bytes(&self, include_cipher_suites: u8, include_private: bool) -> Buffer<MAX_MARSHAL_SIZE> {
        let mut b: Buffer<MAX_MARSHAL_SIZE> = Buffer::new();
        self.marshal(&mut b, include_cipher_suites, include_private).expect("internal error marshaling Identity");
        b
    }

    pub fn marshal<const BL: usize>(&self, buf: &mut Buffer<BL>, include_cipher_suites: u8, include_private: bool) -> std::io::Result<()> {
        let cipher_suites = self.cipher_suites() & include_cipher_suites;
        let secret = self.secret.as_ref();

        buf.append_bytes_fixed(&self.address.to_bytes())?;
        buf.append_u8(IDENTITY_CIPHER_SUITE_X25519)?;
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

        if (cipher_suites & IDENTITY_CIPHER_SUITE_EC_NIST_P521) == IDENTITY_CIPHER_SUITE_EC_NIST_P521 && self.p521.is_some() {
            let p521 = self.p521.as_ref().unwrap();
            let size = if include_private && secret.map_or(false, |s| s.p521.is_some()) {
                (P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE + P521_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE) as u16
            } else {
                (P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE) as u16
            };
            buf.append_u8(IDENTITY_CIPHER_SUITE_EC_NIST_P521)?;
            buf.append_u16(size)?;
            buf.append_bytes_fixed(&p521.ecdh)?;
            buf.append_bytes_fixed(&p521.ecdsa)?;
            buf.append_bytes_fixed(&p521.ecdsa_self_signature)?;
            buf.append_bytes_fixed(&p521.ed25519_self_signature)?;
            if size > (P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE) as u16 {
                let p521s = secret.unwrap().p521.as_ref().unwrap();
                buf.append_bytes_fixed(&p521s.ecdh.secret_key_bytes().0)?;
                buf.append_bytes_fixed(&p521s.ecdsa.secret_key_bytes().0)?;
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
        let mut p521_ecdh_ecdsa_public: Option<([u8; P521_PUBLIC_KEY_SIZE], [u8; P521_PUBLIC_KEY_SIZE], [u8; P521_ECDSA_SIGNATURE_SIZE], [u8; ED25519_SIGNATURE_SIZE])> = None;
        let mut p521_ecdh_ecdsa_secret: Option<([u8; P521_SECRET_KEY_SIZE], [u8; P521_SECRET_KEY_SIZE])> = None;

        loop {
            let cipher_suite = buf.read_u8(cursor);
            if cipher_suite.is_err() {
                break;
            }
            match cipher_suite.unwrap() {
                IDENTITY_CIPHER_SUITE_X25519 => {
                    let a = buf.read_bytes_fixed::<C25519_PUBLIC_KEY_SIZE>(cursor)?;
                    let b = buf.read_bytes_fixed::<ED25519_PUBLIC_KEY_SIZE>(cursor)?;
                    let _ = x25519_public.replace((a.clone(), b.clone()));
                    let sec_size = buf.read_u8(cursor)?;
                    if sec_size == (C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE) as u8 {
                        let a = buf.read_bytes_fixed::<C25519_SECRET_KEY_SIZE>(cursor)?;
                        let b = buf.read_bytes_fixed::<ED25519_SECRET_KEY_SIZE>(cursor)?;
                        let _ = x25519_secret.replace((a.clone(), b.clone()));
                    } else if sec_size != 0 {
                        return std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid x25519 secret"));
                    }
                }
                IDENTITY_CIPHER_SUITE_EC_NIST_P521 => {
                    let size = buf.read_u16(cursor)?;
                    if size < (P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE) as u16 {
                        return std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid p521 public key"));
                    }
                    let a = buf.read_bytes_fixed::<P521_PUBLIC_KEY_SIZE>(cursor)?;
                    let b = buf.read_bytes_fixed::<P521_PUBLIC_KEY_SIZE>(cursor)?;
                    let c = buf.read_bytes_fixed::<P521_ECDSA_SIGNATURE_SIZE>(cursor)?;
                    let d = buf.read_bytes_fixed::<ED25519_SIGNATURE_SIZE>(cursor)?;
                    let _ = p521_ecdh_ecdsa_public.replace((a.clone(), b.clone(), c.clone(), d.clone()));
                    if size > (P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE) as u16 {
                        if size != (P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE + P521_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE) as u16 {
                            return std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid p521 secret key"));
                        }
                        let a = buf.read_bytes_fixed::<P521_SECRET_KEY_SIZE>(cursor)?;
                        let b = buf.read_bytes_fixed::<P521_SECRET_KEY_SIZE>(cursor)?;
                        let _ = p521_ecdh_ecdsa_secret.replace((a.clone(), b.clone()));
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
        Ok(Identity {
            address,
            c25519: x25519_public.0.clone(),
            ed25519: x25519_public.1.clone(),
            p521: if p521_ecdh_ecdsa_public.is_some() {
                let p521_ecdh_ecdsa_public = p521_ecdh_ecdsa_public.as_ref().unwrap();
                Some(IdentityP521Public {
                    ecdh: p521_ecdh_ecdsa_public.0.clone(),
                    ecdsa: p521_ecdh_ecdsa_public.1.clone(),
                    ecdsa_self_signature: p521_ecdh_ecdsa_public.2.clone(),
                    ed25519_self_signature: p521_ecdh_ecdsa_public.3.clone(),
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
                    p521: if p521_ecdh_ecdsa_secret.is_some() && p521_ecdh_ecdsa_public.is_some() {
                        let p521_ecdh_ecdsa_public = p521_ecdh_ecdsa_public.as_ref().unwrap();
                        let p521_ecdh_ecdsa_secret = p521_ecdh_ecdsa_secret.as_ref().unwrap();
                        let p521_ecdh_secret = P521KeyPair::from_bytes(&p521_ecdh_ecdsa_public.0, &p521_ecdh_ecdsa_secret.0);
                        let p521_ecdsa_secret = P521KeyPair::from_bytes(&p521_ecdh_ecdsa_public.1, &p521_ecdh_ecdsa_secret.1);
                        if p521_ecdh_secret.is_none() || p521_ecdsa_secret.is_none() {
                            return std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "p521 secret key invalid"));
                        }
                        Some(IdentityP521Secret {
                            ecdh: p521_ecdh_secret.unwrap(),
                            ecdsa: p521_ecdsa_secret.unwrap(),
                        })
                    } else {
                        None
                    },
                })
            } else {
                None
            },
        })
    }

    /// Marshal this identity as a string with options to control which ciphers are included and whether private keys are included.
    /// Note that x25519 ciphers are always included as they are required. Use IDENTITY_CIPHER_SUITE_INCLUDE_ALL for all.
    pub fn to_string_with_options(&self, include_cipher_suites: u8, include_private: bool) -> String {
        if include_private && self.secret.is_some() {
            let secret = self.secret.as_ref().unwrap();
            if (include_cipher_suites & IDENTITY_CIPHER_SUITE_EC_NIST_P521) == IDENTITY_CIPHER_SUITE_EC_NIST_P521 && secret.p521.is_some() && self.p521.is_some() {
                let p521_secret = secret.p521.as_ref().unwrap();
                let p521 = self.p521.as_ref().unwrap();
                let p521_secret_joined: [u8; P521_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE] = concat_arrays_2(p521_secret.ecdh.secret_key_bytes().as_bytes(), p521_secret.ecdsa.secret_key_bytes().as_bytes());
                let p521_joined: [u8; P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE] = concat_arrays_4(&p521.ecdh, &p521.ecdsa, &p521.ecdsa_self_signature, &p521.ed25519_self_signature);
                format!("{}:0:{}{}:{}{}:1:{}:{}", self.address.to_string(), hex::to_string(&self.c25519), hex::to_string(&self.ed25519), hex::to_string(&secret.c25519.secret_bytes().0), hex::to_string(&secret.ed25519.secret_bytes().0), base64::encode_config(p521_joined, base64::URL_SAFE_NO_PAD), base64::encode_config(p521_secret_joined, base64::URL_SAFE_NO_PAD))
            } else {
                format!("{}:0:{}{}:{}{}", self.address.to_string(), hex::to_string(&self.c25519), hex::to_string(&self.ed25519), hex::to_string(&secret.c25519.secret_bytes().0), hex::to_string(&secret.ed25519.secret_bytes().0))
            }
        } else {
            self.p521.as_ref().map_or_else(|| {
                format!("{}:0:{}{}", self.address.to_string(), hex::to_string(&self.c25519), hex::to_string(&self.ed25519))
            }, |p521| {
                let p521_joined: [u8; P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE] = concat_arrays_4(&p521.ecdh, &p521.ecdsa, &p521.ecdsa_self_signature, &p521.ed25519_self_signature);
                format!("{}:0:{}{}::1:{}", self.address.to_string(), hex::to_string(&self.c25519), hex::to_string(&self.ed25519), base64::encode_config(p521_joined, base64::URL_SAFE_NO_PAD))
            })
        }
    }

    /// Get this identity in string form with all ciphers and with secrets (if present)
    pub fn to_secret_string(&self) -> String { self.to_string_with_options(IDENTITY_CIPHER_SUITE_INCLUDE_ALL, true) }
}

impl ToString for Identity {
    /// Get only the public portion of this identity as a string, including all cipher suites.
    #[inline(always)]
    fn to_string(&self) -> String { self.to_string_with_options(IDENTITY_CIPHER_SUITE_INCLUDE_ALL, false) }
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

        // x25519 public, x25519 secret, p521 public, p521 secret
        let mut keys: [Option<&str>; 4] = [None, None, None, None];

        let mut ptr = 1;
        let mut state = 0;
        let mut key_ptr = 0;
        while ptr < fields.len() {
            match state {
                0 => {
                    if fields[ptr] == "0" {
                        key_ptr = 0;
                    } else if fields[ptr] == "1" {
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

        let keys = [hex::from_string(keys[0].unwrap_or("")), hex::from_string(keys[1].unwrap_or("")), base64::decode_config(keys[2].unwrap_or(""), base64::URL_SAFE_NO_PAD).unwrap_or_else(|_| Vec::new()), base64::decode_config(keys[3].unwrap_or(""), base64::URL_SAFE_NO_PAD).unwrap_or_else(|_| Vec::new())];
        if keys[0].len() != C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE {
            return Err(InvalidFormatError);
        }
        Ok(Identity {
            address,
            c25519: keys[0].as_slice()[0..32].try_into().unwrap(),
            ed25519: keys[0].as_slice()[32..64].try_into().unwrap(),
            p521: if keys[2].is_empty() {
                None
            } else {
                if keys[2].len() != P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE {
                    return Err(InvalidFormatError);
                }
                Some(IdentityP521Public {
                    ecdh: keys[2].as_slice()[0..132].try_into().unwrap(),
                    ecdsa: keys[2].as_slice()[132..264].try_into().unwrap(),
                    ecdsa_self_signature: keys[2].as_slice()[264..396].try_into().unwrap(),
                    ed25519_self_signature: keys[2].as_slice()[396..460].try_into().unwrap(),
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
                    p521: if keys[3].is_empty() {
                        None
                    } else {
                        if keys[2].len() != P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE {
                            return Err(InvalidFormatError);
                        }
                        if keys[3].len() != P521_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE {
                            return Err(InvalidFormatError);
                        }
                        Some(IdentityP521Secret {
                            ecdh: {
                                let tmp = P521KeyPair::from_bytes(&keys[2].as_slice()[0..132], &keys[3].as_slice()[..P521_SECRET_KEY_SIZE]);
                                if tmp.is_none() {
                                    return Err(InvalidFormatError);
                                }
                                tmp.unwrap()
                            },
                            ecdsa: {
                                let tmp = P521KeyPair::from_bytes(&keys[2].as_slice()[132..264], &keys[3].as_slice()[P521_SECRET_KEY_SIZE..]);
                                if tmp.is_none() {
                                    return Err(InvalidFormatError);
                                }
                                tmp.unwrap()
                            },
                        })
                    },
                })
            },
        })
    }
}

impl PartialEq for Identity {
    #[inline(always)]
    fn eq(&self, other: &Self) -> bool { self.address == other.address && self.c25519 == other.c25519 }
}

impl Eq for Identity {}

impl Ord for Identity {
    fn cmp(&self, other: &Self) -> Ordering {
        let addr_ord = self.address.cmp(&other.address);
        match addr_ord {
            Ordering::Equal => self.c25519.cmp(&other.c25519),
            _ => addr_ord
        }
    }
}

impl PartialOrd for Identity {
    #[inline(always)]
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> { Some(self.cmp(other)) }
}

impl Hash for Identity {
    #[inline(always)]
    fn hash<H: Hasher>(&self, state: &mut H) { state.write_u64(self.address.to_u64()) }
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

    let mut s20 = Salsa::new(&digest[0..32], &digest[32..40], false).unwrap();

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
    fn get_memory(&mut self) -> *mut u8 { self.0 }
}

impl Drop for AddressDerivationMemory {
    #[inline(always)]
    fn drop(&mut self) { unsafe { dealloc(self.0, Layout::from_size_align(ADDRESS_DERIVATION_HASH_MEMORY_SIZE, 8).unwrap()) }; }
}

struct AddressDerivationMemoryFactory;

impl PoolFactory<AddressDerivationMemory> for AddressDerivationMemoryFactory {
    #[inline(always)]
    fn create(&self) -> AddressDerivationMemory { AddressDerivationMemory(unsafe { alloc(Layout::from_size_align(ADDRESS_DERIVATION_HASH_MEMORY_SIZE, 8).unwrap()) }) }

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
    unsafe { ADDRESS_DERVIATION_MEMORY_POOL.purge() };
}

#[cfg(test)]
mod tests {
    use std::str::FromStr;
    use std::time::{Duration, SystemTime};
    use crate::vl1::identity::{Identity, IDENTITY_CIPHER_SUITE_INCLUDE_ALL};

    const GOOD_V0_IDENTITIES: [&'static str; 10] = [
        "51ef313c3a:0:79fee239cf79833be3a9068565661dc33e04759fa0f7e2218d10f1a51d441f1bf71332eba26dfc3755ce60e14650fe68dede66cf145e429972a7f51e026374de:6d12b1c5e0eae3983a5ee5872fa9061963d9e2f8cdd85adab54bdec4bd67f538cafc91b8b5b93fca658a630aab030ec10d66235f2443ccf362c55c41ae01b46e%",
        "9532db97eb:0:86a2c3a7d08be09f794188ef86014f54b699577536db1ded58537c9159020b48c962ff7f25501ada8ef20b604dd29fb1a915966aaffe1ef6a589527525599f10:06ab13d2704583451bb326feb5c3d9bfe7879aa327669ff33150a42c04464aa5435cec79d952e0af970142e9d8c8a0dd26deadf9b9ba2f1cb454bf2ac22e53e6%",
        "361d9b8016:0:5935e0d38e690a992d22fdbb587b873e9b6de4de4a45d161c47f249a2dbeed44e917da80736c8c3b61cdcc5a3f0a2c77fc8fa41c1302fa7bb871fe5833f9995f:7cfb67189c36e2588682a065db769a3827f423d099a84c61f30b5ad41c2e51a4c750235820441a524a011facad4555869042750684b01d6eca4b86223e816569%",
        "77f925e5e3:0:161678a69aa19d1de096cd9cd7801745f038f74c3680f28da0890c995ecf56408c1f6022a02ab20c68e21b1afc587a0038f1405cbd3167877a69926788e92620:2e1a73ffb750f201451f5c35693179cfa0de14404c8d55e6bb5749787e7e220b292f9193f454b2e97404c5d136cff665874373e9a6d5139efa1b904f19efc7d3%",
        "e32e883ac6:0:29e41f935cf419d41103a748938ab0dcc978b6fde9fbb82d6f34ef124538f93dc680c8b26ba03f0c66d15be1a3895ef73dc6879843f3720095fa144d33369195:3654e04cac0beb98a94b97bca2b9a0aea4c7001e13c3ebe813fe8096395ecb69b824d3b6ee2d5b149077abd73cff61dd9ee04811c30b0c7f964b59c67eefa799%",
        "6f66865615:0:11443c5c0a6a096245f9790240e15d3b8ea228397447f118bd8b44030b24191f97e11bf704807561cd6d54f627d57d599ca7983547c6d4db52597dbd1c86114b:1d1cb5bbced28b11f2f61ddcbc9693d0233485fc8fe0825c090a7309fe94fd26e8e89d137071ef7567b80cce60672a31da4c1677fa1c37237b0713456788dc81%",
        "c5bdb4a6a6:0:e0a8575bc0277ecf59aaa4a2724acc55554151fff510c8211b0b863398a04224ed918c16405552336ad4c4da3b98eb6224574f1cacaa69e19cdfde184fd9292d:0d45f17d73337cc1898f7be6aae54a050b39ed0259b608b80619c3f898caf8a3a48ae56e51c3d7d8426ef295c0628d81b1a99616a3ed28da49bf8f81e1bec863%",
        "c622dedbe4:0:0cfec354be26b4b2fa9ea29166b4acaf9476d169d51fd741d7e4cd9de93f321c6b80628c50da566d0a6b07d58d651eba8af63e0edc36202c05c3f97c828788ad:31a75d2b46c1b0f33228d3869bc807b42b371bbcef4c96f7232a27c62f56397568558f115d9cff3d6f7b8efb726a1ea49a591662d9aacd1049e295cbb0cf3197%",
        "e28829ab3c:0:8e36c4f6cb524cae6bbea5f26dadb601a76f2a3793961779317365effb17ac6cde4ff4149a1b3480fbdbdbabfe62e1f264e764f95540b63158d1ea8b1eb0df5b:957508a7546df18784cd285da2e6216e4265906c6c7fba9a895f29a724d63a2e0268128c0c9c2cc304c8c3304863cdfe437a7b93b12dc778c0372a116088e9cd%",
        "aec623e59d:0:d7b1a715d95490611b8d467bbee442e3c88949f677371d3692da92f5b23d9e01bb916596cc1ddd2d5e0e5ecd6c750bb71ad2ba594b614b771c6f07b39dbe4126:ae4e4759d67158dcc54ede8c8ddb08acac49baf8b816883fc0ac5b6e328d17ced5f05ee0b4cd20b03bc5005471795c29206b835081b873fef26d3941416bd626%"
    ];
    const GOOD_V1_IDENTITIES: [&'static str; 10] = [
        "f0beef83d5:0:c770a89f8b4c389bbbfbb15f3efa38be12aed830500b12c6b14d235ce4cd976477706b78cc649eae282e590c2bff0a00c2bcd48c6a2e11f75bd447e5c460e6c1:200152c9bc35d6ce4984e67a508cb255a984a47f377747a12465a7f18759904a84e5ef259eb8aa1988e1e3e40317c4ec72445d2c8731f12c3c0f71025beb28e5:1:AHFabiyRt7hZM3UHLQPR3ZnqUDwIsbYINPegBuwazzH6ARy8ajtaDNg8bZn6Y2TEKb7ov1DiedkLEkLwpEnbfqwfAE0BiJebWrTDqcZMGjY0OGbDIeT0U0ZH4cmxWGbn-DRjNuiVqltMxwAQCjahhIXkhLueYj3YOHvKPXBykm4m_5sGAKQog2m-4N7WHpO3dIi0HFbPkaibsSGYk4_ZE7Z4R_j8mxHeQhqnm97krXWMJec6MSzXIXnfQ9-aVSaRSa0ZXBCcALVbWN-N3vMMiEHK0I2DUP1UAFec9FUjxsqEAXGGLJ0dtVTO8dO-fTpi5REsblxdapjiNbMUmwNYuK8iBkV_kYnbAP2Zv5ndxZlXhr5wPsKudP_LGpuGS1BltS8gZDELq8zhOa7U7L3qvFiK68OJWZGgrZZrYjO-tU1UU18qxtgT_VZBAQWNnr6nWVq7eIL4kvASAgttNNe0_9Qb6iOvzEvvXobf0ZgsWUy4NRyFlHKIYH6dOWMZdMekMKAgGSZmLn07RhYKUCe6JqfjI-UQR49tr-JB-H_t1OKYO2wFiKixpdZKh33erUvkxBcVfHOwvJmlykedkQNiCQpHcWInCSzkV19VCw:AF6BKY6fEVmNyg4UxSuUPnG5p011f3-vGF2V3SZjNg6qAI2GIzk-d_DNpeUjAz9u2NQX4gCKb9Xgr1JePQwaSvmCAPiBW5GX3JwJXbYu025q36yFA1e93DEMGnITHjbzIc7jiDk_pZ7n5B_LDUjjqpHQcokl9b5qjr3d0riF-9PQ8-8k",
        "26d8a4c162:0:666dcd76f39de286f7d816af1feeb54ba327874472cfcd88d717e0e551048a78d342a06aee4c6766bd41c54a5bf7b44153aaa4329846988f807146459cd62d6c:90461e88fec66b82b00745840f1f304bee25d442ebf93b21033a3000219f6f6bfd5604557ca27a9bcc6f7f5c10db15262d7145a1884adfce32eae4c5cb77d1c4:1:AdHlp0cmj4KRWQN3oOM6AQjLJSv3_KSXSOftv0TVIUrdtFmAPGf6Duecb9Vtu5WaWOwoN-3WY_YmDES-sfJveioBAJhZhuyqS-wj-pQ0ppQ5u_HP9eAtuDtUIrrtF6wDGNrNsNc9F9SATZho6jVEan7ccjZFeLBd-HOmErj_LfVC66ptAWmAn18CHjNxPUcn-E9D2nVpIN34mM_mHyc2uMLm80aRjqndFll8MPKkYZGz1TPEUjJUfwNT9SdecbBSvqK7gPdzAKY8kohNau9j27vMaUGcf-TTkfHBSmOmoz-X-C_GSGwD7KIyyVQe1xMfKQjdjqPRlK94AWAuX3L5Gb_ywdGjR6_SACDCdVisQFP0HzSWkHVPxbY5Ab1yJme73hlbpm64vK5htiSmdC5JxXGSiQuAIdjWMH_pfL4LgFCmZQjTWLEX0-n_AAjk9A3DjZDkQeBn8gP9z2xlpPKzOAOlqFoVlxSlyRc7Sra--go947886vBN3dpcm8fhTuO4HtlnPdiKdk1NsvyzuvZ4XIvTa6pGV4zhYZRIpn7rn9yDVOLYskEx2I6SxHrovuG--_Yn5pERP4jdA6oGMKR3ZSIROTCTyUuCkQMXBw:AK-hGoppimiEx-4-9up25uWcnSeD4lIl8VL_ufIJGv41NsLFgJkRQ7EQ-4zd_NrB_E42ju24ZKIR-eJOn-eOKTUzAJmuBUCsMXzCzgTPwbW3OBRhCHjDbaz27JDV4HWVRM_bbhQLF6rzJTBioDex-E74HOTIDKS87vTCZ2qujRjhw07V",
        "7e94a163bd:0:72a1fa41c906c7c111798b2f062a695316a41e9e8043ca14c1f7878f20f0ba170d0a2c8442a6eddbe4ba6c29847d8460a2754ca6c5c465cf756619749fd18460:989c4bb4719bc7cc4d1f59810a6a64746d04b3a46d894844c1bfe01d359e086a7cd0f5aafc1936bc4475937b14f02ccf25ca78b77c65d0b34be0aa8bc138f45a:1:ATvs4irZ_1FjoYJojInI5DPuqDMrk8Ost4r41QaqSiTgCONEHRiNtSoQJXgcsfhsbz5AWaG5S0H1Q0irHHF2paAYABBQ8C8DJLG_IQWnZYYH2OOdcMVdA1llQRr1odUDJdwv862fO0dUzMct1qmB7RZiiZW_DyIhmv5ZPayeeyQ0NBDeAEeCvA_J0hvBSDeBPxUr5meAzVizAArotFO58JwvqKjfXin99heJFXnBemlJx1Ec-v3WGcbRN6ikLZYw6uetXS0HAOt3hJlu1-MQXXpUMSQ2fNeE6HrM_bH-JDmFu5jTzpls2end1u6fKtVPW2TCs1pHc8nC6Km3J6Kb60_rqgtESVo4AazOlfiwaC40t-kEDXPbNTxOfAHgm9NimCkEmh4FwTYXTv7hD7koTU5ZtFVBCospRcW5A2Q39keKDawM67isFMnrAcd4CEx5FLvNmJ9aoItyeBzjcP6Nn8RWmEFYObMqNlEH1d2KSGFYYFKp8fWi4zNnEe41H5qQ5xj-fFEiP9bod1g0sV8Vbyo3uj7VoCeabZ96KQwAhWFe-TEdc393Aha65sjoXNEuOeCq7XMtfIrQ2rmMBcMhymy_K89WN1E_iuNRAQ:AcjIXa4COvSKkaU517m3eKzAvmEjTHMWcLN-6gFY9h2-1nNJO6bazeq5jyWqdifOsllksAyKlp4lgFmuvb5oUTdAAFTfsfMwo8eH9K6zvzgbuyxud4ldX3wlZv18brdJW7C05QNM3qYNPWTB1bCwbEBbBmtNR4Qdfe7YDJAqDiSo9t6i",
        "8d7f396a1e:0:5486bc9a8c092fbbc4c447d44ab7e07419ea926cd606af45c3333dc65956261dba09d1160975c4f3a536664c00cfca3a54219a47254831e084b7edbf3251295a:7006dd1e43e7bc362ddd00f6f495dac4c5655f7e2c27abf133b3e9995b78605b1f8dcceaf8a3b3c98fff103f3d7a0a4af35ee59b34be229f434928523d39d879:1:AXUfrrngWX0AKkrDljhBpAGzYnCgap1Y3mnS-BccP5ipIUx6PWDxCZIZQ_H0PTCWP4sysDjZA7KcgmmOlrKVr9hRAJgWy1ljt-Vr-ybYm65xG70jV4dR_q_y8u5fYH9g6_FhFKn-ef8npze5SNKfDbroi_1RtIG8IIqACBTEMm5r8pfsAJ_U9SlPxpDlg6R8VO7sKt8kGwQtVytXsXuZ-Kxvnb_8crYAg5ObaJYVlJlsRvHbpwk6f5FRp_KoNMAE5LNIdvMrAKoTSLa_H37Tb42JraVpRRvj2D04NsNrp9AOus9mDMfrn9XZXX677sbCfN507KdWEQnYd24njnCSGIktEf4bhOmQALXUiArWmEm6g08skjyKs6_ukmsxnZHMaWih6FCkoU04oc2qSOmSC_LsnDjBhqQ7qo6_EkJnX5roiVGv2BjWZSfkAFPRxdxqNVJyNVXoCl-ZJlBXePipjH1Rk3yVuX2WfAMXQ2MoL8eP7vnfHUpBrzJmfImOqYgLZx2dstj-f_X6m1UZ4N9E4xYvyqo3vyzdFDI_YSFJZOZNk4Vz9CBE-WxOGPOklHgCNU2xcewysAw-jTHiHQ-lhoXWfxIBiMwE06nnCw:AYjHjcdewyL08Q3ddXFzdhrTijiAQYnbkbL5oC87GKkZ1S62p5Ny-UrHnOt37F0k65aDOwtvldGVvU8VHVDZAti5AIfl7mNhf6b4MF1LoROulO2j-Ygoc5ixbTuEQaJGSD-NumLvdbZbv2Ik0pJUc61izpwCxVHoYOkUlc167eD1G2li",
        "9eb06e21bb:0:76a7f820f7af06d10a42bd7369b9e4cc96318e535909eb9840d064b1a21cf967826a3da6c0950d5ad68e095af6d2dd648fbef052b13e7876926cde591f5007c4:202df0941105495c8e482e2ebc5551c6fd23f668b387bc0bdcaa8ddc6ad03f5c2871a4263b5ba5bf3ed1cea348fdb88f706f3f2a628926a22828f7209b5fc7c1:1:AcUH-dmXHK75fjCV32witg1HZSj4M27N6w4BF9hnWXY8lSYdgbeN3AuMGnYqQcqu7vPoqrpT2AkIJ-gJI7fGe9jOAO9CcyfSieBeDgaFDW9oAmtJGNRdL-E32-K8kqlmyhkt-r0In-eOYuBWAe0hcQR0pXbTaAvBUB-WNa2Hab2JKVnTAThDvqzCmV57BecsfTtP-TS-Kz3BqZuQfIF0o5EOqDdP0Jib8G8TM7huXO6qrd2JZLGQzZ0uAF2ex6hmdD_obZJfAIpuhUgrgvVB9LFApQWE7Ys5aJXMrYYEtXAuqutGS9NV8yLsshD6uV0OzwqU8R2L-KPNsrOBccwj6E1Cyy7v22XMAPhMWA4oUlwZXuWWVPzYPq8lh-YxPlOFq583AZa2LZC0NUL0jkmzEQwvylctTVjhgaybqtRasM5PRyoRQWg5xAyKALASLfqRw5f1tJksf1JR7bz-WZH_Pngot8YOQLGjdjFxYwZD0CTXoV3v9Muc8bNEByiVTk_MXgeghi4ZWuV5N3jcy58eIbgS_GK_HNFRDDDS5_DtxFoQRxI5vL__wtej5Xl2Iqh0-4rmuUxbAP8fv_fhq9SFK3zoul-uy2cseQbeBw:AUwxKKt2MFKRzzWWwqMOcXBj23hUFZGjoO25vnMsEWTb8RT04oQc3ChRc13WrEGIPUOHuRXoRxSYVbfW7WIFhj1uAJY5zDiN9blmxKHmTj0UB84QMYlFnjok-AT3r8FB8Pj09HTborzJQqwgOtZ54G-UZ-Cn5RYzyWMQnERxQLiC525G",
        "6362ad160b:0:9ea5ae5ecf18e8048e3f425845b12e462f3c6111e6078e97e17c56592cbc5d65e7ac6b2906739f8d6632e2a4323763ca4f33d7357efbbca0a6a7cec783067154:9078565f47bcfc7e4487f70b6c57e30e2863ce737a7997ae4f7a0f3a28e62971b1d42c4b0a7bf8008a18efd820fe40d5083809fbd5c1f63c83efb432b2c469b3:1:ACSRqDRinlje3ob1z2Uied0SyhSJ72KgGWaczFtk5pgR8lFA3OwIQT2UeqVVEwNuFyizM0vVA6eARJtM0fYY4OtlADMuUgs20w-nSHyPpWaYiZpGzIJT1QloiiuMnLaPA4XFPRQ8kNq3zBwVQHCsCTgAp92Y16_Yjkc4wCF4DZkFB_ROAObyJ-tzrC4VdNQKrbPFmTinyyhx0vroRBinX71KTHCL1MvNZK4ubSmgf0MSqjs-DLUo-dBjUvTC6u2F3P-6j60OAACKc3Vf6hVVlaivjeINVY-9W1rv2YfeIVF_EZQooDnZr8ZEn-T0DUq17apsacppvAQ5zLc1WtKYRhYI1ZRhUNI7AUtVCOdph5oHlmlP1a9xLWAiahTzb2Jo55KJ5kaF7DyRkcZCV3Kv6IUNZP4sZ6KO7_g7jLTq5kw57utzA7ntcBu4APl3hahoPWXqxTlbzwaKVb5a4yDV_enbeP04NvoEbR2M6F67olmrO_1-WJP3IpNP9-jYvw6l25qzdMhuiRBfoWD-amN1m9CptAta1zHhNKD4Q24M9_0sxA5brph76tIZEISHIMT7wzWY4S4lT35ZztHuXdWlcAo2sU4pbeItff7BBw:ATBDv_-t-jYfiTgk0nLNo7W4glfaFkig42HUeerp6k6Hi3OJ7OGv6ZPKbBefa8by8weJBgkBX0PK5seL-3IypJ_vAXm9q9xbAP7GHreku8ZSOnwZACDvlJgGXRrPGhjP672lC-t8kFScyttjruyfyXNme1M1nKux8mq2hwtt-JyajlPj",
        "68cc2f8e77:0:14db37ff42c15aa305951911d3ac65748367d1c284ce18c5ba400899d6ce333f9c35ad162464935ab6f4fe94715a8c4f27706f6270a44f46aa46d2539d2e7334:d0c6e6e8779e7a4b2462e74ff097070f8cf325c38de09eac3c5fe7a8216afa7b44f92a6bc9bd38af780b2ec529f80cddb1f733a805a05a89f7149b873dfc98d0:1:AKAzMbwTh1QxdqHB8_TlY7ulKqRCp05PgYxtFPDH-m38mOFHBH4q2kcw7BMbsbODfiNDG8UUmKvW1BJhk1muvA2XAHjiPjWY8jVXHJAXWp1iqmguj93BBDoCJJoSAp7fXthTz5TPViB8_EcnOedfASJUMMODn6iECE80vy_N7sxQQzBJAYAiSD6PGcO8lxHiS2clee1Twbf2PJU35CTaDPOy7yoEi8y8HMpDB8i-FYNrcDuWhO506r8JSNvAgttFmsUHyPB6AAOVEQQHqTnidrpqXnI4pDJXFhopjXkO2WtTub8zjFLwHA73Rgw0U54Q8Ya8uWm2bnhoLOTVgQ51sfV1VP4orH5ZAVo_p_s5_tl-X5PeIc1mGbfXIq5EZ4Cu7D-Gu08gDqxajJov51p1gi9A36NPoxDcDPZrvfhin122VkFrs1NN-ThzAZLqZWHeX7CNAZ_jbD110l-gxP8FkfPpGSmVB9dsXqj_LQxVzEEie13CZSovd0BIscfNcQvPAQn9lD4X-ZtY5DIXluHcve-kFDzNGu15NYQCD5qxDmy8paFigJ83Ad8V1ScsEISR3DNeNocB0uBP6aVlQUNwtaTo20EH1I_gE0oBDQ:AE0E5BpSDnLWor443T0RrfMx0Shel5C73P9ZdFZR58-txJYser1Qn_GFAzsN9wVjHZSSgbks6JAAucNF62S75rbTAbEBtbdm6AhyLveV2LsJ_GxnK1eA2q3DpG97gVCdaLm-6P_b7bRDKhKV0xqbEFCG4rdSjNPI55Nst76IdfRC-YUw",
        "a8dca71e98:0:ae3174d41b37ffcd575be3fedfbbb7014622b61afdae1b4fbbbc880427f7e34841117d09883b575f11aa9b0069a188b9ab496b1fd72dbb95e2b347c07f360174:70ce0bb0899196c1794660266bac0b06c50aca60de42410f12152e5395b7c6717d2acf315f19bbe360ffc94fbac9e59cb1386ecf3ad7a655498caba5dda4d4e8:1:AZUGXrLYWPIDJDbieUC0KIyR4s3Ny3MetrlC72y8LEo7AOAq-B2Zl7qf9G8ECV7pzbjNtOsRl_br6gemO8ju-Y4-AHSVyu3EZJIA-_ff8aqGWO7Qrn7R7BVmGwEPsEIVdIA1p1wNPcRquacSmGVteOMLNXViFDJsEnZbuixFoqwaHOniAfkWV2AX0riULmS83Td89ov3MwWDegRtJHhEBRJEUTGbA0-Lgm6_c1Qsl1ZmOns0rGyQDPu2UY60NTUtNrI-6GS3AL_JcuI8ILqB53F4TNXlDx3kHpwOUQgcipJFYgwaCyRj9CDIah5I3kyfayn09gznro6DsQCQq0QZ_7IYw0nglcTYAI0s8XTChwmz9Sp9yAsbnHnwVLJJWKIpflD2tPxBu4Dmuw-zsMv2mZvF8aSyz6McbaPTVqrFPtqsWbhGrymkDfV_AB2Os-9eoQawvF0Jlu3e8HESXMd7Hm7o68WttF88eIiQoQbwnyQfroiDAb9SkLhPlkO3b6lHH0rUo1cVPtrv7kUx6q2dr1vdrOItINTDFQD8StMEWRWiOyfNw9qDRUF2vj2iYpyi8bQ4z_MlUeAm4pHwg2lK1cfa7W949UbeaNDpAQ:AMudL3SxwtAYlrM8yir2dfOXjcBCe5UaVvW5geQGGUS3CQUgTyyAix_mKCnIHsc2_Sa5wc7tCd8Or2WawPcYjYDuAbTQbuXB1ZZtucGnze_vPRP4yJ9mlSRWlyu10bzBlOAuKofiCd_WYmQwmhLgGhDrun8AIu3_7yx5xOv5VQB_pvJM",
        "ef9bf07932:0:debf307a91682f407ef0656db48beed39edff72280549f2188148fca1320c60efdff2f3279033a67ad6c8fbf10836d992e17f1796b690f715c8f74b286210e36:0016b7da07340bdd71e3922b29feae47884c4d42fa4e41997d9a6f2f6186775321250d014baa16a6c440960189493d1f61d5637ef2bf52f1bc4699b10b70394a:1:AZk47482_xHQyM-4zBVMyDeyPtzXKpuCRtCEc6qr6Nvycqw1LsKwaBxZIVW0S072c33BN7bDBJq3j0FssGVUK2mIAFDQa1BQfEexXkn3BU078B3TOuMLNIWS9SEyixt4jDKeELU8nh9QgIrMH9eL_6Pl-QABF2F8xgb2AJX-Q3zzcdvdAS-Bz4OeCkn1q2h8q9pKUPS8JOVdzByUqP3WPcYfYHA6PY1FumohaEgqjnuIwYFdHg7yFqX2MQBfPZD-j42Efjo2AP1hiR_WOJKjRIMYA63UPjFFk6i5Zr7TipsT473a1rDjLxoniJlsH2Ss7m79Rckq3R89wbWHcZ-MhpW4NpSeOeYQAQHv8jjPeApQE9bq88WOKQ_JbygYhlHv2gOTQdiMjzL9TMDkFJAIkw1RvWa6xOxi4aZzAsk0ns2MM51b2HPwNesmAPYVZd33eXTT7carfOo3g-MvvE3aXyKica7lDGa2mi8fAb5OwmIOBa3U-NhzpjNzwgrCsMx7dRxm7fZKqaMLe2gitFdpqWK0McLowM8S3Ft7CRyHGhm9w9wB4-TAHcepplR3Ahrl-VCn2DzCBJsnSpgWVFzfXYq2uZMlSc6J6InMDg:AT4JX87uzP1OOxkq7eHpEpBItbtIFkC8_X7QjFF5R-cclN64DJAR9RuKYoAOnYc2wmbFdQ-NgYQR2PXgA-sPBJxtAPPpSw0PSg36cbtESzKemq03QLcDg6F9hh0N0stM3uJsgbRApPI-HQN4RNM1AI9-zmezJDzR8hREvLiQoY6DDxua",
        "3710a2e191:0:d4d3f16f6a59b758895b50bd540e68535a3b7803b61a217f437a096937763f7e3537c475bfd107b7ca8c39e3bf37f33e078cecc2fcebc2a66abfd474e47a8acc:e025c2a133160c77cdfdb0b9a8f408bc9d99882da8f2854f9ddf8bfd36cb7a7de72d71edf6d2f5f37e313e700f27550c9b2a5a2c5e226ca8a14797eebb95c34c:1:AVUSQxAMkXFHeAlu_ZLQHUHoXi2zeYd0OqUaY88IJfUwyhZuuJUB1i4l9YAmxRCmvA4W9-0zBMs5VIaRe4BLZwgAAO0ww08FS4mWLwcH-NptOTy8eCitJ89MN6Zt0EQHifyOzxxLWIgY6OZJiKiSeRLrWr1_ZRpeHJJLcF7Zdnr_8f7MAPtBybUcan4kIdL5yI91FpE8CgmjM5Pmx4EWiJC33C3Iu4aV2GEdxPWutP8-XRGNpADwnHqE3XaNV_6rstC9Fu7kALdEb3WgqszTsf_elgCJbZ6IunXZBZrF8FGoAqeGWvV86lUw3vp7hwCUEQ52X4r8imKuqCtIdeYegoBfcEcyq3zDAJEScanA8YjU0y4k2FLyVBCrRjxsZDkyP9ZJXM2p3_5m27NhjzxDDLcb75Sb8Pn9K_CgwOafZw8yGIIK5OTkuEKQAGH6yMdspiUHTYTiEGSVAlJyRWz-Ifo9skohOlV6aQp1LufpNhonjbR3zXcAHo1s0yIkqivM_RXbVfe3QViNx3xeBpBLuxSj05hoiFrGEUuA-Q3tTtnGgbgtEEgzaiKeTu3Sclfd6EaZJc3T35DMl3U9wLlx6_BJsVS3SpfKIZv8AA:AePjhNHvnJr-FqsiI7y7QCI1OXUbG9iK98U8caTUb9VcezhHtMbEjzFehucOrafEnAZXROT6noHrztYxDEzJIfGmAEfPrbP6VYrCGo2fk6MsyzyCK24OJfFU3Z7mnSNZHpN55_P1LBlHoGQUyCAwcKzp8WCY_ceQkxTNNjdkAdq1zB4o"
    ];

    #[test]
    fn marshal_unmarshal_sign_verify_agree() {
        let mut good_identities: Vec<Identity> = Vec::with_capacity(10);
        for id_str in GOOD_V0_IDENTITIES {
            let id = Identity::from_str(id_str).unwrap();
            assert!(id.validate_identity());
            assert!(id.p521.is_none());
            let idb = id.to_bytes(IDENTITY_CIPHER_SUITE_INCLUDE_ALL, true);
            let mut cursor = 0;
            let id_unmarshal = Identity::unmarshal(&idb, &mut cursor).expect("unmarshal v0 failed");
            assert!(id == id_unmarshal);
            assert!(id_unmarshal.secret.is_some());
            let idb2 = id_unmarshal.to_bytes(IDENTITY_CIPHER_SUITE_INCLUDE_ALL, true);
            assert!(idb == idb2);
            let sig = id.sign(&[1, 2, 3, 4, 5], IDENTITY_CIPHER_SUITE_INCLUDE_ALL).unwrap();
            assert!(id_unmarshal.verify(&[1, 2, 3, 4, 5], sig.as_slice()));
            good_identities.push(id);
        }
        for id_str in GOOD_V1_IDENTITIES {
            let id = Identity::from_str(id_str).unwrap();
            assert!(id.validate_identity());
            assert!(id.p521.is_some());
            let idb = id.to_bytes(IDENTITY_CIPHER_SUITE_INCLUDE_ALL, true);
            let mut cursor = 0;
            let id_unmarshal = Identity::unmarshal(&idb, &mut cursor).expect("unmarshal v1 failed");
            assert!(id == id_unmarshal);
            assert!(id_unmarshal.secret.is_some());
            let idb2 = id_unmarshal.to_bytes(IDENTITY_CIPHER_SUITE_INCLUDE_ALL, true);
            assert!(idb == idb2);
            let sig = id.sign(&[1, 2, 3, 4, 5], IDENTITY_CIPHER_SUITE_INCLUDE_ALL).unwrap();
            assert!(id_unmarshal.verify(&[1, 2, 3, 4, 5], sig.as_slice()));
            good_identities.push(id);
        }
        for i in 0..good_identities.len() {
            for j in 0..good_identities.len() {
                let k0 = good_identities.get(i).unwrap().agree(good_identities.get(j).unwrap()).unwrap();
                let k1 = good_identities.get(j).unwrap().agree(good_identities.get(i).unwrap()).unwrap();
                assert!(k0 == k1);
            }
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
            let id = Identity::generate();
            //println!("{}", id.to_secret_string());
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
