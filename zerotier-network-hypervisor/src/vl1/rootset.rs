/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use crate::vl1::{Identity, Endpoint, Dictionary};
use crate::vl1::protocol::PACKET_SIZE_MAX;
use crate::util::buffer::Buffer;
use crate::error::InvalidParameterError;

use zerotier_core_crypto::c25519::{ED25519_SECRET_KEY_SIZE, ED25519_PUBLIC_KEY_SIZE, ED25519_SIGNATURE_SIZE, Ed25519KeyPair, ed25519_verify};
use zerotier_core_crypto::p521::{P521_SECRET_KEY_SIZE, P521_PUBLIC_KEY_SIZE, P521_ECDSA_SIGNATURE_SIZE, P521KeyPair, P521PublicKey};
use zerotier_core_crypto::secret::Secret;
use zerotier_core_crypto::hash::SHA384;

use std::cmp::Ordering;
use crate::vl1::identity::IDENTITY_CIPHER_SUITE_INCLUDE_ALL;

/// Old "planet" type with Ed25519 authenticated updates from ZeroTier v1.
const ROOT_SET_TYPE_LEGACY_PLANET: u8 = 1;

/// Old "moon" type, basically the same as "planet" as far as we are concerned.
const ROOT_SET_TYPE_LEGACY_MOON: u8 = 127;

/// New V2 type with FIPS-compliant extra signing key.
const ROOT_SET_TYPE_P521_ED25519: u8 = 128;

const ROOT_SET_META_DATA_OOB_UPDATE_URL: &'static str = "U";
const ROOT_SET_META_DATA_NAME: &'static str = "N";
const ROOT_SET_META_DATA_CONTACT: &'static str = "C";

/// A single root node with static endpoints where it can be reached.
#[derive(Clone, PartialEq, Eq)]
pub struct Root {
    pub identity: Identity,
    pub endpoints: Vec<Endpoint>,
}

impl PartialOrd for Root {
    #[inline(always)]
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for Root {
    fn cmp(&self, other: &Self) -> Ordering {
        let idcmp = self.identity.cmp(&other.identity);
        if idcmp.is_eq() {
            self.endpoints.cmp(&other.endpoints)
        } else {
            idcmp
        }
    }
}

/// Secret key for creating root set updates.
pub struct RootSetSecretSigningKey {
    ed25519_secret: Ed25519KeyPair,
    p521_secret: P521KeyPair,
}

/// A bundle of roots that can be updated.
#[derive(Clone)]
pub struct RootSet {
    type_: u8,

    /// Unique ID of root set.
    pub id: [u8; 48],

    /// Roots and static endpoints.
    pub roots: Vec<Root>,

    /// Timestamp / revision number.
    pub timestamp: i64,

    /// Additional meta-data fields.
    pub metadata: Dictionary,

    ed25519_signing_key: [u8; 32 + ED25519_PUBLIC_KEY_SIZE], // LEGACY: first 32 bytes are ignored
    ed25519_signature: [u8; 96], // LEGACY: old signatures have a hash appended
    p521_signing_key: [u8; P521_PUBLIC_KEY_SIZE],
    p521_signature: [u8; P521_ECDSA_SIGNATURE_SIZE],
}

impl RootSet {
    /// Create a new root set populated with compiled-in ZeroTier defaults.
    pub fn zerotier_default() -> Self {
        Self::from_bytes(include_bytes!("./rootset-default.bin")).expect("invalid compiled-in default root set")
    }

    /// Create and sign a new root set.
    /// This cannot create legacy "planet" or "moon" type root sets. For those the old mkworld code must be used.
    pub fn create(roots: &[Root], timestamp: i64, oob_update_url: Option<&str>, name: Option<&str>, contact: Option<&str>, signing_key: &RootSetSecretSigningKey) -> Result<Self, InvalidParameterError> {
        let mut sorted_roots = roots.to_vec();
        sorted_roots.sort();
        sorted_roots.dedup();
        if sorted_roots.is_empty() {
            return Err(InvalidParameterError("empty root list"));
        }
        for r in sorted_roots.iter_mut() {
            r.endpoints.sort();
            r.endpoints.dedup();
            if r.endpoints.is_empty() {
                return Err(InvalidParameterError("at least one root has an empty endpoint list"));
            }
        }

        let mut md = Dictionary::new();
        let _ = oob_update_url.map(|s| md.set_str(ROOT_SET_META_DATA_OOB_UPDATE_URL, s));
        let _ = name.map(|s| md.set_str(ROOT_SET_META_DATA_NAME, s));
        let _ = contact.map(|s| md.set_str(ROOT_SET_META_DATA_CONTACT, s));

        let mut rs = RootSet {
            type_: ROOT_SET_TYPE_P521_ED25519,
            id: {
                let mut h = SHA384::new();
                h.update(&signing_key.ed25519_secret.public_bytes());
                h.update(signing_key.p521_secret.public_key_bytes());
                h.finish()
            },
            roots: sorted_roots,
            timestamp,
            metadata: md,
            ed25519_signing_key: {
                let mut tmp = [0_u8; 64];
                tmp[32..64].copy_from_slice(&signing_key.ed25519_secret.public_bytes());
                tmp
            },
            ed25519_signature: [0_u8; 96],
            p521_signing_key: signing_key.p521_secret.public_key_bytes().clone(),
            p521_signature: [0_u8; P521_ECDSA_SIGNATURE_SIZE],
        };

        let mut signing_buf: Buffer<{ PACKET_SIZE_MAX }> = Buffer::new();
        let _ = rs.marshal_internal(&mut signing_buf, true).expect("internal error marshaling for signature");
        rs.ed25519_signature = signing_key.ed25519_secret.sign_zt(signing_buf.as_bytes());
        rs.p521_signature = signing_key.p521_secret.sign(signing_buf.as_bytes()).expect("error signing root set with ECDSA");

        Ok(rs)
    }

