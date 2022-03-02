/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2022 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::alloc::{alloc_zeroed, dealloc, Layout};
use std::mem::size_of;
use std::ptr::write_bytes;
use tokio::io::{AsyncReadExt, AsyncWriteExt};

use crate::varint;

#[inline(always)]
fn xorshift64(mut x: u64) -> u64 {
    x = u64::from_le(x);
    x ^= x.wrapping_shl(13);
    x ^= x.wrapping_shr(7);
    x ^= x.wrapping_shl(17);
    x.to_le()
}

#[inline(always)]
fn splitmix64(mut x: u64) -> u64 {
    x = u64::from_le(x);
    x ^= x.wrapping_shr(30);
    x = x.wrapping_mul(0xbf58476d1ce4e5b9);
    x ^= x.wrapping_shr(27);
    x = x.wrapping_mul(0x94d049bb133111eb);
    x ^= x.wrapping_shr(31);
    x.to_le()
}

#[inline(always)]
fn splitmix64_inverse(mut x: u64) -> u64 {
    x = u64::from_le(x);
    x ^= x.wrapping_shr(31) ^ x.wrapping_shr(62);
    x = x.wrapping_mul(0x319642b2d24d8ec3);
    x ^= x.wrapping_shr(27) ^ x.wrapping_shr(54);
    x = x.wrapping_mul(0x96de1b173f119089);
    x ^= x.wrapping_shr(30) ^ x.wrapping_shr(60);
    x.to_le()
}

#[inline(always)]
fn next_iteration_index(prev_iteration_index: u64) -> u64 {
    splitmix64(prev_iteration_index.wrapping_add(1))
}

#[derive(Clone, PartialEq, Eq)]
struct IBLTEntry {
    key_sum: u64,
    check_hash_sum: u64,
    count: i64
}

impl IBLTEntry {
    #[inline(always)]
    fn is_singular(&self) -> bool {
        if self.count == 1 || self.count == -1 {
            xorshift64(self.key_sum) == self.check_hash_sum
        } else {
            false
        }
    }
}

/// An Invertible Bloom Lookup Table for set reconciliation with 64-bit hashes.
#[derive(Clone, PartialEq, Eq)]
pub struct IBLT<const B: usize> {
    map: *mut [IBLTEntry; B]
}

impl<const B: usize> IBLT<B> {
    /// Number of buckets (capacity) of this IBLT.
    pub const BUCKETS: usize = B;

    /// This was determined to be effective via empirical testing with random keys. This
    /// is a protocol constant that can't be changed without upgrading all nodes in a domain.
    const KEY_MAPPING_ITERATIONS: usize = 2;

    pub fn new() -> Self {
        assert!(B < u32::MAX as usize); // sanity check
        Self {
            map: unsafe { alloc_zeroed(Layout::new::<[IBLTEntry; B]>()).cast() }
        }
    }

    pub fn reset(&mut self) {
        unsafe { write_bytes(self.map.cast::<u8>(), 0, size_of::<[IBLTEntry; B]>()) };
    }

    pub async fn read<R: AsyncReadExt + Unpin>(&mut self, r: &mut R) -> std::io::Result<()> {
        let mut prev_c = 0_i64;
        for b in unsafe { (*self.map).iter_mut() } {
            let _ = r.read_exact(unsafe { &mut *(&mut b.key_sum as *mut u64).cast::<[u8; 8]>() }).await?;
            let _ = r.read_exact(unsafe { &mut *(&mut b.check_hash_sum as *mut u64).cast::<[u8; 8]>() }).await?;
            let mut c = varint::read_async(r).await? as i64;
            if (c & 1) == 0 {
                c = c.wrapping_shr(1);
            } else {
                c = -c.wrapping_shr(1);
            }
            b.count = c + prev_c;
            prev_c = b.count;
        }
        Ok(())
    }

    pub async fn write<W: AsyncWriteExt + Unpin>(&self, w: &mut W) -> std::io::Result<()> {
        let mut prev_c = 0_i64;
        for b in unsafe { (*self.map).iter() } {
            let _ = w.write_all(unsafe { &*(&b.key_sum as *const u64).cast::<[u8; 8]>() }).await?;
            let _ = w.write_all(unsafe { &*(&b.check_hash_sum as *const u64).cast::<[u8; 8]>() }).await?;
            let mut c = (b.count - prev_c).wrapping_shl(1);
            prev_c = b.count;
            if c < 0 {
                c = -c | 1;
            }
            let _ = varint::write_async(w, c as u64).await?;
        }
        Ok(())
    }

    /// Get this IBLT as a byte array.
    pub fn to_bytes(&self) -> Vec<u8> {
        let mut out = std::io::Cursor::new(Vec::<u8>::with_capacity(B * 20));
        let current = tokio::runtime::Handle::try_current();
        if current.is_ok() {
            assert!(current.unwrap().block_on(self.write(&mut out)).is_ok());
        } else {
            assert!(tokio::runtime::Builder::new_current_thread().build().unwrap().block_on(self.write(&mut out)).is_ok());
        }
        out.into_inner()
    }

    fn ins_rem(&mut self, mut key: u64, delta: i64) {
        key = splitmix64(key);
        let check_hash = xorshift64(key);
        let mut iteration_index = u64::from_le(key);
        for _ in 0..Self::KEY_MAPPING_ITERATIONS {
            iteration_index = next_iteration_index(iteration_index);
            let b = unsafe { (*self.map).get_unchecked_mut((iteration_index as usize) % B) };
            b.key_sum ^= key;
            b.check_hash_sum ^= check_hash;
            b.count += delta;
        }
    }

