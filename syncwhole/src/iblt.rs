/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2022 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::mem::{size_of, zeroed};
use std::ptr::write_bytes;
use tokio::io::{AsyncReadExt, AsyncWriteExt};

use crate::varint;

// max value: 6, 5 was determined to be good via empirical testing
const KEY_MAPPING_ITERATIONS: usize = 5;

#[inline(always)]
fn xorshift64(mut x: u64) -> u64 {
    x ^= x.wrapping_shl(13);
    x ^= x.wrapping_shr(7);
    x ^= x.wrapping_shl(17);
    x
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

// https://nullprogram.com/blog/2018/07/31/
#[inline(always)]
fn triple32(mut x: u32) -> u32 {
    x ^= x.wrapping_shr(17);
    x = x.wrapping_mul(0xed5ad4bb);
    x ^= x.wrapping_shr(11);
    x = x.wrapping_mul(0xac4c1b51);
    x ^= x.wrapping_shr(15);
    x = x.wrapping_mul(0x31848bab);
    x ^= x.wrapping_shr(14);
    x
}

#[inline(always)]
fn next_iteration_index(prev_iteration_index: u64, salt: u64) -> u64 {
    prev_iteration_index.wrapping_add(triple32(prev_iteration_index.wrapping_shr(32) as u32) as u64).wrapping_add(salt)
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
    salt: u64,
    map: [IBLTEntry; B]
}

impl<const B: usize> IBLT<B> {
    pub const BUCKETS: usize = B;

    pub fn new(salt: u64) -> Self {
        Self {
            salt,
            map: unsafe { zeroed() }
        }
    }

    pub fn reset(&mut self, salt: u64) {
        self.salt = salt;
        unsafe { write_bytes((&mut self.map as *mut IBLTEntry).cast::<u8>(), 0, size_of::<[IBLTEntry; B]>()) };
    }

    pub async fn read<R: AsyncReadExt + Unpin>(&mut self, r: &mut R) -> std::io::Result<()> {
        r.read_exact(unsafe { &mut *(&mut self.salt as *mut u64).cast::<[u8; 8]>() }).await?;
        let mut prev_c = 0_i64;
        for b in self.map.iter_mut() {
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
        let _ = w.write_all(unsafe { &*(&self.salt as *const u64).cast::<[u8; 8]>() }).await?;
        let mut prev_c = 0_i64;
        for b in self.map.iter() {
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

    fn ins_rem(&mut self, mut key: u64, delta: i64) {
        key = splitmix64(key ^ self.salt);
        let check_hash = xorshift64(key);
        let mut iteration_index = u64::from_le(key);
        for _ in 0..KEY_MAPPING_ITERATIONS {
            iteration_index = next_iteration_index(iteration_index, self.salt);
            let b = unsafe { self.map.get_unchecked_mut((iteration_index as usize) % B) };
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

    pub fn subtract(&mut self, other: &Self) {
        for b in 0..B {
            let s = &mut self.map[b];
            let o = &other.map[b];
            s.key_sum ^= o.key_sum;
            s.check_hash_sum ^= o.check_hash_sum;
            s.count += o.count;
        }
    }

    pub fn list<F: FnMut(&[u8; 8])>(mut self, mut f: F) -> bool {
        let mut singular_buckets = [0_usize; B];
        let mut singular_bucket_count = 0_usize;

        for b in 0..B {
            if self.map[b].is_singular() {
                singular_buckets[singular_bucket_count] = b;
                singular_bucket_count += 1;
            }
        }

        while singular_bucket_count > 0 {
            singular_bucket_count -= 1;
            let b = &self.map[singular_buckets[singular_bucket_count]];

            if b.is_singular() {
                let key = b.key_sum;
                let check_hash = xorshift64(key);
                let mut iteration_index = u64::from_le(key);

                f(&(splitmix64_inverse(key) ^ self.salt).to_ne_bytes());

                for _ in 0..KEY_MAPPING_ITERATIONS {
                    iteration_index = next_iteration_index(iteration_index, self.salt);
                    let b_idx = (iteration_index as usize) % B;
                    let b = unsafe { self.map.get_unchecked_mut(b_idx) };
                    b.key_sum ^= key;
                    b.check_hash_sum ^= check_hash;
                    b.count -= 1;

                    if b.is_singular() {
                        if singular_bucket_count >= B {
                            // This would indicate an invalid IBLT.
                            return false;
                        }
                        singular_buckets[singular_bucket_count] = b_idx;
                        singular_bucket_count += 1;
                    }
                }
            }
        }

        return true;
    }
}

#[cfg(test)]
mod tests {
    use std::collections::HashSet;
    use std::time::SystemTime;
    use crate::iblt::*;

    #[test]
    fn splitmix_is_invertiblex() {
        for i in 1..1024_u64 {
            assert_eq!(i, splitmix64_inverse(splitmix64(i)))
        }
    }

    #[test]
    fn insert_and_list() {
        let mut rn = xorshift64(SystemTime::now().duration_since(SystemTime::UNIX_EPOCH).unwrap().as_nanos() as u64);
        for _ in 0..256 {
            let mut alice: IBLT<1024> = IBLT::new(rn);
            rn = xorshift64(rn);
            let mut expected: HashSet<u64> = HashSet::with_capacity(1024);
            let count = 600;
            for _ in 0..count {
                let x = rn;
                rn = xorshift64(rn);
                expected.insert(x);
                alice.insert(&x.to_ne_bytes());
            }
            let mut cnt = 0;
            alice.list(|x| {
                let x = u64::from_ne_bytes(*x);
                cnt += 1;
                assert!(expected.contains(&x));
            });
            assert_eq!(cnt, count);
        }
    }
}
