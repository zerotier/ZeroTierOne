// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::borrow::Borrow;
use std::fmt::Debug;
use std::hash::Hash;
use std::str::FromStr;

use serde::{Deserialize, Deserializer, Serialize, Serializer};

use zerotier_utils::base24;
use zerotier_utils::error::InvalidParameterError;
use zerotier_utils::hex;
use zerotier_utils::memory;

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord)]
#[repr(transparent)]
pub struct Address(pub(super) [u8; Self::SIZE_BYTES]);

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct PartialAddress(pub(super) Address, pub(super) u16);

impl Address {
    pub const SIZE_BYTES: usize = 48;

    /// The first byte of an address cannot be 0xff.
    pub const RESERVED_PREFIX: u8 = 0xff;

    #[inline(always)]
    pub(super) fn new_uninitialized() -> Self {
        Self([0u8; Self::SIZE_BYTES])
    }

    #[inline]
    pub fn from_bytes(b: &[u8]) -> Result<Self, InvalidParameterError> {
        if b.len() == Self::SIZE_BYTES && b[0] != Address::RESERVED_PREFIX && b[..PartialAddress::LEGACY_SIZE_BYTES].iter().any(|i| *i != 0) {
            Ok(Self(b.try_into().unwrap()))
        } else {
            Err(InvalidParameterError("invalid address"))
        }
    }

    /// Get the first 40 bits of this address (for legacy use)
    #[inline(always)]
    pub(crate) fn legacy_bytes(&self) -> &[u8; 5] {
        memory::array_range::<u8, { Address::SIZE_BYTES }, 0, { PartialAddress::LEGACY_SIZE_BYTES }>(&self.0)
    }

    /// Get a partial address object (with full specificity) for this address
    #[inline(always)]
    pub fn to_partial(&self) -> PartialAddress {
        PartialAddress(Address(self.0), Self::SIZE_BYTES as u16)
    }

    #[inline(always)]
    pub fn as_bytes(&self) -> &[u8; Self::SIZE_BYTES] {
        &self.0
    }
}

impl Borrow<[u8; Self::SIZE_BYTES]> for Address {
    #[inline(always)]
    fn borrow(&self) -> &[u8; Self::SIZE_BYTES] {
        &self.0
    }
}

impl ToString for Address {
    fn to_string(&self) -> String {
        let mut tmp = String::with_capacity(Self::SIZE_BYTES * 2);
        base24::encode_into(&self.0, &mut tmp);
        tmp
    }
}

impl FromStr for Address {
    type Err = InvalidParameterError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let mut tmp = Vec::with_capacity(Self::SIZE_BYTES);
        base24::decode_into(s, &mut tmp);
        Self::from_bytes(tmp.as_slice())
    }
}

impl Hash for Address {
    #[inline(always)]
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        // Since this contains a random hash, the first 64 bits should be enough for a local HashMap etc.
        state.write_u64(memory::load_raw(&self.0))
    }
}

impl Debug for Address {
    #[inline]
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(self.to_string().as_str())
    }
}

impl Serialize for Address {
    #[inline]
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        if serializer.is_human_readable() {
            serializer.serialize_str(self.to_string().as_str())
        } else {
            serializer.serialize_bytes(self.as_bytes())
        }
    }
}

struct AddressDeserializeVisitor;

impl<'de> serde::de::Visitor<'de> for AddressDeserializeVisitor {
    type Value = Address;

    #[inline]
    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("address")
    }

    #[inline]
    fn visit_bytes<E>(self, v: &[u8]) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        Address::from_bytes(v).map_err(|_| E::custom("invalid address"))
    }

    #[inline]
    fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        Address::from_str(v).map_err(|e| E::custom(e.to_string()))
    }
}

impl<'de> Deserialize<'de> for Address {
    #[inline]
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        if deserializer.is_human_readable() {
            deserializer.deserialize_str(AddressDeserializeVisitor)
        } else {
            deserializer.deserialize_bytes(AddressDeserializeVisitor)
        }
    }
}

impl PartialAddress {
    /// Minimum number of specified bits in an address.
    pub const MIN_SPECIFICITY: usize = Self::MIN_SIZE_BYTES * 8;

    /// Maximum number of specified bits in an address.
    pub const MAX_SPECIFICITY: usize = Self::MAX_SIZE_BYTES * 8;

    pub const LEGACY_SIZE_BYTES: usize = 5;
    pub const MIN_SIZE_BYTES: usize = Self::LEGACY_SIZE_BYTES;
    pub const MAX_SIZE_BYTES: usize = Address::SIZE_BYTES;

    /// Create an invalid uninitialized address (used when generating Identity)
    pub(super) fn new_uninitialized() -> Self {
        Self(Address([0u8; Self::MAX_SIZE_BYTES]), 0)
    }

    /// Construct an address from a byte slice with its length determining specificity.
    #[inline]
    pub fn from_bytes(b: &[u8]) -> Result<Self, InvalidParameterError> {
        if b.len() >= Self::MIN_SIZE_BYTES
            && b.len() <= Self::MAX_SIZE_BYTES
            && b[0] != Address::RESERVED_PREFIX
            && b[..Self::LEGACY_SIZE_BYTES].iter().any(|i| *i != 0)
        {
            let mut a = Self(Address([0u8; Address::SIZE_BYTES]), b.len() as u16);
            a.0 .0[..b.len()].copy_from_slice(b);
            Ok(a)
        } else {
            Err(InvalidParameterError("invalid address"))
        }
    }

