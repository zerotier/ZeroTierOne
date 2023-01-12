use alloc::collections::BTreeSet;
use core::hash::{BuildHasher, Hash};
#[cfg(feature = "indexmap")]
use indexmap_crate::IndexSet;
use std::collections::HashSet;

pub trait DuplicateInsertsLastWinsSet<T> {
    fn new(size_hint: Option<usize>) -> Self;

    /// Insert or replace the existing value
    fn replace(&mut self, value: T);
}

impl<T, S> DuplicateInsertsLastWinsSet<T> for HashSet<T, S>
where
    T: Eq + Hash,
    S: BuildHasher + Default,
{
    #[inline]
    fn new(size_hint: Option<usize>) -> Self {
        match size_hint {
            Some(size) => Self::with_capacity_and_hasher(size, S::default()),
            None => Self::with_hasher(S::default()),
        }
    }

    #[inline]
    fn replace(&mut self, value: T) {
        // Hashset already fulfils the contract
        self.replace(value);
    }
}

#[cfg(feature = "indexmap")]
impl<T, S> DuplicateInsertsLastWinsSet<T> for IndexSet<T, S>
where
    T: Eq + Hash,
    S: BuildHasher + Default,
{
    #[inline]
    fn new(size_hint: Option<usize>) -> Self {
        match size_hint {
            Some(size) => Self::with_capacity_and_hasher(size, S::default()),
            None => Self::with_hasher(S::default()),
        }
    }

    #[inline]
    fn replace(&mut self, value: T) {
        // Hashset already fulfils the contract
        self.replace(value);
    }
}

impl<T> DuplicateInsertsLastWinsSet<T> for BTreeSet<T>
where
    T: Ord,
{
    #[inline]
    fn new(_size_hint: Option<usize>) -> Self {
        Self::new()
    }

    #[inline]
    fn replace(&mut self, value: T) {
        // BTreeSet already fulfils the contract
        self.replace(value);
    }
}
