use std::hash::{Hash, Hasher};
use std::io::Write;

use concat_arrays::concat_arrays;

use zerotier_core_crypto::c25519::*;
use zerotier_core_crypto::hash::SHA384;
use zerotier_core_crypto::p521::*;
use zerotier_core_crypto::secret::Secret;

use crate::error::InvalidFormatError;
use crate::vl1::{Endpoint, Identity};
use crate::vl1::buffer::Buffer;
use crate::vl1::protocol::PACKET_SIZE_MAX;

const ROOT_SET_TYPE_LEGACY_PLANET: u8 = 1;
const ROOT_SET_TYPE_LEGACY_MOON: u8 = 127;
const ROOT_SET_TYPE_ED25519_P521: u8 = 128;

/// Root set type.
///
/// Two of these are legacy from ZeroTier V1. The third is a root set signed by both
/// an Ed25519 key and a NIST P-521 key with these keys being bundled together.
#[derive(Clone, PartialEq, Eq)]
pub enum TypeAndID {
    LegacyPlanet(u64),
    LegacyMoon(u64),
    Ed25519P521RootSet([u8; 48]),
}

impl Hash for TypeAndID {
    fn hash<H: Hasher>(&self, state: &mut H) {
        match self {
            Self::LegacyPlanet(id) | Self::LegacyMoon(id) => state.write_u64(*id),
            Self::Ed25519P521RootSet(id) => state.write(id),
        }
    }
}

/// Secret keys that can be used to update root sets after creation.
pub struct RootSetSecretKeys {
    ed25519: Ed25519KeyPair,
    p521: P521KeyPair,
}

impl RootSetSecretKeys {
    const SECRET_BYTES_LEN: usize = 1 + ED25519_PUBLIC_KEY_SIZE + ED25519_SECRET_KEY_SIZE + P521_PUBLIC_KEY_SIZE + P521_SECRET_KEY_SIZE;
    const PUBLIC_BYTES_LEN: usize = 1 + ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE;

    /// Generate a new set of root set secret keys.
    pub fn generate() -> Self {
        Self {
            ed25519: Ed25519KeyPair::generate(false),
            p521: P521KeyPair::generate(false).unwrap(),
        }
    }

    /// Reconstruct from bytes as returned by to_secret_bytes() or return None if invalid.
    pub fn from_bytes(b: &[u8]) -> Option<Self> {
        if b.len() == Self::SECRET_BYTES_LEN && b[0] == ROOT_SET_TYPE_ED25519_P521 {
            let ed25519 = Ed25519KeyPair::from_bytes(&b[1..ED25519_PUBLIC_KEY_SIZE + 1], &b[1 + ED25519_PUBLIC_KEY_SIZE..1 + ED25519_PUBLIC_KEY_SIZE + ED25519_SECRET_KEY_SIZE]);
            let p521 = P521KeyPair::from_bytes(&b[1 + ED25519_PUBLIC_KEY_SIZE + ED25519_SECRET_KEY_SIZE..1 + ED25519_PUBLIC_KEY_SIZE + ED25519_SECRET_KEY_SIZE + P521_PUBLIC_KEY_SIZE], &b[1 + ED25519_PUBLIC_KEY_SIZE + ED25519_SECRET_KEY_SIZE + P521_PUBLIC_KEY_SIZE..]);
            if ed25519.is_none() || p521.is_none() {
                None
            } else {
                Some(Self {
                    ed25519: ed25519.unwrap(),
                    p521: p521.unwrap(),
                })
            }
        } else {
            None
        }
    }

    /// Get both public and secret keys in byte format.
    pub fn to_secret_bytes(&self) -> Secret<{ Self::SECRET_BYTES_LEN }> {
        Secret(concat_arrays!([ROOT_SET_TYPE_ED25519_P521], self.ed25519.public_bytes(), self.ed25519.secret_bytes().0, *self.p521.public_key_bytes(), self.p521.secret_key_bytes().0))
    }

    /// Get only public keys in byte format.
    pub fn to_public_bytes(&self) -> [u8; ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE] {
        concat_arrays!(self.ed25519.public_bytes(), *self.p521.public_key_bytes())
    }
}

