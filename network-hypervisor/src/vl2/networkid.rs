// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::fmt::Debug;
use std::hash::{Hash, Hasher};
use std::num::NonZeroU64;
use std::str::FromStr;

use serde::{Deserialize, Deserializer, Serialize, Serializer};

use zerotier_utils::error::InvalidFormatError;
use zerotier_utils::hex;
use zerotier_utils::hex::HEX_CHARS;

use crate::protocol::ADDRESS_MASK;
use crate::vl1::Address;

#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
#[repr(transparent)]
pub struct NetworkId(NonZeroU64);

impl NetworkId {
    #[inline]
    pub fn from_u64(i: u64) -> Option<NetworkId> {
        // Note that we check both that 'i' is non-zero and that the address of the controller is valid.
        if let Some(ii) = NonZeroU64::new(i) {
            if Address::from_u64(i & ADDRESS_MASK).is_some() {
                return Some(Self(ii));
            }
        }
        return None;
    }

    #[inline]
    pub fn from_controller_and_network_no(controller: Address, network_no: u64) -> Option<NetworkId> {
        Self::from_u64(u64::from(controller).wrapping_shl(24) | (network_no & 0xffffff))
    }

    #[inline]
    pub fn from_bytes(b: &[u8]) -> Option<NetworkId> {
        if b.len() >= 8 {
            Self::from_bytes_fixed(b[0..8].try_into().unwrap())
        } else {
            None
        }
    }

    #[inline]
    pub fn from_bytes_fixed(b: &[u8; 8]) -> Option<NetworkId> {
        Self::from_u64(u64::from_be_bytes(*b))
    }

    #[inline]
    pub fn to_bytes(&self) -> [u8; 8] {
        self.0.get().to_be_bytes()
    }

    /// Get the network controller ID for this network, which is the most significant 40 bits.
    #[inline]
    pub fn network_controller(&self) -> Address {
        Address::from_u64(self.0.get()).unwrap()
    }

    /// Consume this network ID and return one with the same network number but a different controller ID.
    pub fn change_network_controller(self, new_controller: Address) -> NetworkId {
        Self(NonZeroU64::new((self.network_no() as u64) | u64::from(new_controller).wrapping_shl(24)).unwrap())
    }

    /// Get the 24-bit local network identifier minus the 40-bit controller address portion.
    #[inline]
    pub fn network_no(&self) -> u32 {
        (self.0.get() & 0xffffff) as u32
    }
}

impl From<NetworkId> for u64 {
    #[inline(always)]
    fn from(v: NetworkId) -> Self {
        v.0.get()
    }
}

impl From<&NetworkId> for u64 {
    #[inline(always)]
    fn from(v: &NetworkId) -> Self {
        v.0.get()
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

impl Debug for NetworkId {
    #[inline]
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(self.to_string().as_str())
    }
}

impl FromStr for NetworkId {
    type Err = InvalidFormatError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        NetworkId::from_bytes(hex::from_string(s).as_slice()).map_or_else(|| Err(InvalidFormatError), |a| Ok(a))
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
