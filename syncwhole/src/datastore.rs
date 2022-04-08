/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2022 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use async_trait::async_trait;

/// Size of keys, which is the size of a 512-bit hash. This is a protocol constant.
pub const KEY_SIZE: usize = 64;

/// Minimum possible value in a key range (all zero).
pub const MIN_KEY: [u8; KEY_SIZE] = [0; KEY_SIZE];

/// Maximum possible value in a key range (all 0xff).
pub const MAX_KEY: [u8; KEY_SIZE] = [0xff; KEY_SIZE];

/// Result returned by DataStore::store().
pub enum StoreResult {
    /// Entry was accepted.
    Ok,

    /// Entry was a duplicate of one we already have but was otherwise valid.
    Duplicate,

    /// Entry was valid but was ignored for an unspecified reason.
    Ignored,

    /// Entry was rejected as malformed or otherwise invalid (e.g. failed signature check).
    Rejected,
}

/// API to be implemented by the data set we want to replicate.
///
/// Keys as understood by syncwhole are SHA512 hashes of values. The user can of course
/// have their own concept of a "key" separate from this, but that would not be used
/// for data set replication. Replication is content identity based.
///
/// The API specified here supports temporally subjective data sets. These are data sets
/// where the existence or non-existence of a record may depend on the (real world) time.
/// A parameter for reference time allows a remote querying node to send its own "this is
/// what time I think it is" value to be considered locally so that data can be replicated
/// as of any given time.
///
/// In any call with a reference_time it should be ignored if it's zero. A zero reference
/// time should mean include all data that we have.
#[async_trait]
pub trait DataStore: Sync + Send {
    /// Container for values returned by load().
    ///
    /// Making this a trait defined type lets you use Arc<[u8]>, etc. as well as obvious
    /// ones like Box<[u8]> and Vec<u8>.
    type ValueRef: AsRef<[u8]> + Sync + Send + Clone;

    /// Key hash size, always 64 for SHA512.
    const KEY_SIZE: usize = KEY_SIZE;

    /// Maximum size of a value in bytes.
    const MAX_VALUE_SIZE: usize;

    /// Get the current wall clock in milliseconds since Unix epoch.
    fn clock(&self) -> i64;

    /// Get the domain of this data store.
    ///
    /// This is an arbitrary unique identifier that must be the same for all nodes that
    /// are replicating the same data. It's checked on connect to avoid trying to share
    /// data across data sets if this is not desired.
    fn domain(&self) -> &str;

    /// Get an item if it exists as of a given reference time.
    async fn load(&self, reference_time: i64, key: &[u8]) -> Option<Self::ValueRef>;

    /// Check whether this data store contains a key.
    ///
    /// The default implementation just calls load(). Override if a faster version is possible.
    async fn contains(&self, reference_time: i64, key: &[u8]) -> bool {
        self.load(reference_time, key).await.is_some()
    }

    /// Store an item in the data store and return its status.
    ///
    /// Note that no time is supplied here. The data store must determine this in an implementation
    /// dependent manner if this is a temporally subjective data store. It could be determined by
    /// the wall clock, from the object itself, etc.
    ///
    /// The key supplied here will always be the SHA512 hash of the value. There is no need to
    /// re-compute and check the key, but the value must be validated.
    ///
    /// Validation of the value and returning the appropriate StoreResult is important to the
    /// operation of the synchronization algorithm:
    ///
    /// StoreResult::Ok - Value was valid and was accepted and saved.
    ///
    /// StoreResult::Duplicate - Value was valid but is a duplicate of one we already have.
    ///
    /// StoreResult::Ignored - Value was valid but for some other reason was not saved.
    ///
    /// StoreResult::Rejected - Value was not valid, causes link to peer to be dropped.
    ///
    /// Rejected should only be returned if the value actually fails a validity check, signature
    /// verification, proof of work check, or some other required criteria. Ignored must be
    /// returned if the value is valid but is too old or was rejected for some other normal reason.
    async fn store(&self, key: &[u8], value: &[u8]) -> StoreResult;

    /// Get the number of items in a range.
    async fn count(&self, reference_time: i64, key_range_start: &[u8], key_range_end: &[u8]) -> u64;

    /// Get the total number of records in this data store.
    async fn total_count(&self) -> u64;

    /// Iterate through a series of keys in a range (inclusive), stopping when function returns false.
    ///
    /// The default implementation uses for_each() and just drops the value. Specialize if you can do it faster
    /// by only retrieving keys.
    async fn for_each_key<F: Send + FnMut(&[u8]) -> bool>(&self, reference_time: i64, key_range_start: &[u8], key_range_end: &[u8], mut f: F) {
        self.for_each(reference_time, key_range_start, key_range_end, |k, _| f(k)).await;
    }

    /// Iterate through a series of entries in a range (inclusive), stopping when function returns false.
    async fn for_each<F: Send + FnMut(&[u8], &Self::ValueRef) -> bool>(&self, reference_time: i64, key_range_start: &[u8], key_range_end: &[u8], f: F);
}
