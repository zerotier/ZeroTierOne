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

use std::cmp::Ordering;

#[derive(PartialEq, Eq, Clone, Copy)]
pub struct MAC(pub u64);

impl ToString for MAC {
    fn to_string(&self) -> String {
        let x = self.0;
        format!("{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}",
            (x >> 40) & 0xff,
            (x >> 32) & 0xff,
            (x >> 24) & 0xff,
            (x >> 16) & 0xff,
            (x >> 8) & 0xff,
            x & 0xff)
    }
}

impl From<&str> for MAC {
    fn from(s: &str) -> MAC {
        MAC(u64::from_str_radix(s.replace(":","").as_str(), 16).unwrap_or(0))
    }
}

impl Ord for MAC {
    #[inline(always)]
    fn cmp(&self, other: &Self) -> Ordering { self.0.cmp(&other.0) }
}

impl PartialOrd for MAC {
    #[inline(always)]
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> { Some(self.0.cmp(&other.0)) }
}

impl serde::Serialize for MAC {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: serde::Serializer { serializer.serialize_str(self.to_string().as_str()) }
}
struct AddressVisitor;
impl<'de> serde::de::Visitor<'de> for AddressVisitor {
    type Value = MAC;
    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result { formatter.write_str("Ethernet MAC address in string format (with or without : separators)") }
    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E> where E: serde::de::Error { Ok(MAC::from(s)) }
}
impl<'de> serde::Deserialize<'de> for MAC {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: serde::Deserializer<'de> { deserializer.deserialize_str(AddressVisitor) }
}
