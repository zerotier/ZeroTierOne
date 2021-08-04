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

use std::hash::{Hash, Hasher};

#[derive(PartialEq, Eq, Clone, Copy, PartialOrd, Ord)]
pub struct MAC(pub u64);

impl Default for MAC {
    #[inline(always)]
    fn default() -> MAC {
        MAC(0)
    }
}

impl MAC {
    #[inline(always)]
    fn to_bytes(&self) -> [u8; 6] {
        [(self.0 >> 40) as u8, (self.0 >> 32) as u8, (self.0 >> 24) as u8, (self.0 >> 16) as u8, (self.0 >> 8) as u8, self.0 as u8]
    }
}

impl Hash for MAC {
    #[inline(always)]
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.0.hash(state);
    }
}

impl ToString for MAC {
    fn to_string(&self) -> String {
        let x = self.0;
        format!("{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}", (x >> 40) & 0xff, (x >> 32) & 0xff, (x >> 24) & 0xff, (x >> 16) & 0xff, (x >> 8) & 0xff, x & 0xff)
    }
}

impl From<&[u8]> for MAC {
    #[inline(always)]
    fn from(bytes: &[u8]) -> Self {
        if bytes.len() >= 6 {
            MAC(((bytes[0] as u64) << 40) | ((bytes[1] as u64) << 32) | ((bytes[2] as u64) << 24) | ((bytes[3] as u64) << 16) | ((bytes[4] as u64) << 8) | (bytes[5] as u64))
        } else {
            MAC(0)
        }
    }
}

impl From<&str> for MAC {
    fn from(s: &str) -> MAC {
        MAC(u64::from_str_radix(s.replace(":", "").as_str(), 16).unwrap_or(0))
    }
}

impl serde::Serialize for MAC {
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
    type Value = MAC;
    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result { formatter.write_str("Ethernet MAC address in string format (with or without : separators)") }
    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E> where E: serde::de::Error { Ok(MAC::from(s)) }
    fn visit_bytes<E>(self, v: &[u8]) -> Result<Self::Value, E> where E: serde::de::Error { Ok(MAC::from(v)) }
}

impl<'de> serde::Deserialize<'de> for MAC {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: serde::Deserializer<'de> {
        if deserializer.is_human_readable() {
            deserializer.deserialize_str(AddressVisitor)
        } else {
            deserializer.deserialize_bytes(AddressVisitor)
        }
    }
}
