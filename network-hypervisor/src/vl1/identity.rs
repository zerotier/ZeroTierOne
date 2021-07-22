use std::alloc::{Layout, dealloc, alloc};
use std::ptr::{slice_from_raw_parts_mut, slice_from_raw_parts};
use std::io::Write;

use crate::vl1::Address;
use crate::vl1::buffer::{Buffer, RawObject, NoHeader};
use crate::crypto::c25519::{C25519_PUBLIC_KEY_SIZE, ED25519_PUBLIC_KEY_SIZE, C25519_SECRET_KEY_SIZE, ED25519_SECRET_KEY_SIZE, C25519KeyPair, Ed25519KeyPair};
use crate::crypto::p521::{P521KeyPair, P521PublicKey, P521_ECDSA_SIGNATURE_SIZE, P521_PUBLIC_KEY_SIZE, P521_SECRET_KEY_SIZE};
use crate::crypto::hash::{SHA384, SHA512, SHA512_HASH_SIZE};
use crate::crypto::balloon;
use crate::crypto::salsa::Salsa;

const V0_IDENTITY_GEN_MEMORY: usize = 2097152;

const V1_BALLOON_SPACE_COST: usize = 16384;
const V1_BALLOON_TIME_COST: usize = 3;
const V1_BALLOON_DELTA: usize = 3;

#[derive(Copy, Clone)]
#[repr(u8)]
pub enum Type {
    /// Curve25519 / Ed25519 identity (type 0)
    C25519 = 0,
    /// NIST P-521 ECDH / ECDSA identity (also has c25519/ed25519 keys for backward compability) (type 1)
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
    v1: Option<(P521PublicKey, P521PublicKey, [u8; P521_ECDSA_SIGNATURE_SIZE], [u8; SHA512_HASH_SIZE])>,
    secrets: Option<IdentitySecrets>,
}

/// Compute result from the bespoke "frankenhash" from the old V0 work function.
/// The supplied genmem_ptr must be of size V0_IDENTITY_GEN_MEMORY and aligned to an 8-byte boundary.
fn v0_frankenhash(digest: &mut [u8; 64], genmem_ptr: *mut u8) {
    let genmem = unsafe { &mut *slice_from_raw_parts_mut(genmem_ptr, V0_IDENTITY_GEN_MEMORY) };
    let genmem_alias_hack = unsafe { &*slice_from_raw_parts(genmem_ptr, V0_IDENTITY_GEN_MEMORY) };
    let genmem_u64_ptr = genmem_ptr.cast::<u64>();

    let mut s20 = Salsa::new(&digest[0..32], &digest[32..40], false).unwrap();

    s20.crypt(&crate::util::ZEROES[0..64], &mut genmem[0..64]);
    let mut i: usize = 64;
    while i < V0_IDENTITY_GEN_MEMORY {
        let ii = i + 64;
        s20.crypt(&genmem_alias_hack[(i - 64)..i], &mut genmem[i..ii]);
        i = ii;
    }

    i = 0;
    while i < (V0_IDENTITY_GEN_MEMORY / 8) {
        let idx1 = ((unsafe { *genmem_u64_ptr.offset(i as isize) }.to_be() % 8) * 8) as usize;
        let idx2 = (unsafe { *genmem_u64_ptr.offset((i + 1) as isize) }.to_be() % (V0_IDENTITY_GEN_MEMORY as u64 / 8)) as usize;
        let genmem_u64_at_idx2_ptr = unsafe { genmem_u64_ptr.offset(idx2 as isize) };
        let tmp = unsafe { *genmem_u64_at_idx2_ptr };
        let digest_u64_ptr = unsafe { digest.as_mut_ptr().offset(idx1 as isize).cast::<u64>() };
        unsafe { *genmem_u64_at_idx2_ptr = *digest_u64_ptr };
        unsafe { *digest_u64_ptr = tmp };
        s20.crypt_in_place(digest);
        i += 2;
    }
}

