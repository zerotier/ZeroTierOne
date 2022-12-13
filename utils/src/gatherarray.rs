// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::mem::{size_of, MaybeUninit};
use std::ptr::copy_nonoverlapping;

use crate::arrayvec::ArrayVec;

/// A fixed sized array of items to be gathered with fast check logic to return when complete.
///
/// This supports a maximum capacity of 64 and will panic if created with a larger value for C.
pub struct GatherArray<T, const C: usize> {
    a: [MaybeUninit<T>; C],
    have_bits: u64,
    have_count: u8,
    goal: u8,
}

impl<T, const C: usize> GatherArray<T, C> {
    /// Create a new gather array, which must be initialized prior to use.
    #[inline(always)]
    pub fn new(goal: u8) -> Self {
        assert!(C <= 64);
        assert!(goal <= (C as u8));
        assert_eq!(size_of::<[T; C]>(), size_of::<[MaybeUninit<T>; C]>());
        Self {
            a: unsafe { MaybeUninit::uninit().assume_init() },
            have_bits: 0,
            have_count: 0,
            goal,
        }
    }

    /// Add an item to the array if we don't have this index anymore, returning complete array if all parts are here.
    #[inline(always)]
    pub fn add(&mut self, index: u8, value: T) -> Option<ArrayVec<T, C>> {
        if index < self.goal {
            let mut have = self.have_bits;
            let got = 1u64.wrapping_shl(index as u32);
            if (have & got) == 0 {
                have |= got;
                self.have_bits = have;
                let count = self.have_count + 1;
                self.have_count = count;
                let goal = self.goal as usize;
                unsafe {
                    self.a.get_unchecked_mut(index as usize).write(value);
                    if (self.have_count as usize) == goal {
                        debug_assert_eq!(0xffffffffffffffffu64.wrapping_shr(64 - goal as u32), have);
                        let mut tmp = ArrayVec::new();
                        copy_nonoverlapping(
                            self.a.as_ptr().cast::<u8>(),
                            tmp.a.as_mut_ptr().cast::<u8>(),
                            size_of::<MaybeUninit<T>>() * goal,
                        );
                        tmp.s = goal;
                        self.goal = 0;
                        return Some(tmp);
                    }
                }
            }
        }
        return None;
    }
}

impl<T, const C: usize> Drop for GatherArray<T, C> {
    #[inline]
    fn drop(&mut self) {
        let have = self.have_bits;
        for i in 0..self.goal {
            if (have & 1u64.wrapping_shl(i as u32)) != 0 {
                unsafe { self.a.get_unchecked_mut(i as usize).assume_init_drop() };
            }
        }
        self.goal = 0;
    }
}

#[cfg(test)]
mod tests {
    use super::GatherArray;

    #[test]
    fn gather_array() {
        for goal in 2u8..64u8 {
            let mut m = GatherArray::<u8, 64>::new(goal);
            for x in 0..(goal - 1) {
                assert!(m.add(x, x).is_none());
            }
            let r = m.add(goal - 1, goal - 1).unwrap();
            for x in 0..goal {
                assert_eq!(r.as_ref()[x as usize], x);
            }
        }
    }
}
