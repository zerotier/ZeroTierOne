use std::{sync::{
    atomic::{AtomicU64, Ordering, AtomicU32, AtomicI32, AtomicBool}
}, mem};

use zerotier_crypto::random;

use crate::constants::COUNTER_MAX_ALLOWED_OOO;

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
        Self(AtomicU64::new((random::next_u32_secure()/2) as u64))
    }

    /// Get the value most recently used to send a packet.
    #[inline(always)]
    pub fn previous(&self) -> CounterValue {
        CounterValue(self.0.load(Ordering::SeqCst).wrapping_sub(1))
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
pub(crate) struct CounterWindow(AtomicBool, AtomicBool, [AtomicU32; COUNTER_MAX_ALLOWED_OOO]);

impl CounterWindow {
    #[inline(always)]
    pub fn new(initial: u32) -> Self {
        Self(AtomicBool::new(true), AtomicBool::new(false), std::array::from_fn(|_| AtomicU32::new(initial)))
    }
    #[inline(always)]
    pub fn new_uninit() -> Self {
        Self(AtomicBool::new(false), AtomicBool::new(false), std::array::from_fn(|_| AtomicU32::new(0)))
    }
    #[inline(always)]
    pub fn init_authenticated(&self, received_counter_value: u32) {
        self.1.store((u32::MAX/4 < received_counter_value) & (received_counter_value <= u32::MAX/4*3), Ordering::SeqCst);
        for i in 1..COUNTER_MAX_ALLOWED_OOO {
            self.2[i].store(received_counter_value, Ordering::SeqCst);
        }
        self.0.store(true, Ordering::SeqCst);
    }

    #[inline(always)]
    pub fn message_received(&self, received_counter_value: u32) -> bool {
        if self.0.load(Ordering::SeqCst) {
            let idx = (received_counter_value % COUNTER_MAX_ALLOWED_OOO as u32) as usize;
            let pre = self.2[idx].load(Ordering::SeqCst);
            if self.1.load(Ordering::Relaxed) {
                return pre < received_counter_value;
            } else {
                return (pre as i32) < (received_counter_value as i32);
            }
        } else {
            return true;
        }
    }

    #[inline(always)]
    pub fn message_authenticated(&self, received_counter_value: u32) -> bool {
        //if a valid message is received but one of its fragments was lost, it can technically be replayed. However since the message is incomplete, we know it still exists in  the gather array, so the gather array will deduplicate the replayed message. Even if the gather array gets flushed, that flush still effectively deduplicates the replayed message.
        //eventually the counter of that kind of message will be too OOO to be accepted anymore so it can't be used to DOS.
        let idx = (received_counter_value % COUNTER_MAX_ALLOWED_OOO as u32) as usize;
        if self.1.swap((u32::MAX/4 < received_counter_value) & (received_counter_value <= u32::MAX/4*3), Ordering::SeqCst) {
            return self.2[idx].fetch_max(received_counter_value, Ordering::SeqCst) < received_counter_value;
        } else {
            let pre_as_signed: &AtomicI32 = unsafe {mem::transmute(&self.2[idx])};
            return pre_as_signed.fetch_max(received_counter_value as i32, Ordering::SeqCst) < received_counter_value as i32;
        }
    }
}
