// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::mem::MaybeUninit;

/// A FIFO ring buffer.
pub struct RingBuffer<T, const C: usize> {
    a: [MaybeUninit<T>; C],
    p: usize,
}

impl<T, const C: usize> RingBuffer<T, C> {
    #[inline]
    pub fn new() -> Self {
        #[allow(invalid_value)]
        let mut tmp: Self = unsafe { MaybeUninit::uninit().assume_init() };
        tmp.p = 0;
        tmp
    }

    /// Add an element to the buffer, replacing old elements if full.
    #[inline]
    pub fn add(&mut self, o: T) {
        let p = self.p;
        if p < C {
            unsafe { self.a.get_unchecked_mut(p).write(o) };
        } else {
            unsafe { *self.a.get_unchecked_mut(p % C).assume_init_mut() = o };
        }
        self.p = p.wrapping_add(1);
    }

    /// Clear the buffer and drop all elements.
    #[inline]
    pub fn clear(&mut self) {
        for i in 0..C.min(self.p) {
            unsafe { self.a.get_unchecked_mut(i).assume_init_drop() };
        }
        self.p = 0;
    }

    /// Gets an iterator that dumps the contents of the buffer in FIFO order.
    #[inline]
    pub fn iter(&self) -> RingBufferIterator<'_, T, C> {
        let s = C.min(self.p);
        RingBufferIterator { b: self, s, i: self.p.wrapping_sub(s) }
    }
}

impl<T, const C: usize> Default for RingBuffer<T, C> {
    #[inline(always)]
    fn default() -> Self {
        Self::new()
    }
}

impl<T, const C: usize> Drop for RingBuffer<T, C> {
    #[inline(always)]
    fn drop(&mut self) {
        self.clear();
    }
}

pub struct RingBufferIterator<'a, T, const C: usize> {
    b: &'a RingBuffer<T, C>,
    s: usize,
    i: usize,
}

impl<'a, T, const C: usize> Iterator for RingBufferIterator<'a, T, C> {
    type Item = &'a T;

    #[inline]
    fn next(&mut self) -> Option<Self::Item> {
        let s = self.s;
        if s > 0 {
            let i = self.i;
            self.s = s.wrapping_sub(1);
            self.i = i.wrapping_add(1);
            Some(unsafe { self.b.a.get_unchecked(i % C).assume_init_ref() })
        } else {
            None
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn fifo() {
        let mut tmp: RingBuffer<i32, 8> = RingBuffer::new();
        let mut tmp2 = Vec::new();
        for i in 0..4 {
            tmp.add(i);
            tmp2.push(i);
        }
        for (i, j) in tmp.iter().zip(tmp2.iter()) {
            assert_eq!(*i, *j);
        }
        tmp.clear();
        tmp2.clear();
        for i in 0..23 {
            tmp.add(i);
            tmp2.push(i);
        }
        while tmp2.len() > 8 {
            tmp2.remove(0);
        }
        for (i, j) in tmp.iter().zip(tmp2.iter()) {
            assert_eq!(*i, *j);
        }
    }
}
