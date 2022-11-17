// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::fmt::Debug;
use std::hash::{Hash, Hasher};
use std::num::NonZeroU64;
use std::str::FromStr;

use serde::{Deserialize, Deserializer, Serialize, Serializer};

use crate::protocol::{ADDRESS_RESERVED_PREFIX, ADDRESS_SIZE};

use zerotier_utils::error::InvalidFormatError;
use zerotier_utils::hex;

/// A unique address on the global ZeroTier VL1 network.
#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
#[repr(transparent)]
pub struct Address(NonZeroU64);

impl Address {
    /// Get an address from a 64-bit integer or return None if it is zero or reserved.
    #[inline(always)]
    pub fn from_u64(mut i: u64) -> Option<Address> {
        i &= 0xffffffffff;
        NonZeroU64::new(i).and_then(|ii| {
            if (i >> 32) != ADDRESS_RESERVED_PREFIX as u64 {
                Some(Address(ii))
            } else {
                None
            }
        })
    }

    #[inline(always)]
    pub fn from_bytes(b: &[u8]) -> Option<Address> {
        if b.len() >= ADDRESS_SIZE {
            Self::from_u64((b[0] as u64) << 32 | (b[1] as u64) << 24 | (b[2] as u64) << 16 | (b[3] as u64) << 8 | b[4] as u64)
        } else {
            None
        }
    }

    #[inline(always)]
    pub fn from_bytes_fixed(b: &[u8; ADDRESS_SIZE]) -> Option<Address> {
        Self::from_u64((b[0] as u64) << 32 | (b[1] as u64) << 24 | (b[2] as u64) << 16 | (b[3] as u64) << 8 | b[4] as u64)
    }

    #[inline(always)]
    pub fn to_bytes(&self) -> [u8; ADDRESS_SIZE] {
        let i = self.0.get();
        [(i >> 32) as u8, (i >> 24) as u8, (i >> 16) as u8, (i >> 8) as u8, i as u8]
    }
}

impl From<Address> for u64 {
    #[inline(always)]
    fn from(a: Address) -> Self {
        a.0.get()
    }
}

impl From<&Address> for u64 {
    #[inline(always)]
    fn from(a: &Address) -> Self {
        a.0.get()
    }
}

impl ToString for Address {
    fn to_string(&self) -> String {
        let mut v = self.0.get() << 24;
        let mut s = String::with_capacity(ADDRESS_SIZE * 2);
        for _ in 0..(ADDRESS_SIZE * 2) {
            s.push(hex::HEX_CHARS[(v >> 60) as usize] as char);
            v <<= 4;
        }
        s
    }
}

impl FromStr for Address {
    type Err = InvalidFormatError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Address::from_bytes(hex::from_string(s).as_slice()).map_or_else(|| Err(InvalidFormatError), |a| Ok(a))
    }
}

impl Hash for Address {
    #[inline(always)]
    fn hash<H: Hasher>(&self, state: &mut H) {
        state.write_u64(self.0.get());
    }
}

impl Debug for Address {
    #[inline]
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

#[cfg(test)]
mod tests {
    fn safe_address() -> super::Address {
        let mut addr: Option<super::Address>;

        'retry: loop {
            let rawaddr: u64 = rand::random();
            addr = super::Address::from_u64(rawaddr);

            if addr.is_some() {
                break 'retry;
            }
        }

        addr.unwrap()
    }

    #[test]
    fn address_marshal_u64() {
        let mut rawaddr: u64 = rand::random();
        let addr = super::Address::from_u64(rawaddr);
        assert!(addr.is_some());
        let addr: u64 = addr.unwrap().into();
        assert_eq!(addr, rawaddr & 0xffffffffff);

        rawaddr = 0;
        assert!(super::Address::from_u64(rawaddr).is_none());

        rawaddr = (crate::protocol::ADDRESS_RESERVED_PREFIX as u64) << 32;
        assert!(super::Address::from_u64(rawaddr).is_none());
    }

    #[test]
    fn address_marshal_bytes() {
        use crate::protocol::ADDRESS_SIZE;
        let mut v: Vec<u8> = Vec::with_capacity(ADDRESS_SIZE);
        let mut i = 0;
        while i < ADDRESS_SIZE {
            v.push(rand::random());
            i += 1;
        }

        let addr = super::Address::from_bytes(v.as_slice());
        assert!(addr.is_some());
        assert_eq!(addr.unwrap().to_bytes(), v.as_slice());

        let empty: Vec<u8> = Vec::new();
        let emptyaddr = super::Address::from_bytes(empty.as_slice());
        assert!(emptyaddr.is_none());

        let mut v2: [u8; ADDRESS_SIZE] = [0u8; ADDRESS_SIZE];
        let mut i = 0;
        while i < ADDRESS_SIZE {
            v2[i] = v[i];
            i += 1;
        }

        let addr2 = super::Address::from_bytes_fixed(&v2);
        assert!(addr2.is_some());
        assert_eq!(addr2.unwrap().to_bytes(), v2);

        assert_eq!(addr.unwrap(), addr2.unwrap());
    }

    #[test]
    fn address_to_from_string() {
        use std::str::FromStr;

        for _ in 0..1000 {
            let rawaddr: u64 = rand::random();
            let addr = super::Address::from_u64(rawaddr);

            // NOTE: a regression here is covered by other tests and should not break this test
            // accidentally.
            if addr.is_none() {
                continue;
            }

            let addr = addr.unwrap();
            assert_ne!(addr.to_string(), "");
            assert_eq!(addr.to_string().len(), 10);

            assert_eq!(super::Address::from_str(&addr.to_string()).unwrap(), addr);
        }
    }

    #[test]
    fn address_hash() {
        use std::collections::hash_map::DefaultHasher;
        use std::hash::{Hash, Hasher};

        let mut hasher = DefaultHasher::new();

        let addr = safe_address();
        addr.hash(&mut hasher);
        let result1 = hasher.finish();

        // this loop is mostly to ensure that hash returns a consistent result every time.
        for _ in 0..1000 {
            let mut hasher = std::collections::hash_map::DefaultHasher::new();
            addr.hash(&mut hasher);
            let result2 = hasher.finish();
            assert_ne!(result2.to_string(), "");
            assert_eq!(result1.to_string(), result2.to_string());
        }
    }

    #[test]
    fn address_serialize() {
        let addr = safe_address();

        for _ in 0..1000 {
            assert_eq!(
                serde_json::from_str::<super::Address>(&serde_json::to_string(&addr).unwrap()).unwrap(),
                addr
            );
            assert_eq!(
                serde_cbor::from_slice::<super::Address>(&serde_cbor::to_vec(&addr).unwrap()).unwrap(),
                addr
            );
        }
    }
}
