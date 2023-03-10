/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::borrow::Cow;
use std::iter::{FromIterator, Iterator};

use serde::{Deserialize, Serialize};

/// A simple flat sorted map backed by a vector and binary search.
///
/// This doesn't support gradual adding of keys or removal of keys, but only construction
/// from an iterator of keys and values. It also implements Serialize and Deserialize and
/// is mainly intended for memory and space efficient serializable lookup tables.
///
/// If the iterator supplies more than one key with different values, which of these is
/// included is undefined.
#[derive(Serialize, Deserialize, PartialEq, Eq, Clone)]
#[repr(transparent)]
pub struct FlatSortedMap<'a, K: Eq + Ord + Clone, V: Clone>(Cow<'a, [(K, V)]>);

impl<'a, K: Eq + Ord + Clone, V: Clone> FromIterator<(K, V)> for FlatSortedMap<'a, K, V> {
    #[inline]
    fn from_iter<T: IntoIterator<Item = (K, V)>>(iter: T) -> Self {
        let mut tmp = Vec::from_iter(iter);
        tmp.sort_unstable_by(|a, b| a.0.cmp(&b.0));
        tmp.dedup_by(|a, b| a.0.eq(&b.0));
        Self(Cow::Owned(tmp))
    }
}

impl<'a, K: Eq + Ord + Clone, V: Clone> Default for FlatSortedMap<'a, K, V> {
    #[inline(always)]
    fn default() -> Self {
        Self(Cow::Owned(Vec::new()))
    }
}

impl<'a, K: Eq + Ord + Clone, V: Clone> FlatSortedMap<'a, K, V> {
    #[inline]
    pub fn get(&self, k: &K) -> Option<&V> {
        if let Ok(idx) = self.0.binary_search_by(|a| a.0.cmp(k)) {
            Some(unsafe { &self.0.get_unchecked(idx).1 })
        } else {
            None
        }
    }

    #[inline]
    pub fn contains(&self, k: &K) -> bool {
        self.0.binary_search_by(|a| a.0.cmp(k)).is_ok()
    }

    /// Returns true if this map is valid, meaning that it contains only one of each key and is sorted.
    #[inline]
    pub fn is_valid(&self) -> bool {
        let l = self.0.len();
        if l > 1 {
            for i in 1..l {
                if unsafe { !self.0.get_unchecked(i - 1).0.cmp(&self.0.get_unchecked(i).0).is_lt() } {
                    return false;
                }
            }
        }
        return true;
    }

    #[inline(always)]
    pub fn iter(&self) -> impl Iterator<Item = &(K, V)> {
        self.0.iter()
    }

    #[inline(always)]
    pub fn len(&self) -> usize {
        self.0.len()
    }

    #[inline(always)]
    pub fn is_empty(&self) -> bool {
        self.0.is_empty()
    }
}
