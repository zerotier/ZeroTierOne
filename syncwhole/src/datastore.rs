/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2022 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

/// Size of keys, which is the size of a 512-bit hash. This is a protocol constant.
pub const KEY_SIZE: usize = 64;

/// Minimum possible key (all zero).
pub const MIN_KEY: [u8; KEY_SIZE] = [0; KEY_SIZE];

/// Maximum possible key (all 0xff).
pub const MAX_KEY: [u8; KEY_SIZE] = [0xff; KEY_SIZE];

/// Generate a range of SHA512 hashes from a prefix and a number of bits.
/// The range will be inclusive and cover all keys under the prefix.
pub fn range_from_prefix(prefix: &[u8], prefix_bits: usize) -> Option<([u8; KEY_SIZE], [u8; KEY_SIZE])> {
    let mut start = [0_u8; KEY_SIZE];
    let mut end = [0xff_u8; KEY_SIZE];
    if prefix_bits > (KEY_SIZE * 8) {
        return None;
    }
    let whole_bytes = prefix_bits / 8;
    let remaining_bits = prefix_bits % 8;
    if prefix.len() < (whole_bytes + ((remaining_bits != 0) as usize)) {
        return None;
    }
    start[0..whole_bytes].copy_from_slice(&prefix[0..whole_bytes]);
    end[0..whole_bytes].copy_from_slice(&prefix[0..whole_bytes]);
    if remaining_bits != 0 {
        start[whole_bytes] |= prefix[whole_bytes];
        end[whole_bytes] &= prefix[whole_bytes] | ((0xff_u8).wrapping_shr(remaining_bits as u32));
    }
    return Some((start, end));
}

/// Result returned by DataStore::load().
pub enum LoadResult<V: AsRef<[u8]> + Send> {
    /// Object was found.
    Ok(V),

    /// Object was not found, including the case of being excluded due to the value of reference_time.
    NotFound,

    /// Supplied reference_time is outside what is available (usually too old).
    TimeNotAvailable,
}

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
/// The implementation must be thread safe and may be called concurrently.
pub trait DataStore: Sync + Send {
    /// Type to be enclosed in the Ok() enum value in LoadResult.
    ///
    /// Allowing this type to be defined lets you use any type that makes sense with
    /// your implementation. Examples include Box<[u8]>, Arc<[u8]>, Vec<u8>, etc.
    type LoadResultValueType: AsRef<[u8]> + Send;

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
    fn load(&self, reference_time: i64, key: &[u8]) -> LoadResult<Self::LoadResultValueType>;

    /// Check whether this data store contains a key.
    ///
    /// The default implementation just uses load(). Override if you can provide a faster
    /// version.
    fn contains(&self, reference_time: i64, key: &[u8]) -> bool {
        match self.load(reference_time, key) {
            LoadResult::Ok(_) => true,
            _ => false,
        }
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
    fn store(&self, key: &[u8], value: &[u8]) -> StoreResult;

    /// Get the number of items in a range.
    fn count(&self, reference_time: i64, key_range_start: &[u8], key_range_end: &[u8]) -> u64;

    /// Get the total number of records in this data store.
    fn total_count(&self) -> u64;

    /// Iterate through keys, stopping if the function returns false.
    ///
    /// The default implementation uses for_each(). This can be specialized if it's faster to
    /// only load keys.
    fn for_each_key<F: FnMut(&[u8]) -> bool>(&self, reference_time: i64, key_range_start: &[u8], key_range_end: &[u8], mut f: F) {
        self.for_each(reference_time, key_range_start, key_range_end, |k, _| f(k));
    }

    /// Iterate through keys and values, stopping if the function returns false.
    fn for_each<F: FnMut(&[u8], &[u8]) -> bool>(&self, reference_time: i64, key_range_start: &[u8], key_range_end: &[u8], f: F);
}
