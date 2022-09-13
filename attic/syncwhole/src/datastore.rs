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

/// API to be implemented by the data set we want to replicate.
///
/// Keys as used in this API are SHA512 hashes of values.
///
/// Range queries take an optional subset parameter. The format and interpretation of
/// this is entirely up to the implementer of DataStore. It could contain a time, a SQL
/// query, a set of certificates, anything. Its purpose is to select which items we want
/// from remote nodes so that we can replicate only a subset of a larger set of data.
/// Other nodes can also supply a subset to this one, so it's important that remote subset
/// values supplied to the local data store be handled correctly.
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

    /// Get the subset that should be sent to remote nodes in queries.
    async fn local_subset(&self) -> Option<Self::ValueRef>;

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

    /// Iterate through keys in a range.
    ///
    /// Keys MUST be output in ascending binary sort order.
    async fn keys<F: Send + FnMut(&[u8]) -> bool>(
        &self,
        subset: Option<&[u8]>,
        range_start: &[u8; KEY_SIZE],
        range_end: &[u8; KEY_SIZE],
        f: F,
    );

    /// Iterate through values in a range.
    ///
    /// Entries MUST be output in ascending binary sort order.
    async fn values<F: Send + FnMut(&[u8], &[u8]) -> bool>(
        &self,
        subset: Option<&[u8]>,
        range_start: &[u8; KEY_SIZE],
        range_end: &[u8; KEY_SIZE],
        f: F,
    );
}