/// A single root node with static endpoints where it can be reached.
#[derive(PartialEq, Eq, PartialOrd, Ord)]
pub struct Root {
    /// Root node ZeroTier identity.
    pub identity: Identity,

    /// Static endpoints at which this root node may be reached.
    pub endpoints: Vec<Endpoint>,
}

/// A signed bundle of root nodes.
///
/// In v1 this was the "World" object.
///
/// This is how roots are normally specified to nodes. The embedded signing key allows the
/// root set to be updated automatically. Updates can add, remove, or change the endpoints
/// of roots, allowing infrastructure updates with automatic client configuration as long
/// as at least one of the old roots is up to distribute the new ones.
#[derive(PartialEq, Eq)]
pub struct RootSet {
    pub id: TypeAndID,
    pub timestamp: i64,
    pub url: String,
    pub roots: Vec<Root>,
    pub signer: Vec<u8>,
    pub signature: Vec<u8>,
}

impl RootSet {
    pub const MAX_ROOTS: usize = u8::MAX as usize;
    pub const MAX_ENDPOINTS_PER_ROOT: usize = u8::MAX as usize;

    /// Shortcut to copy a byte array to a Buffer and unmarshal().
    pub fn from_bytes(bytes: &[u8]) -> std::io::Result<RootSet> {
        let mut tmp: Buffer<{ PACKET_SIZE_MAX }> = Buffer::new();
        tmp.append_bytes(bytes)?;
        let mut c: usize = 0;
        RootSet::unmarshal(&tmp, &mut c)
    }

    /// Sign this root set and return true on success.
    pub fn sign(&mut self, keys: &RootSetSecretKeys) -> bool {
        self.signer = keys.to_public_bytes().to_vec();
        self.id = TypeAndID::Ed25519P521RootSet(SHA384::hash(self.signer.as_slice()));

        let mut buf: Buffer<{ PACKET_SIZE_MAX }> = Buffer::new();
        if self.marshal_internal(&mut buf, true).is_err() {
            return false;
        }
        let ed25519 = keys.ed25519.sign(buf.as_bytes());
        let p521 = keys.p521.sign(buf.as_bytes());
        if p521.is_none() {
            return false;
        }
        let p521 = p521.unwrap();

        self.signature.clear();
        let _ = self.signature.write_all(&ed25519);
        let _ = self.signature.write_all(&p521);

        true
    }

