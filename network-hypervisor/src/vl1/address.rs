// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::borrow::Borrow;
use std::collections::BTreeMap;
use std::fmt::Debug;
use std::hash::Hash;
use std::ops::Bound;
use std::str::FromStr;

use serde::{Deserialize, Deserializer, Serialize, Serializer};

use zerotier_utils::base24;
use zerotier_utils::error::InvalidParameterError;
use zerotier_utils::hex;
use zerotier_utils::memory;

/// A full (V2) ZeroTier address.
///
/// The first 40 bits (5 bytes) of the address are the legacy 40-bit short ZeroTier address computed from
/// a hash of the identity's X25519 keys. The remaining bits are a SHA384 hash of that short address and
/// all key types and key material. See identity.rs for details.
#[derive(Clone, PartialEq, Eq, PartialOrd, Ord)]
#[repr(transparent)]
pub struct Address(pub(super) [u8; Self::SIZE_BYTES]);

/// A partial address, which is bytes and the number of bytes of specificity (similar to a CIDR IP address).
///
/// Partial addresses are looked up to get full addresses (and identities) via roots using WHOIS messages.
#[derive(Clone, PartialEq, Eq)]
pub struct PartialAddress {
    pub(super) address: Address,
    pub(super) specificity: u16,
}

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

    /// Get the first 40 bits of this address (a legacy V1 ZeroTier address)
    #[inline(always)]
    pub fn legacy_bytes(&self) -> &[u8; 5] {
        memory::array_range::<u8, { Address::SIZE_BYTES }, 0, { PartialAddress::LEGACY_SIZE_BYTES }>(&self.0)
    }

    /// Get the legacy address in the least significant bits of a u64.
    #[inline(always)]
    pub(crate) fn legacy_u64(&self) -> u64 {
        u64::from_be(memory::load_raw(&self.0)).wrapping_shr(24)
    }

    /// Get a partial address object (with full specificity) for this address
    #[inline]
    pub fn to_partial(&self) -> PartialAddress {
        PartialAddress {
            address: Address(self.0),
            specificity: Self::SIZE_BYTES as u16,
        }
    }

    /// Get a partial address covering the 40-bit legacy address.
    #[inline]
    pub fn to_legacy_partial(&self) -> PartialAddress {
        PartialAddress {
            address: Address({
                let mut tmp = [0u8; PartialAddress::MAX_SIZE_BYTES];
                tmp[..PartialAddress::LEGACY_SIZE_BYTES].copy_from_slice(&self.0[..PartialAddress::LEGACY_SIZE_BYTES]);
                tmp
            }),
            specificity: PartialAddress::LEGACY_SIZE_BYTES as u16,
        }
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
    #[inline(always)]
    fn to_string(&self) -> String {
        base24::encode(&self.0)
    }
}

impl FromStr for Address {
    type Err = InvalidParameterError;

    #[inline]
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        base24::decode(s.as_bytes()).and_then(|b| Self::from_bytes(b.as_slice()))
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

    /// Construct an address from a byte slice with its length determining specificity.
    #[inline]
    pub fn from_bytes(b: &[u8]) -> Result<Self, InvalidParameterError> {
        if b.len() >= Self::MIN_SIZE_BYTES
            && b.len() <= Self::MAX_SIZE_BYTES
            && b[0] != Address::RESERVED_PREFIX
            && b[..Self::LEGACY_SIZE_BYTES].iter().any(|i| *i != 0)
        {
            let mut a = Self {
                address: Address([0u8; Address::SIZE_BYTES]),
                specificity: b.len() as u16,
            };
            a.address.0[..b.len()].copy_from_slice(b);
            Ok(a)
        } else {
            Err(InvalidParameterError("invalid address"))
        }
    }

    #[inline]
    pub(crate) fn from_legacy_address_bytes(b: &[u8; 5]) -> Result<Self, InvalidParameterError> {
        if b[0] != Address::RESERVED_PREFIX && b.iter().any(|i| *i != 0) {
            Ok(Self {
                address: Address({
                    let mut tmp = [0u8; Self::MAX_SIZE_BYTES];
                    tmp[..5].copy_from_slice(b);
                    tmp
                }),
                specificity: Self::LEGACY_SIZE_BYTES as u16,
            })
        } else {
            Err(InvalidParameterError("invalid address"))
        }
    }

    #[inline]
    pub(crate) fn from_legacy_address_u64(mut b: u64) -> Result<Self, InvalidParameterError> {
        b &= 0xffffffffff;
        if b.wrapping_shr(32) != (Address::RESERVED_PREFIX as u64) && b != 0 {
            Ok(Self {
                address: Address({
                    let mut tmp = [0u8; Self::MAX_SIZE_BYTES];
                    tmp[..5].copy_from_slice(&b.to_be_bytes()[..5]);
                    tmp
                }),
                specificity: Self::LEGACY_SIZE_BYTES as u16,
            })
        } else {
            Err(InvalidParameterError("invalid address"))
        }
    }

    #[inline(always)]
    pub fn as_bytes(&self) -> &[u8] {
        debug_assert!(self.specificity >= Self::MIN_SIZE_BYTES as u16);
        &self.address.0[..self.specificity as usize]
    }

    #[inline(always)]
    pub(crate) fn legacy_bytes(&self) -> &[u8; 5] {
        debug_assert!(self.specificity >= Self::MIN_SIZE_BYTES as u16);
        memory::array_range::<u8, { Address::SIZE_BYTES }, 0, { PartialAddress::LEGACY_SIZE_BYTES }>(&self.address.0)
    }

