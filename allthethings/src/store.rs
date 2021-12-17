/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use smol::net::SocketAddr;

use crate::IDENTITY_HASH_SIZE;

/// Result code from the put() method in Database.
pub enum StoreObjectResult {
    /// Datum stored successfully.
    Ok,
    /// Datum is one we already have.
    Duplicate,
    /// Value is invalid. (this may result in dropping connections to peers, etc.)
    Invalid,
    /// Value is not invalid but it was not added to the data store for some neutral reason.
    Ignored,
}

/// Trait that must be implemented for the data store that is to be replicated.
pub trait Store: Sync + Send {
    /// Get the total size of this data set in objects.
    fn total_size(&self) -> u64;

    /// Get an object from the database.
    fn get(&self, identity_hash: &[u8; IDENTITY_HASH_SIZE]) -> Option<Vec<u8>>;

    /// Store an entry in the database.
    fn put(&self, identity_hash: &[u8; IDENTITY_HASH_SIZE], object: &[u8]) -> StoreObjectResult;

    /// Check if we have an object by its identity hash.
    fn have(&self, identity_hash: &[u8; IDENTITY_HASH_SIZE]) -> bool;

    /// Count the number of identity hash keys in this range (inclusive) of identity hashes.
    /// This may return None if an error occurs, but should return 0 if the set is empty.
    fn count(&self, start: &[u8; IDENTITY_HASH_SIZE], end: &[u8; IDENTITY_HASH_SIZE]) -> Option<u64>;

    /// Called when a connection to a remote node was successful.
    /// This is always called on successful outbound connect.
    fn save_remote_endpoint(&self, to_address: &SocketAddr);

    /// Get a remote endpoint to try.
    /// This can return endpoints in any order and is used to try to establish outbound links.
    fn get_remote_endpoint(&self) -> Option<SocketAddr>;
}
