// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::array::TryFromSliceError;
use std::io::Write;
use std::str::FromStr;

use serde::de::Error;
use serde::{Deserialize, Deserializer, Serialize, Serializer};

use super::address::{Address, PartialAddress};

use zerotier_crypto::hash::{SHA384, SHA512};
use zerotier_crypto::p384::*;
use zerotier_crypto::salsa::Salsa;
use zerotier_crypto::secret::Secret;
use zerotier_crypto::typestate::Valid;
use zerotier_crypto::x25519::*;
use zerotier_utils::arrayvec::ArrayVec;
use zerotier_utils::base64;
use zerotier_utils::buffer::{Buffer, OutOfBoundsError};
use zerotier_utils::error::InvalidFormatError;
use zerotier_utils::marshalable::{Marshalable, UnmarshalError};
use zerotier_utils::{hex, memory};

#[derive(Clone, PartialEq, Eq)]
pub struct Identity {
    pub address: Address,
    pub x25519: X25519,
    pub p384: Option<P384>,
}

#[derive(Clone, PartialEq, Eq)]
pub struct X25519 {
    pub ecdh: [u8; C25519_PUBLIC_KEY_SIZE],
    pub eddsa: [u8; ED25519_PUBLIC_KEY_SIZE],
}

#[derive(Clone)]
pub struct P384 {
    pub ecdh: P384PublicKey,
    pub ecdsa: P384PublicKey,
    pub ed25519_self_signature: [u8; ED25519_SIGNATURE_SIZE],
    pub p384_self_signature: [u8; P384_ECDSA_SIGNATURE_SIZE],
}

#[derive(Clone)]
pub struct IdentitySecret {
    pub public: Valid<Identity>,
    pub x25519: X25519Secret,
    pub p384: Option<P384Secret>,
}

#[derive(Clone)]
pub struct X25519Secret {
    pub ecdh: X25519KeyPair,
    pub eddsa: Ed25519KeyPair,
}

#[derive(Clone)]
pub struct P384Secret {
    pub ecdh: P384KeyPair,
    pub ecdsa: P384KeyPair,
}

impl Identity {
    pub const MAX_SIGNATURE_SIZE: usize = 96;

    const ALGORITHM_X25519: u8 = 0;
    const ALGORITHM_P384: u8 = 1;

    const LEGACY_ADDRESS_POW_THRESHOLD: u8 = 17;