impl Identity {
    fn generate_c25519() -> Identity {
        let genmem_layout = Layout::from_size_align(V0_IDENTITY_GEN_MEMORY, 8).unwrap();
        let genmem_ptr = unsafe { alloc(genmem_layout) };
        if genmem_ptr.is_null() {
            panic!("unable to allocate memory for V0 identity generation");
        }

        let ed25519 = Ed25519KeyPair::generate();
        let ed25519_pub_bytes = ed25519.public_bytes();
        let mut sha = SHA512::new();
        loop {
            let c25519 = C25519KeyPair::generate();
            let c25519_pub_bytes = c25519.public_bytes();

            sha.update(&ed25519_pub_bytes);
            sha.update(&c25519_pub_bytes);
            let mut digest = sha.finish();

            v0_frankenhash(&mut digest, genmem_ptr);
            if digest[0] < 17 {
                let addr = Address::from_bytes(&digest[59..64]).unwrap();
                if addr.is_valid() {
                    unsafe { dealloc(genmem_ptr, genmem_layout) };
                    return Identity {
                        address: addr,
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
        let c25519 = C25519KeyPair::generate();
        let ed25519 = Ed25519KeyPair::generate();
        let p521_ecdh = P521KeyPair::generate(false).unwrap();
        let p521_ecdsa = P521KeyPair::generate(false).unwrap();

        let c25519_pub_bytes = c25519.public_bytes();
        let ed25519_pub_bytes = ed25519.public_bytes();

        let mut signing_buf = [0_u8; C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE];
        signing_buf[0..C25519_PUBLIC_KEY_SIZE].copy_from_slice(&c25519_pub_bytes);
        signing_buf[C25519_PUBLIC_KEY_SIZE..(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE)].copy_from_slice(&ed25519_pub_bytes);
        signing_buf[(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE)..(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE)].copy_from_slice(p521_ecdh.public_key_bytes());
        signing_buf[(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE)..(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE)].copy_from_slice(p521_ecdsa.public_key_bytes());

        loop {
            // ECDSA is a randomized signature algorithm, so each signature will be different.
            let sig = p521_ecdsa.sign(&signing_buf).unwrap();
            let bh = balloon::hash::<{ V1_BALLOON_SPACE_COST }, { V1_BALLOON_TIME_COST }, { V1_BALLOON_DELTA }>(&sig, b"zt_id_v1");
            if bh[0] < 7 {
                let addr = Address::from_bytes(&bh[59..64]).unwrap();
                if addr.is_valid() {
                    let p521_ecdh_pub = p521_ecdh.public_key().clone();
                    let p521_ecdsa_pub = p521_ecdsa.public_key().clone();
                    return Identity {
                        address: addr,
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
    /// This is time consuming due to the one-time anti-collision proof of work required
    /// to generate an address corresponding with a set of identity keys. V0 identities
    /// take tens to hundreds of milliseconds on a typical 2020 system, while V1 identites
    /// take about 500ms. Generation can take a lot longer on low power devices, but only
    /// has to be done once.
    pub fn generate(id_type: Type) -> Identity {
        match id_type {
            Type::C25519 => Self::generate_c25519(),
            Type::P521 => Self::generate_p521()
        }
    }

    /// Locally validate this identity.
    /// This can take a few milliseconds, especially on slower systems. V0 identities are slower
    /// to fully validate than V1 identities.
    pub fn locally_validate(&self) -> bool {
        if self.v1.is_some() {
            if self.address.is_valid() {
                let genmem_layout = Layout::from_size_align(V0_IDENTITY_GEN_MEMORY, 8).unwrap();
                let genmem_ptr = unsafe { alloc(genmem_layout) };
                if genmem_ptr.is_null() {
                    false
                } else {
                    let mut sha = SHA512::new();
                    sha.update(&self.c25519);
                    sha.update(&self.ed25519);
                    let mut digest = sha.finish();
                    v0_frankenhash(&mut digest, genmem_ptr);
                    unsafe { dealloc(genmem_ptr, genmem_layout) };
                    (digest[0] < 17) && Address::from_bytes(&digest[59..64]).unwrap().eq(&self.address)
                }
            } else {
                false
            }
        } else {
            if self.address.is_valid() {
                let p521 = self.v1.as_ref().unwrap();
                let mut signing_buf = [0_u8; C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE];
                signing_buf[0..C25519_PUBLIC_KEY_SIZE].copy_from_slice(&self.c25519);
                signing_buf[C25519_PUBLIC_KEY_SIZE..(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE)].copy_from_slice(&self.ed25519);
                signing_buf[(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE)..(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE)].copy_from_slice((*p521).0.public_key_bytes());
                signing_buf[(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE)..(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE)].copy_from_slice((*p521).1.public_key_bytes());
                if (*p521).1.verify(&signing_buf, &(*p521).2) {
                    let bh = balloon::hash::<{ V1_BALLOON_SPACE_COST }, { V1_BALLOON_TIME_COST }, { V1_BALLOON_DELTA }>(&(*p521).2, b"zt_id_v1");
                    (bh[0] < 7) && bh.eq(&(*p521).3) && Address::from_bytes(&bh[59..64]).unwrap().eq(&self.address)
                } else {
                    false
                }
            } else {
                false
            }
        }
    }

    /// Execute ECDH key agreement and return SHA384(shared secret).
    /// If both keys are type 1, key agreement is done with NIST P-521. Otherwise it's done
    /// with Curve25519. None is returned if there is an error such as this identity missing
    /// its secrets or a key being invalid.
    pub fn agree(&self, other_identity: &Identity) -> Option<[u8; 48]> {
        self.secrets.as_ref().map_or(None, |secrets| {
            secrets.v1.as_ref().map_or_else(|| {
                Some(SHA384::hash(&secrets.c25519.agree(&other_identity.c25519)))
            }, |p521_secret| {
                other_identity.v1.as_ref().map_or_else(|| {
                    Some(SHA384::hash(&secrets.c25519.agree(&other_identity.c25519)))
                }, |other_p521_public| {
                    p521_secret.0.agree(&other_p521_public.0).map_or(None, |secret| Some(SHA384::hash(&secret)))
                })
            })
        })
    }

    /// Sign this message with this identity.
    /// Signature is performed using ed25519 EDDSA or NIST P-521 ECDSA depending on the identity
    /// type. None is returned if this identity lacks secret keys or another error occurs.
    pub fn sign(&self, msg: &[u8]) -> Option<Vec<u8>> {
        self.secrets.as_ref().map_or(None, |secrets| {
            secrets.v1.as_ref().map_or_else(|| {
                Some(secrets.ed25519.sign(msg).to_vec())
            }, |p521_secret| {
                p521_secret.1.sign(msg).map_or(None, |sig| Some(sig.to_vec()))
            })
        })
    }

    /// Verify a signature.
    pub fn verify(&self, msg: &[u8], signature: &[u8]) -> bool {
        self.v1.as_ref().map_or_else(|| {
            crate::crypto::c25519::ed25519_verify(&self.ed25519, signature, msg)
        }, |p521| {
            (*p521).1.verify(msg, signature)
        })
    }

    /// Get this identity's type.
    #[inline(always)]
    pub fn id_type(&self) -> Type {
        if self.v1.is_some() {
            Type::P521
        } else {
            Type::C25519
        }
    }

    /// Returns true if this identity also holds its secret keys.
    #[inline(always)]
    pub fn has_secrets(&self) -> bool {
        self.secrets.is_some()
    }

    /// Get this identity in string format, including its secret keys.
    pub fn to_secret_string(&self) -> String {
        self.secrets.as_ref().map_or_else(|| {
            self.to_string()
        }, |secrets| {
            secrets.v1.as_ref().map_or_else(|| {
                format!("{}:{}{}", self.to_string(), crate::util::hex::to_string(&secrets.c25519.secret_bytes()), crate::util::hex::to_string(&secrets.ed25519.secret_bytes()))
            }, |p521_secret| {
                let mut secret_key_blob: Vec<u8> = Vec::new();
                secret_key_blob.reserve(C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE);
                let _ = secret_key_blob.write_all(&secrets.c25519.secret_bytes());
                let _ = secret_key_blob.write_all(&secrets.ed25519.secret_bytes());
                let _ = secret_key_blob.write_all(p521_secret.0.secret_key_bytes());
                let _ = secret_key_blob.write_all(p521_secret.1.secret_key_bytes());
                format!("{}:{}", self.to_string(), base64::encode_config(secret_key_blob.as_slice(), base64::URL_SAFE_NO_PAD))
            })
        })
    }

    /// Append this in binary format to a buffer.
    pub fn marshal<BH: RawObject, const BL: usize>(&self, buf: &mut Buffer<BH, BL>, include_private: bool) -> std::io::Result<()> {
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
                    buf.append_u8((C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE) as u8)?;
                    buf.append_bytes_fixed(&secrets.c25519.secret_bytes())?;
                    buf.append_bytes_fixed(&secrets.ed25519.secret_bytes())?;
                    buf.append_bytes_fixed((*p521_secrets).0.secret_key_bytes())?;
                    buf.append_bytes_fixed((*p521_secrets).1.secret_key_bytes())?;
                }
            } else {
                buf.append_u8(0)?; // 0 secret bytes if not adding any
            }
        } else {
            buf.append_and_init_bytes_fixed(|b: &mut [u8; 1 + C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE]| {
                b[0] = 0; // type 0
                b[1..(1 + C25519_PUBLIC_KEY_SIZE)].copy_from_slice(&self.c25519);
                b[(1 + C25519_PUBLIC_KEY_SIZE)..(1 + C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE)].copy_from_slice(&self.ed25519);
            })?;
            if include_private && self.secrets.is_some() {
                let secrets = self.secrets.as_ref().unwrap();
                buf.append_u8((C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE) as u8)?;
                buf.append_bytes_fixed(&secrets.c25519.secret_bytes())?;
                buf.append_bytes_fixed(&secrets.ed25519.secret_bytes())?;
            } else {
                buf.append_u8(0)?; // 0 secret bytes if not adding any
            }
        }
        Ok(())
    }

    /// Deserialize an Identity from a buffer.
    /// The supplied cursor is advanced.
    pub fn unmarshal<BH: RawObject, const BL: usize>(buf: &Buffer<BH, BL>, cursor: &mut usize) -> std::io::Result<Identity> {
        let addr = Address::from_bytes(buf.get_bytes_fixed::<5>(cursor)?).unwrap();
        let id_type = buf.get_u8(cursor)?;
        if id_type == Type::C25519 as u8 {
            let c25519_public_bytes = buf.get_bytes_fixed::<{ C25519_PUBLIC_KEY_SIZE }>(cursor)?;
            let ed25519_public_bytes = buf.get_bytes_fixed::<{ ED25519_PUBLIC_KEY_SIZE }>(cursor)?;
            let secrets_len = buf.get_u8(cursor)?;
            if secrets_len == (C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE) as u8 {
                let c25519_secret_bytes = buf.get_bytes_fixed::<{ C25519_SECRET_KEY_SIZE }>(cursor)?;
                let ed25519_secret_bytes = buf.get_bytes_fixed::<{ ED25519_SECRET_KEY_SIZE }>(cursor)?;
                Ok(Identity {
                    address: addr,
                    c25519: c25519_public_bytes.clone(),
                    ed25519: ed25519_public_bytes.clone(),
                    v1: None,
                    secrets: Some(IdentitySecrets {
                        c25519: C25519KeyPair::from_bytes(c25519_public_bytes, c25519_secret_bytes).unwrap(),
                        ed25519: Ed25519KeyPair::from_bytes(ed25519_public_bytes, ed25519_secret_bytes).unwrap(),
                        v1: None,
                    })
                })
            } else if secrets_len == 0 {
                Ok(Identity {
                    address: addr,
                    c25519: c25519_public_bytes.clone(),
                    ed25519: ed25519_public_bytes.clone(),
                    v1: None,
                    secrets: None
                })
            } else {
                std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "unrecognized scret key length (type 0)"))
            }
        } else if id_type == Type::P521 as u8 {
            let c25519_public_bytes = buf.get_bytes_fixed::<{ C25519_PUBLIC_KEY_SIZE }>(cursor)?;
            let ed25519_public_bytes = buf.get_bytes_fixed::<{ ED25519_PUBLIC_KEY_SIZE }>(cursor)?;
            let p521_ecdh_public_bytes = buf.get_bytes_fixed::<{ P521_PUBLIC_KEY_SIZE }>(cursor)?;
            let p521_ecdsa_public_bytes = buf.get_bytes_fixed::<{ P521_PUBLIC_KEY_SIZE }>(cursor)?;
            let p521_signature = buf.get_bytes_fixed::<{ P521_ECDSA_SIGNATURE_SIZE }>(cursor)?;
            let bh_digest = buf.get_bytes_fixed::<{ SHA512_HASH_SIZE }>(cursor)?;
            let secrets_len = buf.get_u8(cursor)?;
            if secrets_len == (C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE + P521_SECRET_KEY_SIZE) as u8 {
                let c25519_secret_bytes = buf.get_bytes_fixed::<{ C25519_SECRET_KEY_SIZE }>(cursor)?;
                let ed25519_secret_bytes = buf.get_bytes_fixed::<{ ED25519_SECRET_KEY_SIZE }>(cursor)?;
                let p521_ecdh_secret_bytes = buf.get_bytes_fixed::<{ P521_SECRET_KEY_SIZE }>(cursor)?;
                let p521_ecdsa_secret_bytes = buf.get_bytes_fixed::<{ P521_SECRET_KEY_SIZE }>(cursor)?;
                Ok(Identity {
                    address: addr,
                    c25519: c25519_public_bytes.clone(),
                    ed25519: ed25519_public_bytes.clone(),
                    v1: Some((P521PublicKey::from_bytes(p521_ecdh_public_bytes).unwrap(), P521PublicKey::from_bytes(p521_ecdsa_public_bytes).unwrap(), p521_signature.clone(), bh_digest. clone())),
                    secrets: Some(IdentitySecrets {
                        c25519: C25519KeyPair::from_bytes(c25519_public_bytes, c25519_secret_bytes).unwrap(),
                        ed25519: Ed25519KeyPair::from_bytes(ed25519_public_bytes, ed25519_secret_bytes).unwrap(),
                        v1: Some((P521KeyPair::from_bytes(p521_ecdh_public_bytes, p521_ecdh_secret_bytes).unwrap(), P521KeyPair::from_bytes(p521_ecdsa_public_bytes, p521_ecdsa_secret_bytes).unwrap())),
                    })
                })
            } else if secrets_len == 0 {
                Ok(Identity {
                    address: addr,
                    c25519: c25519_public_bytes.clone(),
                    ed25519: ed25519_public_bytes.clone(),
                    v1: Some((P521PublicKey::from_bytes(p521_ecdh_public_bytes).unwrap(), P521PublicKey::from_bytes(p521_ecdsa_public_bytes).unwrap(), p521_signature.clone(), bh_digest. clone())),
                    secrets: None
                })
            } else {
                std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid secret key length (type 1)"))
            }
        } else {
            std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "unrecognized identity type"))
        }
    }

    /// Get this identity in byte array format.
    pub fn marshal_to_vec(&self, include_private: bool) -> Vec<u8> {
        let mut buf: Buffer<NoHeader, 2048> = Buffer::new();
        self.marshal(&mut buf, include_private).expect("overflow");
        buf.as_bytes().to_vec()
    }
}

impl ToString for Identity {
    fn to_string(&self) -> String {
        self.v1.as_ref().map_or_else(|| {
            format!("{:0>10x}:0:{}{}", self.address.to_u64(), crate::util::hex::to_string(&self.c25519), crate::util::hex::to_string(&self.ed25519))
        }, |p521_public| {
            let mut public_key_blob: Vec<u8> = Vec::new();
            public_key_blob.reserve(C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_ECDSA_SIGNATURE_SIZE + SHA512_HASH_SIZE);
            let _ = public_key_blob.write_all(&self.c25519);
            let _ = public_key_blob.write_all(&self.ed25519);
            let _ = public_key_blob.write_all(p521_public.0.public_key_bytes());
            let _ = public_key_blob.write_all(p521_public.1.public_key_bytes());
            let _ = public_key_blob.write_all(&p521_public.2);
            let _ = public_key_blob.write_all(&p521_public.3);
            format!("{:0>10x}:1:{}", self.address.to_u64(), base64::encode_config(public_key_blob.as_slice(), base64::URL_SAFE_NO_PAD))
        })
    }
}

#[cfg(test)]
mod tests {
    #[allow(unused_imports)]
    use crate::vl1::Identity;
    #[allow(unused_imports)]
    use crate::vl1::identity::Type;

    #[test]
    fn p521() {
        /*
        let mut ms = 0.0;
        let mut id: Option<Identity> = None;
        for _ in 0..64 {
            let start = std::time::SystemTime::now();
            id.replace(Identity::generate(Type::P521));
            let duration = std::time::SystemTime::now().duration_since(start).unwrap();
            ms += duration.as_nanos() as f64 / 1000000.0;
        }
        ms /= 64.0;
        println!("{}ms {}", ms, id.unwrap().to_secret_string());
        */
    }
}
