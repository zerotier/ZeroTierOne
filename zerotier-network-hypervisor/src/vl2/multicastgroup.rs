// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use crate::vl1::MAC;
use std::cmp::Ordering;
use std::hash::{Hash, Hasher};

#[derive(Clone, Copy, PartialEq, Eq)]
pub struct MulticastGroup {
    pub mac: MAC,
    pub adi: u32,
}

impl From<&MAC> for MulticastGroup {
    #[inline(always)]
    fn from(mac: &MAC) -> Self {
        Self { mac: mac.clone(), adi: 0 }
    }
}

impl From<MAC> for MulticastGroup {
    #[inline(always)]
    fn from(mac: MAC) -> Self {
        Self { mac, adi: 0 }
    }
}

impl Ord for MulticastGroup {
    #[inline(always)]
    fn cmp(&self, other: &Self) -> Ordering {
        let o = self.mac.cmp(&other.mac);
        match o {
            Ordering::Equal => self.adi.cmp(&other.adi),
            _ => o,
        }
    }
}

impl PartialOrd for MulticastGroup {
    #[inline(always)]
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Hash for MulticastGroup {
    #[inline(always)]
    fn hash<H: Hasher>(&self, state: &mut H) {
        state.write_u64(self.mac.to_u64());
        state.write_u32(self.adi);
    }
}
