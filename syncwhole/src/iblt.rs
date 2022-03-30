/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2022 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use crate::utils::*;

/// Called to get the next iteration index for each KEY_MAPPING_ITERATIONS table lookup.
/// (See IBLT papers, etc.)
#[inline(always)]
fn next_iteration_index(mut x: u64) -> u64 {
    x = x.wrapping_add(1);
    x ^= x.wrapping_shr(30);
    x = x.wrapping_mul(0xbf58476d1ce4e5b9);
    x ^= x.wrapping_shr(27);
    x = x.wrapping_mul(0x94d049bb133111eb);
    x ^= x.wrapping_shr(31);
    x
}

/// An Invertible Bloom Lookup Table for set reconciliation with 64-bit hashes.
///
/// Usage inspired by this paper:
///
/// https://dash.harvard.edu/bitstream/handle/1/14398536/GENTILI-SENIORTHESIS-2015.pdf
#[repr(C, packed)]
pub struct IBLT<const BUCKETS: usize> {
    key: [u64; BUCKETS],
    check_hash: [u64; BUCKETS],
    count: [i8; BUCKETS],
}

impl<const BUCKETS: usize> IBLT<BUCKETS> {
    /// This was determined to be effective via empirical testing with random keys. This
    /// is a protocol constant that can't be changed without upgrading all nodes in a domain.
    const KEY_MAPPING_ITERATIONS: usize = 2;

    /// Number of buckets in this IBLT.
    pub const BUCKETS: usize = BUCKETS;

    /// Size of this IBLT in bytes.
    pub const SIZE_BYTES: usize = BUCKETS * (8 + 8 + 1);

    #[inline(always)]
    fn is_singular(&self, i: usize) -> bool {
        let c = self.count[i];
        if c == 1 || c == -1 {
            xorshift64(self.key[i]) == self.check_hash[i]
        } else {
            false
        }
    }

    /// Create a new zeroed IBLT.
    pub fn new() -> Self {
        assert_eq!(Self::SIZE_BYTES, std::mem::size_of::<Self>());
        assert!(BUCKETS < (i32::MAX as usize));
        unsafe { std::mem::zeroed() }
    }

    /// Cast a byte array to an IBLT if it is of the correct size.
    pub fn ref_from_bytes(b: &[u8]) -> Option<&Self> {
        if b.len() == Self::SIZE_BYTES {
            Some(unsafe { &*b.as_ptr().cast() })
        } else {
            None
        }
    }

    /// Compute the IBLT size in buckets to reconcile a given set difference, or return 0 if no advantage.
    /// This returns zero if an IBLT would take up as much or more space than just sending local_set_size
    /// hashes of hash_size_bytes.
    #[inline(always)]
    pub fn calc_iblt_parameters(hash_size_bytes: usize, local_set_size: u64, difference_size: u64) -> usize {
        let b = (difference_size as f64) * 1.8; // factor determined experimentally for best bytes/item, can be tuned
        if b > 64.0 && (b * (8.0 + 8.0 + 1.0)) < ((hash_size_bytes as f64) * (local_set_size as f64)) {
            b.round() as usize
        } else {
            0
        }
    }

    /// Zero this IBLT.
    #[inline(always)]
    pub fn reset(&mut self) {
        unsafe {
            std::ptr::write_bytes((self as *mut Self).cast::<u8>(), 0, std::mem::size_of::<Self>());
        }
    }

    /// Get this IBLT as a byte slice in place.
    #[inline(always)]
    pub fn as_bytes(&self) -> &[u8] {
        unsafe { &*std::ptr::slice_from_raw_parts((self as *const Self).cast::<u8>(), std::mem::size_of::<Self>()) }
    }

    fn ins_rem(&mut self, key: u64, delta: i8) {
        let check_hash = xorshift64(key);
        let mut iteration_index = u64::from_le(key);
        for _ in 0..Self::KEY_MAPPING_ITERATIONS {
            iteration_index = next_iteration_index(iteration_index);
            let i = (iteration_index as usize) % BUCKETS;
            self.key[i] ^= key;
            self.check_hash[i] ^= check_hash;
            self.count[i] = self.count[i].wrapping_add(delta);
        }
    }

    /// Insert a 64-bit key.
    #[inline(always)]
    pub fn insert(&mut self, key: u64) {
        self.ins_rem(key, 1);
    }

    /// Remove a 64-bit key.
    #[inline(always)]
    pub fn remove(&mut self, key: u64) {
        self.ins_rem(key, -1);
    }

    /// Subtract another IBLT from this one to get a set difference.
    pub fn subtract(&mut self, other: &Self) {
        for i in 0..BUCKETS {
            self.key[i] ^= other.key[i];
        }
        for i in 0..BUCKETS {
            self.check_hash[i] ^= other.check_hash[i];
        }
        for i in 0..BUCKETS {
            self.count[i] = self.count[i].wrapping_sub(other.count[i]);
        }
    }

    /// List as many entries in this IBLT as can be extracted.
    pub fn list<F: FnMut(u64)>(mut self, mut f: F) {
        let mut queue: Vec<u32> = Vec::with_capacity(BUCKETS);

        for i in 0..BUCKETS {
            if self.is_singular(i) {
                queue.push(i as u32);
            }
        }

        loop {
            let i = queue.pop();
            let i = if i.is_some() {
                i.unwrap() as usize
            } else {
                break;
            };

            if self.is_singular(i) {
                let key = self.key[i];

                f(key);

                let check_hash = xorshift64(key);
                let mut iteration_index = u64::from_le(key);
                for _ in 0..Self::KEY_MAPPING_ITERATIONS {
                    iteration_index = next_iteration_index(iteration_index);
                    let i = (iteration_index as usize) % BUCKETS;
                    self.key[i] ^= key;
                    self.check_hash[i] ^= check_hash;
                    self.count[i] = self.count[i].wrapping_sub(1);
                    if self.is_singular(i) {
                        if queue.len() > BUCKETS {
                            // sanity check, should be impossible
                            return;
                        }
                        queue.push(i as u32);
                    }
                }
            }
        }
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
            let mut test = IBLT::<CAPACITY>::new();
            expected.clear();

            for _ in 0..count {
                let x = rn;
                rn = splitmix64(rn);
                expected.insert(x);
                test.insert(x);
            }

            let mut list_count = 0;
            test.list(|x| {
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
            let mut local = IBLT::<CAPACITY>::new();
            let mut remote = IBLT::<CAPACITY>::new();

            for k in 0..REMOTE_SIZE {
                if k >= missing_count {
                    local.insert(rn);
                } else {
                    missing.insert(rn);
                }
                remote.insert(rn);
                rn = splitmix64(rn);
            }

            local.subtract(&mut remote);
            let bytes = local.as_bytes().len();
            let mut cnt = 0;
            local.list(|k| {
                assert!(missing.contains(&k));
                cnt += 1;
            });

            println!("total: {}  missing: {:5}  recovered: {:5}  size: {:5}  score: {:.4}  bytes/item: {:.2}", REMOTE_SIZE, missing.len(), cnt, bytes, (cnt as f64) / (missing.len() as f64), (bytes as f64) / (cnt as f64));

            missing_count += STEP;
        }
    }
}
