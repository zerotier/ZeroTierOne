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
use std::io::Write;
use std::mem::MaybeUninit;
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
/// one wasted bit for a short-sighted aspect of the old identity encoding and HELLO packet
/// encoding.
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
        let mut genmem_pool_obj = unsafe { FRANKENHASH_POW_MEMORY_POOL.get() };
        loop {
            c25519 = C25519KeyPair::generate(false);
            c25519_pub = c25519.public_bytes();

            sha.update(&c25519_pub);
            sha.update(&ed25519_pub);
            let mut digest = sha.finish();
            zt_frankenhash(&mut digest, &mut genmem_pool_obj);

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

        let mut self_sign_buf: Vec<u8> = Vec::with_capacity(ADDRESS_SIZE + 1 + C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + 1 + P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE);
        let _ = self_sign_buf.write_all(&address.to_bytes());
        self_sign_buf.push(IDENTITY_CIPHER_SUITE_X25519 | IDENTITY_CIPHER_SUITE_EC_NIST_P521);
        let _ = self_sign_buf.write_all(&c25519_pub);
        let _ = self_sign_buf.write_all(&ed25519_pub);
        let _ = self_sign_buf.write_all(&p521_ecdh_pub);
        let _ = self_sign_buf.write_all(&p521_ecdsa_pub);

        Self {
            address,
            c25519: c25519_pub,
            ed25519: ed25519_pub,
            p521: Some(IdentityP521Public {
                ecdh: p521_ecdh_pub,
                ecdsa: p521_ecdsa_pub,
                ecdsa_self_signature: p521_ecdsa.sign(self_sign_buf.as_slice()).expect("NIST P-521 signature failed in identity generation"),
                ed25519_self_signature: ed25519.sign(self_sign_buf.as_slice())
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
        // don't prefix x25519 with cipher suite 0 for backward compatibility
        sha.update(&self.c25519);
        sha.update(&self.ed25519);
        let _ = self.p521.as_ref().map(|p521| {
            sha.update(&[IDENTITY_CIPHER_SUITE_EC_NIST_P521]);
            sha.update(&p521.ecdh);
            sha.update(&p521.ecdsa);
            sha.update(&p521.ecdsa_self_signature);
        });
        sha.finish()
    }

    /// Locally check the validity of this identity.
    /// This is somewhat time consuming.
    pub fn validate_identity(&self) -> bool {
        let pow_threshold = if self.p521.is_some() {
            let p521 = self.p521.as_ref().unwrap();
            let mut self_sign_buf: Vec<u8> = Vec::with_capacity(ADDRESS_SIZE + 1 + C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE);
            let _ = self_sign_buf.write_all(&self.address.to_bytes());
            self_sign_buf.push(IDENTITY_CIPHER_SUITE_X25519 | IDENTITY_CIPHER_SUITE_EC_NIST_P521);
            let _ = self_sign_buf.write_all(&self.c25519);
            let _ = self_sign_buf.write_all(&self.ed25519);
            let _ = self_sign_buf.write_all(&p521.ecdh);
            let _ = self_sign_buf.write_all(&p521.ecdsa);

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
        let mut genmem_pool_obj = unsafe { FRANKENHASH_POW_MEMORY_POOL.get() };
        zt_frankenhash(&mut digest, &mut genmem_pool_obj);
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
                P521PublicKey::from_bytes(&other.p521.as_ref().unwrap().ecdh).and_then(|other_p521| secret.p521.as_ref().unwrap().ecdh.agree(&other_p521).map(|p521_secret| Secret(SHA384::hmac(&c25519_secret.0[0..48], &p521_secret.0))))
            } else {
                Some(Secret(array_range::<u8, 64, 0, 48>(&c25519_secret.0).clone()))
            }
        })
    }

    /// Sign a message with this identity.
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

    pub fn marshal<const BL: usize>(&self, buf: &mut Buffer<BL>, include_cipher_suites: u8, include_private: bool) -> std::io::Result<()> {
        let cipher_suites = self.cipher_suites() & include_cipher_suites;

        buf.append_bytes_fixed(&self.address.to_bytes())?;
        buf.append_u8(IDENTITY_CIPHER_SUITE_X25519)?;
        buf.append_bytes_fixed(&self.c25519)?;
        buf.append_bytes_fixed(&self.ed25519)?;
        if include_private && self.secret.is_some() {
            let secret = self.secret.as_ref().unwrap();
            buf.append_u8((C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE) as u8)?;
            buf.append_bytes_fixed(&secret.c25519.secret_bytes().0)?;
            buf.append_bytes_fixed(&secret.ed25519.secret_bytes().0)?;
        } else {
            buf.append_u8(0)?;
        }

        if (cipher_suites & IDENTITY_CIPHER_SUITE_EC_NIST_P521) == IDENTITY_CIPHER_SUITE_EC_NIST_P521 && self.p521.is_some() {
            let p521 = self.p521.as_ref().unwrap();
            let size = if include_private && self.secret.map_or(false, |s| s.p521.is_some()) {
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
                let p521s = self.secret.as_ref().unwrap().p521.as_ref().unwrap();
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
                },
                IDENTITY_CIPHER_SUITE_EC_NIST_P521 => {
                    let size = buf.read_u16(cursor)?;
                    if size < (P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE) as u16 {
                        return std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid p521 key"));
                    }
                    let a = buf.read_bytes_fixed::<P521_PUBLIC_KEY_SIZE>(cursor)?;
                    let b = buf.read_bytes_fixed::<P521_PUBLIC_KEY_SIZE>(cursor)?;
                    let c = buf.read_bytes_fixed::<P521_ECDSA_SIGNATURE_SIZE>(cursor)?;
                    let d = buf.read_bytes_fixed::<ED25519_SIGNATURE_SIZE>(cursor)?;
                    let _ = p521_ecdh_ecdsa_public.replace((a.clone(), b.clone(), c.clone(), d.clone()));
                    if size > (P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE) as u16 {
                        if size != (P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE + P521_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE) as u16 {
                            return std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid p521 key"));
                        }
                        let a = buf.read_bytes_fixed::<P521_SECRET_KEY_SIZE>(cursor)?;
                        let b = buf.read_bytes_fixed::<P521_SECRET_KEY_SIZE>(cursor)?;
                        let _ = p521_ecdh_ecdsa_secret.replace((a.clone(), b.clone()));
                    }
                },
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
                    ed25519_self_signature: p521_ecdh_ecdsa_public.3.clone()
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
                            ecdsa: p521_ecdsa_secret.unwrap()
                        })
                    } else {
                        None
                    }
                })
            } else {
                None
            }
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
                let p521_secret_joined: [u8; P521_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE] = concat_arrays_2(p521_secret.ecdh.public_key_bytes(), p521_secret.ecdsa.public_key_bytes());
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
                },
                1 | 2 => {
                    let _ = keys[key_ptr].replace(fields[ptr]);
                    key_ptr += 1;
                    state = (state + 1) % 3;
                },
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
                            }
                        })
                    }
                })
            }
        })
    }
}

