/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use crate::proto::{AES_GCM_TAG_SIZE, HEADER_SIZE};

/// Minimum size of a valid physical ZSSP packet of any type. Anything smaller is discarded.
pub const MIN_PACKET_SIZE: usize = HEADER_SIZE + AES_GCM_TAG_SIZE;

/// Minimum physical MTU for ZSSP to function.
pub const MIN_TRANSPORT_MTU: usize = 128;

/// Maximum size of init meta-data objects.
pub const MAX_METADATA_SIZE: usize = 256;

/// Start attempting to rekey after a key has been used to send packets this many times.
/// This is 1/4 the recommended NIST limit for AES-GCM key lifetimes under most conditions.
pub(crate) const REKEY_AFTER_USES: u64 = 536870912;

/// Hard expiration after this many uses.
///
/// Use of the key beyond this point is prohibited. If we reach this number of key uses
/// the key will be destroyed in memory and the session will cease to function. A hard
/// error is also generated.
pub(crate) const EXPIRE_AFTER_USES: u64 = REKEY_AFTER_USES * 2;

/// Start attempting to rekey after a key has been in use for this many milliseconds.
pub(crate) const REKEY_AFTER_TIME_MS: i64 = 1000 * 60 * 60; // 1 hour

/// Maximum random jitter to add to rekey-after time.
pub(crate) const REKEY_AFTER_TIME_MS_MAX_JITTER: u32 = 1000 * 60 * 10; // 10 minutes

/// Timeout for incoming sessions in incomplete state in milliseconds.
pub(crate) const INCOMPLETE_SESSION_TIMEOUT: i64 = 1000;