    fn marshal_internal<const BL: usize>(&self, buf: &mut Buffer<BL>, for_signing: bool) -> std::io::Result<()> {
        if self.roots.len() > u8::MAX as usize {
            return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "maximum roots per root set: 255"));
        }

        if for_signing {
            buf.append_u64(0x7f7f7f7f7f7f7f7f)?;
        }

        match &self.id {
            TypeAndID::LegacyPlanet(id) | TypeAndID::LegacyMoon(id) => {
                buf.append_u8(if matches!(self.id, TypeAndID::LegacyPlanet(_)) {
                    ROOT_SET_TYPE_LEGACY_PLANET
                } else {
                    ROOT_SET_TYPE_LEGACY_MOON
                })?;
                buf.append_u64(*id)?;
                buf.append_u64(self.timestamp as u64)?;
                if self.signer.len() != 64 {
                    return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "legacy signer can only be 64 bytes"));
                }
                buf.append_bytes(self.signer.as_slice())?;
                if !for_signing {
                    if self.signature.len() != 96 {
                        return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "legacy signature can only be 96 bytes"));
                    }
                    buf.append_bytes(self.signature.as_slice())?;
                }
            }

            TypeAndID::Ed25519P521RootSet(_) => {
                buf.append_u8(ROOT_SET_TYPE_ED25519_P521)?;
                buf.append_varint(self.timestamp as u64)?;
                let url = self.url.as_bytes();
                buf.append_varint(url.len() as u64)?;
                buf.append_bytes(url)?;
                if self.signer.len() != (ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE) {
                    return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "signer can only be 164 bytes"));
                }
                buf.append_bytes(self.signer.as_slice())?;
                if !for_signing {
                    if self.signature.len() != (ED25519_SIGNATURE_SIZE + P521_ECDSA_SIGNATURE_SIZE) {
                        return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "signature can only be 192 bytes"));
                    }
                    buf.append_bytes(self.signature.as_slice())?;
                }
            }
        }

        buf.append_u8(self.roots.len() as u8)?;
        for root in self.roots.iter() {
            root.identity.marshal(buf, false)?;
            if root.endpoints.len() > u8::MAX as usize {
                return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "maximum endpoints per root: 255"));
            }
            buf.append_u8(root.endpoints.len() as u8)?;
            for ep in root.endpoints.iter() {
                ep.marshal(buf)?;
            }
        }

        if matches!(self.id, TypeAndID::LegacyMoon(_)) {
            buf.append_u8(0)?;
        }

        if for_signing {
            buf.append_u64(0x7f7f7f7f7f7f7f7f)?;
        }

        Ok(())
    }

    #[inline(always)]
    pub fn marshal<const BL: usize>(&self, buf: &mut Buffer<BL>) -> std::io::Result<()> {
        self.marshal_internal(buf, false)
    }

    pub fn unmarshal<const BL: usize>(buf: &Buffer<BL>, cursor: &mut usize) -> std::io::Result<Self> {
        let read_roots = |buf: &Buffer<BL>, cursor: &mut usize| -> std::io::Result<Vec<Root>> {
            let root_count = buf.read_u8(cursor)? as usize;
            let mut roots = Vec::<Root>::with_capacity(root_count);
            for _ in 0..root_count {
                let identity = Identity::unmarshal(buf, cursor)?;
                let endpoint_count = buf.read_u8(cursor)? as usize;
                let mut endpoints = Vec::<Endpoint>::with_capacity(endpoint_count);
                for _ in 0..endpoint_count {
                    endpoints.push(Endpoint::unmarshal(buf, cursor)?);
                }
                roots.push(Root { identity, endpoints });
            }
            Ok(roots)
        };

        let type_id = buf.read_u8(cursor)?;
        match type_id {
            ROOT_SET_TYPE_LEGACY_PLANET | ROOT_SET_TYPE_LEGACY_MOON => {
                let id = buf.read_u64(cursor)?;
                let id = if type_id == ROOT_SET_TYPE_LEGACY_PLANET { TypeAndID::LegacyPlanet(id) } else { TypeAndID::LegacyMoon(id) };
                let timestamp = buf.read_u64(cursor)?;
                let signer = buf.read_bytes(64, cursor)?.to_vec();
                let signature = buf.read_bytes(96, cursor)?.to_vec();
                let roots = read_roots(buf, cursor)?;
                if type_id == ROOT_SET_TYPE_LEGACY_MOON {
                    *cursor += buf.read_u8(cursor)? as usize;
                }
                Ok(Self {
                    id,
                    timestamp: timestamp as i64,
                    url: String::new(),
                    roots,
                    signer,
                    signature,
                })
            }

            ROOT_SET_TYPE_ED25519_P521 => {
                let timestamp = buf.read_varint(cursor)? as i64;
                let url = String::from_utf8_lossy(buf.read_bytes(buf.read_varint(cursor)? as usize, cursor)?).to_string();
                let signer = buf.read_bytes(ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE, cursor)?.to_vec();
                let signature = buf.read_bytes(ED25519_SIGNATURE_SIZE + P521_ECDSA_SIGNATURE_SIZE, cursor)?.to_vec();
                let id = TypeAndID::Ed25519P521RootSet(SHA384::hash(signer.as_slice()));
                Ok(Self {
                    id,
                    timestamp,
                    url,
                    roots: read_roots(buf, cursor)?,
                    signer,
                    signature,
                })
            }

            _ => Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "unrecognized type"))
        }
    }
}

impl Hash for RootSet {
    fn hash<H: Hasher>(&self, state: &mut H) {
        state.write_u64(self.timestamp as u64);
        self.id.hash(state);
    }
}

#[cfg(test)]
mod tests {
    use crate::vl1::rootset::RootSet;

    #[test]
    fn default_root_set() {
        let rs = RootSet::from_bytes(&crate::defaults::ROOT_SET).unwrap();
        rs.roots.iter().for_each(|r| {
            println!("{}", r.identity.to_string());
            r.endpoints.iter().for_each(|ep| {
                println!("  {}", ep.to_string());
            });
        });
    }
}
