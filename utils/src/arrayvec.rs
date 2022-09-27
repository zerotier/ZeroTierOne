// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::io::Write;
use std::mem::{size_of, MaybeUninit};
use std::ptr::{slice_from_raw_parts, slice_from_raw_parts_mut};

use serde::ser::SerializeSeq;
use serde::{Deserialize, Deserializer, Serialize, Serializer};

#[derive(Clone, Copy, Debug)]
pub struct OutOfCapacityError<T>(pub T);

impl<T> std::fmt::Display for OutOfCapacityError<T> {
    fn fmt(self: &Self, stream: &mut std::fmt::Formatter) -> std::fmt::Result {
        std::fmt::Display::fmt("ArrayVec out of space", stream)
    }
}

impl<T: std::fmt::Debug> ::std::error::Error for OutOfCapacityError<T> {
    #[inline(always)]
    fn description(self: &Self) -> &str {
        "ArrayVec out of space"
    }
}

/// A simple vector backed by a static sized array with no memory allocations and no overhead construction.
pub struct ArrayVec<T, const C: usize> {
    pub(crate) s: usize,
    pub(crate) a: [MaybeUninit<T>; C],
}

impl<T, const C: usize> Default for ArrayVec<T, C> {
    #[inline(always)]
    fn default() -> Self {
        Self::new()
    }
}

impl<T: PartialEq, const C: usize> PartialEq for ArrayVec<T, C> {
    #[inline(always)]
    fn eq(&self, other: &Self) -> bool {
        let tmp: &[T] = self.as_ref();
        tmp.eq(other.as_ref())
    }
}

impl<T: Eq, const C: usize> Eq for ArrayVec<T, C> {}

impl<T: Clone, const C: usize> Clone for ArrayVec<T, C> {
    fn clone(&self) -> Self {
        debug_assert!(self.s <= C);
        Self {
            s: self.s,
            a: unsafe {
                let mut tmp: [MaybeUninit<T>; C] = MaybeUninit::uninit().assume_init();
                for i in 0..self.s {
                    tmp.get_unchecked_mut(i).write(self.a[i].assume_init_ref().clone());
                }
                tmp
            },
        }
    }
}

impl<T: Clone, const C: usize, const S: usize> From<[T; S]> for ArrayVec<T, C> {
    #[inline(always)]
    fn from(v: [T; S]) -> Self {
        if S <= C {
            let mut tmp = Self::new();
            for i in 0..S {
                tmp.push(v[i].clone());
            }
            tmp
        } else {
            panic!();
        }
    }
}

impl<const C: usize> Write for ArrayVec<u8, C> {
    #[inline(always)]
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        for i in buf.iter() {
            if self.try_push(*i).is_err() {
                return Err(std::io::Error::new(std::io::ErrorKind::Other, "ArrayVec out of space"));
            }
        }
        Ok(buf.len())
    }

    #[inline(always)]
    fn flush(&mut self) -> std::io::Result<()> {
        Ok(())
    }
}

impl<T, const C: usize> TryFrom<Vec<T>> for ArrayVec<T, C> {
    type Error = OutOfCapacityError<T>;

    #[inline(always)]
    fn try_from(mut value: Vec<T>) -> Result<Self, Self::Error> {
        let mut tmp = Self::new();
        for x in value.drain(..) {
            tmp.try_push(x)?;
        }
        Ok(tmp)
    }
}

impl<T: Clone, const C: usize> TryFrom<&Vec<T>> for ArrayVec<T, C> {
    type Error = OutOfCapacityError<T>;

    #[inline(always)]
    fn try_from(value: &Vec<T>) -> Result<Self, Self::Error> {
        let mut tmp = Self::new();
        for x in value.iter() {
            tmp.try_push(x.clone())?;
        }
        Ok(tmp)
    }
}

impl<T: Clone, const C: usize> TryFrom<&[T]> for ArrayVec<T, C> {
    type Error = OutOfCapacityError<T>;

    #[inline(always)]
    fn try_from(value: &[T]) -> Result<Self, Self::Error> {
        let mut tmp = Self::new();
        for x in value.iter() {
            tmp.try_push(x.clone())?;
        }
        Ok(tmp)
    }
}

impl<T, const C: usize> ArrayVec<T, C> {
    #[inline(always)]
    pub fn new() -> Self {
        assert_eq!(size_of::<[T; C]>(), size_of::<[MaybeUninit<T>; C]>());
        Self { s: 0, a: unsafe { MaybeUninit::uninit().assume_init() } }
    }

    #[inline(always)]
    pub fn push(&mut self, v: T) {
        let i = self.s;
        if i < C {
            unsafe { self.a.get_unchecked_mut(i).write(v) };
            self.s = i + 1;
        } else {
            panic!();
        }
    }

    #[inline(always)]
    pub fn try_push(&mut self, v: T) -> Result<(), OutOfCapacityError<T>> {
        if self.s < C {
            let i = self.s;
            unsafe { self.a.get_unchecked_mut(i).write(v) };
            self.s = i + 1;
            Ok(())
        } else {
            Err(OutOfCapacityError(v))
        }
    }

    #[inline(always)]
    pub fn as_bytes(&self) -> &[T] {
        unsafe { &*slice_from_raw_parts(self.a.as_ptr().cast(), self.s) }
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

impl<T: Copy, const C: usize> ArrayVec<T, C> {
    /// Push a slice of copyable objects, panic if capacity exceeded.
    pub fn push_slice(&mut self, v: &[T]) {
        let start = self.s;
        let end = self.s + v.len();
        if end <= C {
            for i in start..end {
                unsafe { self.a.get_unchecked_mut(i).write(*v.get_unchecked(i - start)) };
            }
            self.s = end;
        } else {
            panic!();
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

impl<T: Serialize, const L: usize> Serialize for ArrayVec<T, L> {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let mut seq = serializer.serialize_seq(Some(self.len()))?;
        let sl: &[T] = self.as_ref();
        for i in 0..self.s {
            seq.serialize_element(&sl[i])?;
        }
        seq.end()
    }
}

struct ArrayVecVisitor<'de, T: Deserialize<'de>, const L: usize>(std::marker::PhantomData<&'de T>);

impl<'de, T: Deserialize<'de>, const L: usize> serde::de::Visitor<'de> for ArrayVecVisitor<'de, T, L> {
    type Value = ArrayVec<T, L>;

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str(format!("array of up to {} elements", L).as_str())
    }

    fn visit_seq<A>(self, mut seq: A) -> Result<Self::Value, A::Error>
    where
        A: serde::de::SeqAccess<'de>,
    {
        let mut a = ArrayVec::<T, L>::new();
        while let Some(x) = seq.next_element()? {
            a.push(x);
        }
        Ok(a)
    }
}

impl<'de, T: Deserialize<'de> + 'de, const L: usize> Deserialize<'de> for ArrayVec<T, L> {
    fn deserialize<D>(deserializer: D) -> Result<ArrayVec<T, L>, D::Error>
    where
        D: Deserializer<'de>,
    {
        deserializer.deserialize_seq(ArrayVecVisitor(std::marker::PhantomData::default()))
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
        assert!(!v.try_push(1000).is_ok());
        assert_eq!(v.len(), 128);
        for _ in 0..128 {
            assert!(v.pop().is_some());
        }
        assert!(v.pop().is_none());
    }
}
