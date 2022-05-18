/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::hash::{Hash, Hasher};
use std::num::NonZeroU64;
use std::str::FromStr;

use serde::{Deserialize, Deserializer, Serialize, Serializer};

use crate::error::InvalidFormatError;
use crate::util::buffer::Buffer;
use crate::util::hex::HEX_CHARS;
use crate::util::marshalable::Marshalable;
use crate::vl1::protocol::{ADDRESS_RESERVED_PREFIX, ADDRESS_SIZE};

/// A unique address on the global ZeroTier VL1 network.
#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
#[repr(transparent)]
pub struct Address(NonZeroU64);

impl Address {
    /// Get an address from a 64-bit integer or return None if it is zero or reserved.
    #[inline]
    pub fn from_u64(mut i: u64) -> Option<Address> {
        i &= 0xffffffffff;
        NonZeroU64::new(i).and_then(|ii| if (i >> 32) != ADDRESS_RESERVED_PREFIX as u64 { Some(Address(ii)) } else { None })
    }

    #[inline]
    pub fn from_bytes(b: &[u8]) -> Option<Address> {
        if b.len() >= ADDRESS_SIZE {
            Self::from_u64((b[0] as u64) << 32 | (b[1] as u64) << 24 | (b[2] as u64) << 16 | (b[3] as u64) << 8 | b[4] as u64)
        } else {
            None
        }
    }

    #[inline]
    pub fn from_bytes_fixed(b: &[u8; ADDRESS_SIZE]) -> Option<Address> {
        Self::from_u64((b[0] as u64) << 32 | (b[1] as u64) << 24 | (b[2] as u64) << 16 | (b[3] as u64) << 8 | b[4] as u64)
    }

    #[inline]
    pub fn to_bytes(&self) -> [u8; ADDRESS_SIZE] {
        let i = self.0.get();
        [(i >> 32) as u8, (i >> 24) as u8, (i >> 16) as u8, (i >> 8) as u8, i as u8]
    }

    #[inline(always)]
    pub fn to_u64(&self) -> u64 {
        self.0.get()
    }
}

impl Marshalable for Address {
    const MAX_MARSHAL_SIZE: usize = ADDRESS_SIZE;

    #[inline]
    fn marshal<const BL: usize>(&self, buf: &mut Buffer<BL>) -> std::io::Result<()> {
        buf.append_bytes(&self.0.get().to_be_bytes()[8 - ADDRESS_SIZE..])
    }

    #[inline]
    fn unmarshal<const BL: usize>(buf: &Buffer<BL>, cursor: &mut usize) -> std::io::Result<Self> {
        Self::from_bytes_fixed(buf.read_bytes_fixed(cursor)?).map_or_else(|| Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "cannot be zero")), |a| Ok(a))
    }
}

impl ToString for Address {
    fn to_string(&self) -> String {
        let mut v = self.0.get() << 24;
        let mut s = String::with_capacity(ADDRESS_SIZE * 2);
        for _ in 0..(ADDRESS_SIZE * 2) {
            s.push(HEX_CHARS[(v >> 60) as usize] as char);
            v <<= 4;
        }
        s
    }
}

impl FromStr for Address {
    type Err = InvalidFormatError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Address::from_bytes(crate::util::hex::from_string(s).as_slice()).map_or_else(|| Err(InvalidFormatError), |a| Ok(a))
    }
}

impl Hash for Address {
    #[inline(always)]
    fn hash<H: Hasher>(&self, state: &mut H) {
        state.write_u64(self.0.get());
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
            serializer.serialize_bytes(&self.to_bytes())
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
        if v.len() == ADDRESS_SIZE {
            Address::from_bytes(v).map_or_else(|| Err(E::custom("object too large")), |a| Ok(a))
        } else {
            Err(E::custom("object size incorrect"))
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
