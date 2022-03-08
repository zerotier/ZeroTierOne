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
use crate::ms_since_epoch;

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

    /// Entry was a duplicate of one we already have but was otherwise valid.
    Duplicate,

    /// Entry was valid but was ignored for an unspecified reason.
    Ignored,

    /// Entry was rejected as malformed or otherwise invalid (e.g. failed signature check).
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
/// The constants KEY_SIZE, MAX_VALUE_SIZE, and KEY_IS_COMPUTED are protocol constants
/// for your replication domain. They can't be changed once defined unless all nodes
/// are upgraded at once.
///
/// The KEY_IS_COMPUTED constant must be set to indicate whether keys are a function of
/// values. If this is true, key_from_value() must be implemented.
///
/// The implementation must be thread safe and may be called concurrently.
pub trait DataStore: Sync + Send {
    /// Type to be enclosed in the Ok() enum value in LoadResult.
    ///
    /// Allowing this type to be defined lets you use any type that makes sense with
    /// your implementation. Examples include Box<[u8]>, Arc<[u8]>, Vec<u8>, etc.
    type LoadResultValueType: AsRef<[u8]> + Send;

    /// Size of keys, which must be fixed in length. These are typically hashes.
    const KEY_SIZE: usize;

    /// Maximum size of a value in bytes.
    const MAX_VALUE_SIZE: usize;

    /// This should be true if the key is computed, such as by hashing the value.
    ///
    /// If this is true then keys do not have to be sent over the wire. Instead they
    /// are computed by calling get_key(). If this is false keys are assumed not to
    /// be computable from values and are explicitly sent.
    const KEY_IS_COMPUTED: bool;

    /// Compute the key corresponding to a value.
    ///
    /// If KEY_IS_COMPUTED is true this must be implemented. The default implementation
    /// panics to indicate this. If KEY_IS_COMPUTED is false this is never called.
    #[allow(unused_variables)]
    fn key_from_value(&self, value: &[u8], key_buffer: &mut [u8]) {
        panic!("key_from_value() must be implemented if KEY_IS_COMPUTED is true");
    }

    /// Get the current wall clock in milliseconds since Unix epoch.
    ///
    /// This is delegated to the data store to support scenarios where you want to fix
    /// the clock or snapshot at a given time.
    fn clock(&self) -> i64;

    /// Get the domain of this data store.
    ///
    /// This is an arbitrary unique identifier that must be the same for all nodes that
    /// are replicating the same data. It's checked on connect to avoid trying to share
    /// data across data sets if this is not desired.
    fn domain(&self) -> &str;

    /// Get an item if it exists as of a given reference time.
    fn load(&self, reference_time: i64, key: &[u8]) -> LoadResult<Self::LoadResultValueType>;

    /// Store an item in the data store and return its status.
    ///
    /// Note that no time is supplied here. The data store must determine this in an implementation
    /// dependent manner if this is a temporally subjective data store. It could be determined by
    /// the wall clock, from the object itself, etc.
    ///
    /// The implementation is responsible for validating inputs and returning 'Rejected' if they
    /// are invalid. A return value of Rejected can be used to do things like drop connections
    /// to peers that send invalid data, so it should only be returned if the data is malformed
    /// or something like a signature check fails. The 'Ignored' enum value should be returned
    /// for inputs that are valid but were not stored for some other reason, such as being
    /// expired. It's important to return 'Ok' for accepted values to hint to the replicator
    /// that they should be aggressively advertised to other peers.
    ///
    /// If KEY_IS_COMPUTED is true, the key supplied here can be assumed to be correct. It will
    /// have been computed via get_key().
    fn store(&self, key: &[u8], value: &[u8]) -> StoreResult;

    /// Get the number of items under a prefix as of a given reference time.
    ///
    /// The default implementation uses for_each_key(). This can be specialized if it can
    /// be done more efficiently than that.
    fn count(&self, reference_time: i64, key_prefix: &[u8]) -> u64 {
        let mut cnt: u64 = 0;
        self.for_each_key(reference_time, key_prefix, |_| {
            cnt += 1;
            true
        });
        cnt
    }

    /// Get the total number of records in this data store.
    fn total_count(&self) -> u64;

    /// Iterate through keys beneath a key prefix, stopping if the function returns false.
    ///
    /// The default implementation uses for_each(). This can be specialized if it's faster to
    /// only load keys.
    fn for_each_key<F: FnMut(&[u8]) -> bool>(&self, reference_time: i64, key_prefix: &[u8], mut f: F) {
        self.for_each(reference_time, key_prefix, |k, _| f(k));
    }

    /// Iterate through keys and values beneath a key prefix, stopping if the function returns false.
    fn for_each<F: FnMut(&[u8], &[u8]) -> bool>(&self, reference_time: i64, key_prefix: &[u8], f: F);
}

/// A simple in-memory data store backed by a BTreeMap.
pub struct MemoryDataStore<const KEY_SIZE: usize> {
    max_age: i64,
    domain: String,
    db: Mutex<BTreeMap<[u8; KEY_SIZE], (i64, Arc<[u8]>)>>
}

impl<const KEY_SIZE: usize> MemoryDataStore<KEY_SIZE> {
    pub fn new(max_age: i64, domain: String) -> Self {
        Self {
            max_age: if max_age > 0 { max_age } else { i64::MAX },
            domain,
            db: Mutex::new(BTreeMap::new())
        }
    }
}

impl<const KEY_SIZE: usize> DataStore for MemoryDataStore<KEY_SIZE> {
    type LoadResultValueType = Arc<[u8]>;
    const KEY_SIZE: usize = KEY_SIZE;
    const MAX_VALUE_SIZE: usize = 65536;
    const KEY_IS_COMPUTED: bool = false;

    fn clock(&self) -> i64 { ms_since_epoch() }

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

    fn total_count(&self) -> u64 { self.db.lock().unwrap().len() as u64 }

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

impl<const KEY_SIZE: usize> PartialEq for MemoryDataStore<KEY_SIZE> {
    fn eq(&self, other: &Self) -> bool {
        self.max_age == other.max_age && self.domain == other.domain && self.db.lock().unwrap().eq(&*other.db.lock().unwrap())
    }
}

impl<const KEY_SIZE: usize> Eq for MemoryDataStore<KEY_SIZE> {}

impl<const KEY_SIZE: usize> Clone for MemoryDataStore<KEY_SIZE> {
    fn clone(&self) -> Self {
        Self {
            max_age: self.max_age,
            domain: self.domain.clone(),
            db: Mutex::new(self.db.lock().unwrap().clone())
        }
    }
}