    /// Generate a new ZeroTier identity.
    /// If x25519_only is true a legacy identity without NIST P-384 key pairs will be generated.
    pub fn generate(x25519_only: bool) -> IdentitySecret {
        // Generate X25519 portions of the identity plus the first 40 bits of the address, which are
        // the legacy "short" address.
        let x25519_ecdh = X25519KeyPair::generate();
        let ed25519_eddsa = Ed25519KeyPair::generate();
        let x25519_ecdh_public = x25519_ecdh.public_bytes();
        let ed25519_eddsa_public = ed25519_eddsa.public_bytes();
        let mut secret = IdentitySecret {
            public: Valid::mark_valid(Identity {
                address: Address::new_uninitialized(),
                x25519: X25519 { ecdh: x25519_ecdh_public, eddsa: ed25519_eddsa_public },
                p384: None,
            }),
            x25519: X25519Secret { ecdh: x25519_ecdh, eddsa: ed25519_eddsa },
            p384: None,
        };
        let mut legacy_address_derivation_hasher = SHA512::new();
        loop {
            legacy_address_derivation_hasher.update(&secret.public.x25519.ecdh);
            legacy_address_derivation_hasher.update(&secret.public.x25519.eddsa);
            let mut legacy_address_derivation_hash = legacy_address_derivation_hasher.finish();
            legacy_address_derivation_work_function(&mut legacy_address_derivation_hash);
            if legacy_address_derivation_hash[0] < Self::LEGACY_ADDRESS_POW_THRESHOLD
                && legacy_address_derivation_hash[59] != Address::RESERVED_PREFIX
                && legacy_address_derivation_hash[59..64].iter().any(|i| *i != 0)
            {
                secret.public.address.0[..PartialAddress::LEGACY_SIZE_BYTES].copy_from_slice(&legacy_address_derivation_hash[59..64]);
                break;
            } else {
                // Regenerate one of the two keys until we meet the legacy address work function criteria.
                secret.x25519.ecdh = X25519KeyPair::generate();
                secret.public.x25519.ecdh = secret.x25519.ecdh.public_bytes();
                legacy_address_derivation_hasher.reset();
            }
        }

        // Generate NIST P-384 key pairs unless we just want a legacy identity.
        if !x25519_only {
            secret.p384 = Some(P384Secret {
                ecdh: P384KeyPair::generate(),
                ecdsa: P384KeyPair::generate(),
            });
            secret.public.p384 = secret.p384.as_ref().map(|p384s| P384 {
                ecdh: p384s.ecdh.public_key().clone(),
                ecdsa: p384s.ecdsa.public_key().clone(),
                ed25519_self_signature: [0u8; ED25519_SIGNATURE_SIZE],
                p384_self_signature: [0u8; P384_ECDSA_SIGNATURE_SIZE],
            });
        }

        // Bits 40-384 of the address are filled from a SHA384 hash of all keys for a full length V2 address.
        secret.public.address = secret.public.populate_extended_address_bits();

        // For V2 identities we include two self signatures to ensure that all these different key pairs
        // are properly bound together and can't be changed independently.
        if !x25519_only {
            let mut for_self_signing =
                [0u8; Address::SIZE_BYTES + 1 + C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P384_PUBLIC_KEY_SIZE + P384_PUBLIC_KEY_SIZE];
            secret.public.encode_for_self_signing(&mut for_self_signing);
            let p384 = secret.public.p384.as_mut().unwrap();
            p384.ed25519_self_signature = secret.x25519.eddsa.sign(&for_self_signing);
            p384.p384_self_signature = secret.p384.as_ref().unwrap().ecdsa.sign(&for_self_signing);
        }

        secret
    }

    /// Locally validate this identity.
    /// This checks address derivation, any self-signatures, etc. It's a little time consuming so results should be cached
    /// if possible. Returns a marked typestate on success, which does sort of cache the results and prevents misuse.
    pub fn validate(self) -> Option<Valid<Self>> {
        // First, check that the full SHA384 (bits 40-384) in the address is correct. Check first since this is fast.
        let mut test_address = Address::new_uninitialized();
        test_address.0[..PartialAddress::LEGACY_SIZE_BYTES].copy_from_slice(&self.address.0[..PartialAddress::LEGACY_SIZE_BYTES]);
        test_address = self.populate_extended_address_bits();
        if self.address != test_address {
            return None;
        }

        // Second, check self-signatures if we have them. Check second since this is somewhat slower.
        if let Some(p384) = self.p384.as_ref() {
            let mut for_self_signing =
                [0u8; Address::SIZE_BYTES + 1 + C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + P384_PUBLIC_KEY_SIZE + P384_PUBLIC_KEY_SIZE];
            self.encode_for_self_signing(&mut for_self_signing);
            if !ed25519_verify(&self.x25519.eddsa, &p384.ed25519_self_signature, &for_self_signing)
                || !p384.ecdsa.verify(&for_self_signing, &p384.p384_self_signature)
            {
                return None;
            }
        }

        // Finally, check the legacy address as this is the most costly check. If we deprecate V1 this could go away
        // in the future since the full SHA384 (checked above) includes the first 40 bits of the address in the hash.
        let mut legacy_address_derivation_hasher = SHA512::new();
        legacy_address_derivation_hasher.update(&self.x25519.ecdh);
        legacy_address_derivation_hasher.update(&self.x25519.eddsa);
        let mut legacy_address_derivation_hash = legacy_address_derivation_hasher.finish();
        legacy_address_derivation_work_function(&mut legacy_address_derivation_hash);
        if legacy_address_derivation_hash[0] >= Self::LEGACY_ADDRESS_POW_THRESHOLD
            || !legacy_address_derivation_hash[59..64].eq(self.address.legacy_bytes())
        {
            return None;
        }

        return Some(Valid::mark_valid(self));
    }

