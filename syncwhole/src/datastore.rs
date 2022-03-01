/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2022 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::ops::Bound::Included;
use std::collections::BTreeMap;
use std::sync::{Arc, Mutex};

/// Result returned by DB::load().
pub enum LoadResult<V: AsRef<[u8]> + Send> {
    /// Object was found.
    Ok(V),

    /// Object was not found, including the case of being excluded due to the value of reference_time.
    NotFound,

    /// Supplied reference_time is outside what is available (usually too old).
    TimeNotAvailable
}

/// Result returned by DB::store().
pub enum StoreResult {
    /// Entry was accepted (whether or not an old value was replaced).
    Ok,

    /// Entry was rejected as a duplicate but was otherwise valid.
    Duplicate,

    /// Entry was rejected as invalid.
    ///
    /// An invalid entry is one that is malformed, fails a signature check, etc., and returning
    /// this causes the synchronization service to drop the link to the node that sent it.
    Rejected
}

/// API to be implemented by the data set we want to replicate.
///
/// The API specified here supports temporally subjective data sets. These are data sets
/// where the existence or non-existence of a record may depend on the (real world) time.
/// A parameter for reference time allows a remote querying node to send its own "this is
/// what time I think it is" value to be considered locally so that data can be replicated
/// as of any given time.
///
/// The KEY_IS_COMPUTED constant must be set to indicate whether keys are a function of
/// values. If this is true, get_key() must be implemented.
///
/// The implementation must be thread safe.
pub trait DataStore: Sync + Send {
    /// Type to be enclosed in the Ok() enum value in LoadResult.
    type LoadResultValueType: AsRef<[u8]> + Send;

    /// Size of keys, which must be fixed in length. These are typically hashes.
    const KEY_SIZE: usize;

    /// Maximum size of a value in bytes.
    const MAX_VALUE_SIZE: usize;

    /// This should be true if the key is computed, such as by hashing the value.
    ///
    /// If this is true only values are sent over the wire and get_key() is used to compute
    /// keys from values. If this is false both keys and values are replicated.
    const KEY_IS_COMPUTED: bool;

    /// Get the key corresponding to a value.
    ///
    /// If KEY_IS_COMPUTED is true this must be implemented. The default implementation
    /// panics to indicate this. If KEY_IS_COMPUTED is false this is never called.
    #[allow(unused_variables)]
    fn get_key(&self, value: &[u8], key: &mut [u8]) {
        panic!("get_key() must be implemented if KEY_IS_COMPUTED is true");
    }

    /// Get the domain of this data store, which is just an arbitrary unique identifier.
    fn domain(&self) -> &str;

    /// Get an item if it exists as of a given reference time.
    ///
    /// The supplied key must be of length KEY_SIZE or this may panic.
    fn load(&self, reference_time: i64, key: &[u8]) -> LoadResult<Self::LoadResultValueType>;

    /// Store an item in the data store and return Ok, Duplicate, or Rejected.
    ///
    /// The supplied key must be of length KEY_SIZE or this may panic.
    ///
    /// Note that no time is supplied here. The data store must determine this in an implementation
    /// dependent manner if this is a temporally subjective data store. It could be determined by
    /// the wall clock, from the object itself, etc.
    fn store(&self, key: &[u8], value: &[u8]) -> StoreResult;

    /// Get the number of items under a prefix as of a given reference time.
    ///
    /// The default implementation uses for_each(). This can be specialized if it can be done
    /// more efficiently than that.
    fn count(&self, reference_time: i64, key_prefix: &[u8]) -> u64 {
        let mut cnt: u64 = 0;
        self.for_each(reference_time, key_prefix, |_, _| {
            cnt += 1;
            true
        });
        cnt
    }

    /// Iterate through keys beneath a key prefix, stopping at the end or if the function returns false.
    ///
    /// The default implementation uses for_each().
    fn for_each_key<F: FnMut(&[u8]) -> bool>(&self, reference_time: i64, key_prefix: &[u8], mut f: F) {
        self.for_each(reference_time, key_prefix, |k, _| f(k));
    }

    /// Iterate through keys and values beneath a key prefix, stopping at the end or if the function returns false.
    fn for_each<F: FnMut(&[u8], &[u8]) -> bool>(&self, reference_time: i64, key_prefix: &[u8], f: F);
}

/// A simple in-memory data store backed by a BTreeMap.
pub struct MemoryDatabase<const KEY_SIZE: usize> {
    max_age: i64,
    domain: String,
    db: Mutex<BTreeMap<[u8; KEY_SIZE], (i64, Arc<[u8]>)>>
}

impl<const KEY_SIZE: usize> MemoryDatabase<KEY_SIZE> {
    pub fn new(max_age: i64, domain: String) -> Self {
        Self {
            max_age: if max_age > 0 { max_age } else { i64::MAX },
            domain,
            db: Mutex::new(BTreeMap::new())
        }
    }
}

impl<const KEY_SIZE: usize> DataStore for MemoryDatabase<KEY_SIZE> {
    type LoadResultValueType = Arc<[u8]>;
    const KEY_SIZE: usize = KEY_SIZE;
    const MAX_VALUE_SIZE: usize = 65536;
    const KEY_IS_COMPUTED: bool = false;

    fn domain(&self) -> &str { self.domain.as_str() }

    fn load(&self, reference_time: i64, key: &[u8]) -> LoadResult<Self::LoadResultValueType> {
        let db = self.db.lock().unwrap();
        let e = db.get(key);
        if e.is_some() {
            let e = e.unwrap();
            if (reference_time - e.0) <= self.max_age {
                LoadResult::Ok(e.1.clone())
            } else {
                LoadResult::NotFound
            }
        } else {
            LoadResult::NotFound
        }
    }

    fn store(&self, key: &[u8], value: &[u8]) -> StoreResult {
        let ts = crate::ms_since_epoch();
        let mut isdup = false;
        self.db.lock().unwrap().entry(key.try_into().unwrap()).and_modify(|e| {
            if e.1.as_ref().eq(value) {
                isdup = true;
            } else {
                *e = (ts, Arc::from(value));
            }
        }).or_insert_with(|| {
            (ts, Arc::from(value))
        });
        if isdup {
            StoreResult::Duplicate
        } else {
            StoreResult::Ok
        }
    }

    fn for_each<F: FnMut(&[u8], &[u8]) -> bool>(&self, reference_time: i64, key_prefix: &[u8], mut f: F) {
        let mut r_start = [0_u8; KEY_SIZE];
        let mut r_end = [0xff_u8; KEY_SIZE];
        (&mut r_start[0..key_prefix.len()]).copy_from_slice(key_prefix);
        (&mut r_end[0..key_prefix.len()]).copy_from_slice(key_prefix);
        for (k, v) in self.db.lock().unwrap().range((Included(r_start), Included(r_end))) {
            if (reference_time - v.0) <= self.max_age {
                if !f(k, &v.1) {
                    break;
                }
            }
        }
    }
}

impl<const KEY_SIZE: usize> PartialEq for MemoryDatabase<KEY_SIZE> {
    fn eq(&self, other: &Self) -> bool {
        self.max_age == other.max_age && self.db.lock().unwrap().eq(&*other.db.lock().unwrap())
    }
}

impl<const KEY_SIZE: usize> Eq for MemoryDatabase<KEY_SIZE> {}