    /// Insert a 64-bit key.
    /// Panics if the key is shorter than 64 bits. If longer, bits beyond 64 are ignored.
    #[inline(always)]
    pub fn insert(&mut self, key: &[u8]) {
        assert!(key.len() >= 8);
        self.ins_rem(unsafe { u64::from_ne_bytes(*(key.as_ptr().cast::<[u8; 8]>())) }, 1);
    }

    /// Remove a 64-bit key.
    /// Panics if the key is shorter than 64 bits. If longer, bits beyond 64 are ignored.
    #[inline(always)]
    pub fn remove(&mut self, key: &[u8]) {
        assert!(key.len() >= 8);
        self.ins_rem(unsafe { u64::from_ne_bytes(*(key.as_ptr().cast::<[u8; 8]>())) }, -1);
    }

    /// Subtract another IBLT from this one to get a set difference.
    pub fn subtract(&mut self, other: &Self) {
        for (s, o) in unsafe { (*self.map).iter_mut().zip((*other.map).iter()) } {
            s.key_sum ^= o.key_sum;
            s.check_hash_sum ^= o.check_hash_sum;
            s.count -= o.count;
        }
    }

    pub fn list<F: FnMut(&[u8; 8])>(mut self, mut f: F) -> bool {
        let mut queue: Vec<u32> = Vec::with_capacity(B);

        for b in 0..B {
            if unsafe { (*self.map).get_unchecked(b).is_singular() } {
                queue.push(b as u32);
            }
        }

        loop {
            let b = queue.pop();
            let b = if b.is_some() {
                unsafe { (*self.map).get_unchecked_mut(b.unwrap() as usize) }
            } else {
                break;
            };
            if b.is_singular() {
                let key = b.key_sum;
                let check_hash = xorshift64(key);
                let mut iteration_index = u64::from_le(key);

                f(&(splitmix64_inverse(key)).to_ne_bytes());

                for _ in 0..Self::KEY_MAPPING_ITERATIONS {
                    iteration_index = next_iteration_index(iteration_index);
                    let b_idx = iteration_index % (B as u64);
                    let b = unsafe { (*self.map).get_unchecked_mut(b_idx as usize) };
                    b.key_sum ^= key;
                    b.check_hash_sum ^= check_hash;
                    b.count -= 1;

                    if b.is_singular() {
                        if queue.len() >= (B * 2) { // sanity check for invalid IBLT
                            return false;
                        }
                        queue.push(b_idx as u32);
                    }
                }
            }
        }

        return true;
    }
}

impl<const B: usize> Drop for IBLT<B> {
    fn drop(&mut self) {
        unsafe { dealloc(self.map.cast(), Layout::new::<[IBLTEntry; B]>()) };
    }
}

#[cfg(test)]
mod tests {
    use std::collections::HashSet;
    use std::time::SystemTime;
    use crate::iblt::*;

    #[test]
    fn splitmix_is_invertiblex() {
        for i in 1..2000_u64 {
            assert_eq!(i, splitmix64_inverse(splitmix64(i)))
        }
    }

    #[test]
    fn fill_list_performance() {
        let mut rn = xorshift64(SystemTime::now().duration_since(SystemTime::UNIX_EPOCH).unwrap().as_nanos() as u64);
        let mut expected: HashSet<u64> = HashSet::with_capacity(4096);
        let mut count = 64;
        const CAPACITY: usize = 4096;
        while count <= CAPACITY {
            let mut test: IBLT<CAPACITY> = IBLT::new();
            expected.clear();

            for _ in 0..count {
                let x = rn;
                rn = splitmix64(rn);
                expected.insert(x);
                test.insert(&x.to_ne_bytes());
            }

            let mut list_count = 0;
            test.list(|x| {
                let x = u64::from_ne_bytes(*x);
                list_count += 1;
                assert!(expected.contains(&x));
            });

            //println!("inserted: {}\tlisted: {}\tcapacity: {}\tscore: {:.4}\tfill: {:.4}", count, list_count, CAPACITY, (list_count as f64) / (count as f64), (count as f64) / (CAPACITY as f64));
            count += 64;
        }
    }

    #[test]
    fn merge_sets() {
        let mut rn = xorshift64(SystemTime::now().duration_since(SystemTime::UNIX_EPOCH).unwrap().as_nanos() as u64);
        const CAPACITY: usize = 16384;
        const REMOTE_SIZE: usize = 1024 * 1024;
        const STEP: usize = 1024;
        let mut missing_count = 1024;
        let mut missing: HashSet<u64> = HashSet::with_capacity(CAPACITY);
        while missing_count <= CAPACITY {
            missing.clear();
            let mut local: IBLT<CAPACITY> = IBLT::new();
            let mut remote: IBLT<CAPACITY> = IBLT::new();

            for k in 0..REMOTE_SIZE {
                if k >= missing_count {
                    local.insert(&rn.to_ne_bytes());
                } else {
                    missing.insert(rn);
                }
                remote.insert(&rn.to_ne_bytes());
                rn = splitmix64(rn);
            }

            local.subtract(&mut remote);
            let bytes = local.to_bytes().len();
            let mut cnt = 0;
            local.list(|k| {
                let k = u64::from_ne_bytes(*k);
                assert!(missing.contains(&k));
                cnt += 1;
            });

            println!("total: {}  missing: {:5}  recovered: {:5}  size: {:5}  score: {:.4}  bytes/item: {:.2}", REMOTE_SIZE, missing.len(), cnt, bytes, (cnt as f64) / (missing.len() as f64), (bytes as f64) / (cnt as f64));

            missing_count += STEP;
        }
    }
}