    #[inline(always)]
    pub(crate) fn legacy_u64(&self) -> u64 {
        u64::from_be(memory::load_raw(&self.address.0)).wrapping_shr(24)
    }

    /// Returns true if this partial address matches a full length address up to this partial's specificity.
    #[inline(always)]
    pub fn matches(&self, k: &Address) -> bool {
        debug_assert!(self.specificity >= Self::MIN_SIZE_BYTES as u16);
        let l = self.specificity as usize;
        self.address.0[..l].eq(&k.0[..l])
    }

    /// Returns true if this partial address matches another up to the lower of the two addresses' specificities.
    #[inline(always)]
    pub fn matches_partial(&self, k: &PartialAddress) -> bool {
        debug_assert!(self.specificity >= Self::MIN_SIZE_BYTES as u16);
        let l = self.specificity.min(k.specificity) as usize;
        self.address.0[..l].eq(&k.address.0[..l])
    }

    /// Get the number of bits of specificity in this address
    #[inline(always)]
    pub fn specificity_bits(&self) -> usize {
        (self.specificity * 8) as usize
    }

    /// Get the number of bytes of specificity in this address (only 8 bit increments in specificity are allowed)
    #[inline(always)]
    pub fn specificity_bytes(&self) -> usize {
        self.specificity as usize
    }

    /// Returns true if this address has legacy 40 bit specificity (V1 ZeroTier address)
    #[inline(always)]
    pub fn is_legacy(&self) -> bool {
        self.specificity == Self::LEGACY_SIZE_BYTES as u16
    }

    /// Get a complete address from this partial if it is in fact complete.
    #[inline]
    pub fn as_complete(&self) -> Option<&Address> {
        if self.specificity == Self::MAX_SIZE_BYTES as u16 {
            Some(&self.address)
        } else {
            None
        }
    }

    /// Returns true if specificity is at the maximum value (384 bits)
    #[inline(always)]
    pub fn is_complete(&self) -> bool {
        self.specificity == Self::MAX_SIZE_BYTES as u16
    }

    /// Efficiently find an entry in a BTreeMap of partial addresses that uniquely matches this partial.
    ///
    /// This returns None if there is no match or if this partial matches more than one entry, in which
    /// case it's ambiguous and may be unsafe to use. This should be prohibited at other levels of the
    /// system but is checked for here as well.
    #[inline]
    pub fn find_unique_match<'a, T>(&self, map: &'a BTreeMap<PartialAddress, T>) -> Option<&'a T> {
        // Search for an exact or more specific match.
        let mut m = None;

        // First search for exact or more specific matches, which would appear later in the sorted key list.
        let mut pos = map.range((Bound::Included(self), Bound::Unbounded));
        while let Some(e) = pos.next() {
            if self.matches_partial(e.0) {
                if m.is_some() {
                    // Ambiguous!
                    return None;
                }
                let _ = m.insert(e.1);
            } else {
                break;
            }
        }

        // Then search for less specific matches or verify that the match we found above is not ambiguous.
        let mut pos = map.range((Bound::Unbounded, Bound::Excluded(self)));
        while let Some(e) = pos.next_back() {
            if self.matches_partial(e.0) {
                if m.is_some() {
                    return None;
                }
                let _ = m.insert(e.1);
            } else {
                break;
            }
        }

        return m;
    }

    /// Efficiently find an entry in a BTreeMap of partial addresses that uniquely matches this partial.
    ///
    /// This returns None if there is no match or if this partial matches more than one entry, in which
    /// case it's ambiguous and may be unsafe to use. This should be prohibited at other levels of the
    /// system but is checked for here as well.
    #[inline(always)]
    pub fn find_unique_match_mut<'a, T>(&self, map: &'a mut BTreeMap<PartialAddress, T>) -> Option<&'a mut T> {
        // This not only saves some repetition but is in fact the only way to easily do this. The same code as
        // find_unique_match() but with range_mut() doesn't compile because the second range_mut() would
        // borrow 'map' a second time (since 'm' may have it borrowed). This is primarily due to the too-limited
        // API of BTreeMap which is missing a good way to find the nearest match. This should be safe since
        // we do not mutate the map and the signature of find_unique_match_mut() should properly guarantee
        // that the semantics of mutable references are obeyed in the calling context.
        unsafe { std::mem::transmute(self.find_unique_match::<T>(map)) }
    }
}

impl Ord for PartialAddress {
    #[inline(always)]
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.address.cmp(&other.address).then(self.specificity.cmp(&other.specificity))
    }
}

impl PartialOrd for PartialAddress {
    #[inline(always)]
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl ToString for PartialAddress {
    fn to_string(&self) -> String {
        if self.is_legacy() {
            hex::to_string(&self.address.0[..Self::LEGACY_SIZE_BYTES])
        } else {
            base24::encode(self.as_bytes())
        }
    }
}

impl FromStr for PartialAddress {
    type Err = InvalidParameterError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        if s.len() == 10 {
            Self::from_bytes(hex::from_string(s).as_slice())
        } else {
            base24::decode(s.as_bytes()).and_then(|b| Self::from_bytes(b.as_slice()))
        }
    }
}

impl Hash for PartialAddress {
    #[inline(always)]
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        // Since this contains a random hash, the first 64 bits should be enough for a local HashMap etc.
        state.write_u64(memory::load_raw(&self.address.0))
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

#[cfg(test)]
mod tests {
    use super::*;
    use zerotier_crypto::random;

    #[test]
    fn to_from_string() {
        for _ in 0..64 {
            let mut tmp = Address::new_uninitialized();
            random::fill_bytes_secure(&mut tmp.0);
            println!("{}", tmp.to_string());
        }
    }
}
