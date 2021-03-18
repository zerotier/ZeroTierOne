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

impl From<&[u8]> for Address {
    #[inline(always)]
    fn from(bytes: &[u8]) -> Self {
        if bytes.len() >= 5 {
            Address(((bytes[0] as u64) << 32) | ((bytes[0] as u64) << 24) | ((bytes[0] as u64) << 16) | ((bytes[0] as u64) << 8) | (bytes[0] as u64))
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
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: serde::Serializer { serializer.serialize_str(self.to_string().as_str()) }
}
struct AddressVisitor;
impl<'de> serde::de::Visitor<'de> for AddressVisitor {
    type Value = Address;
    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result { formatter.write_str("ZeroTier Address in string format") }
    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E> where E: serde::de::Error { Ok(Address::from(s)) }
}
impl<'de> serde::Deserialize<'de> for Address {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: serde::Deserializer<'de> { deserializer.deserialize_str(AddressVisitor) }
}
