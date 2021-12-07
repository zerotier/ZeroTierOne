/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::ptr::{slice_from_raw_parts, slice_from_raw_parts_mut};
use zerotier_core_crypto::c25519::{C25519_PUBLIC_KEY_SIZE, C25519KeyPair, ED25519_PUBLIC_KEY_SIZE, Ed25519KeyPair};
use zerotier_core_crypto::p521::{P521_ECDSA_SIGNATURE_SIZE, P521_PUBLIC_KEY_SIZE, P521KeyPair};
use zerotier_core_crypto::salsa::Salsa;

// Work function used to derive address from keys.
fn zt_frankenhash(digest: &mut [u8; 64], genmem_ptr: *mut u8) {
    let (genmem, genmem_alias_hack) = unsafe { (&mut *slice_from_raw_parts_mut(genmem_ptr, V0_POW_MEMORY), &*slice_from_raw_parts(genmem_ptr, V0_POW_MEMORY)) };
    let genmem_u64_ptr = genmem_ptr.cast::<u64>();

    let mut s20 = Salsa::new(&digest[0..32], &digest[32..40], false).unwrap();

    s20.crypt(&crate::util::ZEROES[0..64], &mut genmem[0..64]);
    let mut i: usize = 64;
    while i < V0_POW_MEMORY {
        let ii = i + 64;
        s20.crypt(&genmem_alias_hack[(i - 64)..i], &mut genmem[i..ii]);
        i = ii;
    }

    i = 0;
    while i < (V0_POW_MEMORY / 8) {
        unsafe {
            let idx1 = (((*genmem_u64_ptr.offset(i as isize)).to_be() % 8) * 8) as usize;
            let idx2 = ((*genmem_u64_ptr.offset((i + 1) as isize)).to_be() % (V0_POW_MEMORY as u64 / 8)) as usize;
            let genmem_u64_at_idx2_ptr = genmem_u64_ptr.offset(idx2 as isize);
            let tmp = *genmem_u64_at_idx2_ptr;
            let digest_u64_ptr = digest.as_mut_ptr().offset(idx1 as isize).cast::<u64>();
            *genmem_u64_at_idx2_ptr = *digest_u64_ptr;
            *digest_u64_ptr = tmp;
        }
        s20.crypt_in_place(digest);
        i += 2;
    }
}

struct P521Secret {
    ecdh: P521KeyPair,
    ecdsa: P521KeyPair,
}

struct P521Public {
    ecdh: [u8; P521_PUBLIC_KEY_SIZE],
    ecdsa: [u8; P521_PUBLIC_KEY_SIZE],
    self_signature: [u8; P521_ECDSA_SIGNATURE_SIZE]
}

pub struct IdentitySecrets {
    c25519: C25519KeyPair,
    ed25519: Ed25519KeyPair,
    p521: Option<P521Secret>,
}

pub struct Identity {
    c25519: [u8; C25519_PUBLIC_KEY_SIZE],
    ed25519: [u8; ED25519_PUBLIC_KEY_SIZE],
    p521: Option<P521Public>,
}

