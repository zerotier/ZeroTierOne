use crate::MAC;
use std::cmp::Ordering;

#[derive(Clone, PartialEq, Eq)]
pub struct MulticastGroup {
    pub mac: MAC,
    pub adi: u32,
}

impl Ord for MulticastGroup {
    #[inline(always)]
    fn cmp(&self, other: &Self) -> Ordering {
        let o1 = self.mac.0.cmp(&other.mac.0);
        if o1 == Ordering::Equal {
            return self.adi.cmp(&other.adi);
        }
        o1
    }
}

impl PartialOrd for MulticastGroup {
    #[inline(always)]
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}
