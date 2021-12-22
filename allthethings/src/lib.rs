/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

mod store;
mod replicator;
mod protocol;
mod varint;
mod memorystore;
mod iblt;

pub struct Config {
    /// Number of P2P connections desired.
    pub target_link_count: usize,

    /// Maximum allowed size of an object.
    pub max_object_size: usize,

    /// TCP port to which this should bind.
    pub tcp_port: u16,

    /// A name for this replicated data set. This is just used to prevent linking to peers replicating different data.
    pub domain: String,
}

pub(crate) fn ms_since_epoch() -> u64 {
    std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).unwrap().as_millis() as u64
}

pub(crate) fn ms_monotonic() -> u64 {
    std::time::Instant::now().elapsed().as_millis() as u64
}

/// SHA384 is the hash currently used. Others could be supported in the future.
pub const IDENTITY_HASH_SIZE: usize = 48;

pub use store::{Store, StorePutResult};
pub use replicator::Replicator;