/*

use std::alloc::{alloc, dealloc, Layout};
use std::cmp::Ordering;
use std::convert::TryInto;
use std::hash::{Hash, Hasher};
use std::io::Write;
use std::ptr::{slice_from_raw_parts, slice_from_raw_parts_mut};
use std::str::FromStr;

use zerotier_core_crypto::balloon;
use zerotier_core_crypto::c25519::*;
use zerotier_core_crypto::hash::*;
use zerotier_core_crypto::p521::*;
use zerotier_core_crypto::salsa::Salsa;
use zerotier_core_crypto::secret::Secret;

use crate::error::InvalidFormatError;
use crate::vl1::Address;
use crate::vl1::protocol::{PACKET_SIZE_MAX, ADDRESS_SIZE};
use crate::util::buffer::Buffer;

pub const IDENTITY_TYPE_0_SIGNATURE_SIZE: usize = 96;
pub const IDENTITY_TYPE_1_SIGNATURE_SIZE: usize = P521_ECDSA_SIGNATURE_SIZE + ED25519_SIGNATURE_SIZE;

const V0_POW_MEMORY: usize = 2097152;
const V0_POW_THRESHOLD: u8 = 17;
const V1_POW_THRESHOLD: u8 = 5;
const V1_BALLOON_SPACE_COST: usize = 262144;
const V1_BALLOON_TIME_COST: usize = 3;
const V1_BALLOON_DELTA: usize = 3;

const V1_PUBLIC_KEYS_SIZE: usize = C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE;
const V1_PUBLIC_KEYS_SIGNATURE_AND_POW_SIZE: usize = V1_PUBLIC_KEYS_SIZE + P521_ECDSA_SIGNATURE_SIZE + SHA384_HASH_SIZE;
const V1_SECRET_KEYS_SIZE: usize = C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE;

fn concat_v1_public_keys(c25519: &[u8], ed25519: &[u8], p521_ecdh: &[u8], p521_ecdsa: &[u8]) -> [u8; V1_PUBLIC_KEYS_SIZE] {
    let mut k = [0_u8; V1_PUBLIC_KEYS_SIZE];
    debug_assert!(c25519.len() <= C25519_PUBLIC_KEY_SIZE);
    debug_assert!(ed25519.len() <= ED25519_PUBLIC_KEY_SIZE);
    debug_assert!(p521_ecdh.len() <= P521_PUBLIC_KEY_SIZE);
    debug_assert!(p521_ecdsa.len() <= P521_PUBLIC_KEY_SIZE);
    k[(C25519_PUBLIC_KEY_SIZE - c25519.len())..C25519_PUBLIC_KEY_SIZE].copy_from_slice(c25519);
    k[(C25519_PUBLIC_KEY_SIZE + (ED25519_PUBLIC_KEY_SIZE - ed25519.len()))..(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE)].copy_from_slice(ed25519);
    k[(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + (P521_PUBLIC_KEY_SIZE - p521_ecdh.len()))..(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE)].copy_from_slice(p521_ecdh);
    k[(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + (P521_PUBLIC_KEY_SIZE - p521_ecdsa.len()))..(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE)].copy_from_slice(p521_ecdsa);
    k
}

fn concat_v1_public_all(c25519: &[u8], ed25519: &[u8], p521_ecdh: &[u8], p521_ecdsa: &[u8], signature: &[u8], balloon_hash_digest: &[u8]) -> [u8; V1_PUBLIC_KEYS_SIGNATURE_AND_POW_SIZE] {
    let mut k = [0_u8; V1_PUBLIC_KEYS_SIGNATURE_AND_POW_SIZE];
    debug_assert!(c25519.len() <= C25519_PUBLIC_KEY_SIZE);
    debug_assert!(ed25519.len() <= ED25519_PUBLIC_KEY_SIZE);
    debug_assert!(p521_ecdh.len() <= P521_PUBLIC_KEY_SIZE);
    debug_assert!(p521_ecdsa.len() <= P521_PUBLIC_KEY_SIZE);
    debug_assert!(signature.len() <= P521_ECDSA_SIGNATURE_SIZE);
    debug_assert_eq!(balloon_hash_digest.len(), SHA384_HASH_SIZE);
    k[(C25519_PUBLIC_KEY_SIZE - c25519.len())..C25519_PUBLIC_KEY_SIZE].copy_from_slice(c25519);
    k[(C25519_PUBLIC_KEY_SIZE + (ED25519_PUBLIC_KEY_SIZE - ed25519.len()))..(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE)].copy_from_slice(ed25519);
    k[(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + (P521_PUBLIC_KEY_SIZE - p521_ecdh.len()))..(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE)].copy_from_slice(p521_ecdh);
    k[(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + (P521_PUBLIC_KEY_SIZE - p521_ecdsa.len()))..(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE)].copy_from_slice(p521_ecdsa);
    k[(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + (P521_ECDSA_SIGNATURE_SIZE - signature.len()))..(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_ECDSA_SIGNATURE_SIZE)].copy_from_slice(signature);
    k[(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_ECDSA_SIGNATURE_SIZE)..].copy_from_slice(balloon_hash_digest);
    k
}

fn concat_v1_secret_keys(c25519: &[u8], ed25519: &[u8], p521_ecdh: &[u8], p521_ecdsa: &[u8]) -> [u8; V1_SECRET_KEYS_SIZE] {
    let mut k = [0_u8; V1_SECRET_KEYS_SIZE];
    debug_assert!(c25519.len() <= C25519_SECRET_KEY_SIZE);
    debug_assert!(ed25519.len() <= ED25519_SECRET_KEY_SIZE);
    debug_assert!(p521_ecdh.len() <= P521_SECRET_KEY_SIZE);
    debug_assert!(p521_ecdsa.len() <= P521_SECRET_KEY_SIZE);
    k[(C25519_SECRET_KEY_SIZE - c25519.len())..C25519_SECRET_KEY_SIZE].copy_from_slice(c25519);
    k[(C25519_SECRET_KEY_SIZE + (ED25519_SECRET_KEY_SIZE - ed25519.len()))..(C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE)].copy_from_slice(ed25519);
    k[(C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE + (P521_SECRET_KEY_SIZE - p521_ecdh.len()))..(C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE)].copy_from_slice(p521_ecdh);
    k[(C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE + (P521_SECRET_KEY_SIZE - p521_ecdsa.len()))..(C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE)].copy_from_slice(p521_ecdsa);
    k
}

#[derive(Copy, Clone)]
#[repr(u8)]
pub enum IdentityType {
    /// Curve25519 / Ed25519 identity (type 0)
    C25519 = 0,
    /// Dual NIST P-521 ECDH / ECDSA + Curve25519 / Ed25519 (type 1)
    P521 = 1,
}

struct IdentitySecrets {
    c25519: C25519KeyPair,
    ed25519: Ed25519KeyPair,
    v1: Option<(P521KeyPair, P521KeyPair)>, // ecdh key, ecdsa key
}

pub struct Identity {
    address: Address,
    c25519: [u8; C25519_PUBLIC_KEY_SIZE],
    ed25519: [u8; ED25519_PUBLIC_KEY_SIZE],
    v1: Option<(P521PublicKey, P521PublicKey, [u8; P521_ECDSA_SIGNATURE_SIZE], [u8; SHA384_HASH_SIZE])>,
    secrets: Option<IdentitySecrets>,
}

/// Compute result from the bespoke "frankenhash" from the old V0 work function.
/// The supplied genmem_ptr must be of size V0_IDENTITY_GEN_MEMORY and aligned to an 8-byte boundary.
fn v0_frankenhash(digest: &mut [u8; 64], genmem_ptr: *mut u8) {
    let (genmem, genmem_alias_hack) = unsafe { (&mut *slice_from_raw_parts_mut(genmem_ptr, V0_POW_MEMORY), &*slice_from_raw_parts(genmem_ptr, V0_POW_MEMORY)) };
    let genmem_u64_ptr = genmem_ptr.cast::<u64>();

    let mut s20 = Salsa::new(&digest[0..32], &digest[32..40], false).unwrap();

    s20.crypt(&crate::util::ZEROES[0..64], &mut genmem[0..64]);
    let mut i: usize = 64;
    while i < V0_POW_MEMORY {
        let ii = i + 64;
        s20.crypt(&genmem_alias_hack[(i - 64)..i], &mut genmem[i..ii]);
        i = ii;
    }

    i = 0;
    while i < (V0_POW_MEMORY / 8) {
        unsafe {
            let idx1 = (((*genmem_u64_ptr.offset(i as isize)).to_be() % 8) * 8) as usize;
            let idx2 = ((*genmem_u64_ptr.offset((i + 1) as isize)).to_be() % (V0_POW_MEMORY as u64 / 8)) as usize;
            let genmem_u64_at_idx2_ptr = genmem_u64_ptr.offset(idx2 as isize);
            let tmp = *genmem_u64_at_idx2_ptr;
            let digest_u64_ptr = digest.as_mut_ptr().offset(idx1 as isize).cast::<u64>();
            *genmem_u64_at_idx2_ptr = *digest_u64_ptr;
            *digest_u64_ptr = tmp;
        }
        s20.crypt_in_place(digest);
        i += 2;
    }
}

impl Identity {
    fn generate_c25519() -> Identity {
        let genmem_layout = Layout::from_size_align(V0_POW_MEMORY, 8).unwrap();
        let genmem_ptr = unsafe { alloc(genmem_layout) };
        if genmem_ptr.is_null() {
            panic!("unable to allocate memory for V0 identity generation");
        }

        let ed25519 = Ed25519KeyPair::generate(false);
        let ed25519_pub_bytes = ed25519.public_bytes();
        let mut sha = SHA512::new();
        loop {
            let c25519 = C25519KeyPair::generate(false);
            let c25519_pub_bytes = c25519.public_bytes();

            sha.update(&c25519_pub_bytes);
            sha.update(&ed25519_pub_bytes);
            let mut digest = sha.finish();

            v0_frankenhash(&mut digest, genmem_ptr);
            if digest[0] < V0_POW_THRESHOLD {
                let addr = Address::from_bytes(&digest[59..64]);
                if addr.is_some() {
                    unsafe { dealloc(genmem_ptr, genmem_layout) };
                    return Identity {
                        address: addr.unwrap(),
                        c25519: c25519_pub_bytes,
                        ed25519: ed25519_pub_bytes,
                        v1: None,
                        secrets: Some(IdentitySecrets {
                            c25519,
                            ed25519,
                            v1: None,
                        }),
                    };
                }
            }

            sha.reset();
        }
    }

    fn generate_p521() -> Identity {
        let c25519 = C25519KeyPair::generate(false);
        let ed25519 = Ed25519KeyPair::generate(false);
        let p521_ecdh = P521KeyPair::generate(false).unwrap();
        let p521_ecdsa = P521KeyPair::generate(false).unwrap();
        let c25519_pub_bytes = c25519.public_bytes();
        let ed25519_pub_bytes = ed25519.public_bytes();
        let signing_buf = concat_v1_public_keys(&c25519_pub_bytes, &ed25519_pub_bytes, p521_ecdh.public_key_bytes(), p521_ecdsa.public_key_bytes());
        loop {
            // ECDSA is a randomized signature algorithm, so each signature will be different.
            let sig = p521_ecdsa.sign(&signing_buf).unwrap();
            let bh = balloon::zt_variant_hash::<{ V1_BALLOON_SPACE_COST }, { V1_BALLOON_TIME_COST }, { V1_BALLOON_DELTA }>(&sig);
            if bh[0] < V1_POW_THRESHOLD {
                let addr = Address::from_bytes(&bh[43..48]);
                if addr.is_some() {
                    let p521_ecdh_pub = p521_ecdh.public_key().clone();
                    let p521_ecdsa_pub = p521_ecdsa.public_key().clone();
                    return Identity {
                        address: addr.unwrap(),
                        c25519: c25519_pub_bytes,
                        ed25519: ed25519_pub_bytes,
                        v1: Some((p521_ecdh_pub, p521_ecdsa_pub, sig, bh)),
                        secrets: Some(IdentitySecrets {
                            c25519,
                            ed25519,
                            v1: Some((p521_ecdh, p521_ecdsa)),
                        }),
                    };
                }
            }
        }
    }

    /// Generate a new identity.
    ///
    /// This is time consuming due to the one-time anti-collision proof of work required
    /// to generate an address corresponding with a set of identity keys. V0 identities
    /// take tens to hundreds of milliseconds on a typical 2020 system, while V1 identites
    /// take about 500ms. Generation can take a lot longer on low power devices, but only
    /// has to be done once.
    pub fn generate(id_type: IdentityType) -> Identity {
        match id_type {
            IdentityType::C25519 => Self::generate_c25519(),
            IdentityType::P521 => Self::generate_p521()
        }
    }

    /// Get this identity's 40-bit address.
    #[inline(always)]
    pub fn address(&self) -> Address { self.address }

    /// Locally validate this identity.
    ///
    /// This can take a few milliseconds, especially on slower systems. V0 identities are slower
    /// to fully validate than V1 identities.
    pub fn locally_validate(&self) -> bool {
        if self.v1.is_none() {
            let genmem_layout = Layout::from_size_align(V0_POW_MEMORY, 8).unwrap();
            let genmem_ptr = unsafe { alloc(genmem_layout) };
            if !genmem_ptr.is_null() {
                let mut sha = SHA512::new();
                sha.update(&self.c25519);
                sha.update(&self.ed25519);
                let mut digest = sha.finish();
                v0_frankenhash(&mut digest, genmem_ptr);
                unsafe { dealloc(genmem_ptr, genmem_layout) };
                (digest[0] < V0_POW_THRESHOLD) && Address::from_bytes(&digest[59..64]).unwrap().eq(&self.address)
            } else {
                false
            }
        } else {
            let p521 = self.v1.as_ref().unwrap();
            let signing_buf = concat_v1_public_keys(&self.c25519, &self.ed25519, (*p521).0.public_key_bytes(), (*p521).1.public_key_bytes());
            if (*p521).1.verify(&signing_buf, &(*p521).2) {
                let bh = balloon::zt_variant_hash::<{ V1_BALLOON_SPACE_COST }, { V1_BALLOON_TIME_COST }, { V1_BALLOON_DELTA }>(&(*p521).2);
                (bh[0] < V1_POW_THRESHOLD) && bh.eq(&(*p521).3) && Address::from_bytes(&bh[43..48]).unwrap().eq(&self.address)
            } else {
                false
            }
        }
    }

    /// Execute ECDH key agreement and return SHA384(shared secret).
    ///
    /// If both keys are type 1, key agreement is done with NIST P-521. Otherwise it's done
    /// with Curve25519. None is returned if there is an error such as this identity missing
    /// its secrets or a key being invalid.
    pub fn agree(&self, other_identity: &Identity) -> Option<Secret<48>> {
        self.secrets.as_ref().map_or(None, |secrets| {
            let c25519_secret = || Secret::<48>(SHA384::hash(&secrets.c25519.agree(&other_identity.c25519).as_ref()));
            secrets.v1.as_ref().map_or_else(|| Some(c25519_secret()), |p521_secret| {
                other_identity.v1.as_ref().map_or_else(|| Some(c25519_secret()), |other_p521_public| {
                    p521_secret.0.agree(&other_p521_public.0).map_or(None, |p521_secret| {
                        //
                        // For NIST P-521 key agreement, we use a single step key derivation function to derive
                        // the final shared secret using the C25519 shared secret as a "salt." This should be
                        // FIPS-compliant as per section 8.2 of:
                        //
                        // https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-56Cr2.pdf
                        //
                        // This is also stated in the following FAQ, which though it pertains to post-quantum
                        // algorithms states that non-FIPS derived shared secrets can be used as salts in key
                        // derivation from a FIPS-compliant algorithm derived shared secret:
                        //
                        // https://csrc.nist.gov/Projects/post-quantum-cryptography/faqs
                        //
                        // Hashing the C25519 secret with the P521 secret results in a secret that is as strong
                        // as the stronger of the two algorithms. This should make the FIPS people happy and the
                        // people who are paranoid about NIST curves happy.
                        //
                        Some(Secret(SHA384::hmac(c25519_secret().as_ref(), p521_secret.as_ref())))
                    })
                })
            })
        })
    }

    /// Sign this message with this identity.
    pub fn sign(&self, msg: &[u8]) -> Option<Vec<u8>> {
        self.secrets.as_ref().map_or(None, |secrets| {
            let c25519_sig = secrets.ed25519.sign_zt(msg);
            secrets.v1.as_ref().map_or_else(|| Some(c25519_sig.to_vec()), |p521_secret| p521_secret.1.sign(msg).map_or(None, |p521_sig| {
                //
                // For type 1 identity signatures we sign with both algorithms and append the Ed25519
                // signature to the NIST P-521 signature. The Ed25519 signature is only checked if the
                // P-521 signature validates. Note that we only append the first 64 bytes of sign_zt()
                // output. For legacy reasons type 0 signatures include the first 32 bytes of the message
                // hash after the signature, but this is not required and isn't included here.
                //
                // This should once again make both the FIPS people and the people paranoid about NIST
                // curves happy.
                //
                let mut p521_sig = p521_sig.to_vec();
                let _ = p521_sig.write_all(&c25519_sig[0..64]);
                Some(p521_sig)
            }))
        })
    }

    /// Verify a signature.
    pub fn verify(&self, msg: &[u8], signature: &[u8]) -> bool {
        self.v1.as_ref().map_or_else(|| {
            zerotier_core_crypto::c25519::ed25519_verify(&self.ed25519, signature, msg)
        }, |p521| {
            signature.len() == IDENTITY_TYPE_1_SIGNATURE_SIZE && (*p521).1.verify(msg, &signature[0..P521_ECDSA_SIGNATURE_SIZE]) && zerotier_core_crypto::c25519::ed25519_verify(&self.ed25519, &signature[P521_ECDSA_SIGNATURE_SIZE..], msg)
        })
    }

    /// Get this identity's type.
    #[inline(always)]
    pub fn id_type(&self) -> IdentityType { if self.v1.is_some() { IdentityType::P521 } else { IdentityType::C25519 } }

    /// Returns true if this identity also holds its secret keys.
    #[inline(always)]
    pub fn has_secrets(&self) -> bool { self.secrets.is_some() }

    /// Erase secrets from this identity object, if present.
    pub fn forget_secrets(&mut self) { let _ = self.secrets.take(); }

    /// Append this in binary format to a buffer.
    pub fn marshal<const BL: usize>(&self, buf: &mut Buffer<BL>, include_private: bool) -> std::io::Result<()> {
        buf.append_bytes_fixed(&self.address.to_bytes())?;
        if self.v1.is_some() {
            let p521 = self.v1.as_ref().unwrap();
            buf.append_u8(1)?; // type 1
            buf.append_bytes_fixed(&self.c25519)?;
            buf.append_bytes_fixed(&self.ed25519)?;
            buf.append_bytes_fixed((*p521).0.public_key_bytes())?;
            buf.append_bytes_fixed((*p521).1.public_key_bytes())?;
            buf.append_bytes_fixed(&(*p521).2)?;
            buf.append_bytes_fixed(&(*p521).3)?;
            if include_private && self.secrets.is_some() {
                let secrets = self.secrets.as_ref().unwrap();
                if secrets.v1.is_some() {
                    let p521_secrets = secrets.v1.as_ref().unwrap();
                    buf.append_u8(V1_SECRET_KEYS_SIZE as u8)?;
                    buf.append_bytes_fixed(&secrets.c25519.secret_bytes().as_ref())?;
                    buf.append_bytes_fixed(&secrets.ed25519.secret_bytes().as_ref())?;
                    buf.append_bytes_fixed((*p521_secrets).0.secret_key_bytes().as_ref())?;
                    buf.append_bytes_fixed((*p521_secrets).1.secret_key_bytes().as_ref())?;
                }
            } else {
                buf.append_u8(0)?; // 0 secret bytes if not adding any
            }
        } else {
            buf.append_u8(0)?; // type 0
            buf.append_bytes_fixed(&self.c25519)?;
            buf.append_bytes_fixed(&self.ed25519)?;
            if include_private && self.secrets.is_some() {
                let secrets = self.secrets.as_ref().unwrap();
                buf.append_u8((C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE) as u8)?;
                buf.append_bytes_fixed(&secrets.c25519.secret_bytes().as_ref())?;
                buf.append_bytes_fixed(&secrets.ed25519.secret_bytes().as_ref())?;
            } else {
                buf.append_u8(0)?; // 0 secret bytes if not adding any
            }
        }
        Ok(())
    }

    /// Deserialize an Identity from a buffer.
    /// The supplied cursor is advanced.
    pub fn unmarshal<const BL: usize>(buf: &Buffer<BL>, cursor: &mut usize) -> std::io::Result<Identity> {
        let addr = Address::from_bytes(buf.read_bytes_fixed::<{ ADDRESS_SIZE }>(cursor)?);
        if addr.is_none() {
            return std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid address"));
        }
        let addr = addr.unwrap();

        let id_type = buf.read_u8(cursor)?;
        if id_type == IdentityType::C25519 as u8 {

            let c25519_public_bytes = buf.read_bytes_fixed::<{ C25519_PUBLIC_KEY_SIZE }>(cursor)?;
            let ed25519_public_bytes = buf.read_bytes_fixed::<{ ED25519_PUBLIC_KEY_SIZE }>(cursor)?;
            let secrets_len = buf.read_u8(cursor)?;
            if secrets_len == (C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE) as u8 {
                let c25519_secret_bytes = buf.read_bytes_fixed::<{ C25519_SECRET_KEY_SIZE }>(cursor)?;
                let ed25519_secret_bytes = buf.read_bytes_fixed::<{ ED25519_SECRET_KEY_SIZE }>(cursor)?;
                Ok(Identity {
                    address: addr,
                    c25519: c25519_public_bytes.clone(),
                    ed25519: ed25519_public_bytes.clone(),
                    v1: None,
                    secrets: Some(IdentitySecrets {
                        c25519: C25519KeyPair::from_bytes(c25519_public_bytes, c25519_secret_bytes).unwrap(),
                        ed25519: Ed25519KeyPair::from_bytes(ed25519_public_bytes, ed25519_secret_bytes).unwrap(),
                        v1: None,
                    }),
                })
            } else if secrets_len == 0 {
                Ok(Identity {
                    address: addr,
                    c25519: c25519_public_bytes.clone(),
                    ed25519: ed25519_public_bytes.clone(),
                    v1: None,
                    secrets: None,
                })
            } else {
                std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "unrecognized secret key length (type 0)"))
            }

        } else if id_type == IdentityType::P521 as u8 {

            let c25519_public_bytes = buf.read_bytes_fixed::<{ C25519_PUBLIC_KEY_SIZE }>(cursor)?;
            let ed25519_public_bytes = buf.read_bytes_fixed::<{ ED25519_PUBLIC_KEY_SIZE }>(cursor)?;
            let p521_ecdh_public_bytes = buf.read_bytes_fixed::<{ P521_PUBLIC_KEY_SIZE }>(cursor)?;
            let p521_ecdsa_public_bytes = buf.read_bytes_fixed::<{ P521_PUBLIC_KEY_SIZE }>(cursor)?;
            let p521_signature = buf.read_bytes_fixed::<{ P521_ECDSA_SIGNATURE_SIZE }>(cursor)?;
            let bh_digest = buf.read_bytes_fixed::<{ SHA384_HASH_SIZE }>(cursor)?;
            let secrets_len = buf.read_u8(cursor)?;
            if secrets_len == (C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE) as u8 {
                let c25519_secret_bytes = buf.read_bytes_fixed::<{ C25519_SECRET_KEY_SIZE }>(cursor)?;
                let ed25519_secret_bytes = buf.read_bytes_fixed::<{ ED25519_SECRET_KEY_SIZE }>(cursor)?;
                let p521_ecdh_secret_bytes = buf.read_bytes_fixed::<{ P521_SECRET_KEY_SIZE }>(cursor)?;
                let p521_ecdsa_secret_bytes = buf.read_bytes_fixed::<{ P521_SECRET_KEY_SIZE }>(cursor)?;
                Ok(Identity {
                    address: addr,
                    c25519: c25519_public_bytes.clone(),
                    ed25519: ed25519_public_bytes.clone(),
                    v1: Some((P521PublicKey::from_bytes(p521_ecdh_public_bytes).unwrap(), P521PublicKey::from_bytes(p521_ecdsa_public_bytes).unwrap(), p521_signature.clone(), bh_digest.clone())),
                    secrets: Some(IdentitySecrets {
                        c25519: C25519KeyPair::from_bytes(c25519_public_bytes, c25519_secret_bytes).unwrap(),
                        ed25519: Ed25519KeyPair::from_bytes(ed25519_public_bytes, ed25519_secret_bytes).unwrap(),
                        v1: Some((P521KeyPair::from_bytes(p521_ecdh_public_bytes, p521_ecdh_secret_bytes).unwrap(), P521KeyPair::from_bytes(p521_ecdsa_public_bytes, p521_ecdsa_secret_bytes).unwrap())),
                    }),
                })
            } else if secrets_len == 0 {
                Ok(Identity {
                    address: addr,
                    c25519: c25519_public_bytes.clone(),
                    ed25519: ed25519_public_bytes.clone(),
                    v1: Some((P521PublicKey::from_bytes(p521_ecdh_public_bytes).unwrap(), P521PublicKey::from_bytes(p521_ecdsa_public_bytes).unwrap(), p521_signature.clone(), bh_digest.clone())),
                    secrets: None,
                })
            } else {
                std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid secret key length (type 1)"))
            }

        } else {
            std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "unrecognized identity type"))
        }
    }

    /// Get this identity in byte array format.
    pub fn marshal_to_bytes(&self, include_private: bool) -> Vec<u8> {
        let mut buf: Buffer<{ PACKET_SIZE_MAX }> = Buffer::new();
        self.marshal(&mut buf, include_private).expect("overflow");
        buf.as_bytes().to_vec()
    }

    /// Unmarshal an identity from a byte slice.
    /// On success the identity and the number of bytes actually read from the slice are returned.
    pub fn unmarshal_from_bytes(bytes: &[u8]) -> std::io::Result<(Identity, usize)> {
        let mut cursor: usize = 0;
        let id = Self::unmarshal(&Buffer::<2048>::from_bytes(bytes)?, &mut cursor)?;
        Ok((id, cursor))
    }

    /// Get this identity in string format, including its secret keys.
    pub fn to_secret_string(&self) -> String {
        self.secrets.as_ref().map_or_else(|| self.to_string(), |secrets| {
            secrets.v1.as_ref().map_or_else(|| {
                format!("{}:{}{}", self.to_string(), crate::util::hex::to_string(secrets.c25519.public_bytes().as_ref()), crate::util::hex::to_string(secrets.ed25519.secret_bytes().as_ref()))
            }, |p521_secret| {
                let secrets_concat = concat_v1_secret_keys(&secrets.c25519.secret_bytes().0, &secrets.ed25519.secret_bytes().0, &p521_secret.0.secret_key_bytes().0, &p521_secret.1.secret_key_bytes().0);
                format!("{}:{}", self.to_string(), base64::encode_config(secrets_concat, base64::URL_SAFE_NO_PAD))
            })
        })
    }
}

impl ToString for Identity {
    fn to_string(&self) -> String {
        self.v1.as_ref().map_or_else(|| {
            format!("{:0>10x}:0:{}{}", self.address.to_u64(), crate::util::hex::to_string(&self.c25519), crate::util::hex::to_string(&self.ed25519))
        }, |p521_public| {
            let public_concat = concat_v1_public_all(&self.c25519, &self.ed25519, (*p521_public).0.public_key_bytes(), (*p521_public).1.public_key_bytes(), &(*p521_public).2, &(*p521_public).3);
            format!("{:0>10x}:1:{}", self.address.to_u64(), base64::encode_config(public_concat, base64::URL_SAFE_NO_PAD))
        })
    }
}

impl FromStr for Identity {
    type Err = InvalidFormatError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let fields_v: Vec<&str> = s.split(':').collect();
        let fields = fields_v.as_slice();
        if fields.len() == 3 || fields.len() == 4 {
            let addr = Address::from_str(fields[0])?;
            if fields[1] == "0" {
                let public_keys = crate::util::hex::from_string(fields[2]);
                if public_keys.len() == (C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE) {
                    let mut secrets: Option<IdentitySecrets> = None;
                    if fields.len() == 4 {
                        let secret_keys = crate::util::hex::from_string(fields[3]);
                        if secret_keys.len() == (C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE) {
                            let c25519_secret = C25519KeyPair::from_bytes(&public_keys.as_slice()[0..32], &secret_keys.as_slice()[0..32]);
                            let ed25519_secret = Ed25519KeyPair::from_bytes(&public_keys.as_slice()[32..64], &secret_keys.as_slice()[32..64]);
                            if c25519_secret.is_some() && ed25519_secret.is_some() {
                                secrets = Some(IdentitySecrets {
                                    c25519: c25519_secret.unwrap(),
                                    ed25519: ed25519_secret.unwrap(),
                                    v1: None,
                                });
                            } else {
                                return Err(InvalidFormatError);
                            }
                        } else {
                            return Err(InvalidFormatError);
                        }
                    }
                    return Ok(Identity {
                        address: addr,
                        c25519: public_keys.as_slice()[0..32].try_into().unwrap(),
                        ed25519: public_keys.as_slice()[32..64].try_into().unwrap(),
                        v1: None,
                        secrets,
                    });
                }
            } else if fields[1] == "1" {
                let public_keys_and_sig = base64::decode_config(fields[2], base64::URL_SAFE_NO_PAD);
                if public_keys_and_sig.is_ok() {
                    let public_keys_and_sig = public_keys_and_sig.unwrap();
                    if public_keys_and_sig.len() == V1_PUBLIC_KEYS_SIGNATURE_AND_POW_SIZE {
                        let p521_ecdh_public = P521PublicKey::from_bytes(&public_keys_and_sig.as_slice()[(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE)..(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE)]);
                        let p521_ecdsa_public = P521PublicKey::from_bytes(&public_keys_and_sig.as_slice()[(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE)..(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE)]);
                        if p521_ecdh_public.is_some() && p521_ecdsa_public.is_some() {
                            let p521_ecdh_public = p521_ecdh_public.unwrap();
                            let p521_ecdsa_public = p521_ecdsa_public.unwrap();
                            let mut secrets: Option<IdentitySecrets> = None;
                            if fields.len() == 4 {
                                let secret_keys = base64::decode_config(fields[3], base64::URL_SAFE_NO_PAD);
                                if secret_keys.is_ok() {
                                    let secret_keys = secret_keys.unwrap();
                                    if secret_keys.len() == (C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE) {
                                        let p521_ecdh_secret = P521KeyPair::from_bytes(p521_ecdh_public.public_key_bytes(), &secret_keys.as_slice()[(C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE)..(C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE)]);
                                        let p521_ecdsa_secret = P521KeyPair::from_bytes(p521_ecdsa_public.public_key_bytes(), &secret_keys.as_slice()[(C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE)..(C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE)]);
                                        if p521_ecdh_secret.is_some() && p521_ecdsa_secret.is_some() {
                                            secrets = Some(IdentitySecrets {
                                                c25519: C25519KeyPair::from_bytes(&public_keys_and_sig.as_slice()[0..32], &secret_keys.as_slice()[0..32]).unwrap(),
                                                ed25519: Ed25519KeyPair::from_bytes(&public_keys_and_sig.as_slice()[32..64], &secret_keys.as_slice()[32..64]).unwrap(),
                                                v1: Some((p521_ecdh_secret.unwrap(), p521_ecdsa_secret.unwrap())),
                                            });
                                        } else {
                                            return Err(InvalidFormatError);
                                        }
                                    } else {
                                        return Err(InvalidFormatError);
                                    }
                                } else {
                                    return Err(InvalidFormatError);
                                }
                            }
                            return Ok(Identity {
                                address: addr,
                                c25519: public_keys_and_sig.as_slice()[0..32].try_into().unwrap(),
                                ed25519: public_keys_and_sig.as_slice()[32..64].try_into().unwrap(),
                                v1: Some((
                                    p521_ecdh_public,
                                    p521_ecdsa_public,
                                    public_keys_and_sig.as_slice()[(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE)..(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_ECDSA_SIGNATURE_SIZE)].try_into().unwrap(),
                                    public_keys_and_sig.as_slice()[(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_ECDSA_SIGNATURE_SIZE)..].try_into().unwrap()
                                )),
                                secrets,
                            });
                        }
                    }
                }
            }
        }
        Err(InvalidFormatError)
    }
}

impl Clone for Identity {
    fn clone(&self) -> Self {
        let bytes = self.marshal_to_bytes(true);
        Identity::unmarshal_from_bytes(bytes.as_slice()).unwrap().0
    }
}

impl PartialEq for Identity {
    fn eq(&self, other: &Self) -> bool {
        self.address.eq(&other.address) && self.c25519.eq(&other.c25519) && self.ed25519.eq(&other.ed25519) && self.v1.as_ref().map_or_else(|| other.v1.is_none(), |v1| other.v1.as_ref().map_or(false, |other_v1| (*v1).0.eq(&(*other_v1).0) && (*v1).1.eq(&(*other_v1).1)))
    }
}

impl Eq for Identity {}

impl PartialOrd for Identity {
    #[inline(always)]
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> { Some(self.cmp(other)) }
}

impl Ord for Identity {
    #[inline(always)]
    fn cmp(&self, other: &Self) -> Ordering {
        let c = self.address.cmp(&other.address);
        if c.is_eq() {
            self.c25519.cmp(&other.c25519)
        } else {
            c
        }
    }
}

impl Hash for Identity {
    #[inline(always)]
    fn hash<H: Hasher>(&self, state: &mut H) {
        state.write_u64(self.address.to_u64());
    }
}

#[cfg(test)]
mod tests {
    use std::str::FromStr;

    use crate::vl1::identity::{Identity, IdentityType};

    #[test]
    fn type0() {
        let id = Identity::generate(IdentityType::C25519);
        //println!("V0: {}", id.to_string());
        if !id.locally_validate() {
            panic!("new V0 identity validation failed");
        }
        let sig = id.sign(&[1_u8]).unwrap();
        //println!("sig: {}", crate::util::hex::to_string(sig.as_slice()));
        if !id.verify(&[1_u8], sig.as_slice()) {
            panic!("valid signature verification failed");
        }
        if id.verify(&[0_u8], sig.as_slice()) {
            panic!("invalid signature verification succeeded");
        }
        for good_id in [
            "7f3a8e50db:0:936b698c68f51508e9184f7510323a01da0e5778158244c83520614822e2352855ff4d82443823b866cdb553d02d8fa5da833fbee62472e666a60605b76194b9:0d46684e30d561c859bf7d530d2de0452605d8cf392db4beb2768ceda55e63673f11d84a9f31ce7504f0e3ce5dc9ab7ecf9662e555846d130422916482be5fbb",
            "8bd225d6a9:0:08e7fc755ee0aa2e10bf37c0b8dd6f33b3164de04cf3f716584ee44df1fe9506ce1f3f2874c6d1450fc8fab339a95092ec7e628cddd26af93c4392e6564d9ee7:431bb44d22734d925538cbcdc7c2a80c0f71968041949f76ccb6f690f01b6cf45976071c86fcf2ddda2d463c8cfe6444b36c8ee0d057d665350acdcb86dff06f"
        ] {
            let id = Identity::from_str(good_id).unwrap();
            if !id.locally_validate() {
                panic!("known-good V0 identity failed local validation");
            }
            let id_bytes = id.marshal_to_bytes(true);
            let id2 = Identity::unmarshal_from_bytes(id_bytes.as_slice()).unwrap().0;
            if !id.eq(&id2) {
                panic!("identity V0 marshal/unmarshal failed");
            }
        }
    }

    #[test]
    fn type1() {
        let start = std::time::SystemTime::now();
        let id = Identity::generate(IdentityType::P521);
        let end = std::time::SystemTime::now();
        println!("V1 generate: {}ms {}", end.duration_since(start).unwrap().as_millis(), id.to_string());
        if !id.locally_validate() {
            panic!("new V1 identity validation failed");
        }
        let sig = id.sign(&[1_u8]).unwrap();
        //println!("sig: {}", crate::util::hex::to_string(sig.as_slice()));
        if !id.verify(&[1_u8], sig.as_slice()) {
            panic!("valid signature verification failed");
        }
        if id.verify(&[0_u8], sig.as_slice()) {
            panic!("invalid signature verification succeeded");
        }
        for good_id in [
            "39ac93e603:1:kbBpk8QuSmNuSJFMLLXLbXcQMLXTMQOfGB8phFansHwxFrXO0RLNJB4HX4bHwq1UScYv2gVlwqmluH1J9_zp2gDaXD1c4jS17Zfdwz672wvkEMIRLUMxxxS4Cc4G7Xfhwpeot2qsslOTVw6h0FTM5izdH6ddnMmJGEg-84a_eQwSQgCNAYvczolmQg9D_MRp6wPdJdBFsnUMmj4F73jro4EGSAhE39AE8hPSxnLNOdhyRHriV5kMkIMsukJrQ09ZxKTmrQCvPzrzUrugkSu6BAqrXSm-h8S9tJo1TVIG0cSbBOK1nJsAoYhOq-T-hBvyqvKsFi9Wkz5bnG6NiYN95Yd38d2VRQBm4Xx7MPQDH0Yk10A1hc_dgyHtFGKjyTrpkKIBHICx_ijg3iS8Uut0aZpIb6pnOs1mZyjSCDLkSKJoNkxE8btHOwHzDIY0utnmsh6sCZ8wWUzYyXKfA506o7z1x57U4qxSI8-_cqZg6DUEoLb0idYanObcN9YmXdUtacpRoLhJTMlowQHqJK3_i0xh5P4UoBHuK7xST1Yu411y9keyZAie771kJFttzR3AHP89IEn0rbfbUZmAL1icL2LxWIq6kIXF6vdfgAHngQ-C1kjC3pXoWyvglJ7B33AfejmktuytTdGxe0ve1MhsXk0J3WIgokQ5rJPmAw",
            "953f45090f:1:Ghn8_lcEX4bovP90tjJQ4oTXoCf5muJYVaC599_5RW6BN7IgGCsxodiN9U1Pq1_IDYnuJ98KmlPXksE77kvGrQCWK1BnhUFVthtkk7CkEsLdtMu5NqNK_1SVuxnfVCNNELBBIZMIl22YVw8PLTwlPcWUn16m9sAD9jqptUrwAmGUcQANpUiED6k3heqszrR9IjGVVYAcIOOHYJrVz13dv0-Ty6iyoNYG12AuSPd-7suG1Ztgt2Zll8c80BGUAib0-dgJBwF8fV8Bn_Sji6kz7qyp8566U6NkbAA4znM5UvrUPIw6O5016RuO8YzDLE_LKI_erwB2CjZsC0TDOFEppkPLSxDBWgBe1PRwnKTUhP9Bq7vYVbpBs_mEd4b7a-ilce8dX-dxJJwIXIioCdL6fZzA5KqfjW1pG-M72ct6iLCmq1R4b26eiwDG9z-HwX-fRVEkzDeE1rQ2bAEnqTNtzMTItAdzF62LiPOZlZ8LQ-fNTbdThphx58uKczo8Dl44R89d1l81yTKFbgGrUU6lw5qByH6VuLuprOn96sQLv4w6M8DPVnS_5E5AOIoXJ7KmEltIUz_B45ZzgDgPrBjE3-E2ipSSxQe72ADAkQL-D2uCtZVa-UhRFbjUqzBMoTu2NqHYotUoxCsbTVI1d3duD5NefnkjjmqVP0UJDw"
        ] {
            let id = Identity::from_str(good_id).unwrap();
            if !id.locally_validate() {
                panic!("known-good V1 identity failed local validation");
            }
            let id_bytes = id.marshal_to_bytes(true);
            let id2 = Identity::unmarshal_from_bytes(id_bytes.as_slice()).unwrap().0;
            if !id.eq(&id2) {
                panic!("identity V1 marshal/unmarshal failed");
            }
        }
    }
}

 */