    /// Verify a signature with this identity.
    pub fn verify(&self, data: &[u8], signature: &[u8]) -> bool {
        if let Some(p384) = self.p384.as_ref() {
            p384.ecdsa.verify(data, signature)
        } else {
            ed25519_verify(&self.x25519.eddsa, signature, data)
        }
    }

    /// Populate bits 40-384 of the address with a hash of everything else and return the completed address.
    fn populate_extended_address_bits(&self) -> Address {
        let mut sha = SHA384::new();
        sha.update(&self.address.0[..PartialAddress::LEGACY_SIZE_BYTES]); // include short address in full hash
        sha.update(&[Self::ALGORITHM_X25519
            | if self.p384.is_some() {
                Self::ALGORITHM_P384
            } else {
                0
            }]);
        sha.update(&self.x25519.ecdh);
        sha.update(&self.x25519.eddsa);
        if let Some(p384) = self.p384.as_ref() {
            sha.update(p384.ecdh.as_bytes());
            sha.update(p384.ecdsa.as_bytes());
        }
        let sha = sha.finish();
        let mut address = self.address.clone();
        address.0[PartialAddress::LEGACY_SIZE_BYTES..].copy_from_slice(&sha[..Address::SIZE_BYTES - PartialAddress::LEGACY_SIZE_BYTES]);
        address
    }

    /// Encode for self-signing, used only with p384 keys enabled and panics otherwise.
    fn encode_for_self_signing(&self, mut buf: &mut [u8]) {
        let _ = buf.write_all(self.address.as_bytes());
        let _ = buf.write_all(&[Self::ALGORITHM_X25519 | Self::ALGORITHM_P384]);
        let _ = buf.write_all(&self.x25519.ecdh);
        let _ = buf.write_all(&self.x25519.eddsa);
        let p384 = self.p384.as_ref().unwrap();
        let _ = buf.write_all(p384.ecdh.as_bytes());
        let _ = buf.write_all(p384.ecdsa.as_bytes());
    }

    /// Decode a byte serialized identity.
    pub fn from_bytes(b: &[u8]) -> Result<Self, InvalidFormatError> {
        let mut id;
        if b.len() == packed::V2_PUBLIC_SIZE && b[PartialAddress::LEGACY_SIZE_BYTES] == (Self::ALGORITHM_X25519 | Self::ALGORITHM_P384) {
            let p: &packed::V2Public = memory::cast_to_struct(b);
            id = Self {
                address: Address::new_uninitialized(),
                x25519: X25519 { ecdh: p.c25519, eddsa: p.ed25519 },
                p384: Some(P384 {
                    ecdh: P384PublicKey::from_bytes(&p.p384_ecdh).ok_or(InvalidFormatError)?,
                    ecdsa: P384PublicKey::from_bytes(&p.p384_ecdsa).ok_or(InvalidFormatError)?,
                    ed25519_self_signature: p.ed25519_self_signature,
                    p384_self_signature: p.p384_self_signature,
                }),
            };
            id.address.0[..PartialAddress::LEGACY_SIZE_BYTES].copy_from_slice(&p.short_address);
        } else if b.len() == packed::V1_PUBLIC_SIZE && b[PartialAddress::LEGACY_SIZE_BYTES] == Self::ALGORITHM_X25519 {
            let p: &packed::V1Public = memory::cast_to_struct(b);
            id = Self {
                address: Address::new_uninitialized(),
                x25519: X25519 { ecdh: p.c25519, eddsa: p.ed25519 },
                p384: None,
            };
            id.address.0[..PartialAddress::LEGACY_SIZE_BYTES].copy_from_slice(&p.short_address);
        } else {
            return Err(InvalidFormatError);
        }
        id.address = id.populate_extended_address_bits();
        return Ok(id);
    }

