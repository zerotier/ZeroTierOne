// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::fmt::Debug;
use std::hash::Hash;
use std::num::NonZeroU64;
use std::str::FromStr;

use serde::{Deserialize, Deserializer, Serialize, Serializer};

use zerotier_utils::error::InvalidFormatError;
use zerotier_utils::hex;
use zerotier_utils::memory;

const BASE62_ALPHABET: &'static [u8; 62] = b"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
const BASE62_ALPHABET_REVERSE: [u8; 256] = [0; 256];

#[derive(Clone, PartialEq, Eq)]
#[repr(transparent)]
pub struct Address([u128; 3]);

impl Address {
    /// Size of a full length address in bytes.
    pub const SIZE_BYTES: usize = 48;

    /// Addresses may not begin with 0xff; reserved for special signaling or future use.
    pub const RESERVED_PREFIX: u8 = 0xff;

    #[inline(always)]
    pub(crate) fn new_uninitialized() -> Self {
        Self([0, 0, 0])
    }

    #[inline(always)]
    pub(crate) fn as_bytes_mut(&mut self) -> &mut [u8; 48] {
        memory::as_byte_array_mut(&mut self.0)
    }

    #[inline(always)]
    pub fn from_bytes(b: &[u8]) -> Result<Self, InvalidFormatError> {
        if b.len() >= Self::SIZE_BYTES {
            let a = Self(memory::load_raw(b));
            if b[0] != Self::RESERVED_PREFIX && memory::load_raw::<u64>(b) != 0 {
                Ok(a)
            } else {
                Err(InvalidFormatError)
            }
        } else {
            Err(InvalidFormatError)
        }
    }

    #[inline(always)]
    pub fn from_bytes_exact(b: &[u8; Self::SIZE_BYTES]) -> Result<Self, InvalidFormatError> {
        let a = Self(memory::load_raw(b));
        if b[0] != Self::RESERVED_PREFIX && memory::load_raw::<u64>(b) != 0 {
            Ok(a)
        } else {
            Err(InvalidFormatError)
        }
    }

    #[inline(always)]
    pub fn legacy_address(&self) -> LegacyAddress {
        LegacyAddress(NonZeroU64::new(memory::load_raw::<u64>(self.as_bytes())).unwrap())
    }

    /// Get all bits in this address (last 344 will be zero if this is only a V1 address).
    #[inline(always)]
    pub fn as_bytes(&self) -> &[u8; Self::SIZE_BYTES] {
        memory::as_byte_array(&self.0)
    }
}

impl ToString for Address {
    fn to_string(&self) -> String {
        let mut s = String::with_capacity(66);
        let mut remainders = 0u16;
        for qq in self.0.iter() {
            let mut q = u128::from_be(*qq);
            for _ in 0..21 {
                let (x, y) = (q % 62, q / 62);
                q = y;
                s.push(BASE62_ALPHABET[x as usize] as char);
            }
            debug_assert!(q <= 7);
            remainders = remainders.wrapping_shl(3);
            remainders |= q as u16;
        }
        debug_assert!(remainders <= 511);
        s.push(BASE62_ALPHABET[(remainders % 62) as usize] as char);
        s.push(BASE62_ALPHABET[(remainders / 62) as usize] as char);
        s
    }
}

impl FromStr for Address {
    type Err = InvalidFormatError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let mut s = s.as_bytes();
        let mut a = Self([0, 0, 0]);
        for qi in 0..3 {
            let mut q = 0u128;
            for _ in 0..21 {
                let r = BASE62_ALPHABET_REVERSE[s[0] as usize];
                s = &s[1..];
                if r == 255 {
                    return Err(InvalidFormatError);
                }
                q *= 62;
                q += r as u128;
            }
            a.0[qi] = q;
        }
        let mut remainders = 0u16;
        for _ in 0..2 {
            let r = BASE62_ALPHABET_REVERSE[s[0] as usize];
            s = &s[1..];
            if r == 255 {
                return Err(InvalidFormatError);
            }
            remainders *= 62;
            remainders += r as u16;
        }
        if remainders > 511 {
            return Err(InvalidFormatError);
        }
        a.0[0] += (remainders.wrapping_shr(6) & 7) as u128;
        a.0[1] += (remainders.wrapping_shr(3) & 7) as u128;
        a.0[2] += (remainders & 7) as u128;
        return Ok(a);
    }
}

