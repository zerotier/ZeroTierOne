/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::alloc::{alloc_zeroed, dealloc, Layout};
use std::mem::size_of;
use std::ptr::slice_from_raw_parts;

use crate::IDENTITY_HASH_SIZE;

// The number of indexing sub-hashes to use, must be <= IDENTITY_HASH_SIZE / 8
const KEY_MAPPING_ITERATIONS: usize = 5;

#[cfg(not(any(target_arch = "x86", target_arch = "x86_64", target_arch = "aarch64", target_arch = "powerpc64")))]
#[inline(always)]
fn read_unaligned_u64(i: *const u64) -> u64 {
    let mut tmp = 0_u64;
    unsafe { copy_nonoverlapping(i.cast::<u8>(), (&mut tmp as *mut u64).cast(), 8) };
    tmp
}

#[cfg(any(target_arch = "x86", target_arch = "x86_64", target_arch = "aarch64", target_arch = "powerpc64"))]
#[inline(always)]
fn read_unaligned_u64(i: *const u64) -> u64 { unsafe { *i } }

#[inline(always)]
fn xorshift64(mut x: u64) -> u64 {
    x ^= x.wrapping_shl(13);
    x ^= x.wrapping_shr(7);
    x ^= x.wrapping_shl(17);
    x
}

#[repr(C, packed)]
struct IBLTEntry {
    key_sum: [u64; IDENTITY_HASH_SIZE / 8],
    check_hash_sum: u64,
    count: i64,
}

/// An IBLT (invertible bloom lookup table) specialized for reconciling sets of identity hashes.
///
/// This makes some careful use of unsafe as it's heavily optimized. It's a CPU bottleneck when
/// replicating large dynamic data sets.
pub struct IBLT {
    map: *mut IBLTEntry,
    buckets: usize
}

impl Drop for IBLT {
    fn drop(&mut self) {
        unsafe { dealloc(self.map.cast(), Layout::from_size_align(size_of::<IBLTEntry>() * self.buckets, 8).unwrap()) };
    }
}

impl IBLTEntry {
    #[inline(always)]
    fn is_singular(&self) -> bool {
        if self.count == 1 || self.count == -1 {
            u64::from_le(self.key_sum[0]).wrapping_add(xorshift64(u64::from_le(self.key_sum[1]))) == u64::from_le(self.check_hash_sum)
        } else {
            false
        }
    }
}

impl IBLT {
    /// Construct a new IBLT with a given capacity.
    pub fn new(buckets: usize) -> Self {
        assert!(buckets > 0 && buckets <= u32::MAX as usize);
        Self {
            map: unsafe { alloc_zeroed(Layout::from_size_align(size_of::<IBLTEntry>() * buckets, 8).unwrap()).cast() },
            buckets,
        }
    }

    #[inline(always)]
    pub fn buckets(&self) -> usize { self.buckets }

    #[inline(always)]
    pub fn as_bytes(&self) -> &[u8] { unsafe { &*slice_from_raw_parts(self.map.cast::<u8>(), size_of::<IBLTEntry>() * self.buckets) } }

    fn ins_rem(&mut self, key: &[u8; IDENTITY_HASH_SIZE], delta: i64) {
        let key = key.as_ptr().cast::<u64>();
        let (k0, k1, k2, k3, k4, k5) = (read_unaligned_u64(key.wrapping_add(0)), read_unaligned_u64(key.wrapping_add(1)), read_unaligned_u64(key.wrapping_add(2)), read_unaligned_u64(key.wrapping_add(3)), read_unaligned_u64(key.wrapping_add(4)), read_unaligned_u64(key.wrapping_add(5)));
        let check_hash = u64::from_le(k0).wrapping_add(xorshift64(u64::from_le(k1))).to_le();
        for mapping_sub_hash in 0..KEY_MAPPING_ITERATIONS {
            let b = unsafe { &mut *self.map.wrapping_add((u64::from_le(read_unaligned_u64(key.wrapping_add(mapping_sub_hash))) as usize) % self.buckets) };
            b.key_sum[0] ^= k0;
            b.key_sum[1] ^= k1;
            b.key_sum[2] ^= k2;
            b.key_sum[3] ^= k3;
            b.key_sum[4] ^= k4;
            b.key_sum[5] ^= k5;
            b.check_hash_sum ^= check_hash;
            b.count = i64::from_le(b.count).wrapping_add(delta).to_le();
        }
    }

    #[inline(always)]
    pub fn insert(&mut self, key: &[u8; IDENTITY_HASH_SIZE]) { self.ins_rem(key, 1); }

    #[inline(always)]
    pub fn remove(&mut self, key: &[u8; IDENTITY_HASH_SIZE]) { self.ins_rem(key, -1); }

    /// Subtract another IBLT from this one to compute set difference.
    /// The other may be in the form of a raw byte array or an IBLT, which implements
    /// AsRef<[u8]>. It must have the same number of buckets.
    pub fn subtract<O: AsRef<[u8]>>(&mut self, other: &O) {
        let other_slice = other.as_ref();
        if (other_slice.len() / size_of::<IBLTEntry>()) == self.buckets {
            let other_map: *const IBLTEntry = other_slice.as_ptr().cast();
            for i in 0..self.buckets {
                let self_b = unsafe { &mut *self.map.wrapping_add(i) };
                let other_b = unsafe { &*other_map.wrapping_add(i) };
                self_b.key_sum[0] ^= other_b.key_sum[0];
                self_b.key_sum[1] ^= other_b.key_sum[1];
                self_b.key_sum[2] ^= other_b.key_sum[2];
                self_b.key_sum[3] ^= other_b.key_sum[3];
                self_b.key_sum[4] ^= other_b.key_sum[4];
                self_b.key_sum[5] ^= other_b.key_sum[5];
                self_b.check_hash_sum ^= other_b.check_hash_sum;
                self_b.count = i64::from_le(self_b.count).wrapping_sub(i64::from_le(other_b.count)).to_le();
            }
        }
    }