    /// Write as a byte serialized identity to a writer.
    pub fn write_bytes<W: Write>(&self, w: &mut W, x25519_only: bool) -> Result<(), std::io::Error> {
        if let (false, Some(p384)) = (x25519_only, self.p384.as_ref()) {
            w.write_all(memory::as_byte_array::<packed::V2Public, { packed::V2_PUBLIC_SIZE }>(&packed::V2Public {
                short_address: *self.address.legacy_bytes(),
                algorithms: Self::ALGORITHM_X25519 | Self::ALGORITHM_P384,
                c25519: self.x25519.ecdh,
                ed25519: self.x25519.eddsa,
                p384_ecdh: *p384.ecdh.as_bytes(),
                p384_ecdsa: *p384.ecdsa.as_bytes(),
                ed25519_self_signature: p384.ed25519_self_signature,
                p384_self_signature: p384.p384_self_signature,
            }))
        } else {
            w.write_all(memory::as_byte_array::<packed::V1Public, { packed::V1_PUBLIC_SIZE }>(&packed::V1Public {
                short_address: *self.address.legacy_bytes(),
                algorithms: Self::ALGORITHM_X25519,
                c25519: self.x25519.ecdh,
                ed25519: self.x25519.eddsa,
                secret_bytes: 0,
            }))
        }
    }
}

impl ToString for Identity {
    fn to_string(&self) -> String {
        if let Some(p384) = self.p384.as_ref() {
            let mut s = String::with_capacity(1024);
            s.push_str(self.address.to_string().as_str());
            s.push_str(":1:");
            base64::encode_into(&self.x25519.ecdh, &mut s);
            s.push(':');
            base64::encode_into(&self.x25519.eddsa, &mut s);
            s.push(':');
            base64::encode_into(p384.ecdh.as_bytes(), &mut s);
            s.push(':');
            base64::encode_into(p384.ecdsa.as_bytes(), &mut s);
            s.push(':');
            base64::encode_into(&p384.ed25519_self_signature, &mut s);
            s.push(':');
            base64::encode_into(&p384.p384_self_signature, &mut s);
            s
        } else {
            format!(
                "{}:0:{}{}",
                hex::to_string(self.address.legacy_bytes()),
                hex::to_string(&self.x25519.ecdh),
                hex::to_string(&self.x25519.eddsa)
            )
        }
    }
}

impl FromStr for Identity {
    type Err = InvalidFormatError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let ss: Vec<&str> = s.split(':').collect();
        if ss.len() >= 2 {
            if ss[1] == "1" && ss.len() >= 8 {
                return Ok(Self {
                    address: Address::from_str(ss[0]).map_err(|_| InvalidFormatError)?,
                    x25519: X25519 {
                        ecdh: base64::decode(ss[2].as_bytes())
                            .map_err(|_| InvalidFormatError)?
                            .try_into()
                            .map_err(|_| InvalidFormatError)?,
                        eddsa: base64::decode(ss[3].as_bytes())
                            .map_err(|_| InvalidFormatError)?
                            .try_into()
                            .map_err(|_| InvalidFormatError)?,
                    },
                    p384: Some(P384 {
                        ecdh: P384PublicKey::from_bytes(base64::decode(ss[4].as_bytes()).map_err(|_| InvalidFormatError)?.as_slice())
                            .ok_or(InvalidFormatError)?,
                        ecdsa: P384PublicKey::from_bytes(base64::decode(ss[5].as_bytes()).map_err(|_| InvalidFormatError)?.as_slice())
                            .ok_or(InvalidFormatError)?,
                        ed25519_self_signature: base64::decode(ss[6].as_bytes())
                            .map_err(|_| InvalidFormatError)?
                            .try_into()
                            .map_err(|_| InvalidFormatError)?,
                        p384_self_signature: base64::decode(ss[7].as_bytes())
                            .map_err(|_| InvalidFormatError)?
                            .try_into()
                            .map_err(|_| InvalidFormatError)?,
                    }),
                });
            } else if ss[1] == "0" && ss.len() >= 3 {
                let ecdh_eddsa = hex::from_string(ss[2]);
                if ecdh_eddsa.len() != C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE {
                    return Err(InvalidFormatError);
                }
                let mut a = Self {
                    address: {
                        let legacy_address = hex::from_string(ss[0]);
                        if legacy_address.len() != PartialAddress::LEGACY_SIZE_BYTES {
                            return Err(InvalidFormatError);
                        }
                        let mut tmp = [0u8; Address::SIZE_BYTES];
                        tmp[..PartialAddress::LEGACY_SIZE_BYTES].copy_from_slice(legacy_address.as_slice());
                        Address(tmp)
                    },
                    x25519: X25519 {
                        ecdh: ecdh_eddsa[..C25519_PUBLIC_KEY_SIZE].try_into().unwrap(),
                        eddsa: ecdh_eddsa[C25519_PUBLIC_KEY_SIZE..].try_into().unwrap(),
                    },
                    p384: None,
                };
                a.address = a.populate_extended_address_bits();
                return Ok(a);
            }
        }
        return Err(InvalidFormatError);
    }
}

