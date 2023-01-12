use alloc::collections::{BTreeMap, BTreeSet};
use core::hash::{BuildHasher, Hash};
#[cfg(feature = "indexmap")]
use indexmap_crate::IndexMap;
use std::collections::{HashMap, HashSet};

#[deprecated = "This is serde's default behavior."]
pub trait DuplicateInsertsFirstWinsSet<T> {
    fn new(size_hint: Option<usize>) -> Self;

    /// Insert the value into the set, if there is not already an existing value
    fn insert(&mut self, value: T);
}

pub trait DuplicateInsertsFirstWinsMap<K, V> {
    fn new(size_hint: Option<usize>) -> Self;

    /// Insert the value into the map, if there is not already an existing value
    fn insert(&mut self, key: K, value: V);
}

#[allow(deprecated)]
impl<T, S> DuplicateInsertsFirstWinsSet<T> for HashSet<T, S>
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
    fn insert(&mut self, value: T) {
        // Hashset already fulfils the contract and always keeps the first value
        self.insert(value);
    }
}

#[allow(deprecated)]
impl<T> DuplicateInsertsFirstWinsSet<T> for BTreeSet<T>
where
    T: Ord,
{
    #[inline]
    fn new(_size_hint: Option<usize>) -> Self {
        Self::new()
    }

    #[inline]
    fn insert(&mut self, value: T) {
        // BTreeSet already fulfils the contract and always keeps the first value
        self.insert(value);
    }
}

impl<K, V, S> DuplicateInsertsFirstWinsMap<K, V> for HashMap<K, V, S>
where
    K: Eq + Hash,
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
    fn insert(&mut self, key: K, value: V) {
        use std::collections::hash_map::Entry;

        match self.entry(key) {
            // we want to keep the first value, so do nothing
            Entry::Occupied(_) => {}
            Entry::Vacant(vacant) => {
                vacant.insert(value);
            }
        }
    }
}

#[cfg(feature = "indexmap")]
impl<K, V, S> DuplicateInsertsFirstWinsMap<K, V> for IndexMap<K, V, S>
where
    K: Eq + Hash,
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
    fn insert(&mut self, key: K, value: V) {
        use indexmap_crate::map::Entry;

        match self.entry(key) {
            // we want to keep the first value, so do nothing
            Entry::Occupied(_) => {}
            Entry::Vacant(vacant) => {
                vacant.insert(value);
            }
        }
    }
}

impl<K, V> DuplicateInsertsFirstWinsMap<K, V> for BTreeMap<K, V>
where
    K: Ord,
{
    #[inline]
    fn new(_size_hint: Option<usize>) -> Self {
        Self::new()
    }

    #[inline]
    fn insert(&mut self, key: K, value: V) {
        use alloc::collections::btree_map::Entry;

        match self.entry(key) {
            // we want to keep the first value, so do nothing
            Entry::Occupied(_) => {}
            Entry::Vacant(vacant) => {
                vacant.insert(value);
            }
        }
    }
}
