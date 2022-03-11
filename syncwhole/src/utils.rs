/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2022 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::collections::HashMap;
use std::future::Future;
use std::sync::Arc;
use std::sync::atomic::{AtomicUsize, Ordering};
use tokio::task::JoinHandle;

/// Get the real time clock in milliseconds since Unix epoch.
pub fn ms_since_epoch() -> i64 {
    std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).unwrap().as_millis() as i64
}

/// Get the current monotonic clock in milliseconds.
pub fn ms_monotonic() -> i64 {
    std::time::Instant::now().elapsed().as_millis() as i64
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
    x = u64::from_le(x);
    x ^= x.wrapping_shl(13);
    x ^= x.wrapping_shr(7);
    x ^= x.wrapping_shl(17);
    x.to_le()
}

#[inline(always)]
pub fn splitmix64(mut x: u64) -> u64 {
    x = u64::from_le(x);
    x ^= x.wrapping_shr(30);
    x = x.wrapping_mul(0xbf58476d1ce4e5b9);
    x ^= x.wrapping_shr(27);
    x = x.wrapping_mul(0x94d049bb133111eb);
    x ^= x.wrapping_shr(31);
    x.to_le()
}

#[inline(always)]
pub fn splitmix64_inverse(mut x: u64) -> u64 {
    x = u64::from_le(x);
    x ^= x.wrapping_shr(31) ^ x.wrapping_shr(62);
    x = x.wrapping_mul(0x319642b2d24d8ec3);
    x ^= x.wrapping_shr(27) ^ x.wrapping_shr(54);
    x = x.wrapping_mul(0x96de1b173f119089);
    x ^= x.wrapping_shr(30) ^ x.wrapping_shr(60);
    x.to_le()
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
            handles: Arc::new(std::sync::Mutex::new(HashMap::new()))
        }
    }

    /// Spawn a new task.
    /// Note that currently any output is ignored. This is primarily for background tasks
    /// that are used similarly to goroutines in Go.
    pub fn spawn<F: Future + Send + 'static>(&self, future: F) {
        let id = self.ctr.fetch_add(1, Ordering::Relaxed);
        let handles = self.handles.clone();
        self.handles.lock().unwrap().insert(id, tokio::spawn(async move {
            let _ = future.await;
            let _ = handles.lock().unwrap().remove(&id);
        }));
    }
}

impl Drop for AsyncTaskReaper {
    fn drop(&mut self) {
        for (_, h) in self.handles.lock().unwrap().iter() {
            h.abort();
        }
    }
}