impl Ord for Identity {
    #[inline(always)]
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.address.cmp(&other.address)
    }
}

impl PartialOrd for Identity {
    #[inline(always)]
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.address.cmp(&other.address))
    }
}

impl Marshalable for Identity {
    const MAX_MARSHAL_SIZE: usize = packed::V2_PUBLIC_SIZE;

    fn marshal<const BL: usize>(&self, buf: &mut Buffer<BL>) -> Result<(), OutOfBoundsError> {
        self.write_bytes(buf, false).map_err(|_| OutOfBoundsError)
    }

    fn unmarshal<const BL: usize>(buf: &Buffer<BL>, cursor: &mut usize) -> Result<Self, UnmarshalError> {
        const V1_ALG: u8 = Identity::ALGORITHM_X25519;
        const V2_ALG: u8 = Identity::ALGORITHM_X25519 | Identity::ALGORITHM_P384;
        match buf.u8_at(*cursor + PartialAddress::LEGACY_SIZE_BYTES)? {
            V1_ALG => Identity::from_bytes(buf.read_bytes_fixed::<{ packed::V1_PUBLIC_SIZE }>(cursor)?).map_err(|_| UnmarshalError::InvalidData),
            V2_ALG => Identity::from_bytes(buf.read_bytes_fixed::<{ packed::V2_PUBLIC_SIZE }>(cursor)?).map_err(|_| UnmarshalError::InvalidData),
            _ => Err(UnmarshalError::UnsupportedVersion),
        }
    }
}

impl Serialize for Identity {
    #[inline]
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        if serializer.is_human_readable() {
            serializer.serialize_str(self.to_string().as_str())
        } else {
            let mut tmp = Vec::with_capacity(Identity::MAX_MARSHAL_SIZE);
            let _ = self.write_bytes(&mut tmp, false);
            serializer.serialize_bytes(tmp.as_slice())
        }
    }
}

struct IdentityDeserializeVisitor;

impl<'de> serde::de::Visitor<'de> for IdentityDeserializeVisitor {
    type Value = Identity;

    #[inline]
    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("an Identity")
    }

    #[inline]
    fn visit_bytes<E>(self, v: &[u8]) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        Identity::from_bytes(v).map_err(|_| E::custom("invalid identity"))
    }

    #[inline]
    fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        Identity::from_str(v).map_err(|_| E::custom("invalid identity"))
    }
}

impl<'de> Deserialize<'de> for Identity {
    #[inline]
    fn deserialize<D>(deserializer: D) -> Result<Identity, D::Error>
    where
        D: Deserializer<'de>,
    {
        if deserializer.is_human_readable() {
            deserializer.deserialize_str(IdentityDeserializeVisitor)
        } else {
            deserializer.deserialize_bytes(IdentityDeserializeVisitor)
        }
    }
}

impl Eq for P384 {}

impl PartialEq for P384 {
    #[inline]
    fn eq(&self, other: &Self) -> bool {
        self.ecdh.as_bytes() == other.ecdh.as_bytes()
            && self.ecdsa.as_bytes() == other.ecdsa.as_bytes()
            && self.ed25519_self_signature == other.ed25519_self_signature
            && self.p384_self_signature == other.p384_self_signature
    }
}

