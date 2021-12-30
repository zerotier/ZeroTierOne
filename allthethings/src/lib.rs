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
mod config;
mod link;

pub(crate) fn ms_since_epoch() -> u64 {
    std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).unwrap().as_millis() as u64
}

pub(crate) fn ms_monotonic() -> u64 {
    std::time::Instant::now().elapsed().as_millis() as u64
}

#[inline(always)]
pub(crate) async fn io_timeout<T, F: smol::future::Future<Output = smol::io::Result<T>>>(d: std::time::Duration, f: F) -> smol::io::Result<T> {
    smol::future::or(f, async {
        let _ = smol::Timer::after(d).await;
        Err(smol::io::Error::new(smol::io::ErrorKind::TimedOut, "I/O timeout"))
    }).await
}

/// SHA384 is the hash currently used. Others could be supported in the future.
/// If this size changes check iblt.rs for a few things that must be changed. This
/// is checked in "cargo test."
pub const IDENTITY_HASH_SIZE: usize = 48;

pub use config::Config;
pub use store::{Store, StorePutResult};
//pub use replicator::Replicator;
