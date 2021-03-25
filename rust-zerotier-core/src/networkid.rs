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

#[derive(PartialEq, Eq, Clone, Copy, PartialOrd, Ord)]
pub struct NetworkId(pub u64);

impl Default for NetworkId {
    #[inline(always)]
    fn default() -> NetworkId {
        NetworkId(0)
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
        NetworkId(u64::from_str_radix(s, 16).unwrap_or(0))
    }
}

impl serde::Serialize for NetworkId {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: serde::Serializer {
        if serializer.is_human_readable() {
            serializer.serialize_str(self.to_string().as_str())
        } else {
            serializer.serialize_u64(self.0)
        }
    }
}

struct NetworkIdVisitor;

impl<'de> serde::de::Visitor<'de> for NetworkIdVisitor {
    type Value = NetworkId;
    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result { formatter.write_str("ZeroTier network ID") }
    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E> where E: serde::de::Error { Ok(NetworkId::from(s)) }
    fn visit_u64<E>(self, v: u64) -> Result<Self::Value, E> where E: serde::de::Error { Ok(NetworkId(v)) }
}

impl<'de> serde::Deserialize<'de> for NetworkId {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: serde::Deserializer<'de> {
        if deserializer.is_human_readable() {
            deserializer.deserialize_str(NetworkIdVisitor)
        } else {
            deserializer.deserialize_u64(NetworkIdVisitor)
        }
    }
}