impl IdentitySecret {
    pub fn sign(&self, data: &[u8]) -> ArrayVec<u8, { Identity::MAX_SIGNATURE_SIZE }> {
        let mut s = ArrayVec::new();
        if let Some(p384) = self.p384.as_ref() {
            s.push_slice(&p384.sign(data));
        } else {
            s.push_slice(&self.x25519.sign(data));
        }
        s
    }
}

impl ToString for IdentitySecret {
    fn to_string(&self) -> String {
        let mut s = self.public.to_string();
        if let Some(p384) = self.p384.as_ref() {
            s.push(':');
            base64::encode_into(self.x25519.ecdh.secret_bytes().as_bytes(), &mut s);
            s.push(':');
            base64::encode_into(self.x25519.eddsa.secret_bytes().as_bytes(), &mut s);
            s.push(':');
            base64::encode_into(p384.ecdh.secret_key_bytes().as_bytes(), &mut s);
            s.push(':');
            base64::encode_into(p384.ecdsa.secret_key_bytes().as_bytes(), &mut s);
        } else {
            s.push(':');
            s.push_str(hex::to_string(self.x25519.ecdh.secret_bytes().as_bytes()).as_str());
            s.push_str(hex::to_string(self.x25519.eddsa.secret_bytes().as_bytes()).as_str());
        }
        s
    }
}

impl FromStr for IdentitySecret {
    type Err = InvalidFormatError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let public = Identity::from_str(s)?;
        let ss: Vec<&str> = s.split(':').collect();
        if ss.len() >= 2 {
            if ss[1] == "1" && ss.len() >= 12 && public.p384.is_some() {
                let x25519_ecdh = X25519KeyPair::from_bytes(
                    &public.x25519.ecdh,
                    base64::decode(ss[8].as_bytes()).map_err(|_| InvalidFormatError)?.as_slice(),
                )
                .ok_or(InvalidFormatError)?;
                let x25519_eddsa = Ed25519KeyPair::from_bytes(
                    &public.x25519.ecdh,
                    base64::decode(ss[9].as_bytes()).map_err(|_| InvalidFormatError)?.as_slice(),
                )
                .ok_or(InvalidFormatError)?;
                let p384_ecdh = P384KeyPair::from_bytes(
                    public.p384.as_ref().unwrap().ecdh.as_bytes(),
                    base64::decode(ss[10].as_bytes()).map_err(|_| InvalidFormatError)?.as_slice(),
                )
                .ok_or(InvalidFormatError)?;
                let p384_ecdsa = P384KeyPair::from_bytes(
                    public.p384.as_ref().unwrap().ecdh.as_bytes(),
                    base64::decode(ss[11].as_bytes()).map_err(|_| InvalidFormatError)?.as_slice(),
                )
                .ok_or(InvalidFormatError)?;
                return Ok(Self {
                    public: public.validate().ok_or(InvalidFormatError)?,
                    x25519: X25519Secret { ecdh: x25519_ecdh, eddsa: x25519_eddsa },
                    p384: Some(P384Secret { ecdh: p384_ecdh, ecdsa: p384_ecdsa }),
                });
            } else if ss[1] == "0" && ss.len() >= 4 && public.p384.is_none() {
                let ecdh_eddsa_secrets = hex::from_string(ss[3]);
                if ecdh_eddsa_secrets.len() != C25519_SECRET_KEY_SIZE + ED25519_SECRET_KEY_SIZE {
                    return Err(InvalidFormatError);
                }
                let x25519_ecdh =
                    X25519KeyPair::from_bytes(&public.x25519.ecdh, &ecdh_eddsa_secrets[..C25519_SECRET_KEY_SIZE]).ok_or(InvalidFormatError)?;
                let x25519_eddsa =
                    Ed25519KeyPair::from_bytes(&public.x25519.ecdh, &ecdh_eddsa_secrets[C25519_SECRET_KEY_SIZE..]).ok_or(InvalidFormatError)?;
                return Ok(Self {
                    public: public.validate().ok_or(InvalidFormatError)?,
                    x25519: X25519Secret { ecdh: x25519_ecdh, eddsa: x25519_eddsa },
                    p384: None,
                });
            }
        }
        return Err(InvalidFormatError);
    }
}