    #[inline]
    pub(crate) fn from_legacy_address_bytes(b: &[u8; 5]) -> Result<Self, InvalidParameterError> {
        if b[0] != Address::RESERVED_PREFIX && b.iter().any(|i| *i != 0) {
            Ok(Self(
                Address({
                    let mut tmp = [0u8; Self::MAX_SIZE_BYTES];
                    tmp[..5].copy_from_slice(b);
                    tmp
                }),
                Self::LEGACY_SIZE_BYTES as u16,
            ))
        } else {
            Err(InvalidParameterError("invalid address"))
        }
    }

    #[inline]
    pub(crate) fn from_legacy_address_u64(mut b: u64) -> Result<Self, InvalidParameterError> {
        b &= 0xffffffffff;
        if b.wrapping_shr(32) != (Address::RESERVED_PREFIX as u64) && b != 0 {
            Ok(Self(
                Address({
                    let mut tmp = [0u8; Self::MAX_SIZE_BYTES];
                    tmp[..5].copy_from_slice(&b.to_be_bytes()[..5]);
                    tmp
                }),
                Self::LEGACY_SIZE_BYTES as u16,
            ))
        } else {
            Err(InvalidParameterError("invalid address"))
        }
    }

    #[inline(always)]
    pub fn as_bytes(&self) -> &[u8] {
        debug_assert!(self.1 >= Self::MIN_SIZE_BYTES as u16);
        &self.0 .0[..self.1 as usize]
    }

    #[inline(always)]
    pub(crate) fn legacy_bytes(&self) -> &[u8; 5] {
        debug_assert!(self.1 >= Self::MIN_SIZE_BYTES as u16);
        memory::array_range::<u8, { Address::SIZE_BYTES }, 0, { PartialAddress::LEGACY_SIZE_BYTES }>(&self.0 .0)
    }

    #[inline(always)]
    pub(super) fn matches(&self, k: &Address) -> bool {
        debug_assert!(self.1 >= Self::MIN_SIZE_BYTES as u16);
        let l = self.1 as usize;
        self.0 .0[..l].eq(&k.0[..l])
    }

    /// Get the number of bits of specificity in this address
    #[inline(always)]
    pub fn specificity(&self) -> usize {
        (self.1 * 8) as usize
    }

    /// Returns true if this address has legacy 40 bit specificity (V1 ZeroTier address)
    #[inline(always)]
    pub fn is_legacy(&self) -> bool {
        self.1 == Self::LEGACY_SIZE_BYTES as u16
    }

    /// Get a full length address if this partial address is actually complete (384 bits of specificity)
    #[inline(always)]
    pub fn as_address(&self) -> Option<&Address> {
        if self.1 == Self::MAX_SIZE_BYTES as u16 {
            Some(&self.0)
        } else {
            None
        }
    }

    /// Returns true if specificity is at the maximum value (384 bits)
    #[inline(always)]
    pub fn is_complete(&self) -> bool {
        self.1 == Self::MAX_SIZE_BYTES as u16
    }
}

impl ToString for PartialAddress {
    fn to_string(&self) -> String {
        if self.is_legacy() {
            hex::to_string(&self.0 .0[..Self::LEGACY_SIZE_BYTES])
        } else {
            let mut tmp = String::with_capacity(Self::MAX_SIZE_BYTES * 2);
            base24::encode_into(&self.0 .0[..self.1 as usize], &mut tmp);
            tmp
        }
    }
}

impl FromStr for PartialAddress {
    type Err = InvalidParameterError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        if s.len() == 10 {
            return Self::from_bytes(hex::from_string(s).as_slice());
        } else {
            let mut tmp = Vec::with_capacity(Self::MAX_SIZE_BYTES);
            base24::decode_into(s, &mut tmp)?;
            return Self::from_bytes(tmp.as_slice());
        }
    }
}

impl Hash for PartialAddress {
    #[inline(always)]
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        // Since this contains a random hash, the first 64 bits should be enough for a local HashMap etc.
        state.write_u64(memory::load_raw(&self.0 .0))
    }
}

impl Debug for PartialAddress {
    #[inline]
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(self.to_string().as_str())
    }
}

impl Serialize for PartialAddress {
    #[inline]
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        if serializer.is_human_readable() {
            serializer.serialize_str(self.to_string().as_str())
        } else {
            serializer.serialize_bytes(self.as_bytes())
        }
    }
}

struct PartialAddressDeserializeVisitor;

impl<'de> serde::de::Visitor<'de> for PartialAddressDeserializeVisitor {
    type Value = PartialAddress;

    #[inline]
    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("address")
    }

    #[inline]
    fn visit_bytes<E>(self, v: &[u8]) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        PartialAddress::from_bytes(v).map_err(|_| E::custom("invalid address"))
    }

    #[inline]
    fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        PartialAddress::from_str(v).map_err(|e| E::custom(e.to_string()))
    }
}

impl<'de> Deserialize<'de> for PartialAddress {
    #[inline]
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        if deserializer.is_human_readable() {
            deserializer.deserialize_str(PartialAddressDeserializeVisitor)
        } else {
            deserializer.deserialize_bytes(PartialAddressDeserializeVisitor)
        }
    }
}
