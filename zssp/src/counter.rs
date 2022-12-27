use std::sync::{
    atomic::{AtomicU64, Ordering},
    Mutex, RwLock,
};

use zerotier_crypto::random;

use crate::constants::{COUNTER_MAX_DELTA, COUNTER_MAX_ALLOWED_OOO};

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
pub(crate) struct CounterWindowAlt(RwLock<(u32, [u32; COUNTER_MAX_DELTA as usize])>);

impl CounterWindowAlt {
    #[inline(always)]
    pub fn new(initial: u32) -> Self {
        Self(RwLock::new((initial, std::array::from_fn(|_| initial))))
    }

    #[inline(always)]
    pub fn message_received(&self, received_counter_value: u32) -> bool {
        let idx = (received_counter_value % COUNTER_MAX_DELTA) as usize;
        let data = self.0.read().unwrap();
        let max_counter_seen = data.0;
        let lower_window = max_counter_seen.wrapping_sub(COUNTER_MAX_DELTA / 2);
        let upper_window = max_counter_seen.wrapping_add(COUNTER_MAX_DELTA / 2);
        if lower_window < upper_window {
            if (lower_window <= received_counter_value) & (received_counter_value < upper_window) {
                if data.1[idx] != received_counter_value {
                    return true;
                }
            }
        } else if (lower_window <= received_counter_value) | (received_counter_value < upper_window) {
            if data.1[idx] != received_counter_value {
                return true;
            }
        }
        return false;
    }

    #[inline(always)]
    pub fn message_authenticated(&self, received_counter_value: u32) -> bool {
        let idx = (received_counter_value % COUNTER_MAX_DELTA) as usize;
        let mut data = self.0.write().unwrap();
        let max_counter_seen = data.0;
        let lower_window = max_counter_seen.wrapping_sub(COUNTER_MAX_DELTA / 2);
        let upper_window = max_counter_seen.wrapping_add(COUNTER_MAX_DELTA / 2);
        if lower_window < upper_window {
            if (lower_window <= received_counter_value) & (received_counter_value < upper_window) {
                if data.1[idx] != received_counter_value {
                    data.1[idx] = received_counter_value;
                    data.0 = max_counter_seen.max(received_counter_value);
                    return true;
                }
            }
        } else if (lower_window <= received_counter_value) | (received_counter_value < upper_window) {
            if data.1[idx] != received_counter_value {
                data.1[idx] = received_counter_value;
                data.0 = (max_counter_seen as i32).max(received_counter_value as i32) as u32;
                return true;
            }
        }
        return false;
    }
}

pub(crate) struct CounterWindow(Mutex<Option<[u64; COUNTER_MAX_ALLOWED_OOO as usize]>>);

impl CounterWindow {
    #[inline(always)]
    pub fn new(initial: u32) -> Self {
        // we want our nonces to wrap at the exact same time that the counter wraps, so we shift them up to the u64 boundary
        let initial_nonce = (initial as u64).wrapping_shl(32);
        Self(Mutex::new(Some([initial_nonce; COUNTER_MAX_ALLOWED_OOO as usize])))
    }
    #[inline(always)]
    pub fn new_uninit() -> Self {
        Self(Mutex::new(None))
    }
    #[inline(always)]
    pub fn init(&self, initial: u32) {
        let initial_nonce = (initial as u64).wrapping_shl(6);
        let mut data = self.0.lock().unwrap();
        *data = Some([initial_nonce; COUNTER_MAX_ALLOWED_OOO as usize]);
    }


    #[inline(always)]
    pub fn message_received(&self, received_counter_value: u32, received_fragment_no: u8) -> bool {
        let fragment_nonce = (received_counter_value as u64).wrapping_shl(32) | (received_fragment_no as u64);
        //everything past this point must be atomic, i.e. these instructions must be run mutually exclusive to completion;
        //atomic instructions are only ever atomic within themselves;
        //sequentially consistent atomics do not guarantee that the thread is not preempted between individual atomic instructions
        if let Some(history) = self.0.lock().unwrap().as_mut() {
            let mut is_in = false;
            let mut idx = 0;
            let mut smallest = fragment_nonce;
            for i in 0..history.len() {
                let nonce = history[i];
                is_in |= nonce == fragment_nonce;
                let delta = (smallest as i64).wrapping_sub(nonce as i64);
                if delta > 0 {
                    smallest = nonce;
                    idx = i;
                }
            }
            if !is_in & (smallest != fragment_nonce) {
                history[idx] = fragment_nonce;
                return true
            }
            return false
        } else {
            return true
        }
    }
}
