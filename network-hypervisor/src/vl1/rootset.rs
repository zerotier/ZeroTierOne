use std::collections::BTreeSet;
use std::hash::{Hash, Hasher};
use std::io::Write;

use concat_arrays::concat_arrays;

use crate::crypto::c25519::{ED25519_PUBLIC_KEY_SIZE, ED25519_SECRET_KEY_SIZE, ED25519_SIGNATURE_SIZE, ed25519_verify, Ed25519KeyPair};
use crate::crypto::hash::SHA384;
use crate::crypto::p521::{P521_ECDSA_SIGNATURE_SIZE, P521_PUBLIC_KEY_SIZE, P521_SECRET_KEY_SIZE, P521KeyPair};
use crate::crypto::secret::Secret;
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
pub enum Type {
    LegacyPlanet(u64),
    LegacyMoon(u64),
    Ed25519P521RootSet([u8; 48]),
}

impl Hash for Type {
    fn hash<H: Hasher>(&self, state: &mut H) {
        match self {
            Self::LegacyPlanet(id) => state.write_u64(*id),
            Self::LegacyMoon(id) => state.write_u64(*id),
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
    pub endpoints: BTreeSet<Endpoint>,
}

/// A signed bundle of root nodes.
///
/// This is how roots are normally specified to nodes. The embedded signing key allows the
/// root set to be updated automatically. Updates can add, remove, or change the endpoints
/// of roots, allowing infrastructure updates with automatic client configuration as long
/// as at least one of the old roots is up to distribute the new ones.
#[derive(PartialEq, Eq)]
pub struct RootSet {
    pub timestamp: i64,
    pub name: String,
    pub contact: String,
    pub roots: BTreeSet<Root>,
    signer: Vec<u8>,
    signature: Vec<u8>,
    root_set_type: Type,
}

impl RootSet {
    pub const MAX_ROOTS: usize = u8::MAX as usize;
    pub const MAX_ENDPOINTS_PER_ROOT: usize = u8::MAX as usize;

    /// Sign this root set and return true on success.
    /// The fields timestamp, name, contact, and roots must have been set. The signer, signature, and type will be set.
    /// This can only sign new format root sets. Legacy "planet" and "moon" root sets can be used by V2 but
    /// cannot be created by this code.
    pub fn sign(&mut self, keys: &RootSetSecretKeys) -> bool {
        self.signer = keys.to_public_bytes().to_vec();
        self.root_set_type = Type::Ed25519P521RootSet(SHA384::hash(self.signer.as_slice()));

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

        let name = self.name.as_bytes();
        let contact = self.contact.as_bytes();
        if name.len() > u8::MAX as usize {
            return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "maximum roots per root set: 255"));
        }
        if contact.len() > u8::MAX as usize {
            return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "maximum roots per root set: 255"));
        }

        if for_signing {
            buf.append_u64(0x7f7f7f7f7f7f7f7f)?;
        }

        match &self.root_set_type {
            Type::LegacyPlanet(id) | Type::LegacyMoon(id) => {
                buf.append_u8(if matches!(self.root_set_type, Type::LegacyPlanet(_)) {
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

            Type::Ed25519P521RootSet(_) => {
                buf.append_u8(ROOT_SET_TYPE_ED25519_P521)?;
                buf.append_u64(self.timestamp as u64)?;
                buf.append_u8(name.len() as u8)?;
                buf.append_bytes(name);
                buf.append_u8(contact.len() as u8)?;
                buf.append_bytes(contact);
                if self.signer.len() != (ED25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE) {
                    return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "signer can only be 164 bytes"));
                }
                buf.append_u8(self.signer.len() as u8)?;
                buf.append_bytes(self.signer.as_slice())?;
                if !for_signing {
                    if self.signature.len() != (ED25519_SIGNATURE_SIZE + P521_ECDSA_SIGNATURE_SIZE) {
                        return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "signature can only be 192 bytes"));
                    }
                    buf.append_u8(self.signature.len() as u8)?;
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

        if matches!(self.root_set_type, Type::LegacyMoon(_)) {
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
        let read_roots = |buf: &Buffer<BL>, cursor: &mut usize| -> std::io::Result<BTreeSet<Root>> {
            let mut roots = BTreeSet::<Root>::new();
            let root_count = buf.read_u8(cursor)? as usize;
            for _ in 0..root_count {
                let identity = Identity::unmarshal(buf, cursor)?;
                let mut endpoints = BTreeSet::<Endpoint>::new();
                let endpoint_count = buf.read_u8(cursor)? as usize;
                for _ in 0..endpoint_count {
                    endpoints.insert(Endpoint::unmarshal(buf, cursor)?);
                }
                roots.insert(Root {
                    identity,
                    endpoints
                });
            }
            Ok(roots)
        };

        let type_id = buf.read_u8(cursor)?;
        match type_id {
            ROOT_SET_TYPE_LEGACY_PLANET | ROOT_SET_TYPE_LEGACY_MOON => {
                let root_set_type = if type_id == ROOT_SET_TYPE_LEGACY_PLANET {
                    Type::LegacyPlanet(buf.read_u64(cursor)?)
                } else {
                    Type::LegacyMoon(buf.read_u64(cursor)?)
                };
                let timestamp = buf.read_u64(cursor)?;
                let signer = buf.read_bytes(64, cursor)?.to_vec();
                let signature = buf.read_bytes(96, cursor)?.to_vec();
                let roots = read_roots(buf, cursor)?;
                if type_id == ROOT_SET_TYPE_LEGACY_MOON {
                    *cursor += buf.read_u8(cursor)? as usize;
                }
                Ok(Self {
                    timestamp: timestamp as i64,
                    name: String::new(),
                    contact: String::new(),
                    roots,
                    signer,
                    signature,
                    root_set_type,
                })
            }

            ROOT_SET_TYPE_ED25519_P521 => {
                let timestamp = buf.read_u64(cursor)?;
                let name = String::from_utf8_lossy(buf.read_bytes(buf.read_u8(cursor)? as usize, cursor)?).to_string();
                let contact = String::from_utf8_lossy(buf.read_bytes(buf.read_u8(cursor)? as usize, cursor)?).to_string();
                let signer = buf.read_bytes(buf.read_u8(cursor)? as usize, cursor)?.to_vec();
                let signature = buf.read_bytes(buf.read_u8(cursor)? as usize, cursor)?.to_vec();
                let root_set_type = Type::Ed25519P521RootSet(SHA384::hash(signer.as_slice()));
                Ok(Self {
                    timestamp: timestamp as i64,
                    name,
                    contact,
                    roots: read_roots(buf, cursor)?,
                    signer,
                    signature,
                    root_set_type,
                })
            }

            _ => {
                Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "unrecognized type"))
            }
        }
    }

    /// Get this root set's globally unique ID.
    ///
    /// For new root set format this is a hash of its public keys. For old style planet/moon
    /// this is a user assigned 64-bit ID. The latter is deprecated but still supported.
    pub fn id(&self) -> Vec<u8> {
        match self.root_set_type {
            Type::LegacyPlanet(id) => id.to_be_bytes().to_vec(),
            Type::LegacyMoon(id) => id.to_be_bytes().to_vec(),
            Type::Ed25519P521RootSet(id) => id.to_vec(),
        }
    }
}

impl Hash for RootSet {
    fn hash<H: Hasher>(&self, state: &mut H) {
        state.write_u64(self.timestamp as u64);
        self.root_set_type.hash(state);
    }
}
