// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::sync::atomic::{AtomicI64, Ordering};

/// Boolean rate limiter with normal (non-atomic) semantics.
#[repr(transparent)]
pub struct IntervalGate<const FREQ: i64>(i64);

impl<const FREQ: i64> Default for IntervalGate<FREQ> {
    #[inline(always)]
    fn default() -> Self {
        Self(0)
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

/// Boolean rate limiter with atomic semantics.
#[repr(transparent)]
pub struct AtomicIntervalGate<const FREQ: i64>(AtomicI64);

impl<const FREQ: i64> Default for AtomicIntervalGate<FREQ> {
    #[inline(always)]
    fn default() -> Self {
        Self(AtomicI64::new(0))
    }
}

impl<const FREQ: i64> AtomicIntervalGate<FREQ> {
    #[inline(always)]
    pub fn new(initial_ts: i64) -> Self {
        Self(AtomicI64::new(initial_ts))
    }

    #[inline(always)]
    pub fn gate(&self, mut time: i64) -> bool {
        let prev_time = self.0.load(Ordering::Acquire);
        if (time - prev_time) < FREQ {
            false
        } else {
            self.0.store(time, Ordering::Release);
            true
        }
    }
}
