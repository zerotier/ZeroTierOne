/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2022 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

pub(crate) mod varint;
pub(crate) mod protocol;
pub(crate) mod iblt;

pub mod datastore;
pub mod node;
pub mod host;

pub(crate) fn ms_since_epoch() -> i64 {
    std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).unwrap().as_millis() as i64
}

pub(crate) fn ms_monotonic() -> i64 {
    std::time::Instant::now().elapsed().as_millis() as i64
}