    fn marshal_internal<const BL: usize>(&self, buf: &mut Buffer<BL>, for_signing: bool) -> std::io::Result<()> {
        if for_signing {
            buf.append_u64(0x7f7f7f7f7f7f7f7f)?;
        }

        buf.append_u8(self.type_)?;
        if self.type_ == ROOT_SET_TYPE_LEGACY_PLANET || self.type_ == ROOT_SET_TYPE_LEGACY_MOON {
            buf.append_bytes(&self.id[0..8]);
        } else {
            buf.append_bytes_fixed(&self.id);
        }
        buf.append_u64(self.timestamp as u64)?;
        buf.append_bytes_fixed(&self.ed25519_signing_key)?;
        if !for_signing {
            buf.append_bytes_fixed(&self.ed25519_signature)?;
        }

        if self.type_ == ROOT_SET_TYPE_LEGACY_PLANET || self.type_ == ROOT_SET_TYPE_LEGACY_MOON {
            if self.roots.len() > 4 as usize || self.id.len() != 8 {
                return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid legacy type root set"));
            }
        } else if self.type_ == ROOT_SET_TYPE_P521_ED25519 {
            buf.append_bytes_fixed(&self.p521_signing_key);
            if !for_signing {
                buf.append_bytes_fixed(&self.p521_signature);
            }
        } else {
            return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid root set type"));
        }

        buf.append_varint(self.roots.len() as u64)?;
        for root in self.roots.iter() {
            root.identity.marshal(buf, IDENTITY_CIPHER_SUITE_INCLUDE_ALL, false)?;
            if (self.type_ == ROOT_SET_TYPE_LEGACY_PLANET || self.type_ == ROOT_SET_TYPE_LEGACY_MOON) && root.endpoints.len() > 127 {
                return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid legacy type root set"));
            }
            buf.append_varint(root.endpoints.len() as u64)?;
            for ep in root.endpoints.iter() {
                ep.marshal(buf)?;
            }
        }

        if self.type_ == ROOT_SET_TYPE_LEGACY_MOON {
            buf.append_u16(0)?;
        } else if self.type_ == ROOT_SET_TYPE_P521_ED25519 {
            let mdb = self.metadata.to_bytes();
            buf.append_varint(mdb.len() as u64)?;
            buf.append_bytes(mdb.as_slice())?;
        }

        if for_signing {
            buf.append_u64(0x7f7f7f7f7f7f7f7f)?;
        }

        Ok(())
    }

    /// Shortcut to copy a byte array to a Buffer and unmarshal().
    pub fn from_bytes(bytes: &[u8]) -> std::io::Result<RootSet> {
        let mut tmp: Buffer<{ PACKET_SIZE_MAX }> = Buffer::new();
        tmp.append_bytes(bytes)?;
        let mut c: usize = 0;
        RootSet::unmarshal(&tmp, &mut c)
    }

    #[inline(always)]
    pub fn marshal<const BL: usize>(&self, buf: &mut Buffer<BL>) -> std::io::Result<()> {
        self.marshal_internal(buf, false)
    }

    pub fn unmarshal<const BL: usize>(buf: &Buffer<BL>, cursor: &mut usize) -> std::io::Result<RootSet> {
        let type_ = buf.read_u8(cursor)?;
        if type_ != ROOT_SET_TYPE_LEGACY_PLANET && type_ != ROOT_SET_TYPE_LEGACY_MOON && type_ != ROOT_SET_TYPE_P521_ED25519 {
            return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "unrecognized root set type"));
        }

        let is_legacy = type_ == ROOT_SET_TYPE_LEGACY_PLANET || type_ == ROOT_SET_TYPE_LEGACY_MOON;

