/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use smol::net::SocketAddr;

use crate::IDENTITY_HASH_SIZE;

pub const MIN_IDENTITY_HASH: [u8; 48] = [0_u8; 48];
pub const MAX_IDENTITY_HASH: [u8; 48] = [0xff_u8; 48];

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
    /// Get an object from the database, storing it in the supplied buffer.
    /// A return of 'false' leaves the buffer state undefined. If the return is true any previous
    /// data in the supplied buffer will have been cleared and replaced with the retrieved object.
    fn get(&self, reference_time: u64, identity_hash: &[u8; IDENTITY_HASH_SIZE], buffer: &mut Vec<u8>) -> bool;

    /// Store an entry in the database.
    fn put(&self, reference_time: u64, identity_hash: &[u8; IDENTITY_HASH_SIZE], object: &[u8]) -> StorePutResult;

    /// Check if we have an object by its identity hash.
    fn have(&self, reference_time: u64, identity_hash: &[u8; IDENTITY_HASH_SIZE]) -> bool;

    /// Get the total count of objects.
    fn total_count(&self, reference_time: u64) -> Option<u64>;

    /// Get the time the last object was received in milliseconds since epoch.
    fn last_object_receive_time(&self) -> Option<u64>;

    /// Count the number of identity hash keys in this range (inclusive) of identity hashes.
    /// This may return None if an error occurs, but should return 0 if the set is empty.
    fn count(&self, reference_time: u64, start: &[u8; IDENTITY_HASH_SIZE], end: &[u8; IDENTITY_HASH_SIZE]) -> Option<u64>;

    /// Called when a connection to a remote node was successful.
    /// This is always called on successful outbound connect.
    fn save_remote_endpoint(&self, to_address: &SocketAddr);

    /// Get a remote endpoint to try.
    /// This can return endpoints in any order and is used to try to establish outbound links.
    fn get_remote_endpoint(&self) -> Option<SocketAddr>;
}
