use std::sync::atomic::{Ordering, AtomicU32};

use crate::constants::COUNTER_MAX_ALLOWED_OOO;

/// Outgoing packet counter with strictly ordered atomic semantics.
/// Count sequence always starts at 1u32, it must never be allowed to overflow
///
#[repr(transparent)]
pub(crate) struct Counter(AtomicU32);

impl Counter {
    #[inline(always)]
    pub fn new() -> Self {
        // Using a random value has no security implication. Zero would be fine. This just
        // helps randomize packet contents a bit.
        Self(AtomicU32::new(1u32))
    }

    #[inline(always)]
    pub fn reset_for_initial_offer(&self) {
        self.0.store(1u32, Ordering::SeqCst);
    }

    /// Get the value most recently used to send a packet.
    #[inline(always)]
    pub fn current(&self) -> CounterValue {
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
pub(crate) struct CounterValue(u32);

impl CounterValue {
    /// Get the 32-bit counter value used to build packets.
    #[inline(always)]
    pub fn to_u32(&self) -> u32 {
        self.0 as u32
    }
}

/// Incoming packet deduplication and replay protection window.
pub(crate) struct CounterWindow([AtomicU32; COUNTER_MAX_ALLOWED_OOO]);

impl CounterWindow {
    #[inline(always)]
    pub fn new() -> Self {
        Self(std::array::from_fn(|_| AtomicU32::new(0)))
    }
    ///this creates a counter window that rejects everything
    pub fn new_invalid() -> Self {
            Self(std::array::from_fn(|_| AtomicU32::new(u32::MAX)))
    }
    pub fn reset_for_initial_offer(&self) {
        for i in 0..COUNTER_MAX_ALLOWED_OOO {
            self.0[i].store(0, Ordering::SeqCst)
        }
    }
    pub fn invalidate(&self) {
        for i in 0..COUNTER_MAX_ALLOWED_OOO {
            self.0[i].store(u32::MAX, Ordering::SeqCst)
        }
    }

    #[inline(always)]
    pub fn message_received(&self, received_counter_value: u32) -> bool {
        let idx = (received_counter_value % COUNTER_MAX_ALLOWED_OOO as u32) as usize;
        //it is highly likely this can be a Relaxed ordering, but I want someone else to confirm that is the case
        let pre = self.0[idx].load(Ordering::SeqCst);
        return pre < received_counter_value;
    }

    #[inline(always)]
    pub fn message_authenticated(&self, received_counter_value: u32) -> bool {
        //if a valid message is received but one of its fragments was lost, it can technically be replayed. However since the message is incomplete, we know it still exists in the gather array, so the gather array will deduplicate the replayed message. Even if the gather array gets flushed, that flush still effectively deduplicates the replayed message.
        //eventually the counter of that kind of message will be too OOO to be accepted anymore so it can't be used to DOS.
        let idx = (received_counter_value % COUNTER_MAX_ALLOWED_OOO as u32) as usize;
        return self.0[idx].fetch_max(received_counter_value, Ordering::SeqCst) < received_counter_value;
    }
}
