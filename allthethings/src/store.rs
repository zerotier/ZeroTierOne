/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use crate::IDENTITY_HASH_SIZE;

/// Result returned by Store::put().
pub enum StorePutResult {
    /// Datum stored successfully.
    Ok,
    /// Datum is one we already have.
    Duplicate,
    /// Value is invalid. (this may result in dropping connections to peers, etc.)
    Invalid,
    /// Value is not invalid but it was not added to the data store for some neutral reason.
    Ignored,
}

/// Trait that must be implemented by the data store that is to be replicated.
pub trait Store: Sync + Send {
    /// Type returned by get(), which can be anything that contains a byte slice.
    type Object: AsRef<[u8]>;

    /// Get the local time in milliseconds since Unix epoch.
    fn local_time(&self) -> u64;

    /// Get an object from the database.
    fn get(&self, reference_time: u64, identity_hash: &[u8; IDENTITY_HASH_SIZE]) -> Option<Self::Object>;

    /// Store an entry in the database.
    fn put(&self, reference_time: u64, identity_hash: &[u8; IDENTITY_HASH_SIZE], object: &[u8]) -> StorePutResult;

    /// Check if we have an object by its identity hash.
    fn have(&self, reference_time: u64, identity_hash: &[u8; IDENTITY_HASH_SIZE]) -> bool;

    /// Get the total count of objects.
    fn total_count(&self, reference_time: u64) -> Option<u64>;

    /// Iterate over a range of identity hashes and values.
    /// This calls the supplied function for each object. If the function returns false iteration stops.
    fn for_each<F: FnMut(&[u8], &Self::Object) -> bool>(&self, reference_time: u64, start: &[u8; IDENTITY_HASH_SIZE], end: &[u8; IDENTITY_HASH_SIZE], f: F);

    /// Iterate over a range of identity hashes.
    /// This calls the supplied function for each hash. If the function returns false iteration stops.
    fn for_each_identity_hash<F: FnMut(&[u8]) -> bool>(&self, reference_time: u64, start: &[u8; IDENTITY_HASH_SIZE], end: &[u8; IDENTITY_HASH_SIZE], f: F);

    /// Count the number of identity hash keys in this range (inclusive) of identity hashes.
    /// This may return None if an error occurs, but should return 0 if the set is empty.
    fn count(&self, reference_time: u64, start: &[u8; IDENTITY_HASH_SIZE], end: &[u8; IDENTITY_HASH_SIZE]) -> Option<u64>;
}
