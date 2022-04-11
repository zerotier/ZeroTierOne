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

/// Convert a prefix into an inclusive range of keys.
///
/// This is a convenience function for implementing keys_under() with data stores that support
/// straightforward range queries with full keys.
pub fn prefix_to_range(prefix: u64, prefix_bits: u32) -> ([u8; KEY_SIZE], [u8; KEY_SIZE]) {
    let mut a = [0_u8; KEY_SIZE];
    a[0..8].copy_from_slice(&((prefix & 0xffffffffffffffff_u64.wrapping_shl(64 - prefix_bits)).to_be_bytes()));
    let mut b = [0xff_u8; KEY_SIZE];
    b[0..8].copy_from_slice(&((prefix | 0xffffffffffffffff_u64.wrapping_shr(prefix_bits)).to_be_bytes()));
    (a, b)
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

    /// Get the domain of this data store.
    ///
    /// This is an arbitrary unique identifier that must be the same for all nodes that
    /// are replicating the same data. It's checked on connect to avoid trying to share
    /// data across data sets if this is not desired.
    fn domain(&self) -> &str;

    /// Get the reference time that should be used on this side to query remote peers.
    ///
    /// This is typically the local "wall clock" time in milliseconds since Unix epoch.
    fn reference_time(&self) -> i64;

    /// Get an item by identity hash key if it exists.
    async fn load(&self, key: &[u8; KEY_SIZE]) -> Option<Self::ValueRef>;

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
    async fn store(&self, key: &[u8; KEY_SIZE], value: &[u8]) -> StoreResult;

    /// Iterate through keys under a given key prefix.
    ///
    /// The prefix is a bit string up to 64 bits long. The implementation can technically interpret this
    /// any way it wants, but usually this would be the first 64 bits of the key as a big-endian bit string.
    ///
    /// Keys MUST be output in ascending binary sort order.
    async fn keys_under<F: Send + FnMut(&[u8]) -> bool>(&self, reference_time: i64, prefix: u64, prefix_bits: u32, f: F);

    /// Load all record values under a given key prefix.
    ///
    /// This should clear and fill the result, fetching up to the limit values under a given key prefix.
    /// Values may be pushed into the vector in any order.
    async fn values_under(&self, prefix: u64, prefix_bits: u32, result: &mut Vec<Option<Self::ValueRef>>, limit: usize);
}
