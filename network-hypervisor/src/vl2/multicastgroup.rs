// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::fmt::Debug;
use std::hash::{Hash, Hasher};
use std::str::FromStr;

use crate::vl1::MAC;

use serde::{Deserialize, Deserializer, Serialize, Serializer};

#[derive(Clone, Copy, PartialEq, Eq)]
pub struct MulticastGroup {
    pub mac: MAC,
    pub adi: u32,
}

impl From<&MAC> for MulticastGroup {
    #[inline(always)]
    fn from(mac: &MAC) -> Self {
        Self { mac: mac.clone(), adi: 0 }
    }
}

impl From<MAC> for MulticastGroup {
    #[inline(always)]
    fn from(mac: MAC) -> Self {
        Self { mac, adi: 0 }
    }
}

impl Hash for MulticastGroup {
    #[inline(always)]
    fn hash<H: Hasher>(&self, state: &mut H) {
        state.write_u64(self.mac.into());
        state.write_u32(self.adi);
    }
}

impl ToString for MulticastGroup {
    fn to_string(&self) -> String {
        format!("{}/{}", self.mac.to_string(), self.adi.to_string())
    }
}

impl Debug for MulticastGroup {
    #[inline]
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(self.to_string().as_str())
    }
}

impl Serialize for MulticastGroup {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        if serializer.is_human_readable() {
            serializer.serialize_str(format!("{}/{}", self.mac.to_string(), self.adi.to_string()).as_str())
        } else {
            let mut tmp = [0u8; 10];
            tmp[0..6].copy_from_slice(&self.mac.to_bytes());
            tmp[6..10].copy_from_slice(&self.adi.to_be_bytes());
            serializer.serialize_bytes(&tmp)
        }
    }
}

struct MulticastGroupVisitor;

impl<'de> serde::de::Visitor<'de> for MulticastGroupVisitor {
    type Value = MulticastGroup;

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("a ZeroTier network ID")
    }

    fn visit_bytes<E>(self, v: &[u8]) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        if v.len() == 10 {
            Ok(MulticastGroup {
                mac: MAC::from_bytes(&v[..6]).ok_or(E::custom("invalid MAC address"))?,
                adi: u32::from_be_bytes((&v[6..10]).try_into().unwrap()),
            })
        } else {
            Err(E::custom("object too large"))
        }
    }

    fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        let mut mac = None;
        let mut adi: u32 = 0;
        let mut x = 0;
        for f in v.split('/') {
            if x == 0 {
                mac = Some(MAC::from_str(f).map_err(|_| E::custom("invalid MAC address"))?);
                x = 1;
            } else {
                adi = u32::from_str(f).unwrap_or(0);
                break;
            }
        }
        Ok(MulticastGroup { mac: mac.ok_or(E::custom("invalid MAC address"))?, adi })
    }
}

impl<'de> Deserialize<'de> for MulticastGroup {
    fn deserialize<D>(deserializer: D) -> Result<MulticastGroup, D::Error>
    where
        D: Deserializer<'de>,
    {
        if deserializer.is_human_readable() {
            deserializer.deserialize_str(MulticastGroupVisitor)
        } else {
            deserializer.deserialize_bytes(MulticastGroupVisitor)
        }
    }
}