impl X25519Secret {
    #[inline]
    pub fn agree(&self, public: &Identity) -> Option<Secret<64>> {
        Some(Secret(SHA512::hash(self.ecdh.agree(&public.x25519.ecdh).as_bytes())))
    }

    /// Sign with Ed25519 using the legacy signature format used by ZeroTier V1.
    /// This just means the last 32 bytes of a 96-byte signature are the first 32 bytes of the
    /// SHA512 hash. This isn't used even in V1 but was once used long ago to rapidly check
    /// signatures as part of a different design. Some nodes still expect it to be there though.
    #[inline(always)]
    pub fn sign(&self, data: &[u8]) -> [u8; 96] {
        self.eddsa.sign_zt(data)
    }
}

impl P384Secret {
    #[inline(always)]
    pub fn sign(&self, data: &[u8]) -> [u8; P384_ECDSA_SIGNATURE_SIZE] {
        self.ecdsa.sign(data)
    }
}

/// The actual serialization format for secret identities.
#[derive(Serialize, Deserialize)]
struct IdentitySecretForSerialization<'a> {
    pub address: &'a [u8],
    pub ed25519_self_signature: Option<&'a [u8]>,
    pub p384_self_signature: Option<&'a [u8]>,
    pub x25519_ecdh_public: Option<&'a [u8]>,
    pub x25519_ecdh_secret: Option<&'a [u8]>,
    pub x25519_eddsa_public: Option<&'a [u8]>,
    pub x25519_eddsa_secret: Option<&'a [u8]>,
    pub p384_ecdh_public: Option<&'a [u8]>,
    pub p384_ecdh_secret: Option<&'a [u8]>,
    pub p384_ecdsa_public: Option<&'a [u8]>,
    pub p384_ecdsa_secret: Option<&'a [u8]>,
}

impl Serialize for IdentitySecret {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let p384_ecdh_secret = self.p384.as_ref().map(|p384| p384.ecdh.secret_key_bytes());
        let p384_ecdsa_secret = self.p384.as_ref().map(|p384| p384.ecdsa.secret_key_bytes());
        IdentitySecretForSerialization {
            address: self.public.address.as_bytes(),
            ed25519_self_signature: self.public.p384.as_ref().map(|p384| &p384.ed25519_self_signature[..]),
            p384_self_signature: self.public.p384.as_ref().map(|p384| &p384.p384_self_signature[..]),
            x25519_ecdh_public: Some(&self.public.x25519.ecdh),
            x25519_ecdh_secret: Some(&self.x25519.ecdh.secret_bytes().as_bytes()[..]),
            x25519_eddsa_public: Some(&self.public.x25519.eddsa),
            x25519_eddsa_secret: Some(&self.x25519.eddsa.secret_bytes().as_bytes()[..]),
            p384_ecdh_public: self.p384.as_ref().map(|p384| &p384.ecdh.public_key_bytes()[..]),
            p384_ecdh_secret: p384_ecdh_secret.as_ref().map(|s| &s.as_bytes()[..]),
            p384_ecdsa_public: self.p384.as_ref().map(|p384| &p384.ecdsa.public_key_bytes()[..]),
            p384_ecdsa_secret: p384_ecdsa_secret.as_ref().map(|s| &s.as_bytes()[..]),
        }
        .serialize(serializer)
    }
}