impl PartialEq for Identity {
    fn eq(&self, other: &Self) -> bool {
        self.address == other.address &&
            self.c25519 == other.c25519 &&
            self.ed25519 == other.ed25519 &&
            self.p521.map_or(other.p521.is_none(), |p521| {
                other.p521.map_or(false, |other_p521| {
                    p521.ecdh == other_p521.ecdh && p521.ecdsa == other_p521.ecdsa
                })
            })
    }
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

const FRANKENHASH_POW_MEMORY_SIZE: usize = 2097152;

/// This is a compound hasher used for the work function that derives an address.
///
/// FIPS note: addresses are just unique identifiers based on a hash. The actual key is
/// what truly determines node identity. For FIPS purposes this can be considered a
/// non-cryptographic hash. Its memory hardness and use in a work function is a defense
/// in depth feature rather than a primary security feature.
fn zt_frankenhash(digest: &mut [u8; 64], genmem_pool_obj: &mut Pooled<FrankenhashMemory, FrankenhashMemoryFactory>) {
    let genmem_ptr = genmem_pool_obj.0.as_mut_ptr().cast::<u8>();
    let (genmem, genmem_alias_hack) = unsafe { (&mut *slice_from_raw_parts_mut(genmem_ptr, FRANKENHASH_POW_MEMORY_SIZE), &*slice_from_raw_parts(genmem_ptr, FRANKENHASH_POW_MEMORY_SIZE)) };
    let genmem_u64_ptr = genmem_ptr.cast::<u64>();

    let mut s20 = Salsa::new(&digest[0..32], &digest[32..40], false).unwrap();

    s20.crypt(&crate::util::ZEROES[0..64], &mut genmem[0..64]);
    let mut i: usize = 64;
    while i < FRANKENHASH_POW_MEMORY_SIZE {
        let ii = i + 64;
        s20.crypt(&genmem_alias_hack[(i - 64)..i], &mut genmem[i..ii]);
        i = ii;
    }

    i = 0;
    while i < (FRANKENHASH_POW_MEMORY_SIZE / 8) {
        unsafe {
            let idx1 = (((*genmem_u64_ptr.add(i)).to_be() & 7) * 8) as usize;
            let idx2 = ((*genmem_u64_ptr.add(i + 1)).to_be() % (FRANKENHASH_POW_MEMORY_SIZE as u64 / 8)) as usize;
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
struct FrankenhashMemory([u128; FRANKENHASH_POW_MEMORY_SIZE / 16]); // use u128 to align by 16 bytes

struct FrankenhashMemoryFactory;

impl PoolFactory<FrankenhashMemory> for FrankenhashMemoryFactory {
    #[inline(always)]
    fn create(&self) -> FrankenhashMemory { FrankenhashMemory([0_u128; FRANKENHASH_POW_MEMORY_SIZE / 16]) }

    #[inline(always)]
    fn reset(&self, _: &mut FrankenhashMemory) {}
}

lazy_static! {
    static ref FRANKENHASH_POW_MEMORY_POOL: Pool<FrankenhashMemory, FrankenhashMemoryFactory> = Pool::new(0, FrankenhashMemoryFactory);
}

/// Purge the memory pool used to verify identities. This can be called periodically
/// from the maintenance function to prevent memory buildup from bursts of identity
/// verification.
#[inline(always)]
pub(crate) fn purge_verification_memory_pool() {
    unsafe { FRANKENHASH_POW_MEMORY_POOL.purge() };
}
