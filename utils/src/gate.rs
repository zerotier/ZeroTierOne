/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

/// Boolean rate limiter with normal (non-atomic) semantics.
#[repr(transparent)]
pub struct IntervalGate<const FREQ: i64>(i64);

impl<const FREQ: i64> Default for IntervalGate<FREQ> {
    #[inline(always)]
    fn default() -> Self {
        Self(crate::NEVER_HAPPENED_TICKS)
    }
}

impl<const FREQ: i64> IntervalGate<FREQ> {
    #[inline(always)]
    pub fn new(initial_ts: i64) -> Self {
        Self(initial_ts)
    }

    #[inline(always)]
    pub fn gate(&mut self, time: i64) -> bool {
        if (time - self.0) >= FREQ {
            self.0 = time;
            true
        } else {
            false
        }
    }
}