impl<'de> Deserialize<'de> for IdentitySecret {
    fn deserialize<D>(deserializer: D) -> Result<IdentitySecret, D::Error>
    where
        D: Deserializer<'de>,
    {
        IdentitySecretForSerialization::deserialize(deserializer).and_then(|tmp| {
            if let (Some(x25519_ecdh_public), Some(x25519_ecdh_secret), Some(x25519_eddsa_public), Some(x25519_eddsa_secret)) = (
                tmp.x25519_ecdh_public,
                tmp.x25519_ecdh_secret,
                tmp.x25519_eddsa_public,
                tmp.x25519_eddsa_secret,
            ) {
                let tmp_p384 = if let (Some(a), Some(b), Some(c), Some(d), Some(e), Some(f)) = (
                    tmp.ed25519_self_signature,
                    tmp.p384_self_signature,
                    tmp.p384_ecdh_public,
                    tmp.p384_ecdh_secret,
                    tmp.p384_ecdsa_public,
                    tmp.p384_ecdsa_secret,
                ) {
                    Some((a, b, c, d, e, f))
                } else {
                    None
                };
                let e2 = || D::Error::custom("invalid key");
                let e = |_e: TryFromSliceError| e2();
                Ok(IdentitySecret {
                    public: Identity {
                        address: Address::from_bytes(tmp.address).map_err(|_| e2())?,
                        x25519: X25519 {
                            ecdh: x25519_ecdh_public.try_into().map_err(e)?,
                            eddsa: x25519_eddsa_public.try_into().map_err(e)?,
                        },
                        p384: if let Some(tmp_p384) = tmp_p384.as_ref() {
                            Some(P384 {
                                ecdh: P384PublicKey::from_bytes(tmp_p384.2).ok_or_else(e2)?,
                                ecdsa: P384PublicKey::from_bytes(tmp_p384.3).ok_or_else(e2)?,
                                ed25519_self_signature: tmp_p384.0.try_into().map_err(e)?,
                                p384_self_signature: tmp_p384.1.try_into().map_err(e)?,
                            })
                        } else {
                            None
                        },
                    }
                    .validate()
                    .ok_or_else(e2)?,
                    x25519: X25519Secret {
                        ecdh: X25519KeyPair::from_bytes(x25519_ecdh_public, x25519_ecdh_secret).ok_or_else(e2)?,
                        eddsa: Ed25519KeyPair::from_bytes(x25519_eddsa_public, x25519_eddsa_secret).ok_or_else(e2)?,
                    },
                    p384: if let Some(tmp_p384) = tmp_p384.as_ref() {
                        Some(P384Secret {
                            ecdh: P384KeyPair::from_bytes(tmp_p384.2, tmp_p384.3).ok_or_else(e2)?,
                            ecdsa: P384KeyPair::from_bytes(tmp_p384.4, tmp_p384.5).ok_or_else(e2)?,
                        })
                    } else {
                        None
                    },
                })
            } else {
                Err(D::Error::custom("missing required fields"))
            }
        })
    }
}

mod packed {
    use super::*;

    pub(super) const V1_PUBLIC_SIZE: usize = 1 + C25519_PUBLIC_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE + 1;
    pub(super) const V2_PUBLIC_SIZE: usize = 1
        + C25519_PUBLIC_KEY_SIZE
        + ED25519_PUBLIC_KEY_SIZE
        + ED25519_SIGNATURE_SIZE
        + P384_PUBLIC_KEY_SIZE
        + P384_PUBLIC_KEY_SIZE
        + P384_ECDSA_SIGNATURE_SIZE;

    #[derive(Clone, Copy)]
    #[repr(C, packed)]
    pub(super) struct V1Public {
        pub short_address: [u8; 5],
        pub algorithms: u8,
        pub c25519: [u8; C25519_PUBLIC_KEY_SIZE],
        pub ed25519: [u8; ED25519_PUBLIC_KEY_SIZE],
        pub secret_bytes: u8,
    }

    #[derive(Clone, Copy)]
    #[repr(C, packed)]
    pub(super) struct V2Public {
        pub short_address: [u8; 5],
        pub algorithms: u8,
        pub c25519: [u8; C25519_PUBLIC_KEY_SIZE],
        pub ed25519: [u8; ED25519_PUBLIC_KEY_SIZE],
        pub p384_ecdh: [u8; P384_PUBLIC_KEY_SIZE],
        pub p384_ecdsa: [u8; P384_PUBLIC_KEY_SIZE],
        pub ed25519_self_signature: [u8; ED25519_SIGNATURE_SIZE],
        pub p384_self_signature: [u8; P384_ECDSA_SIGNATURE_SIZE],
    }

    unsafe impl memory::FlatBuffer for V1Public {}
    unsafe impl memory::FlatBuffer for V2Public {}
}

fn legacy_address_derivation_work_function(digest: &mut [u8; 64]) {
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