        let mut id = [0_u8; 48];
        if is_legacy {
            id[0..8].copy_from_slice(buf.read_bytes_fixed::<8>(cursor)?);
        } else {
            id.copy_from_slice(buf.read_bytes_fixed::<48>(cursor)?);
        }
        let timestamp = buf.read_u64(cursor)? as i64;
        let ed25519_signing_key = buf.read_bytes_fixed::<{ 32 + ED25519_PUBLIC_KEY_SIZE }>(cursor)?;
        let ed25519_signature = buf.read_bytes_fixed::<96>(cursor)?;
        let mut p521_signing_key = [0_u8; P521_PUBLIC_KEY_SIZE];
        let mut p521_signature = [0_u8; P521_ECDSA_SIGNATURE_SIZE];
        let mut root_count;
        if type_ == ROOT_SET_TYPE_P521_ED25519 {
            p521_signing_key.copy_from_slice(buf.read_bytes_fixed::<P521_PUBLIC_KEY_SIZE>(cursor)?);
            p521_signature.copy_from_slice(buf.read_bytes_fixed::<P521_ECDSA_SIGNATURE_SIZE>(cursor)?);
            root_count = buf.read_varint(cursor)? as usize;
        } else {
            root_count = buf.read_u8(cursor)? as usize;
        }

        if is_legacy && root_count > 4 {
            return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid legacy type root set (max roots: 4)"));
        }
        let mut roots: Vec<Root> = Vec::new();
        for _ in 0..root_count {
            let id = Identity::unmarshal(buf, cursor)?;
            let endpoint_count = if is_legacy {
                buf.read_u8(cursor)? as usize
            } else {
                buf.read_varint(cursor)? as usize
            };
            if is_legacy && endpoint_count > 127 {
                return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid legacy type root set (max endpoints per root: 127)"));
            }
            roots.push(Root {
                identity: id,
                endpoints: {
                    let mut ep: Vec<Endpoint> = Vec::new();
                    for _ in 0..endpoint_count {
                        ep.push(Endpoint::unmarshal(buf, cursor)?);
                    }
                    ep
                }
            })
        }

        let mut dict = None;
        if type_ == ROOT_SET_TYPE_LEGACY_MOON {
            *cursor += buf.read_u16(cursor)? as usize;
        } else if type_ == ROOT_SET_TYPE_P521_ED25519 {
            let dict_size = buf.read_varint(cursor)? as usize;
            if dict_size > 0 {
                dict = Dictionary::from_bytes(buf.read_bytes(dict_size, cursor)?);
                if dict.is_none() {
                    return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid meta-data dictionary"));
                }
            }
        }

        Ok(RootSet {
            type_,
            id,
            roots,
            timestamp,
            metadata: dict.unwrap_or_else(|| Dictionary::new()),
            ed25519_signing_key: ed25519_signing_key.clone(),
            ed25519_signature: ed25519_signature.clone(),
            p521_signing_key,
            p521_signature
        })
    }

    /// Test whether this root set should replace another root set with the same unique ID.
    pub fn should_replace(&self, other: &RootSet) -> bool {
        if self.type_ == other.type_ && self.timestamp > other.timestamp && self.id.eq(&other.id) {
            if self.type_ == ROOT_SET_TYPE_LEGACY_PLANET || self.type_ == ROOT_SET_TYPE_LEGACY_MOON {
                self.ed25519_signing_key.eq(&other.ed25519_signing_key)
            } else if self.type_ == ROOT_SET_TYPE_P521_ED25519 {
                self.ed25519_signing_key.eq(&other.ed25519_signing_key) && self.p521_signing_key.eq(&other.p521_signing_key)
            } else {
                false
            }
        } else {
            false
        }
    }

    /// Verify this root set's signatures.
    pub fn verify_signatures(&self) -> bool {
        let mut signing_buf: Buffer<{ PACKET_SIZE_MAX }> = Buffer::new();
        if self.marshal_internal(&mut signing_buf, true).is_ok() {
            if ed25519_verify(&self.ed25519_signing_key[32..64], &self.ed25519_signature[0..64], signing_buf.as_bytes()) {
                if self.type_ != ROOT_SET_TYPE_P521_ED25519 {
                    P521PublicKey::from_bytes(&self.p521_signing_key).map_or(false, |p521| p521.verify(signing_buf.as_bytes(), &self.p521_signature))
                } else {
                    true
                }
            } else {
                false
            }
        } else {
            false
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::vl1::rootset::RootSet;

    /*
    #[test]
    fn default_root_set() {
        let rs = RootSet::zerotier_default();
        rs.roots.iter().for_each(|r| {
            println!("{}", r.identity.to_string());
            r.endpoints.iter().for_each(|ep| {
                println!("  {}", ep.to_string());
            });
        });
    }
    */
}
