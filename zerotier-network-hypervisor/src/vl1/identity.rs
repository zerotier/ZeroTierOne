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
///
/// This "flag" is zero for backward compatibility with old identity formats that expect
/// a type byte of zero. It's always included anyway even if it is not used since it's
/// the feedstock from which the address is computed. Even if we fully deprecate this
/// scheme we would have to include it if we wanted address backward compatibility.
pub const IDENTITY_ALGORITHM_X25519: u8 = 0x00;

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
        self_sign_buf.push(IDENTITY_ALGORITHM_X25519);
        let _ = self_sign_buf.write_all(&c25519_pub);
        let _ = self_sign_buf.write_all(&ed25519_pub);
        self_sign_buf.push(IDENTITY_ALGORITHM_X25519);
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
        // don't prefix x25519 with cipher suite for backward compatibility
        sha.update(&self.c25519);
        sha.update(&self.ed25519);
        let _ = self.p521.as_ref().map(|p521| {
            sha.update(&[IDENTITY_ALGORITHM_EC_NIST_P521]);
            sha.update(&p521.ecdh);
            sha.update(&p521.ecdsa);
            sha.update(&p521.ecdsa_self_signature);
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
            self_sign_buf.push(IDENTITY_ALGORITHM_X25519);
            let _ = self_sign_buf.write_all(&self.c25519);
            let _ = self_sign_buf.write_all(&self.ed25519);
            self_sign_buf.push(IDENTITY_ALGORITHM_X25519);
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
    pub fn sign(&self, msg: &[u8], use_algorithms: u8) -> Option<Vec<u8>> {
        self.secret.as_ref().and_then(|secret| {
            if (use_algorithms & IDENTITY_ALGORITHM_EC_NIST_P521) != 0 && secret.p521.is_some() {
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
        buf.append_u8(IDENTITY_ALGORITHM_X25519)?;
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
                IDENTITY_ALGORITHM_X25519 => {
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
        "36a25ffc6f:0:60f00f28aab589a669be31fd5648c9202363eff2e5e631f48e4c00b8161a03284e7716d297d9a3dfcbe215b63335684ab23b4104aa6b4bc2dc6e9af6e680187c:086f386a41ba42b3216bd3d4faf9623c566bf30956bc2886630d424f909a067843f19b390bce158f8d428eba9d48ec1cb78e63ec0ba126e3709828d16dc3632b:1:ABf6opWWSc5l2ZaY7eiGWpfnHnZnp2pneZtA0LRTIfDbdmygI3kYpzoA_crqX-_8eoyjqAF00XokDcUTiH_rLoa5AL6wardGqRL-3HQNR8683-EslKLD5OvyRJpmyhkFkOQ53MSwyy55aSwGt66drbsfwaa65u2p-UZ4R3KB1Z547yQbAdMbSb5J0Igys6bg2IaR19Kg3ZUthiL0WQLgEPHfEkDMwANel8yqkmjN7ozPYBvSTSjNtMuJMgzLwLpY7ScZpLK6AGOt73CtwAHK5-AUFyS87JF_i8athmRGRryGnzg4mtescD_tkyV-Z75agwtjKiuyJtaoIpkoAE79w8tjmBx81qbbAN4Qdz6mXrKSpl7wblM4yLB-wjWMkr2LFraAIVH7iW52WyPWO2113Kzzko8-B2nXn-pOb_bQ8lxmhH36924Uk2cHAUzPis0M0Tz9imWmEm35zP7_pnQL7WaxzUe9qQ8EQkPsw-SixX1fNqoBuDXrMkHTXKuNXpqhDtkYoHFPuD8ohWKaan8gzcAldlEeJOQrnSPmJ94u6uKDty5rfDpfmMKCg2uht_wjpePdYhRpO_qtxIkTYMrSfGPSFpWUPpHJzCzeDg:AMrd6JuMSabnyOTF6poYgU6onv7i3pJm9Jr75dbt0TnMFryJPKYtwIZSLz4Luw37-Hb9Sq8GiikspNXCqKmwei25AakHubBpLg1UCrqmhZ_ssNArudqht5VqGXazkNPg336Gu6pu6vPD9m9JDWYUTUtfCdlYDw3n-NApnKktcwNM0J4n",
        "ca1142b59c:0:55340d183ec6bcad9d3d7d2ca602a1351ca1726cb02cc33b68308de801714d4d9227b705d22ed1c782bb788767b416dee98f60b48ef85ab7d6ef4c108863c98e:b05c5affee80dfd9828e3cada2be1406b0921cfe0f94a127ec47738fa7bc3d4ea918a1cc8de196ca0bd111b0ff150e2e53b68e08dc5e4a3dd0e1d7ea405c1c69:1:AF1rbcAdlfEGk_Cp30uHic9jdI6P6hc90jk_lyaMVK_tbqwyay3kq5NmLkIClSg8n0lgG6yg3bn7Z_H3YO2DvCAHAO3xmsqjGtRitrnzUYVBD7xE82196UHpp0UdS201aZItIUrIwRVtlO2bWvlPMuZvWfy6UNdQLnDbJsDc4p-_dP0AADvcwkeHGHwaHxHdTKY2VNVzwpLGHC9hf1EAhpR_otNMpz9Kv-czpqrokD0RPx0WhoW9-RMlT51hlJo9UiCIYLBmAHsmiInhF04Fb2gMmEoA92_0PzLfgXxgMCYoWMpEm4BvK_InUfyHM8V4rR3W8z8F73TY-jDxjI_AkNeo4d0UCnKEAUcLbukR_DvFLjGXSwnFLDGhUoYsroHB2AGmWh24wc8dwmIXBx8jTbSdzsW0sc55OBR1AT2KABfUGjfKAqjp29mOAVJjDgXYhXERvFTWMzemEUBRWZddK_kjVSBfLtlvZ3aybTmkoZ6-s-w3zc8G5tvC2DZjUhejRBeA6WNrT6oitN3g2OTBT_cB5-GwQO1yd781BrvhDX21wrkqz9VSY57quBSH9NGUTm1Zu99G-UQRDLY-R9iVCgGCwpQ7vqqIu8rqAg:AdMlVKSw3vRd63YcsdSzCTvmDCY_ltVgjrilHa0BinzzuaAHTqDORE8QzaykUwrxBnU9T8DftEO3cNd1X8mnHDI6AQvzNGR-0LcOHWLwUz8cswNM6unxDfrFNrtjcJToApZiCY_ghVtiC1Fun1Jv1crsJQEJ6enFgCz5snEab4b-jqo3",
        "c5c4eb1d61:0:0270c900ebc9049cccc408e5ffa7057141eba6b9a9bcccff90c662f6f3f76670c29f6a17a7f665406ab4e68d800cc9baba00e7f394ffb01580486202cc46264a:8097f8723d9584a7a35e24fb681ce27a9f2132c36b343a4eb4069f5292f72355a77702cd1e748a1d9085b414c43db3bb8655c64cf907a0e068224f2f58037598:1:AX1IluG8q4EMG2FKzOwJ7W0I-2H8gyUZR2qnO1PbxB6zM4T3Go2TN5h9xFZR3_ndoQUkVRKfPRjhCTz1L87Q9NjwAJwfTgvXcQLJlJy5I--_-E4OadVpCsZvIJlpe2oaWFVUoCr6tJ-se0sce6QIV8d1n8Zf7EAP3cf1MLlIjjTJwcG9ALEBcdR4OAR5HcBU4l0Z9HpyfL7Cs8IcvVia0RpM9JZ20zoEPxvr3RmwB3nN7IGaDFntOFSE1g2dTSb_k1Ix-6PDADC_R0prel6gHl6DaQvg5Gvj4QRF1Ts4gRRZMgZEKJXoXIpW9soY_Xxo6ZD0i82Rj0bfBvZ_-4fWW9BI_AGETlJ5AQZKz0rdu6tEk8Ag2RVhYmXJPv7ihK6il6nWoaTpvczSUKDRKUo69wMLL_IOe3LlkBy_uEfLBS2LoZFg7MO4EjPXAX54XoT2kOR5TRKB08MxlDrp6Q-kkCVwe91FgZ6GOgcHrJPQE89oUqJGkM70NibtuoLCPjod0tkH_IRCFiDfCX610_g3XEni4Q1JvyoHmwVqPhlmkzd3ZN6a6ixhHh9rE6krDyVVQfkfzwhaVZkbIkscb0IC_xbA38895WdTpu0TAw:AbufrsIlMmAs_GPBalRbDRgWwkXe3rWQrKlMKvGVGw7nNdrhzdZoiR_d9hWojRf3Fcwxm4DpMfQAgK78ML7Q1Sc6ACaFy3Qdpe_8Pm6HWQCyUw6XQdPwdOWa4YgH75ijd7d9hVYzZoBIMEE_MnEnQjFAlM_BkJFH400A0JKhoxYF_iQJ",
        "9527fead35:0:d97abd1ff324270bdfdbb54c1fe6f000a2291fa973d0de5dc49db6662a341d0536f9cf94706a7f93fc6302e0f127c688db5d9256e295ae4f6f470066187506cb:1876fdaca528ba1b8343358ff7b5adb85c00da8c9a9f3dda49d432c9edb38554f9120264cecd5c832ea734d3ad94e75b3c7ef1b0f3a0c1b85e3fedfb13101f4f:1:AaBhstq_p3F072IIEs5Ipq2-pykNM2RW3ptHR2F1DEPTsDEdyoNz-q5-y1XAT4PtS84d1eUn8eCC2VegNxFvqQTrAAO2N8Ql53IfWHYYrJttQHksXqynK_g6nKIemaN8D-yRiHJK7Z1Unejxrj0TvAkHkoDFzPP7ZxUEhFRdugQwn-zwAGraqP2tvm_y0lrBmP8SaHcAdrAF-ntyeazf_2InbvTTSVnKX3hub1RWqOyvfc5QtKiy-lL8AzCjAuwW-i1Me5nkAD7gDN9mY0W--1wRXJ8ohTy0g8-5eSMsjziFmVa0UyAfl4XPhGd3fHMlxvFK-4wsCSLylmyLY7gw2AGi7pALTKsgAVLImpDXwFYpUC6Xt5EHZQ0nTlfazPRckDvN1ffV0Ss6I4n-bcC_RpqPTXrogZOkRnsLRdH_WsLyBlXWhqtH7q7TAejLt4ODR4HU82arOPi61w6LQSMejOgHoqBuouGe3rnfceR7HSiRSRG3gJpmC_aYKmu3-bWD4QFynZEb7qWNo2lzqjt9-RPcpSmws82m1maD7V9vji0zAEL8nWPku8wCke8dvP3iacHc7BYE98aqAKvvbXzaXMpVVLpMSO5ybHTEBQ:AKwLNN0VOYQcF6YnSMzLQKEn5tFZbu7Kd2AzfPRhUe0OQK0OMFB_RHeHeQHy94oX5_n9tA_mhPlmouAPoLqK8RaSAAmEc79yM13NnlGthTdqDoAXHAeGq8Yv49q4514wf6vSJol0lBoPi6SKWqt2Vv4oyQU-k3eJcpglK2yZXzip6UWZ",
        "33ad7165da:0:b2f992c9580c2dc90eb1edd5a42fdf4ee3fa561e5ce3f357f9375ef8210e7760e749551094e1916781f84fb2820f96bf055833f70897bc0af98ed50e5ae8c593:e895125598f26e511ea3b27d8e4b520f285167fa83acf5dd34ac63054ced2467a38bf218b4d0770c677e8f274dd9630bca4c31abad2a4d194b4ac032590efeb0:1:ABO2kwfuQPVeWcw5ZHuU4k3UqkSrYU4nuSIsn-dNyoIyzzsufLL-D05irQo1Y5PY9Qr25WtyeEJ5YMtW1LGnNUL-ADLpGPd5aVW5PxrE-CzOl15JxwSSplebgRJzYRyhfPlkm5quq1BOrZubrcfAdbx3cYHol1n9Gw4_rjEMizQaVs-RAUpWMf1KDEh0p-r6mg57RpwlTc1A1r-U49ML7wCfwFuPrqARXsthuXUSKBBgCVpCh9WoWFKkIVhvwkquRonqR87CAF-w219Qe_PKpQCA8yyP3yo7Cmwky57O833K6TfE6t_qw85C2t2sNKEybRfxT_w-PJvWOs9C6GQ0njGjyUIkrBlBAF_Y7-BCTehWlzg_6YCnMCCOskzUq9zDJGl11ohILVD1tlnm5PFO-mO4LoHHdMBmpWy5_9U19FAi5KQ9x4XnrK2oAN6g8cdQzQjjbU6XGfsp2oEoLyjc65t39zv8B1cvzQWZmxd_MUHFDddDDa0G620ZKArwb8jySeS-iViDnZPgVmaRJhAH8354fIOqIX6e-owtXzWG_8Vc96RaNe-CU0j-mC8kVAj2Dt7_7texUw8OOEBXiwq1NdlpY_1k52dYIBQABQ:AaFHCsaZNHDG-Eqd5eR-on7Fsn9KlQQLDDU5OeLrbaTYn5VtqEr0p_a-_5tO1ZeFDEjTi6tCwalx7ivHO-O8HEOZAJIzR_JB_lA8fVYy5FSVwrLeEz1RoNwTESYLjRw-xeTiDVFjbC55fqvFZGnQIFxrf4uNRrFGNxLIHVbrsVgRed-v",
        "f5dcd74071:0:306845d6f24862ec7955d33e9bad44c0c4fcf63c49dd0113ed3857c779ad6372dcedd5d8e0beb96ee9b04a25d41c7a314db97f2e6f5f5a33ed01ddb38841dcdc:783ec289c4419c4aed75712695c4fd29b534d724a8aa99292c4f88471cf66e6d52aa01dba977a308b5b70a2b81c596e443abf302b8957c4d18e88b77bfd57d40:1:ASqV-crRLKQ_c595CW6vcmxtvglrwW8_EcnwNinO73yFKyiVKeJKTJIf2eXRWT2U7U1P01cdPLAp-2lY2MC3sYy2AHz_yeV1DRFLUK7WEXmBiS8SkJeVZ__WMqp-V6h2NAY-XYWwGNxp2AULb1OExTC2xsURAURs-6ce8vmWZriDgF7bAZicebp5ietT3Q4bK9lnrcedOoS-xyfwlpecLtMpmCRwh3E-GPNoViIVJxxaxhf2PYQvUuv3KRZNLF2ksQXr4kIAAIQh6LkFlhZPuvSzvdKf_u6ZQn1p3ibspwY5NH9sfdjYnvRAuefPyzJJVcFyUUpn9zm5P9-ELHdFisBtbc69LMHOALa1lr-9pUW-eHcpdkQYhAyRn-XuYuJ4wpEzHCV643TSTe4ZWbaIUodtkCkwQbJcemuwaFaymImqNlBLVndjzZd2AJcl3rdEZ29zGgDV65VzwaExfUSGGDixId3D1jDyfC-FKIexy9stELiqZD84p0ctvyoyJ0KMgMDeSaPl7PtbeFHlPSH6IVOFzzlYgWXb4WXcBMMmRfGfCAhCSUzDgwbl5SAGZ_sv-8mFEIlvu7h6nUzDqBQSI3TWIg5VE2oa5Cy9AQ:AEjdCO1fWvQg1gX9MK38vIlLjUu9oLRfKXGDQv5W30Q-bc94_p55F8osecwK-nr10gwHb8eO2YDgg0jzcXY6F4vyAaqK5Anp2Y6paC0pHc5cE_P1fNvEbNJ2MWTN2t0QR5-VhUvCv-sgN7wk2pQ96TARuAiDUvGpwtOcMKE3RdApeBiX",
        "5db8a63b4c:0:51215b9fb97db9ac0bfaf50f8c7650e7ab8fbf9ff600c2e0173e35df37560553bbf6c23d42d8e99f755c61e1814e1751403a6c5241012a3b37fcee9540a25b0b:e0de051dac79182d0f816d6550c5d8dfb768df7e6259fbe3654c6950661eac654f75098eda2d20af0b680902ed600393aec24a8347912e0f0e4b6c742616a728:1:Aax-lby-vmb6YPkvXwZPqdmnC72yyAXd-_o2n5yP7wr0dgxKefQB8vtP0UlPyPG9ugpAhOyNp6FRMZoQFeOaTp_8AJm3pexIWwHaJwdoddgSxlGp2hPTwQ78m9ZBGfQJ-veFcNd1ZK4z1F3-OFLBRx9HRjt5ZjHJm-vaxhPa_UqsOAHwAFdnugAYGsvCnXLlXW3WYFZhbRFS4vjtVvEYN09te3ug7TaFkONCzFHbCuDIu9puxyrW6zR8cFdzDAe0hJhKIyuHALth_MikQCoPUipiArHPLKZLk1DGHbT1GK8k_d2yhJxOuiYKGGgVGldt-U6qVlW_kmIMgrbpptRIH1_ffwifR-0XAdO2FH6Vf0SITn_sIy-nuIgIFrphPt1nGlhfuArfhmUszfstDts321XEl6SBq8GCd_YdbXtbEcM3RxyIXwo0KxbKAcQop9qDkseiWEcCVl3F7HK6m2xqRK9KoMlGcQAfzNaUToKcYFLky8TL7YykBLbuxgSVTxlaMmyRqRKm0njTE4c_679kmtijGFF0kwNk7li7f1zyojeKtWxD1d1S2ssrsuoeUd-g_mxG1Ai0P-gvCkyWBn9v-MwhfPE-ai_53ZGuBA:AG6Z5ylc_9j-fJtpepvBGrkMKwaPOl67KUFfhLRNZOnbj5ZMCunUuTcjHaX7vhqMsDA9GjlFtdpzOp49xOZUGqayAN3rzvdolSixF3U1ozeUjbZppoAW2TTyp5Cc63C8ukbM8WIWK-kHuLF7pugLUU34Y68-0XwBsiRQSiHfepDO4pYh",
        "44a9db2f0b:0:7f678a7cef792ae7986dc7b0138d3116052daa2d18eed4f5cdf7498279629676d200a3f0c233f40c3f7893036916251a1c539a8d39dd2ec99e46a99d75cbab49:90598d4d0562d82451b7cf01b7b9c4a99ca00e366dff437a63c9d84fd78cc164a6b2ebb9edfa62b6c654793046b81f93b3216368d1949265e80c05196e9e1e3e:1:AWEF-eBvlzDv0BwsFHfQuW0fm3WkvOOEAj3Yei9GFauHqrhPVY09s65Qz2enX7MK5OEAhK-IKE7rdqxiRhzS5WiYAA4GJnvTng__VgGGaeodk2RpxUYYpXglss8cJC3Lp9q3UhnQ2N2SYOCentrAmiFqYfxYvURhEOxWRIYQUPF6NE7mATAcfvmp_nJPex_4qAOVLi1za1MLFuq7z_ywOb2qUt3vqikmRO7SAsNrk1kAGeeizR22TQYwEoyoQyjPuDho5Js7AIy0TyIigonxIoSGzLDxfrI2S5_9s6aR_bfq2F7alA1f_uB8AXSwLKGWfn822B0PEPiHrL6ucv7aEEuYDXKrvx4tASmg0UusyunTPVH1_xs5YbUipET_nTcqmtxV_RWZAqpBW0J8Yrmyo8f-Gktzek-NbHsx9TXdJ_WjvkjCsgkxilIVAY3XwBL5aDafppv2ZQv1b-0dhUi53bRhon6BmMByoFRglzm6Qk-tuyPt6klntL8H_XcDmybnhl60NuvJCmPnUUCxCzJ-Lb_xR-onNjAY-5qr35cbrGC_FP5HGnt8YtGrO21UcFDBrGEnYv2KgUv7hie1d3wP7Gi0OQkZRx75Fm-rBg:ARzgLo5WQVIpVfW2JwkYQ0jBsFht0-GR8RfeV5zR3mzgg7SycFenA9j362E-U9JYstKjrYuBSHHB3ka1EzBZgI6TAIgarHok6NHXkr-kS7i0WBQjSNV6AeFf9hoYSYaWWertCT0W4QcrJ_7DD6zkRE4BHh2udYrsFnNEWal5SQQ-zRH1",
        "b81c8c5f43:0:bd35767e61047d68424a87c5f543f710a9f946115154e5424dbe44dd7703e84ea8a51b7380e6b77499fe11a8b1c258cd11b3343baec5b3bf1a32514baaa6f424:a850d4a6426e9e2dc5cbd0275192743ad10b9edb17e3529e4971f9598a42bb5577440bb0ef31ce541d9aaddbfa91b25c5fea032b55b36fa8923a557bef9f25d7:1:AIDKMnNB9o1kj1H75oNPVcqUW3lsNkH-vEFThnrfwU7pFd4fytOvmlFyMxexr-u24UgVJH_kcAbgvR_7OHSm-AUOAHmW8Y4KgVXIA4f7Ll9t4kwbtkCnqYp-MYN1sMzGWPeafao2WS5HYoG0fgWzILT61Gpmam11bRrzFYUft6_dBt0lAV8fb0CVoxx4-O8V4SGEwUWiE0WEuIOfVG6LKw3alqcHXEUHOPnrikifrMeNCTxhnAXvAaQZ_QMUH5fcHLLM-6ioAGsF_rQRKQxTjDoCF4GdALNzdT4Nkf1cKTFN5WGcpOBr3Oy2bkriO0X0oIyTCzsX34dqRQJwZ6x67kOuRimL6bqZAHJrq7pOrlAKvpjnoiZo561gToaKeY1ynPA2rJuKlVEZ7__hYRCopC2SiKGankzmdI5mvLcEwRLzPTiCMZggebDkAfrN4aD07buM-FlgXLDT01R28ndZ0qRb7cdRD-U6NJTaNCQfE2Jol4rCrOysXZLgRuu9p6FZFvJ3RqsEa4mkSWR6E8kbfeYIFkuMw5yrgWEU3MOzA5zssxdv27KWfHMyxIRoBi0eqwY95G-6GYKp5XEfV4KwiSE3wXyX-4RlOO6BCg:AY9wg5yHH5AvxBG5wN1CORN4S7zeBHUohMXcEGRTier-OMMLCfFH2mEj-Cbse8hKSnT0dQaqo4ukc1Eb37RYuhNQAX7LhTnAKwt3HqCS-hUSpgAhG0UwXVfG5EtPvfEpvBqLnf5FXfD7f6IsNDVepwElD3bvy8EKBSFL6aUQrid3Ogln",
        "935c76c5e7:0:a80ff0e47ed18978efee5b01f06c605a2762a05b0a539f93575453011a3d0766b38ed43777ce59cc577d33bfac00052040c33c24577f2ccb74e1393e1c9ee279:f0c3327bb9da5bce4488661ebe1120b62320c4c5a5bac4e981802c9f1bc65f7902d3b99c67f11dc8bcf788f9d8f291cbdc10aa826ae345d138b5a971ae3f0746:1:AW16A1MtEXyCisH0xmSobOojtjxAODKw1wYAOnCbW0DVXNgAfz7xyMB-d6YIeZ9VqsURLYPpQdRgxdDBXipeyNpJAJOvgyL6IdOnQtgXHYxRqUFsiwIfkVYquzGkRMEyqjcQyErmbNstvZr2foVbxKA0UyLhOnkmoOTfbo2wI00v3MXrAUW1-7B4qZLhmPcI4eT__M96t4AUyot4JC5dqw33K-CgGAuOffIGz0FLSOj68R0nCEYHyPF3eaFRfPdcjqFJyec7AELlofXMLqk-AcljygjFsNggGZYwYb0iFGIYcb3ZbBhnkymzSITHIn20xhG3B4E-a7CrBfSpx20xSObdlDgfzz5CAV0-thYInQClIvst1VxfmQArE0OEWiZsf8Fak5EiWy6mIpww5L5ZHLBdt_fqET5ULZUmI0JFp2NWYhzgFo5HE9NQAPFhm2Po1XmEyFVMDLWFgGVRhbOInrAtQQ7hSGgts8a8aZk7jWl1oUFLj7sXhvyIKos_opF1S6phL_SY8yF3RKgnkhRxi-FD4zAH4bwVf8OVqeSvN1qfZAn6EIJOH2Kfhv2stUlS9hCM4SfJIfSnjtsSMJ3Ph2Ep_e2THDH_rUFnCg:ADDuA2ZCjV-1N-1BCbYFtSgV5c5YfvLR6HvcBo3XIjOfYpndxJBNw63-WG9NQbgNSCpfDRPbBJtP53CqDlAAsjsHAbkiQ37sl0H8JxuSgqIxy3aoEcRlVLWy28PAcSke0Xl1IgaRsqWRObSy1bmFdsJBpxbW45xwxm6w4glNtM2ZHzKI"
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
            let sig = id.sign(&[1, 2, 3, 4, 5], IDENTITY_ALGORITHM_ALL).unwrap();
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
            let sig = id.sign(&[1, 2, 3, 4, 5], IDENTITY_ALGORITHM_ALL).unwrap();
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
