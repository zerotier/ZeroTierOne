/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

use std::cmp::Ordering;

pub struct Address(pub u64);

impl ToString for Address {
    fn to_string(&self) -> String {
        format!("{:0>10x}", self.0)
    }
}

impl From<u64> for Address {
    #[inline(always)]
    fn from(i: u64) -> Address {
        Address(i)
    }
}

impl From<&str> for Address {
    #[inline(always)]
    fn from(s: &str) -> Address {
        Address(u64::from_str_radix(s, 16).unwrap_or(0))
    }
}

impl PartialEq for Address {
    #[inline(always)]
    fn eq(&self, other: &Self) -> bool {
        self.0 == other.0
    }
}

impl Eq for Address {}

impl Ord for Address {
    #[inline(always)]
    fn cmp(&self, other: &Self) -> Ordering {
        self.0.cmp(&other.0)
    }
}

impl PartialOrd for Address {
    #[inline(always)]
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.0.cmp(&other.0))
    }
}

impl Clone for Address {
    #[inline(always)]
    fn clone(&self) -> Self {
        Address(self.0)
    }
}

impl serde::Serialize for Address {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: serde::Serializer {
        serializer.serialize_str(self.to_string().as_str())
    }
}

struct AddressVisitor;

impl<'de> serde::de::Visitor<'de> for AddressVisitor {
    type Value = Address;

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("ZeroTier Address in string format")
    }

    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E> where E: serde::de::Error {
        Ok(Address::from(s))
    }
}

impl<'de> serde::Deserialize<'de> for Address {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: serde::Deserializer<'de> {
        deserializer.deserialize_str(AddressVisitor)
    }
}
