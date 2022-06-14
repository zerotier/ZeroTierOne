/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2022 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::collections::HashMap;
use std::future::Future;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;
use std::time::SystemTime;

use tokio::task::JoinHandle;

/// Get the real time clock in milliseconds since Unix epoch.
pub fn ms_since_epoch() -> i64 {
    std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).unwrap().as_millis() as i64
}

/// Encode a byte slice to a hexadecimal string.
pub fn to_hex_string(b: &[u8]) -> String {
    const HEX_CHARS: [u8; 16] = [b'0', b'1', b'2', b'3', b'4', b'5', b'6', b'7', b'8', b'9', b'a', b'b', b'c', b'd', b'e', b'f'];
    let mut s = String::new();
    s.reserve(b.len() * 2);
    for c in b {
        let x = *c as usize;
        s.push(HEX_CHARS[x >> 4] as char);
        s.push(HEX_CHARS[x & 0xf] as char);
    }
    s
}

#[inline(always)]
pub fn xorshift64(mut x: u64) -> u64 {
    x ^= x.wrapping_shl(13);
    x ^= x.wrapping_shr(7);
    x ^= x.wrapping_shl(17);
    x
}

#[inline(always)]
pub fn splitmix64(mut x: u64) -> u64 {
    x ^= x.wrapping_shr(30);
    x = x.wrapping_mul(0xbf58476d1ce4e5b9);
    x ^= x.wrapping_shr(27);
    x = x.wrapping_mul(0x94d049bb133111eb);
    x ^= x.wrapping_shr(31);
    x
}

/*
#[inline(always)]
pub fn splitmix64_inverse(mut x: u64) -> u64 {
    x ^= x.wrapping_shr(31) ^ x.wrapping_shr(62);
    x = x.wrapping_mul(0x319642b2d24d8ec3);
    x ^= x.wrapping_shr(27) ^ x.wrapping_shr(54);
    x = x.wrapping_mul(0x96de1b173f119089);
    x ^= x.wrapping_shr(30) ^ x.wrapping_shr(60);
    x
}
*/

static mut RANDOM_STATE_0: u64 = 0;
static mut RANDOM_STATE_1: u64 = 0;

/// Get a non-cryptographic pseudorandom number.
pub fn random() -> u64 {
    let (mut s0, mut s1) = unsafe { (RANDOM_STATE_0, RANDOM_STATE_1) };
    if s0 == 0 {
        s0 = SystemTime::now().duration_since(SystemTime::UNIX_EPOCH).unwrap().as_nanos() as u64;
    }
    if s1 == 0 {
        s1 = splitmix64(std::process::id() as u64);
    }
    let s1_new = xorshift64(s1);
    s0 = splitmix64(s0.wrapping_add(s1));
    s1 = s1_new;
    unsafe {
        RANDOM_STATE_0 = s0;
        RANDOM_STATE_1 = s1;
    };
    s0
}

/// Wrapper for tokio::spawn() that aborts tasks not yet completed when it is dropped.
pub struct AsyncTaskReaper {
    ctr: AtomicUsize,
    handles: Arc<std::sync::Mutex<HashMap<usize, JoinHandle<()>>>>,
}

impl AsyncTaskReaper {
    pub fn new() -> Self {
        Self {
            ctr: AtomicUsize::new(0),
            handles: Arc::new(std::sync::Mutex::new(HashMap::new())),
        }
    }

    /// Spawn a new task.
    ///
    /// Note that currently any task output is ignored. This is for fire and forget
    /// background tasks that you want to be collected on loss of scope.
    pub fn spawn<F: Future + Send + 'static>(&self, future: F) {
        let id = self.ctr.fetch_add(1, Ordering::Relaxed);
        let handles = self.handles.clone();
        self.handles.lock().unwrap().insert(
            id,
            tokio::spawn(async move {
                let _ = future.await;
                let _ = handles.lock().unwrap().remove(&id);
            }),
        );
    }
}

impl Drop for AsyncTaskReaper {
    fn drop(&mut self) {
        for (_, h) in self.handles.lock().unwrap().iter() {
            h.abort();
        }
    }
}
