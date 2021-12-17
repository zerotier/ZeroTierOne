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

pub(crate) fn ms_since_epoch() -> u64 {
    std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).unwrap().as_millis() as u64
}

pub(crate) fn ms_monotonic() -> u64 {
    std::time::Instant::now().elapsed().as_millis() as u64
}

pub const IDENTITY_HASH_SIZE: usize = 48;

pub use store::{Store, StoreObjectResult};
pub use replicator::{Replicator, Config};
