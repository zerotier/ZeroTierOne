use std::sync::atomic::{AtomicI64, Ordering};

/// Boolean rate limiter with normal (non-atomic) semantics.
pub struct IntervalGate<const FREQ: i64>(i64);

impl<const FREQ: i64> Default for IntervalGate<FREQ> {
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
    pub fn reset(&mut self) {
        self.0 = 0;
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
pub struct AtomicIntervalGate<const FREQ: i64>(AtomicI64);

impl<const FREQ: i64> Default for AtomicIntervalGate<FREQ> {
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
    pub fn reset(&self) {
        self.0.store(0, Ordering::Relaxed);
    }

    #[inline(always)]
    pub fn gate(&self, time: i64) -> bool {
        // Note that if two or more threads are using this at once, any thread's time might
        // end up being the one stored. This is okay since these times should either be the
        // same or very close, and slight differences won't cause issues with the use cases
        // for this. This is primarily used to rate gate operations to prevent DOS attacks.
        if (time - self.0.load(Ordering::Relaxed)) >= FREQ {
            self.0.store(time, Ordering::Relaxed);
            true
        } else {
            false
        }
    }
}
