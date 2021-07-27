use std::str::FromStr;
use std::hash::{Hash, Hasher};

use crate::error::InvalidFormatError;

#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
#[repr(transparent)]
pub struct MAC(u64);

impl MAC {
    #[inline(always)]
    pub fn from_bytes(b: &[u8]) -> Result<MAC, InvalidFormatError> {
        if b.len() >= 6 {
            Ok(MAC((b[0] as u64) << 40 | (b[1] as u64) << 32 | (b[2] as u64) << 24 | (b[3] as u64) << 16 as u64 | (b[4] as u64) << 8 | b[5] as u64))
        } else {
            Err(InvalidFormatError)
        }
    }

    #[inline(always)]
    pub fn to_bytes(&self) -> [u8; 6] {
        [(self.0 >> 40) as u8, (self.0 >> 32) as u8, (self.0 >> 24) as u8, (self.0 >> 16) as u8, (self.0 >> 8) as u8, self.0 as u8]
    }

    #[inline(always)]
    pub fn to_u64(&self) -> u64 {
        self.0
    }
}

impl ToString for MAC {
    #[inline(always)]
    fn to_string(&self) -> String {
        let b: [u8; 6] = self.to_bytes();
        format!("{}:{}:{}:{}:{}:{}", b[0], b[1], b[2], b[3], b[4], b[5])
    }
}

impl FromStr for MAC {
    type Err = InvalidFormatError;

    #[inline(always)]
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        MAC::from_bytes(crate::util::hex::from_string(s).as_slice())
    }
}

impl Default for MAC {
    #[inline(always)]
    fn default() -> MAC {
        MAC(0)
    }
}

impl Hash for MAC {
    #[inline(always)]
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.0.hash(state)
    }
}

impl From<&[u8; 6]> for MAC {
    #[inline(always)]
    fn from(b: &[u8; 6]) -> MAC {
        MAC((b[0] as u64) << 40 | (b[1] as u64) << 32 | (b[2] as u64) << 24 | (b[3] as u64) << 16 as u64 | (b[4] as u64) << 8 | b[5] as u64)
    }
}

impl From<[u8; 6]> for MAC {
    #[inline(always)]
    fn from(b: [u8; 6]) -> MAC {
        Self::from(&b)
    }
}

impl From<u64> for MAC {
    #[inline(always)]
    fn from(i: u64) -> MAC {
        MAC(i)
    }
}
