// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::fmt::Debug;
use std::hash::{Hash, Hasher};
use std::num::NonZeroU64;
use std::str::FromStr;

use serde::{Deserialize, Deserializer, Serialize, Serializer};

use crate::error::InvalidFormatError;
use crate::util::buffer::Buffer;
use crate::util::marshalable::Marshalable;

use zerotier_utils::hex;

/// An Ethernet MAC address.
#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
#[repr(transparent)]
pub struct MAC(NonZeroU64);

impl Debug for MAC {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(&self.to_string())
    }
}

impl MAC {
    #[inline(always)]
    pub fn from_u64(i: u64) -> Option<MAC> {
        NonZeroU64::new(i & 0xffffffffffff).map(|i| MAC(i))
    }

    #[inline(always)]
    pub fn from_bytes(b: &[u8]) -> Option<MAC> {
        if b.len() >= 6 {
            NonZeroU64::new(
                (b[0] as u64) << 40
                    | (b[1] as u64) << 32
                    | (b[2] as u64) << 24
                    | (b[3] as u64) << 16 as u64
                    | (b[4] as u64) << 8
                    | b[5] as u64,
            )
            .map(|i| MAC(i))
        } else {
            None
        }
    }

    #[inline(always)]
    pub fn from_bytes_fixed(b: &[u8; 6]) -> Option<MAC> {
        NonZeroU64::new(
            (b[0] as u64) << 40 | (b[1] as u64) << 32 | (b[2] as u64) << 24 | (b[3] as u64) << 16 as u64 | (b[4] as u64) << 8 | b[5] as u64,
        )
        .map(|i| MAC(i))
    }

    #[inline(always)]
    pub fn to_bytes(&self) -> [u8; 6] {
        let i = self.0.get();
        [
            (i >> 40) as u8,
            (i >> 32) as u8,
            (i >> 24) as u8,
            (i >> 16) as u8,
            (i >> 8) as u8,
            i as u8,
        ]
    }
}

impl From<MAC> for u64 {
    #[inline(always)]
    fn from(m: MAC) -> Self {
        m.0.get()
    }
}

impl From<&MAC> for u64 {
    #[inline(always)]
    fn from(m: &MAC) -> Self {
        m.0.get()
    }
}

impl Marshalable for MAC {
    const MAX_MARSHAL_SIZE: usize = 6;

    #[inline(always)]
    fn marshal<const BL: usize>(&self, buf: &mut Buffer<BL>) -> std::io::Result<()> {
        buf.append_bytes(&self.0.get().to_be_bytes()[2..])
    }

    #[inline(always)]
    fn unmarshal<const BL: usize>(buf: &Buffer<BL>, cursor: &mut usize) -> std::io::Result<Self> {
        Self::from_bytes_fixed(buf.read_bytes_fixed(cursor)?).map_or_else(
            || Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "cannot be zero")),
            |a| Ok(a),
        )
    }
}

impl ToString for MAC {
    fn to_string(&self) -> String {
        let b: [u8; 6] = self.to_bytes();
        format!(
            "{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}",
            b[0], b[1], b[2], b[3], b[4], b[5]
        )
    }
}

impl FromStr for MAC {
    type Err = InvalidFormatError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        MAC::from_bytes(hex::from_string(s).as_slice()).map_or_else(|| Err(InvalidFormatError), |m| Ok(m))
    }
}

impl Hash for MAC {
    #[inline(always)]
    fn hash<H: Hasher>(&self, state: &mut H) {
        state.write_u64(self.0.get());
    }
}

impl Serialize for MAC {
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

struct MACVisitor;

impl<'de> serde::de::Visitor<'de> for MACVisitor {
    type Value = MAC;

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("a ZeroTier address")
    }

    fn visit_bytes<E>(self, v: &[u8]) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        if v.len() == 6 {
            MAC::from_bytes(v).map_or_else(|| Err(E::custom("object too large")), |a| Ok(a))
        } else {
            Err(E::custom("object too large"))
        }
    }

    fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        MAC::from_str(v).map_err(|e| E::custom(e.to_string()))
    }
}

impl<'de> Deserialize<'de> for MAC {
    fn deserialize<D>(deserializer: D) -> Result<MAC, D::Error>
    where
        D: Deserializer<'de>,
    {
        if deserializer.is_human_readable() {
            deserializer.deserialize_str(MACVisitor)
        } else {
            deserializer.deserialize_bytes(MACVisitor)
        }
    }
}
