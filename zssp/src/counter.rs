use std::sync::atomic::{AtomicU32, AtomicU64, Ordering};

use zerotier_crypto::random;

use crate::constants::COUNTER_MAX_DELTA;

/// Outgoing packet counter with strictly ordered atomic semantics.
///
/// The counter used in packets is actually 32 bits, but using a 64-bit integer internally
/// lets us more safely implement key lifetime limits without confusing logic to handle 32-bit
/// wrap-around.
#[repr(transparent)]
pub(crate) struct Counter(AtomicU64);

impl Counter {
    #[inline(always)]
    pub fn new() -> Self {
        // Using a random value has no security implication. Zero would be fine. This just
        // helps randomize packet contents a bit.
        Self(AtomicU64::new(random::next_u32_secure() as u64))
    }

    /// Get the value most recently used to send a packet.
    #[inline(always)]
    pub fn previous(&self) -> CounterValue {
        CounterValue(self.0.load(Ordering::SeqCst))
    }

    /// Get a counter value for the next packet being sent.
    #[inline(always)]
    pub fn next(&self) -> CounterValue {
        CounterValue(self.0.fetch_add(1, Ordering::SeqCst))
    }
}

/// A value of the outgoing packet counter.
#[repr(transparent)]
#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub(crate) struct CounterValue(u64);

impl CounterValue {
    /// Get the 32-bit counter value used to build packets.
    #[inline(always)]
    pub fn to_u32(&self) -> u32 {
        self.0 as u32
    }

    /// Get the counter value after N more uses of the parent counter.
    ///
    /// This checks for u64 overflow for the sake of correctness. Be careful if using ZSSP in a
    /// generational starship where sessions may last for millions of years.
    #[inline(always)]
    pub fn counter_value_after_uses(&self, uses: u64) -> Self {
        Self(self.0.checked_add(uses).unwrap())
    }
}

/// Incoming packet deduplication and replay protection window.
pub(crate) struct CounterWindow(AtomicU32, [AtomicU32; COUNTER_MAX_DELTA as usize]);

impl CounterWindow {
    #[inline(always)]
    pub fn new(initial: u32) -> Self {
        Self(AtomicU32::new(initial), std::array::from_fn(|_| AtomicU32::new(initial)))
    }

    #[inline(always)]
    pub fn message_received(&self, received_counter_value: u32) -> bool {
        let prev_max = self.0.fetch_max(received_counter_value, Ordering::AcqRel);
        if received_counter_value >= prev_max || prev_max.wrapping_sub(received_counter_value) <= COUNTER_MAX_DELTA {
            // First, the most common case: counter is higher than the previous maximum OR is no older than MAX_DELTA.
            // In that case we accept the packet if it is not a duplicate. Duplicate check is this swap/compare.
            self.1[(received_counter_value % COUNTER_MAX_DELTA) as usize].swap(received_counter_value, Ordering::AcqRel)
                != received_counter_value
        } else if received_counter_value.wrapping_sub(prev_max) <= COUNTER_MAX_DELTA {
            // If the received value is lower and wraps when the previous max is subtracted, this means the
            // unsigned integer counter has wrapped. In that case we write the new lower-but-actually-higher "max"
            // value and then check the deduplication window.
            self.0.store(received_counter_value, Ordering::Release);
            self.1[(received_counter_value % COUNTER_MAX_DELTA) as usize].swap(received_counter_value, Ordering::AcqRel)
                != received_counter_value
        } else {
            // If the received value is more than MAX_DELTA in the past and wrapping has NOT occurred, this packet
            // is too old and is rejected.
            false
        }
    }
}
