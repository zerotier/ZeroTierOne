use std::str::FromStr;
use std::hash::{Hash, Hasher};

use crate::vl1::protocol::ADDRESS_RESERVED_PREFIX;
use crate::error::InvalidFormatError;
use crate::util::hex::HEX_CHARS;

#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
#[repr(transparent)]
pub struct Address(u64);

impl Address {
    #[inline(always)]
    pub fn from_bytes(b: &[u8]) -> Result<Address, InvalidFormatError> {
        if b.len() >= 5 {
            Ok(Address((b[0] as u64) << 32 | (b[1] as u64) << 24 | (b[2] as u64) << 16 | (b[3] as u64) << 8 as u64 | b[4] as u64))
        } else {
            Err(InvalidFormatError("invalid ZeroTier address"))
        }
    }

    #[inline(always)]
    pub fn is_reserved(&self) -> bool {
        (self.0 >> 32) as usize == ADDRESS_RESERVED_PREFIX as usize
    }

    #[inline(always)]
    pub fn is_valid(&self) -> bool {
        self.0 != 0 && !self.is_reserved()
    }

    #[inline(always)]
    pub fn is_nil(&self) -> bool {
        self.0 == 0
    }

    #[inline(always)]
    pub fn to_bytes(&self) -> [u8; 5] {
        [(self.0 >> 32) as u8, (self.0 >> 24) as u8, (self.0 >> 16) as u8, (self.0 >> 8) as u8, self.0 as u8]
    }

    #[inline(always)]
    pub fn to_u64(&self) -> u64 {
        self.0
    }
}

impl ToString for Address {
    #[inline(always)]
    fn to_string(&self) -> String {
        let mut v = self.0 << 24;
        let mut s = String::new();
        s.reserve(10);
        for _ in 0..10 {
            s.push(HEX_CHARS[(v >> 60) as usize] as char);
            v <<= 4;
        }
        s
    }
}

impl FromStr for Address {
    type Err = InvalidFormatError;

    #[inline(always)]
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Address::from_bytes(crate::util::hex::from_string(s).as_slice())
    }
}

impl Default for Address {
    #[inline(always)]
    fn default() -> Address {
        Address(0)
    }
}

impl Hash for Address {
    #[inline(always)]
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.0.hash(state)
    }
}

impl From<&[u8; 5]> for Address {
    #[inline(always)]
    fn from(b: &[u8; 5]) -> Address {
        Address((b[0] as u64) << 32 | (b[1] as u64) << 24 | (b[2] as u64) << 16 | (b[3] as u64) << 8 as u64 | b[4] as u64)
    }
}

impl From<[u8; 5]> for Address {
    #[inline(always)]
    fn from(b: [u8; 5]) -> Address {
        Self::from(&b)
    }
}

impl From<u64> for Address {
    #[inline(always)]
    fn from(i: u64) -> Address {
        Address(i)
    }
}
