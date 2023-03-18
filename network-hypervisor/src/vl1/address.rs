// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::fmt::Debug;
use std::str::FromStr;

use serde::{Deserialize, Deserializer, Serialize, Serializer};

use zerotier_utils::error::InvalidFormatError;
use zerotier_utils::hex;
use zerotier_utils::memory;

const BASE62_ALPHABET: &'static [u8; 62] = b"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
const BASE62_ALPHABET_REVERSE: [u8; 256] = [0; 256];

#[derive(Clone, PartialEq, Eq, Hash)]
#[repr(transparent)]
pub struct Address([u128; 3]);

impl Address {
    pub const V1_ADDRESS_SIZE: usize = 5;
    pub const V1_ADDRESS_STRING_SIZE: usize = 10;
    pub const V2_ADDRESS_SIZE: usize = 48;
    pub const V2_ADDRESS_STRING_SIZE: usize = 65;

    pub const RESERVED_PREFIX: u8 = 0xff;

    #[inline(always)]
    pub(crate) fn new_uninitialized() -> Self {
        Self([0, 0, 0])
    }

    pub fn from_bytes(b: &[u8]) -> Result<Option<Self>, InvalidFormatError> {
        if b.len() == Self::V1_ADDRESS_SIZE {
            Ok(Self::from_bytes_v1(b))
        } else if b.len() == Self::V2_ADDRESS_SIZE {
            Ok(Self::from_bytes_v2(b))
        } else {
            Err(InvalidFormatError)
        }
    }

    pub(crate) fn from_bytes_v1(b: &[u8]) -> Option<Self> {
        let mut a = Self([0; 3]);
        memory::as_byte_array_mut::<[u128; 3], 48>(&mut a.0)[..Self::V1_ADDRESS_SIZE].copy_from_slice(b);
        if a.0[0] != 0 {
            Some(a)
        } else {
            None
        }
    }

    pub(crate) fn from_bytes_v2(b: &[u8]) -> Option<Self> {
        let a = Self(memory::load_raw(b));
        if a.0.iter().any(|i| *i != 0) {
            Some(a)
        } else {
            None
        }
    }

    #[inline(always)]
    pub(crate) fn from_bytes_raw(b: &[u8; 48]) -> Option<Self> {
        Self::from_bytes_v2(b)
    }

    #[inline(always)]
    pub(crate) fn from_u64_v1(i: u64) -> Option<Self> {
        if i != 0 {
            Some(Self([i.wrapping_shl(24 + 64).to_be() as u128, 0, 0]))
        } else {
            None
        }
    }

    /// True if this address lacks extended hash information.
    #[inline(always)]
    pub fn is_v1_only(&self) -> bool {
        self.0[1] == 0 && self.0[2] == 0
    }

    #[inline(always)]
    pub(crate) fn as_bytes_raw(&self) -> &[u8; 48] {
        memory::as_byte_array::<[u128; 3], 48>(&self.0)
    }

    #[inline(always)]
    pub(crate) fn as_bytes_raw_mut(&mut self) -> &mut [u8; 48] {
        memory::as_byte_array_mut::<[u128; 3], 48>(&mut self.0)
    }

    #[inline(always)]
    pub(crate) fn as_u64_v1(&self) -> u64 {
        u128::from_be(self.0[0]).wrapping_shr(24 + 64) as u64
    }

    #[inline(always)]
    pub(crate) fn as_bytes_v1(&self) -> &[u8; Self::V1_ADDRESS_SIZE] {
        memory::array_range::<u8, 48, 0, 5>(memory::as_byte_array::<[u128; 3], 48>(&self.0))
    }

    /// Get all bits in this address (last 344 will be zero if this is only a V1 address).
    #[inline(always)]
    pub fn as_bytes_full(&self) -> &[u8; Self::V2_ADDRESS_SIZE] {
        memory::as_byte_array::<[u128; 3], 48>(&self.0)
    }

    /// Get a byte serialized address.
    /// This returns either a 40-bit short address or a full 384 bits depending on whether this
    /// contains only a short V1 address or a full length V2 address. Use as_bytes_full() to
    /// always get all 384 bits with the last 344 being zero for V1-only addresses.
    pub fn as_bytes(&self) -> &[u8] {
        if self.is_v1_only() {
            &memory::as_byte_array::<[u128; 3], 48>(&self.0)[..Self::V1_ADDRESS_SIZE]
        } else {
            memory::as_byte_array::<[u128; 3], 48>(&self.0)
        }
    }

    /// Get the short 10-digit address string for this address.
    pub fn to_short_string(&self) -> String {
        hex::to_string(&memory::as_byte_array::<[u128; 3], 48>(&self.0)[..Self::V1_ADDRESS_SIZE])
    }
}

impl ToString for Address {
    fn to_string(&self) -> String {
        let mut s = String::with_capacity(Self::V2_ADDRESS_STRING_SIZE);
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
        if s.len() == Self::V1_ADDRESS_STRING_SIZE {
            return Ok(Self::from_bytes_v1(hex::from_string(s).as_slice()).ok_or(InvalidFormatError)?);
        } else if s.len() == Self::V2_ADDRESS_STRING_SIZE {
            let mut s = s.as_bytes();
            let mut a = Self([0, 0, 0]);
            for qi in 0..3 {
                let mut q = 0u128;
                for _ in 0..21 {
                    let r = BASE62_ALPHABET_REVERSE[s[0] as usize];
                    if r == 255 {
                        return Err(InvalidFormatError);
                    }
                    q += r as u128;
                    s = &s[1..];
                    q *= 62;
                }
                a.0[qi] = q;
            }
            let mut remainders = 0u16;
            for _ in 0..2 {
                let r = BASE62_ALPHABET_REVERSE[s[0] as usize];
                if r == 255 {
                    return Err(InvalidFormatError);
                }
                remainders += r as u16;
                s = &s[1..];
                remainders *= 62;
            }
            if remainders > 511 {
                return Err(InvalidFormatError);
            }
            a.0[0] += (remainders.wrapping_shr(6) & 7) as u128;
            a.0[1] += (remainders.wrapping_shr(3) & 7) as u128;
            a.0[2] += (remainders & 7) as u128;
            return Ok(a);
        }
        return Err(InvalidFormatError);
    }
}

impl PartialOrd for Address {
    #[inline(always)]
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for Address {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        u128::from_be(self.0[0])
            .cmp(&u128::from_be(other.0[0]))
            .then(u128::from_be(self.0[1]).cmp(&u128::from_be(other.0[1])))
            .then(u128::from_be(self.0[2]).cmp(&u128::from_be(other.0[2])))
    }
}

impl Debug for Address {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(self.to_string().as_str())
    }
}

impl Serialize for Address {
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

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("a ZeroTier address")
    }

    fn visit_bytes<E>(self, v: &[u8]) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        if let Ok(Some(v)) = Address::from_bytes(v) {
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
