use std::sync::atomic::{AtomicU64, Ordering};

use zerotier_crypto::random;

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
