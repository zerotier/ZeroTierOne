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

#[derive(Copy)]
pub struct NetworkId(pub u64);

impl NetworkId {
    #[inline(always)]
    pub fn new_from_string(s: &str) -> NetworkId {
        return NetworkId(u64::from_str_radix(s, 16).unwrap_or(0));
    }
}

impl ToString for NetworkId {
    fn to_string(&self) -> String {
        format!("{:0>16x}", self.0)
    }
}

impl From<u64> for NetworkId {
    #[inline(always)]
    fn from(n: u64) -> Self {
        NetworkId(n)
    }
}

impl From<&str> for NetworkId {
    #[inline(always)]
    fn from(s: &str) -> Self {
        NetworkId::new_from_string(s)
    }
}

impl Ord for NetworkId {
    #[inline(always)]
    fn cmp(&self, other: &Self) -> Ordering {
        self.0.cmp(&other.0)
    }
}

impl PartialOrd for NetworkId {
    #[inline(always)]
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.0.cmp(&other.0))
    }
}

impl PartialEq for NetworkId {
    #[inline(always)]
    fn eq(&self, other: &Self) -> bool {
        self.0 == other.0
    }
}

impl Clone for NetworkId {
    #[inline(always)]
    fn clone(&self) -> Self {
        NetworkId(self.0)
    }
}

impl Eq for NetworkId {}

impl serde::Serialize for NetworkId {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: serde::Serializer {
        serializer.serialize_str(self.to_string().as_str())
    }
}

struct NetworkIdVisitor;

impl<'de> serde::de::Visitor<'de> for NetworkIdVisitor {
    type Value = NetworkId;

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("ZeroTier Address in string format")
    }

    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E> where E: serde::de::Error {
        Ok(NetworkId::new_from_string(s))
    }
}

impl<'de> serde::Deserialize<'de> for NetworkId {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: serde::Deserializer<'de> {
        deserializer.deserialize_str(NetworkIdVisitor)
    }
}
