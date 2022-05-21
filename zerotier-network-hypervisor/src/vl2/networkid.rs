// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::hash::{Hash, Hasher};
use std::num::NonZeroU64;
use std::str::FromStr;

use serde::{Deserialize, Deserializer, Serialize, Serializer};

use crate::error::InvalidFormatError;
use crate::util::buffer::Buffer;
use crate::util::hex::HEX_CHARS;

#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
#[repr(transparent)]
pub struct NetworkId(NonZeroU64);

impl NetworkId {
    #[inline(always)]
    pub fn from_u64(i: u64) -> Option<NetworkId> {
        NonZeroU64::new(i).map(|i| Self(i))
    }

    #[inline(always)]
    pub fn from_bytes(b: &[u8]) -> Option<NetworkId> {
        if b.len() >= 8 {
            Self::from_bytes_fixed(b[0..8].try_into().unwrap())
        } else {
            None
        }
    }

    #[inline(always)]
    pub fn from_bytes_fixed(b: &[u8; 8]) -> Option<NetworkId> {
        Self::from_u64(u64::from_be_bytes(*b))
    }

    #[inline(always)]
    pub fn to_bytes(&self) -> [u8; 8] {
        self.0.get().to_be_bytes()
    }

    #[inline(always)]
    pub fn to_u64(&self) -> u64 {
        self.0.get()
    }

    #[inline(always)]
    pub(crate) fn marshal<const BL: usize>(&self, buf: &mut Buffer<BL>) -> std::io::Result<()> {
        buf.append_u64(self.0.get())
    }

    #[inline(always)]
    pub(crate) fn unmarshal<const BL: usize>(buf: &Buffer<BL>, cursor: &mut usize) -> std::io::Result<Option<Self>> {
        Ok(Self::from_u64(buf.read_u64(cursor)?))
    }
}

impl ToString for NetworkId {
    fn to_string(&self) -> String {
        let mut v = self.0.get();
        let mut s = String::with_capacity(16);
        for _ in 0..16 {
            s.push(HEX_CHARS[(v >> 60) as usize] as char);
            v <<= 4;
        }
        s
    }
}

impl FromStr for NetworkId {
    type Err = InvalidFormatError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        NetworkId::from_bytes(crate::util::hex::from_string(s).as_slice()).map_or_else(|| Err(InvalidFormatError), |a| Ok(a))
    }
}

impl Hash for NetworkId {
    #[inline(always)]
    fn hash<H: Hasher>(&self, state: &mut H) {
        state.write_u64(self.0.get());
    }
}

impl Serialize for NetworkId {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        if serializer.is_human_readable() {
            serializer.serialize_str(self.to_string().as_str())
        } else {
            serializer.serialize_bytes(&self.to_bytes())
        }
    }
}

struct NetworkIdVisitor;

impl<'de> serde::de::Visitor<'de> for NetworkIdVisitor {
    type Value = NetworkId;

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("a ZeroTier network ID")
    }

    fn visit_bytes<E>(self, v: &[u8]) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        if v.len() == 8 {
            NetworkId::from_bytes(v).map_or_else(|| Err(E::custom("object too large")), |a| Ok(a))
        } else {
            Err(E::custom("object too large"))
        }
    }

    fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        NetworkId::from_str(v).map_err(|e| E::custom(e.to_string()))
    }
}

impl<'de> Deserialize<'de> for NetworkId {
    fn deserialize<D>(deserializer: D) -> Result<NetworkId, D::Error>
    where
        D: Deserializer<'de>,
    {
        if deserializer.is_human_readable() {
            deserializer.deserialize_str(NetworkIdVisitor)
        } else {
            deserializer.deserialize_bytes(NetworkIdVisitor)
        }
    }
}