    /// Extract every enumerable value from this IBLT.
    ///
    /// This consumes the IBLT instance since listing requires destructive modification
    /// of the digest data.
    pub fn list<F: FnMut(&[u8; IDENTITY_HASH_SIZE]) -> bool>(self, mut f: F) {
        let buckets = self.buckets;
        let mut singular_buckets: Vec<u32> = Vec::with_capacity(buckets + 2);

        for i in 0..buckets {
            unsafe {
                if (&*self.map.wrapping_add(i)).is_singular() {
                    singular_buckets.push(i as u32);
                }
            }
        }

        let mut key = [0_u64; IDENTITY_HASH_SIZE / 8];
        let mut bucket_ptr = 0;
        while bucket_ptr < singular_buckets.len() {
            let b = unsafe { &*self.map.wrapping_add(*singular_buckets.get_unchecked(bucket_ptr) as usize) };
            bucket_ptr += 1;
            if b.is_singular() {
                key[0] = b.key_sum[0];
                key[1] = b.key_sum[1];
                key[2] = b.key_sum[2];
                key[3] = b.key_sum[3];
                key[4] = b.key_sum[4];
                key[5] = b.key_sum[5];
                if f(unsafe { &*key.as_ptr().cast::<[u8; IDENTITY_HASH_SIZE]>() }) {
                    let check_hash = u64::from_le(key[0]).wrapping_add(xorshift64(u64::from_le(key[1]))).to_le();
                    for mapping_sub_hash in 0..KEY_MAPPING_ITERATIONS {
                        let bi = (u64::from_le(unsafe { *key.get_unchecked(mapping_sub_hash) }) as usize) % buckets;
                        let b = unsafe { &mut *self.map.wrapping_add(bi) };
                        b.key_sum[0] ^= key[0];
                        b.key_sum[1] ^= key[1];
                        b.key_sum[2] ^= key[2];
                        b.key_sum[3] ^= key[3];
                        b.key_sum[4] ^= key[4];
                        b.key_sum[5] ^= key[5];
                        b.check_hash_sum ^= check_hash;
                        b.count = i64::from_le(b.count).wrapping_sub(1).to_le();
                        if b.is_singular() {
                            singular_buckets.push(bi as u32);
                        }
                    }
                } else {
                    break;
                }
            }
        }
    }
}

impl AsRef<[u8]> for IBLT {
    /// Get this IBLT in raw byte array form.
    #[inline(always)]
    fn as_ref(&self) -> &[u8] { self.as_bytes() }
}

#[cfg(test)]
mod tests {
    use zerotier_core_crypto::hash::SHA384;
    use crate::iblt::*;

    #[test]
    fn compiler_behavior() {
        // A number of things above like unrolled key XORing must be changed if this size is changed.
        assert_eq!(IDENTITY_HASH_SIZE, 48);
        assert!(KEY_MAPPING_ITERATIONS <= (IDENTITY_HASH_SIZE / 8) && (IDENTITY_HASH_SIZE % 8) == 0);

        // Make sure this packed struct is actually packed.
        assert_eq!(size_of::<IBLTEntry>(), IDENTITY_HASH_SIZE + 8 + 8);
    }

    #[allow(unused_variables)]
    #[test]
    fn insert_and_list() {
        for _ in 0..10 {
            for expected_cnt in 0..768 {
                let random_u64 = zerotier_core_crypto::random::xorshift64_random();
                let mut t = IBLT::new(2048);
                for i in 0..expected_cnt {
                    let k = SHA384::hash(&((i + random_u64) as u64).to_le_bytes());
                    t.insert(&k);
                }
                let mut cnt = 0;
                t.list(|k| {
                    cnt += 1;
                    true
                });
                assert_eq!(cnt, expected_cnt);
            }
        }
    }

    #[allow(unused_variables)]
    #[test]
    fn set_reconciliation() {
        for _ in 0..10 {
            let random_u64 = zerotier_core_crypto::random::xorshift64_random();
            let mut alice = IBLT::new(2048);
            let mut bob = IBLT::new(2048);
            let mut alice_total = 0_i32;
            let mut bob_total = 0_i32;
            for i in 0..1500 {
                let k = SHA384::hash(&((i ^ random_u64) as u64).to_le_bytes());
                if (k[0] & 1) == 1 {
                    alice.insert(&k);
                    alice_total += 1;
                }
                if (k[0] & 3) == 2 {
                    bob.insert(&k);
                    bob_total += 1;
                }
            }
            alice.subtract(&bob);
            let mut diff_total = 0_i32;
            alice.list(|k| {
                diff_total += 1;
                true
            });
            // This is a probabilistic process so we tolerate a little bit of failure. The idea is that each
            // pass reconciles more and more differences.
            assert!(((alice_total + bob_total) - diff_total).abs() <= 128);
        }
    }
}
