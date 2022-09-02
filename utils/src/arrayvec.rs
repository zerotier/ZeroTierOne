// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::mem::{size_of, MaybeUninit};
use std::ptr::{slice_from_raw_parts, slice_from_raw_parts_mut};

/// A simple vector backed by a static sized array with no memory allocations and no overhead construction.
pub struct ArrayVec<T, const C: usize> {
    pub(crate) a: [MaybeUninit<T>; C],
    pub(crate) s: usize,
}

impl<T, const C: usize> ArrayVec<T, C> {
    #[inline(always)]
    pub fn new() -> Self {
        assert_eq!(size_of::<[T; C]>(), size_of::<[MaybeUninit<T>; C]>());
        Self { a: unsafe { MaybeUninit::uninit().assume_init() }, s: 0 }
    }

    #[inline(always)]
    pub fn push(&mut self, v: T) {
        if self.s < C {
            let i = self.s;
            unsafe { self.a.get_unchecked_mut(i).write(v) };
            self.s = i + 1;
        } else {
            panic!();
        }
    }

    #[inline(always)]
    pub fn try_push(&mut self, v: T) -> bool {
        if self.s < C {
            let i = self.s;
            unsafe { self.a.get_unchecked_mut(i).write(v) };
            self.s = i + 1;
            true
        } else {
            false
        }
    }

    #[inline(always)]
    pub fn is_empty(&self) -> bool {
        self.s == 0
    }

    #[inline(always)]
    pub fn len(&self) -> usize {
        self.s
    }

    #[inline(always)]
    pub fn pop(&mut self) -> Option<T> {
        if self.s > 0 {
            let i = self.s - 1;
            debug_assert!(i < C);
            self.s = i;
            Some(unsafe { self.a.get_unchecked(i).assume_init_read() })
        } else {
            None
        }
    }
}

impl<T, const C: usize> Drop for ArrayVec<T, C> {
    #[inline(always)]
    fn drop(&mut self) {
        for i in 0..self.s {
            unsafe { self.a.get_unchecked_mut(i).assume_init_drop() };
        }
    }
}

impl<T, const C: usize> AsRef<[T]> for ArrayVec<T, C> {
    #[inline(always)]
    fn as_ref(&self) -> &[T] {
        unsafe { &*slice_from_raw_parts(self.a.as_ptr().cast(), self.s) }
    }
}

impl<T, const C: usize> AsMut<[T]> for ArrayVec<T, C> {
    #[inline(always)]
    fn as_mut(&mut self) -> &mut [T] {
        unsafe { &mut *slice_from_raw_parts_mut(self.a.as_mut_ptr().cast(), self.s) }
    }
}

#[cfg(test)]
mod tests {
    use super::ArrayVec;

    #[test]
    fn array_vec() {
        let mut v = ArrayVec::<usize, 128>::new();
        for i in 0..128 {
            v.push(i);
        }
        assert_eq!(v.len(), 128);
        assert!(!v.try_push(1000));
        assert_eq!(v.len(), 128);
        for _ in 0..128 {
            assert!(v.pop().is_some());
        }
        assert!(v.pop().is_none());
    }
}
