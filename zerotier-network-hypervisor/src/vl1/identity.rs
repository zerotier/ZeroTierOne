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
use highway::HighwayHash;

use lazy_static::lazy_static;

use zerotier_core_crypto::c25519::{C25519_PUBLIC_KEY_SIZE, C25519_SECRET_KEY_SIZE, C25519KeyPair, ED25519_PUBLIC_KEY_SIZE, ED25519_SECRET_KEY_SIZE, ED25519_SIGNATURE_SIZE, ed25519_verify, Ed25519KeyPair};
use zerotier_core_crypto::hash::{SHA384, SHA384_HASH_SIZE, SHA512};
use zerotier_core_crypto::hex;
use zerotier_core_crypto::p521::{P521_ECDSA_SIGNATURE_SIZE, P521_PUBLIC_KEY_SIZE, P521_SECRET_KEY_SIZE, P521KeyPair, P521PublicKey};
use zerotier_core_crypto::salsa::Salsa;
use zerotier_core_crypto::secret::Secret;

use crate::error::InvalidFormatError;
use crate::util::{array_range, highwayhasher};
use crate::util::buffer::Buffer;
use crate::util::pool::{Pool, Pooled, PoolFactory};
use crate::vl1::Address;
use crate::vl1::protocol::{ADDRESS_SIZE, ADDRESS_SIZE_STRING, IDENTITY_V0_POW_THRESHOLD, IDENTITY_V1_POW_THRESHOLD};

/// Curve25519 and Ed25519
pub const IDENTITY_ALGORITHM_X25519: u8 = 0x01;

/// NIST P-521 ECDH and ECDSA
pub const IDENTITY_ALGORITHM_EC_NIST_P521: u8 = 0x02;