impl PartialOrd for Address {
    #[inline(always)]
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for Address {
    #[inline]
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        u128::from_be(self.0[0])
            .cmp(&u128::from_be(other.0[0]))
            .then(u128::from_be(self.0[1]).cmp(&u128::from_be(other.0[1])))
            .then(u128::from_be(self.0[2]).cmp(&u128::from_be(other.0[2])))
    }
}

impl Debug for Address {
    #[inline]
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(self.to_string().as_str())
    }
}

impl Hash for Address {
    #[inline(always)]
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        state.write_u128(self.0[0])
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

struct AddressVisitor;

impl<'de> serde::de::Visitor<'de> for AddressVisitor {
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
        if let Ok(v) = Address::from_bytes(v) {
            Ok(v)
        } else {
            Err(E::custom("invalid address"))
        }
    }

    fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        Address::from_str(v).map_err(|e| E::custom(e.to_string()))
    }
}

impl<'de> Deserialize<'de> for Address {
    #[inline]
    fn deserialize<D>(deserializer: D) -> Result<Address, D::Error>
    where
        D: Deserializer<'de>,
    {
        if deserializer.is_human_readable() {
            deserializer.deserialize_str(AddressVisitor)
        } else {
            deserializer.deserialize_bytes(AddressVisitor)
        }
    }
}

#[derive(Clone, Copy, PartialEq, Eq, Hash)]
#[repr(transparent)]
pub struct LegacyAddress(NonZeroU64);

impl LegacyAddress {
    pub const SIZE_BYTES: usize = 5;
    pub const SIZE_HEX_STRING: usize = 10;

    #[inline(always)]
    pub fn from_bytes(b: &[u8]) -> Option<Self> {
        if b.len() >= Self::SIZE_BYTES && b[0] != Address::RESERVED_PREFIX {
            let mut tmp = 0u64.to_ne_bytes();
            tmp[..Address::SIZE_BYTES].copy_from_slice(b);
            NonZeroU64::new(u64::from_ne_bytes(tmp)).map(|i| Self(i))
        } else {
            None
        }
    }

    #[inline(always)]
    pub fn from_bytes_exact(b: &[u8; Self::SIZE_BYTES]) -> Option<Self> {
        if b[0] != Address::RESERVED_PREFIX {
            let mut tmp = 0u64.to_ne_bytes();
            tmp[..Address::SIZE_BYTES].copy_from_slice(b);
            NonZeroU64::new(u64::from_ne_bytes(tmp)).map(|i| Self(i))
        } else {
            None
        }
    }

    #[inline(always)]
    pub(crate) fn from_u64(i: u64) -> Option<Self> {
        NonZeroU64::new(i.wrapping_shl(24).to_be()).map(|i| Self(i))
    }

    #[inline(always)]
    pub(crate) fn to_u64(&self) -> u64 {
        u64::from_be(self.0.get()).wrapping_shr(24)
    }

    #[inline(always)]
    pub fn as_bytes(&self) -> &[u8; Self::SIZE_BYTES] {
        debug_assert_eq!(std::mem::size_of::<NonZeroU64>(), 8);
        memory::array_range::<u8, 8, 0, 5>(memory::as_byte_array::<NonZeroU64, 8>(&self.0))
    }
}

impl ToString for LegacyAddress {
    fn to_string(&self) -> String {
        hex::to_string(&memory::as_byte_array::<NonZeroU64, 8>(&self.0)[..Self::SIZE_BYTES])
    }
}

impl FromStr for LegacyAddress {
    type Err = InvalidFormatError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        if s.len() == Self::SIZE_HEX_STRING {
            Self::from_bytes(hex::from_string(s).as_slice()).ok_or(InvalidFormatError)
        } else {
            Err(InvalidFormatError)
        }
    }
}

impl Debug for LegacyAddress {
    #[inline]
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(self.to_string().as_str())
    }
}

impl Serialize for LegacyAddress {
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

struct LegacyAddressVisitor;

impl<'de> serde::de::Visitor<'de> for LegacyAddressVisitor {
    type Value = LegacyAddress;

    #[inline]
    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("address")
    }

    #[inline]
    fn visit_bytes<E>(self, v: &[u8]) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        if let Some(v) = LegacyAddress::from_bytes(v) {
            Ok(v)
        } else {
            Err(E::custom("invalid address"))
        }
    }

    fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        LegacyAddress::from_str(v).map_err(|e| E::custom(e.to_string()))
    }
}

impl<'de> Deserialize<'de> for LegacyAddress {
    #[inline]
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        if deserializer.is_human_readable() {
            deserializer.deserialize_str(LegacyAddressVisitor)
        } else {
            deserializer.deserialize_bytes(LegacyAddressVisitor)
        }
    }
}
