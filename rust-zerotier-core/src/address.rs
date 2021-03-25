/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#[derive(PartialEq, Eq, Clone, Copy, Ord, PartialOrd)]
pub struct Address(pub u64);

impl Default for Address {
    #[inline(always)]
    fn default() -> Address {
        Address(0)
    }
}

impl Address {
    #[inline(always)]
    fn to_bytes(&self) -> [u8; 5] {
        [(self.0 >> 32) as u8, (self.0 >> 24) as u8, (self.0 >> 16) as u8, (self.0 >> 8) as u8, self.0 as u8]
    }
}

impl From<&[u8]> for Address {
    #[inline(always)]
    fn from(bytes: &[u8]) -> Self {
        if bytes.len() >= 5 {
            Address(((bytes[0] as u64) << 32) | ((bytes[1] as u64) << 24) | ((bytes[2] as u64) << 16) | ((bytes[3] as u64) << 8) | (bytes[4] as u64))
        } else {
            Address(0)
        }
    }
}

impl ToString for Address {
    fn to_string(&self) -> String {
        format!("{:0>10x}", self.0)
    }
}

impl From<u64> for Address {
    #[inline(always)]
    fn from(i: u64) -> Self {
        Address(i)
    }
}

impl From<&str> for Address {
    #[inline(always)]
    fn from(s: &str) -> Self {
        Address(u64::from_str_radix(s, 16).unwrap_or(0))
    }
}

impl serde::Serialize for Address {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: serde::Serializer {
        if serializer.is_human_readable() {
            serializer.serialize_str(self.to_string().as_str())
        } else {
            let b = self.to_bytes();
            serializer.serialize_bytes(b.as_ref())
        }
    }
}

struct AddressVisitor;

impl<'de> serde::de::Visitor<'de> for AddressVisitor {
    type Value = Address;
    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result { formatter.write_str("ZeroTier Address") }
    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E> where E: serde::de::Error { Ok(Address::from(s)) }
    fn visit_bytes<E>(self, v: &[u8]) -> Result<Self::Value, E> where E: serde::de::Error { Ok(Address::from(v)) }
}

impl<'de> serde::Deserialize<'de> for Address {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: serde::Deserializer<'de> {
        if deserializer.is_human_readable() {
            deserializer.deserialize_str(AddressVisitor)
        } else {
            deserializer.deserialize_bytes(AddressVisitor)
        }
    }
}