/// Bit mask to include all algorithms.
pub const IDENTITY_ALGORITHM_ALL: u8 = 0xff;

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
    fast_eq_hash: u128, // highwayhash used internally for very fast eq()
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
        let address;
        let mut c25519;
        let mut c25519_pub;
        let mut genmem_pool_obj = ADDRESS_DERVIATION_MEMORY_POOL.get();
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
        let _ = self_sign_buf.write_all(&c25519_pub);
        let _ = self_sign_buf.write_all(&ed25519_pub);
        self_sign_buf.push(IDENTITY_ALGORITHM_EC_NIST_P521);
        let _ = self_sign_buf.write_all(&p521_ecdh_pub);
        let _ = self_sign_buf.write_all(&p521_ecdsa_pub);
        self_sign_buf.push(IDENTITY_ALGORITHM_EC_NIST_P521);

        let mut hh = highwayhasher();
        Hasher::write_u64(&mut hh, address.to_u64());
        Hasher::write(&mut hh, &c25519_pub);
        Hasher::write(&mut hh, &ed25519_pub);
        Hasher::write(&mut hh, &p521_ecdh_pub);
        Hasher::write(&mut hh, &p521_ecdsa_pub);

        Self {
            address,
            fast_eq_hash: u128::from_ne_bytes(unsafe { *hh.finalize128().as_ptr().cast() }),
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

    #[inline(always)]
    pub fn algorithms(&self) -> u8 {
        if self.p521.is_some() {
            IDENTITY_ALGORITHM_X25519 | IDENTITY_ALGORITHM_EC_NIST_P521
        } else {
            IDENTITY_ALGORITHM_X25519
        }
    }

    /// Get a SHA384 hash of this identity's address and public keys.
    /// This provides a globally unique 384-bit fingerprint of this identity.
    pub fn hash(&self) -> [u8; SHA384_HASH_SIZE] {
        let mut sha = SHA384::new();
        sha.update(&self.address.to_bytes());
        sha.update(&self.c25519);
        sha.update(&self.ed25519);
        let _ = self.p521.as_ref().map(|p521| {
            sha.update(&[IDENTITY_ALGORITHM_EC_NIST_P521]);
            sha.update(&p521.ecdh);
            sha.update(&p521.ecdsa);
            sha.update(&p521.ecdsa_self_signature);
            sha.update(&p521.ed25519_self_signature);
            sha.update(&[IDENTITY_ALGORITHM_EC_NIST_P521]);
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
            let _ = self_sign_buf.write_all(&self.c25519);
            let _ = self_sign_buf.write_all(&self.ed25519);
            self_sign_buf.push(IDENTITY_ALGORITHM_EC_NIST_P521);
            let _ = self_sign_buf.write_all(&p521.ecdh);
            let _ = self_sign_buf.write_all(&p521.ecdsa);
            self_sign_buf.push(IDENTITY_ALGORITHM_EC_NIST_P521);

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
        let mut genmem_pool_obj = ADDRESS_DERVIATION_MEMORY_POOL.get();
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
    pub fn sign(&self, msg: &[u8], legacy_compatibility: bool) -> Option<Vec<u8>> {
        if self.secret.is_some() {
            let secret = self.secret.as_ref().unwrap();
            if legacy_compatibility {
                Some(secret.ed25519.sign_zt(msg).to_vec())
            } else if secret.p521.is_some() {
                let p521 = secret.p521.as_ref().unwrap();
                let p521_sig = p521.ecdsa.sign(msg).unwrap();
                let ed25519_sig = secret.ed25519.sign(msg);
                let mut tmp: Vec<u8> = Vec::with_capacity(1 + P521_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE);
                tmp.push(IDENTITY_ALGORITHM_X25519 | IDENTITY_ALGORITHM_EC_NIST_P521);
                let _ = tmp.write_all(&ed25519_sig);
                let _ = tmp.write_all(&p521_sig);
                Some(tmp)
            } else {
                let ed25519_sig = secret.ed25519.sign(msg);
                let mut tmp: Vec<u8> = Vec::with_capacity(1 + ED25519_SIGNATURE_SIZE);
                tmp.push(IDENTITY_ALGORITHM_X25519);
                let _ = tmp.write_all(&ed25519_sig);
                Some(tmp)
            }
        } else {
            None
        }
    }

    /// Verify a signature against this identity.
    pub fn verify(&self, msg: &[u8], mut signature: &[u8]) -> bool {
        if signature.len() == 96 { // legacy ed25519-only signature with hash included
            ed25519_verify(&self.ed25519, signature, msg)
        } else if signature.len() > 1 {
            let algorithms = signature[0];
            signature = &signature[1..];
            let mut ok = true;
            let mut checked = false;
            if ok && (algorithms & IDENTITY_ALGORITHM_X25519) != 0 && signature.len() >= ED25519_SIGNATURE_SIZE {
                ok = ed25519_verify(&self.ed25519, &signature[..ED25519_SIGNATURE_SIZE], msg);
                signature = &signature[ED25519_SIGNATURE_SIZE..];
                checked = true;
            }
            if ok && (algorithms & IDENTITY_ALGORITHM_EC_NIST_P521) != 0 && signature.len() >= P521_ECDSA_SIGNATURE_SIZE && self.p521.is_some() {
                ok = P521PublicKey::from_bytes(&self.p521.as_ref().unwrap().ecdsa).map_or(false, |p521| p521.verify(msg, &signature[..P521_ECDSA_SIGNATURE_SIZE]));
                signature = &signature[P521_ECDSA_SIGNATURE_SIZE..];
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

    const P521_PUBLIC_AND_PRIVATE_BUNDLE_SIZE: u16 = (P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE + P521_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE) as u16;
    const P521_PUBLIC_ONLY_BUNDLE_SIZE: u16 = (P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE) as u16;

    pub fn marshal<const BL: usize>(&self, buf: &mut Buffer<BL>, include_algorithms: u8, include_private: bool) -> std::io::Result<()> {
        let algorithms = self.algorithms() & include_algorithms;
        let secret = self.secret.as_ref();

        buf.append_bytes_fixed(&self.address.to_bytes())?;
        buf.append_u8(0x00)?; // use 0x00 here for backward compatibility
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

        if (algorithms & IDENTITY_ALGORITHM_EC_NIST_P521) == IDENTITY_ALGORITHM_EC_NIST_P521 && self.p521.is_some() {
            let p521 = self.p521.as_ref().unwrap();

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
            let p521_has_private = if include_private && secret.map_or(false, |s| s.p521.is_some()) {
                buf.append_u16(Self::P521_PUBLIC_AND_PRIVATE_BUNDLE_SIZE + 1 + 2);
                true
            } else {
                buf.append_u16(Self::P521_PUBLIC_ONLY_BUNDLE_SIZE + 1 + 2);
                false
            };

            buf.append_u8(IDENTITY_ALGORITHM_EC_NIST_P521)?;
            if p521_has_private {
                buf.append_u16(Self::P521_PUBLIC_AND_PRIVATE_BUNDLE_SIZE);
            } else {
                buf.append_u16(Self::P521_PUBLIC_ONLY_BUNDLE_SIZE);
            }
            buf.append_bytes_fixed(&p521.ecdh)?;
            buf.append_bytes_fixed(&p521.ecdsa)?;
            buf.append_bytes_fixed(&p521.ecdsa_self_signature)?;
            buf.append_bytes_fixed(&p521.ed25519_self_signature)?;
            if p521_has_private {
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
                IDENTITY_ALGORITHM_EC_NIST_P521 => {
                    let size = buf.read_u16(cursor)?;
                    if size < Self::P521_PUBLIC_ONLY_BUNDLE_SIZE {
                        return std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid p521 public key"));
                    }
                    let a = buf.read_bytes_fixed::<P521_PUBLIC_KEY_SIZE>(cursor)?;
                    let b = buf.read_bytes_fixed::<P521_PUBLIC_KEY_SIZE>(cursor)?;
                    let c = buf.read_bytes_fixed::<P521_ECDSA_SIGNATURE_SIZE>(cursor)?;
                    let d = buf.read_bytes_fixed::<ED25519_SIGNATURE_SIZE>(cursor)?;
                    p521_ecdh_ecdsa_public = Some((a.clone(), b.clone(), c.clone(), d.clone()));
                    if size > Self::P521_PUBLIC_ONLY_BUNDLE_SIZE {
                        if size != Self::P521_PUBLIC_AND_PRIVATE_BUNDLE_SIZE {
                            return std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid p521 secret key"));
                        }
                        let a = buf.read_bytes_fixed::<P521_SECRET_KEY_SIZE>(cursor)?;
                        let b = buf.read_bytes_fixed::<P521_SECRET_KEY_SIZE>(cursor)?;
                        p521_ecdh_ecdsa_secret = Some((a.clone(), b.clone()));
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

        let mut hh = highwayhasher();
        Hasher::write_u64(&mut hh, address.to_u64());
        Hasher::write(&mut hh, &x25519_public.0);
        Hasher::write(&mut hh, &x25519_public.1);
        let _ = p521_ecdh_ecdsa_public.as_ref().map(|p521| {
            Hasher::write(&mut hh, &p521.0);
            Hasher::write(&mut hh, &p521.1);
        });

        Ok(Identity {
            address,
            fast_eq_hash: u128::from_ne_bytes(unsafe { *hh.finalize128().as_ptr().cast() }),
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
    pub fn to_string_with_options(&self, include_algorithms: u8, include_private: bool) -> String {
        if include_private && self.secret.is_some() {
            let secret = self.secret.as_ref().unwrap();
            if (include_algorithms & IDENTITY_ALGORITHM_EC_NIST_P521) == IDENTITY_ALGORITHM_EC_NIST_P521 && secret.p521.is_some() && self.p521.is_some() {
                let p521_secret = secret.p521.as_ref().unwrap();
                let p521 = self.p521.as_ref().unwrap();
                let p521_secret_joined: [u8; P521_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE] = concat_arrays_2(p521_secret.ecdh.secret_key_bytes().as_bytes(), p521_secret.ecdsa.secret_key_bytes().as_bytes());
                let p521_joined: [u8; P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE] = concat_arrays_4(&p521.ecdh, &p521.ecdsa, &p521.ecdsa_self_signature, &p521.ed25519_self_signature);
                format!("{}:0:{}{}:{}{}:2:{}:{}",
                    self.address.to_string(),
                    hex::to_string(&self.c25519),
                    hex::to_string(&self.ed25519),
                    hex::to_string(&secret.c25519.secret_bytes().0),
                    hex::to_string(&secret.ed25519.secret_bytes().0),
                    base64::encode_config(p521_joined, base64::URL_SAFE_NO_PAD),
                    base64::encode_config(p521_secret_joined, base64::URL_SAFE_NO_PAD))
            } else {
                format!("{}:0:{}{}:{}{}", self.address.to_string(), hex::to_string(&self.c25519), hex::to_string(&self.ed25519), hex::to_string(&secret.c25519.secret_bytes().0), hex::to_string(&secret.ed25519.secret_bytes().0))
            }
        } else {
            self.p521.as_ref().map_or_else(|| {
                format!("{}:0:{}{}", self.address.to_string(), hex::to_string(&self.c25519), hex::to_string(&self.ed25519))
            }, |p521| {
                let p521_joined: [u8; P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE] = concat_arrays_4(&p521.ecdh, &p521.ecdsa, &p521.ecdsa_self_signature, &p521.ed25519_self_signature);
                format!("{}:0:{}{}::2:{}",
                    self.address.to_string(),
                    hex::to_string(&self.c25519),
                    hex::to_string(&self.ed25519),
                    base64::encode_config(p521_joined, base64::URL_SAFE_NO_PAD))
            })
        }
    }

    /// Get this identity in string form with all ciphers and with secrets (if present)
    pub fn to_secret_string(&self) -> String { self.to_string_with_options(IDENTITY_ALGORITHM_ALL, true) }
}

impl ToString for Identity {
    /// Get only the public portion of this identity as a string, including all cipher suites.
    #[inline(always)]
    fn to_string(&self) -> String { self.to_string_with_options(IDENTITY_ALGORITHM_ALL, false) }
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

        let keys = [hex::from_string(keys[0].unwrap_or("")), hex::from_string(keys[1].unwrap_or("")), base64::decode_config(keys[2].unwrap_or(""), base64::URL_SAFE_NO_PAD).unwrap_or_else(|_| Vec::new()), base64::decode_config(keys[3].unwrap_or(""), base64::URL_SAFE_NO_PAD).unwrap_or_else(|_| Vec::new())];
        if keys[0].len() != C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE {
            return Err(InvalidFormatError);
        }

        let mut hh = highwayhasher();
        Hasher::write_u64(&mut hh, address.to_u64());
        Hasher::write(&mut hh, keys[0].as_slice());
        if !keys[2].is_empty() {
            Hasher::write(&mut hh, &keys[2].as_slice()[0..264]);
        }

        Ok(Identity {
            address,
            fast_eq_hash: u128::from_ne_bytes(unsafe { *hh.finalize128().as_ptr().cast() }),
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
    fn eq(&self, other: &Self) -> bool { self.address == other.address && self.fast_eq_hash == other.fast_eq_hash }
}

impl Eq for Identity {}

impl Ord for Identity {
    fn cmp(&self, other: &Self) -> Ordering {
        self.address.cmp(&other.address).then_with(|| self.c25519.cmp(&other.c25519).then_with(|| self.ed25519.cmp(&other.ed25519).then_with(|| {
            if self.p521.is_some() {
                if other.p521.is_some() {
                    let p521_a = self.p521.as_ref().unwrap();
                    let p521_b = other.p521.as_ref().unwrap();
                    p521_a.ecdh.cmp(&p521_b.ecdh).then_with(|| p521_a.ecdsa.cmp(&p521_b.ecdsa))
                } else {
                    Ordering::Greater
                }
            } else {
                if other.p521.is_none() {
                    Ordering::Equal
                } else {
                    Ordering::Less
                }
            }
        })))
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
    ADDRESS_DERVIATION_MEMORY_POOL.purge();
}

#[cfg(test)]
mod tests {
    use std::str::FromStr;
    use std::time::{Duration, SystemTime};
    use crate::vl1::identity::{Identity, IDENTITY_ALGORITHM_ALL};

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
        "a145374edd:0:552f4f3244d1cef146dfbf781e4a64d1b529b8a30b496100dbd4f2e60553280f4089f1a0eea27821991604d7bd8de93c1c24fd172558287c73c72a313865fe5f:c845de3ec427903e97feec496698b83d130a0fe03fe165f53f3c3dd1b86c1d4d808936369637e1d023f25b2081176d31ca1cc828c1b135da23eca5aa37476813:2:AItC2JQyFwA10ijnEmkPqNHKlH5L-nl7HiPA9bF6DlL6-qST7OALZhgDWTFjR3oqjTaxTwqeq4f9UUOZasMlSqw0ACV1H-JFhORbhzMu5Koj4Ufe6c7QZvpVrmPp6Ka2QsVQOkoca3E-hD94IJqA01FYA4tHtvW5X32tdg5YKFFNjXs5AFSm9MQeO21kIWTG-Xyu-BGGRvOTK3f0DXsjS3tiIWiECy0HIJaNBDaSg3qme7wvQ-v2xAiHt8OtI_vL6_-vpUbNAAknWgKeoP_ZTrtd4ReyKPWT_cP_lv6ba9x9FReNEnYNaAIXSqNFpZIeIBF3YMw33xabt4vKLFBaGW_2hGqzqWOhATyWjq077SLpCRFNqm_s6VODhowOsOHX6IUhYsLsE-f9t6jPykhEJsYEkqwh2yBVMAL4kTyfK_18o7s01_Ju8xsMADW7PJAGgypm4MTxNiODpHdnKqAxyXJblNPA2b9mv9YgOScDlYLg6gS2pk0od1CezPgLhEulOdaCfr8MddsW4_WH_I9wy2xuLAUQZdoZH5l5pdgOuForVSp2vgCYiXEmZfc1ybl81m5QnstUef6NQe9iwKJBd09A_tC11kGID-SaCQ:AdXjxecCo3AW9X-H4RIfvQiOItl4aESgTQem5RvWPlp5mNYi_AmPXdwl2ay5Ul1Ev55W3lLHuxK4kpzWH4pMdOktAAA-UD-xk-4cvGvE5VFo8XV9eGf0rsTXMYFx66GM5nqhMY7SqwNpi-CRPNzzwzOEeVj5J_monJlTd46pO0nkOz7j",
        "3fb14dca24:0:bf46edea78200f176766c6b9c96ec68bc513908b974db5f877a0c9789a26ea4dd4fe1326fba39fc1bf37de1053948a200770ee2d145fb7f074f83b243f26b30e:406d507510278d9d3a1b8bbe183a9cbc054d6eefb4e66ea38bb85203aed1a74502782fed8a3b12060fcf311f2b12dc006f3680e091b9a7ef8eecd0dfad91ad57:2:AJM7WjBgO24HVjoSQ2TXaKvH5krzZEeIekF_vV_A05-s30j0EJK0ZcVB-RJtt410JOmxA_wpdvp6m5CZdoDi_R6PAJ66ah7noFWdJ_BZ8tV4Q5AK15rA9AvmONZLoxvDAsmWFzHPOeAQ-nWCfFDqPNiI1kTRc3gOIzheOFZ4Esy-AjwuAEvqydx15Y38oC-B1TeS5Ey1fCFqo19R4f-c2hxIUM_6TjPYPFWuM26Q06SyAkedetjiSAkj95tSLqB9Xb4UXCS2AGZqnOLT2tq5pE9XlUAze3K8e9ubtThKOYLzUM1kxYDA8UqxeIf-JFIv-ukcpB2XL2J0vcoeaI0Ki-a205inXX78AJ6BQJut63ovH46OfGP0YkKnY7VdX5N6bZGLONHxtCd1JCVphqa60ze3Yh2nkCSPHGxK6LJ-kTO17ypGcy6C6ztbATj_yXeADyZAbfjiWLDDJXngYWMGu3-gp0gdQM6iv2vuLlrQ8BrBWR9W77xF6mHjPTCo58YBHtu86zdoY5ecBFMwbXrpwSYb_m_UvQXYbY1EcyTT5A5NPkjvE4wd6J48dj-0M5te58v2RBVDLZkDzT7e86vUeKrxABzk2YPr6jLUAQ:AABH1HPwGnfdyciVwch0PYH67utoUtk4XxqmPiNZx9b9oUepuv22rlBd0JdnCgPbvMjfj7MygtneVG48pDbeMe28AYOOjZGjHNZ5cKxQIoOzYuKPDuLY67BcQOOUBFKsmz9cRyCe8sQsc3HtnyYDcLcNCVS3c2Hi529xAvX3qhjuxh-d",
        "467ca2d39c:0:5836b22097530a21999b17dc1bade971e8c8d92f653da1428eaeeb1c7d25296506081a3660f968a81463d75b86fc8f01455c6d86ff13aac28a63d5aef159645f:d0d23ddad71bf39ccaea2bca7ab777100632b3904a077d6f5e5bbf580b32ef50ced7be0090ea6d8775e62bd11acadcacae6effc2e162bf9e694d2b3c498c9d26:2:AE2mcfDzyoksMh-uhinx5BYTTdLgbQCq9Tzfhb5-MdJKIZC0drgBJLAsQTlL1DEjWdSqJGC3c5J__Ydu3Q78qgU5AKUu-YpMKx8gK1EYYNnJLOiN5vxSdViOmUX91_4YxpbDXPWAXoGynKlW0XaHPk6jtrvlZcvOY08BbOc6-3XckKQwASx8yJ13H_wwnuAEhm_9Mv6tgFVroR2cWfn1AtZKoPdpfMg0u8nghu4ULfiyiusXcxc9I9c6wedr_gk1F2O1_y9bAA_pdFwN-JuHRO0QuU4UDC9cS4G3Gv8ZZHgNIswgjWWMvwrE6kcBtvoP5pgPYFTqaxQ5s_86qe7EIW6vF6InjLk_AfkaAfFjZ2tgbjh2u-tWwPmieYcRCyvifZ3pCyFpNtCy5ILhXgejUWg68aEqTgTLiL__2Jg3V4bTL7TpWWiWmNnnAeN9uGHULx3AJD-I-Uq1A2hNldlwe0pmRSc8VLQld5EClXM_RIh_WtkOwUB0yG1N6rfbtXf7wY-Pq21DzsDFezNukf97fACJfoIRiH73gHCyizM4_NkbbQL9W_7ehsVzvIJBqfyGd2BxuqxRxVPL_SkoACOo9BsWUXyMDCPWzFMTBA:AU3cy-juui20tWVrYndPqsmeJmQoSJtxvyr0RPjb6Zr5wg7q8gFWgdZiPVRh_oz3-k_upRUNaGFbpW6CW-d9P8NcAamCreeQ5m42hdzd5x5ao8ouE-aR9aBhBFOfusPAIgqm53857KwtWwoaRjkRo4RH-_oZXEKn6sG16XFFs-rBlG7p",
        "344397cd0e:0:3f847dfd3b747bc5f9da128945e9b36199dc742c7a51bb2590cbeeb0eb3ef07b5fa68a7a97b7d13fd6d4dc122f3fe71094cc177456c3d92e05c191e18ef78f01:c88e4895defd13ae763e68cff50e7406c6b601f18c1a24406ecfe9392a9f9c50206194a29bdfc1b256547407540ca14d91d3c2069515c754dcd5065fdda64032:2:ACXUR9j2GTh2jEyCDKj5qmAdI2XVMHeQMJYnpTVL3dVlODbrTPOc326YQ-vxNmIvIQa5KYPqhYIqbYfLeeKv1wWaAMmkPgmcLNKz2lZ3GqE-dvQ4--rUOWITIpSTcg1Kwo1o9twmU5ynCyah826-xhcJnfWg19AzPqR6Puwh2IGj9Q7oAGMVoTob4feGAj0G90nD4V-Uylqyb-bh83NWo61jsUm42UGptsq397LTR3oM3AzqNV_ntO1Aiq-exy6zBVizOMJfAFXJx6pc8CzeS-pPeg3h_e4MKU7_GZGayrIbEoN8SUoBrl7yonKkfm2geSNCDf3fy9VXYcKvdGlZKSvGpxQ8p9PBAQIc8gBz1iCfggx3XCMd4d3cRMdsbxmlp0vlb9DRKbxakAlzcz6g92wUJWsZ1b6p5F7b9glyL4BezlPyGxPFNHHNASpkkx1YsGdjm_KhllbAzeIBwLd4ghwsRXKzFQ2Gm2lf6CjVECDAY77pe4oX4RJTZPvBSC1sWMndBTqqpioxMYOHY9iVC6im7i44nUrW3q4OSe7PfC5LRN7fExpbq5czH9bQ9FS_uxY9v-gDiL9MdLNNs_o-iLsy3u6vq0mosn5nBQ:ACZPO033kuvXj-vtlimDWcdTj-aLRo3VlAUd2PduaakvxdVHoBN7wIvWFlZRbi7rx842vS49tZAQtIvPSnEUMms7AZc2iXkvAFvQiecTKN3Dsa-iOX0zf8f-cUHK2OJ0VVkp4XoXHvVeL_Me-l1MUZBUEQVx2uYVrU7uPxext8qqpeRc",
        "33c9267872:0:8a50ee533b2fe15a03ab351a26f0347cdaa087f66093b71005de4660202e7932a41dc38d8c47f38b1d0adb39446b1d58f8f5eb1f1d28ca2b6bb7f7ef0bb22fdd:00e1a9b356a1f5f28515df24015623fa09f5fb72e83de115a66dfe7222b8266fc37f0bc8fa948e80e40067246b315b275e31d045622d047b2a4de55161b089b2:2:AOO1k926VACCHyWxMkCRZheDch28mvwN6jW46yDGm7gPQwroB6Yy9VaftCWNKa3togYSLTfowEac-0v9dBns7HHcABbJUmsAJ4038GoIeXsFIBSYP_uvNFZN4Xn4ZtyHeRCZDmR2rKaGB9hjMEZ6XkdtV17UmFLjlO9VG1SosOZJQhvVAWSbEbDyo7kIynMJmo94pyp7ZDYzWiZ8ubfhXZNAxnTNpdz252s3__O3xUUQcHr-gmKLVXGnHfPrFrG0XhLyZH-oAHK2jZ-FhirXn7v7gyW3JA6s_S-zBYb-qdY6ZlTM0WpT0kys_AJ9fYY7TB3AS0q27Ag-swg-jGf5qedcXpYUg7GJAQ6Tq__uN4ILH4NryKq6RQArqdTxB6tmCSTkjnFbZ4YrDUX-_eHB-i9oisZoIRz3pjEYR3EcY52hn6EI1sRPxFQOAPPxSW0vGqr5JMM8sZp7S-jUmVTnu742e9m9dfpO7bEPnPaeEhJUFVzPvLKeIrkhbug20hd5CkClRgiUgJXQ3IpQoDes1t1aGL8uZudFknGsm3QJXpw6Tb5h8ZSuQj6bueWw7QdNcHV7fA0UTJpy41lgx4WZ7xv2-M0jkOhmliu9BQ:ABtZ0WVVe7SywNQ1oVeh4VIXkk0IT0ML8kUwy4GnCGCKLOit084F14eoDss0okk248jDJEjiRTmmSXQbC2I_SrVZAXL-Q-MhOUuHdiR_ES2YATalcWfDH534_g6D6MI1l-dJ7HyQu9Oz-xF1SMiP6-XhvS9vnmMBuyNkirbI7c1eGE5Q",
        "00ee1db06b:0:3d60bd6ef991ee621cc77573407d6be66bd60dbd82f2ada4a28afafcf8b00c6a626ed3d000a4bc2630a013fa4f25a8e60116bdda14bd1380e48a3360e9eff6f0:603314e715652db9b8e0b2de217051a9a017962e7349a94ad2f4f1787f3e1c66b881c47c73a8dcb7d020b13e4a5858b0cfc392ce348e83c1a2c4c76471346ea2:2:ALEHh95ZRAP1I5lahFTXRa_51YeG0l3t6vroG-q1YFpTnIYTN4JajBBXmQhmTf9hHDc-R9iQstPXRV4FPsFmCihwAOAenFhNDDRlr7otUP5o2T-Oc3yX3MEZZa3jY8GxSuL_2aVQEcYgPfWrierDiXi7QQTGyvmkt-wMy414nPcRTP2NAaEys8AKMRISLFh631kJ_b-_KdViUmAnAMcxG7zVu-tr2Zi00tZ2i8KY_9LCXofEtUdGTg29CqqkNlMX2y0cRz6pAHXWIUNqq7EVy6DzAUEnMZvcKW-Ig2zJW-yOOvPsIQZ_naOOquyFSyQoRGfvVajI6CIjmwgBaOL1hD1EG50AoVu3APwCbvUhRgU-48s0gqY9fQ5buC9Gy-cH78qmxKv5a0kZWSeXtu6J8CeH68tCd8yZhgqA4c4TBc1T6QE5NSJRbmM8AMF_O2TBwoMwWhs5KZ7ljEX5L4RwbBu79g3R_3ub8s8iQ9wHuSyZDlICDlbH__pYY0jBwNIwH6wOnyp9bZ9u610ltU-spahjDy4Kh1W_J3-N35dT6psg234-ctpEKoj264kG7WsXOOohRsdSEGqgFuFlKNclevdx4fQsKzsXQkmCDw:AXBt-nAitsr0E_kjc8DhvMVXLOXjKnz9cK9t--mm6_iUmYnAjjVzkvIjS9_2r3ba9UM2vWJ5et9EvOj6rZfZjhkJAZ6tGwq3TrANuzn88lQq845i18rI5VWjMYJLQIl-_uQC-5vt8VJrNYAwPay_o2LV73rMcWMroNFZy6U0sLB5qOs_",
        "50cd888fe0:0:577a71938a275240667d381b80fe4eb4a99a49d564f2bd32d6b4aa58fe2db832837c321d02f2c9f1ac5228572f6446c3706e09c7c81340450ebcc42c5cde50a8:d0f23d0a727636bdf9d8ff7a2ac4836972e0f1a754cdb6d70f070261f5f2f154d17fa6424f2cd2da9cdaca18eeab11181b40463c8070f4f5f22b286f2040cecd:2:AacV20WYy_3mvFze-EdowUu4w1K_tcvy6U1iU62k5UdvgdWdpPHY-vXDMffN-Hs8eYes9rxucBQTTL7oxIs3L6u8ANDr4kd6ErsTuK7ewI597clD51pi9B7QFPRf_KxktFGQvVGY2IYbZ8tS4vRX1MKPdwojwea02QT0N5aDqH-NXv-nARfC8hnCY8XSrkdrXMcIS_5tBsbKOXfz49yb1yWBaOwWXDtTQesPDfesdQYMQ8WNKuDo_Fj3-EaZTwWiYa619bn4ACdIoI0YtWge-IGbETyeEic8KXXsrKOxXCC8bTwmRUmACTZc5e8XX9zsSNLxjffbZJMyXUAFU1AS8tc6kPrknD6aAP6P-c84D-bfS5rC7-YVs-YyW8-LZ8YrjWDTqWkYM2PdNISfe1yGygzoapEfHK6Np-0Q5jYTkhvsPXgsBFwEmI2CASLjj3BqkotcfrL9dPv4FNQgmffVBM__LAX8kkPglXzBfCwJ70o8-NmMoYF-TNtEwUqHnpv5ypLd05N2U66Z1aZ7__TalVxWFK_fdr8UTHw35c6HgCMlaneuIfb2zKwt3_F-v_iSlvWE8C2SS8cAm5PBnC8vYaI4ysxYgxlwJaImDg:AD-Iwly2Yj-bbmMMerQWMVG37luX0Nfa1g49j0nAYMXqDRaENAtQ75ZyY0hwst1xtYG0xC3PtqDGQVhjQX23xVC4AIuTSqG5VuFd5PRAXqXAGj487EKt3GNdj8ChbmZeIxETcQH7d_Zj4By1KB26Qzt3qQRiSbKW2cxF62_YBry3SUza",
        "939576391c:0:487f993e2f270e840eaaa3d1fe82c5027ff8ee62c15c9e9867589693e8ae2f7747583863a81e8a886916d57989f6c0ab35219d605b45990d8d4682b86549f718:08a6139b7c55b402ad0ad0d8c8541581a506563f2d018636e613b7a7ba472355ea4ea13e08de962876d4b4fd5163b486dcc2be0c47a1fdecfbb54f217c1273f7:2:ATqdfUlPE7Tp9cXniiWiIZMPsjtlfWoijyfDzKGGu_OTk3btuKt6vHQMQeuswYiFaKmjqxB0hkUccdrsWTNQ-jaJAG_8aFsePJ0VjSaytfFjLNyCvNLweVfiGflJVMAWnN6otiSe0zozPMGZ-RpJJFaHV7uuNolhl_3jWPHRgcgWubLMAcxnki1xqixmzb3Ygp-9VvY6JlCEAo50sDCKX0ll3noz6RGrwe5SEyxwtsdvGJJnpLB0-2JlEjpkg2TIR-FUsP8QAMKS_zF1546vKwhTpz8FfjQOjVJ2wxIzeMxz96rLQxN5y0DIGQCSKc3E0U3u7TKtveid4qQ_2NfNhDy4bjNjdSb9AcTBzzkexWL0uu87_jy62y2AJG5xFFbsRItReM_NrLpk21lXH7xKC2pkvCavp2rX8nkl_Zprs4GjONnnrjQqUPBJABd64QpuON1P-Azv3FSAY419QXj2QmhfB-g2KxNGLXXc7gXSjWr58EY_BPP7fKuQxr-TDmkgq-QAdzAibEoJS0swK1yYDoHJeDHgG8-BrRRivHxAkGW0sYiW8ovNg92RPfMelU9SLpgw-mC85hkL5y2iUwR15EmiCkF_REkPpNUmAg:ACuwxcroXHk3FPGSQyOixwz_MFtzk1cy6TEqeT-2pQ9m5fd9AIf4xAEOKHo5hsuXNkaE42deXntrm6Kzde2uNTXYAfkXNAQL_j5oJu_9GROif3v_ZwzSTSMkEMhu-FwXGaUZR3gIST_2Sddt1Psakivczd6toHcBfFBSNIa7mcxzPj7m",
        "0d7224be02:0:31c616d4ba918e1a59a7efea6633a51ddbb94056ef5576fe61268ca0c0f3035a3a1bb4200c737c6e4a87bf979f9767f230066bb41e0641210d5676493845ddb5:1868a4cd394d0e5b0904dff18094333893ebeaf29561391ff442235fc6d0406f8ff119a8ce948b417a1fdf5067d8649e481fdfa67d85e865726a960d941dea5e:2:AcJ0WaHJyhDYold1aIwTqxJOwV-a_IJSqsQS-1x-YxOdBQFsQEEu23A1WlyqY3z0_KdEvJ8vG8NItpa53H4AuYIpAASlWlJG1KxCnWj4_5W3Xzj3eVu_gyoEgs46vQkCsSk6IqQzEs9orPwbUq3Wajwbd9O5D_CSdkYCa-chB1SZFIoPAGrzJc99-zdAQayev3j4WkSlr9jHvnYWdUeei6AE49VKXbg78BuioWkeH7FRCI2PoFH1WZnvFQoEzLJSotefchGGAG3RrxQJXbxzEKoFaLTAEuFYd5OkwUT3nKRrTuHgc4vaYEas5fmylqpmeozJMSzNfcmMKE-Kpr34xN-4Wn5kpTsdAdrCG0okMPhveRz0qdjLe7D_Rb10zAZoed3M5N99kIxDwUNVyFkMPAv4Y3gl0k_Kcg761GA5Sf8UxEFLLdKEJDD6AVMq9_18YBxe3fu96VvMpriK3sPKN2ICUZDauy9WvT35okNxp_RibID6FtipShYRHhz8tsqA2HUOUqbzcWEMqasyWritiqy8wpelBCg2nCHYWorR64Af0x9naZyNaqYuuEQ2Q6cjeoS1odAi--5QUbf8d1eQs83IDX1XUufb8qT3BQ:AGwwFjs3XifAex32HyTeSwzdjUYiCoQhM3GrRPStTI1Oxxq0nU4bPxCcvsM4xzzIIw0o9bcQkJDGBqaH_L_eWu7dAEFI_WCuaI7nS-5sLjtAQMeaQbeAy4omS5XuODVHBHadgHEQwZ2xroQgqH_3Wy7E2jphUqSWqro7cLdRbSZuxkL2",
        "8c3ba1296c:0:6137b816948f56158455356514eb396ae7fe14846af5b237003c394a38e1347bb159fffed778b958734eb3a3e43aef56c03a908af08f32d817fd8fd57be0b1b4:8048d42cedc04bc6e6fe0f358200239f4db1a5f16f4d156354ac55448fb6d5444131ec66ce53012765aefd98a45d12dbcf39e572ed83e370e0ba6595cb2252e5:2:AK0oHAiihtJ0d4YgirwblDtM9y4AFbUnMu6PcS9k6wBBCLnB5YlmbM3Ejx0MY-T1V99nmKydwmfvxdNZudDQ90qrABhXTfrDWBY1XVJmNHxQvvBRXyrlBn5ZM662JLI_qyBMo9VsdKKeSQHX4YFcu7n0mCCIDGH_jbVIyXqjLJDscoriAP8JKjOGN-XL7YizUxewM3nsaJCDED8vs0Y_iFh9owmf3KKJ2BPXlNv4hWExVoK4YAG0OjDERJoO_Y6LCnxO89xVAOyFR0PH_83mUN8I_DX8KVHiPM_Od7bgQnkB9Di3QvIzzqX8NKEEnJfOTuEUnptqmXeSih01e9KiJsLAdViU3VxxAcDoO6JOUkmGspZqPF2ms9FrKH1xRpx0EMPsFv1YJcAgBxxjvNaG-FBT8xWzpNRwBg13-XiDzRdOHNy4T60APXtnALW7J6zoXwhnQSM5XyBb54uO2IsZSeO2HEzFnEttGlKMEZTXSHfEmG4x8teYyIPC9glom-5FVWBxgu6p7xAkc6uktCKigNzHDiyxaF3m6b_oWRsSIe4FbeZv0IqidkbxU2fUFnDVfIuq5vp5knyzawnfv91pne6oWdQzQPIW1wD4Dw:AIBIHwGOEpJTwZVxMElofxxUlTOgNVnixX6mPE92OdIKOVGd87jiTBaRx66LjSIyRS5GiFk54F7XSXlQSx7eRfoDAR6whTQKxhFOh8XGfO_0bRqXyna2BAWTVFoKNcS8TmpPw2Ug-IOMl5USI92lF53P-yDPi_QouJu50uZ4SV4_bScC"
    ];

    #[test]
    fn marshal_unmarshal_sign_verify_agree() {
        let mut good_identities: Vec<Identity> = Vec::with_capacity(10);
        for id_str in GOOD_V0_IDENTITIES {
            let id = Identity::from_str(id_str).unwrap();
            assert!(id.validate_identity());
            assert!(id.p521.is_none());
            let idb = id.to_bytes(IDENTITY_ALGORITHM_ALL, true);
            let mut cursor = 0;
            let id_unmarshal = Identity::unmarshal(&idb, &mut cursor).expect("unmarshal v0 failed");
            assert!(id == id_unmarshal);
            assert!(id_unmarshal.secret.is_some());
            let idb2 = id_unmarshal.to_bytes(IDENTITY_ALGORITHM_ALL, true);
            assert!(idb == idb2);
            let sig = id.sign(&[1, 2, 3, 4, 5], false).unwrap();
            assert!(id_unmarshal.verify(&[1, 2, 3, 4, 5], sig.as_slice()));
            let sig = id.sign(&[1, 2, 3, 4, 5], true).unwrap();
            assert!(id_unmarshal.verify(&[1, 2, 3, 4, 5], sig.as_slice()));
            assert!(Identity::from_str(id.to_string().as_str()).unwrap().eq(&id));
            good_identities.push(id);
        }
        for id_str in GOOD_V1_IDENTITIES {
            let id = Identity::from_str(id_str).unwrap();
            assert!(id.validate_identity());
            assert!(id.p521.is_some());
            let idb = id.to_bytes(IDENTITY_ALGORITHM_ALL, true);
            let mut cursor = 0;
            let id_unmarshal = Identity::unmarshal(&idb, &mut cursor).expect("unmarshal v1 failed");
            assert!(id == id_unmarshal);
            assert!(id_unmarshal.secret.is_some());
            let idb2 = id_unmarshal.to_bytes(IDENTITY_ALGORITHM_ALL, true);
            assert!(idb == idb2);
            let sig = id.sign(&[1, 2, 3, 4, 5], false).unwrap();
            assert!(id_unmarshal.verify(&[1, 2, 3, 4, 5], sig.as_slice()));
            let sig = id.sign(&[1, 2, 3, 4, 5], true).unwrap();
            assert!(id_unmarshal.verify(&[1, 2, 3, 4, 5], sig.as_slice()));
            assert!(Identity::from_str(id.to_string().as_str()).unwrap().eq(&id));
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
            let _id = Identity::generate();
            //println!("{}", _id.to_secret_string());
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
