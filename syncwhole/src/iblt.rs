/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2022 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::mem::size_of;
use std::ptr::{slice_from_raw_parts, slice_from_raw_parts_mut, write_bytes};

use tokio::io::{AsyncReadExt, AsyncWriteExt};

use crate::utils::*;

#[inline(always)]
fn next_iteration_index(prev_iteration_index: u64) -> u64 {
    splitmix64(prev_iteration_index.wrapping_add(1))
}

#[derive(Clone, PartialEq, Eq)]
#[repr(C, packed)]
struct IBLTEntry {
    key_sum: u64,
    check_hash_sum: u64,
    count: i32
}

impl IBLTEntry {
    #[inline(always)]
    fn is_singular(&self) -> bool {
        if i32::from_le(self.count) == 1 || i32::from_le(self.count) == -1 {
            xorshift64(self.key_sum) == self.check_hash_sum
        } else {
            false
        }
    }
}

/// An Invertible Bloom Lookup Table for set reconciliation with 64-bit hashes.
///
/// Usage inspired by this paper:
///
/// https://dash.harvard.edu/bitstream/handle/1/14398536/GENTILI-SENIORTHESIS-2015.pdf
#[derive(Clone, PartialEq, Eq)]
pub struct IBLT {
    map: Vec<IBLTEntry>
}

impl IBLT {
    /// This was determined to be effective via empirical testing with random keys. This
    /// is a protocol constant that can't be changed without upgrading all nodes in a domain.
    const KEY_MAPPING_ITERATIONS: usize = 2;

    pub fn new(buckets: usize) -> Self {
        assert!(buckets < i32::MAX as usize);
        assert_eq!(size_of::<IBLTEntry>(), 20);
        let mut iblt = Self { map: Vec::with_capacity(buckets) };
        unsafe {
            iblt.map.set_len(buckets);
            iblt.reset();
        }
        iblt
    }

    /// Compute the size in bytes of an IBLT with the given number of buckets.
    #[inline(always)]
    pub fn size_bytes_with_buckets(buckets: usize) -> usize { buckets * size_of::<IBLTEntry>() }

    /// Compute the IBLT size in buckets to reconcile a given set difference, or return 0 if no advantage.
    /// This returns zero if an IBLT would take up as much or more space than just sending local_set_size
    /// hashes of hash_size_bytes.
    pub fn calc_iblt_parameters(hash_size_bytes: usize, local_set_size: u64, difference_size: u64) -> usize {
        let hashes_would_be = (hash_size_bytes as f64) * (local_set_size as f64);
        let buckets_should_be = (difference_size as f64) * 1.8; // factor determined experimentally for best bytes/item, can be tuned
        let iblt_would_be = buckets_should_be * (size_of::<IBLTEntry>() as f64);
        if iblt_would_be < hashes_would_be {
            buckets_should_be.ceil() as usize
        } else {
            0
        }
    }

    /// Get the size of this IBLT in buckets.
    #[inline(always)]
    pub fn buckets(&self) -> usize { self.map.len() }

    /// Get the size of this IBLT in bytes.
    pub fn size_bytes(&self) -> usize { self.map.len() * size_of::<IBLTEntry>() }

    /// Zero this IBLT.
    #[inline(always)]
    pub fn reset(&mut self) {
        unsafe { write_bytes(self.map.as_mut_ptr().cast::<u8>(), 0, self.map.len() * size_of::<IBLTEntry>()); }
    }

    /// Get this IBLT as a byte slice in place.
    #[inline(always)]
    pub fn as_bytes(&self) -> &[u8] {
        unsafe { &*slice_from_raw_parts(self.map.as_ptr().cast::<u8>(), self.map.len() * size_of::<IBLTEntry>()) }
    }

    /// Construct an IBLT from an input reader and a size in bytes.
    pub async fn new_from_reader<R: AsyncReadExt + Unpin>(r: &mut R, bytes: usize) -> std::io::Result<Self> {
        assert_eq!(size_of::<IBLTEntry>(), 20);
        if (bytes % size_of::<IBLTEntry>()) != 0 {
            Err(std::io::Error::new(std::io::ErrorKind::UnexpectedEof, "incomplete or invalid IBLT"))
        } else {
            let buckets = bytes / size_of::<IBLTEntry>();
            let mut iblt = Self { map: Vec::with_capacity(buckets) };
            unsafe {
                iblt.map.set_len(buckets);
                r.read_exact(&mut *slice_from_raw_parts_mut(iblt.map.as_mut_ptr().cast::<u8>(), bytes)).await?;
            }
            Ok(iblt)
        }
    }

    /// Write this IBLT to a stream.
    /// Note that the size of the IBLT in bytes must be stored separately. Use size_bytes() to get that.
    #[inline(always)]
    pub async fn write<W: AsyncWriteExt + Unpin>(&self, w: &mut W) -> std::io::Result<()> {
        w.write_all(self.as_bytes()).await
    }

    fn ins_rem(&mut self, mut key: u64, delta: i32) {
        key = splitmix64(key);
        let check_hash = xorshift64(key);
        let mut iteration_index = u64::from_le(key);
        let buckets = self.map.len();
        for _ in 0..Self::KEY_MAPPING_ITERATIONS {
            iteration_index = next_iteration_index(iteration_index);
            let b = unsafe { self.map.get_unchecked_mut((iteration_index as usize) % buckets) };
            b.key_sum ^= key;
            b.check_hash_sum ^= check_hash;
            b.count = (i32::from_le(b.count) + delta).to_le();
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
    ///
    /// This returns true on success or false on error, which right now can only happen if the
    /// other IBLT has a different number of buckets or if it contains so many entries
    pub fn subtract(&mut self, other: &Self) -> bool {
        if other.map.len() == self.map.len() {
            for (s, o) in self.map.iter_mut().zip(other.map.iter()) {
                s.key_sum ^= o.key_sum;
                s.check_hash_sum ^= o.check_hash_sum;
                s.count = (i32::from_le(s.count) - i32::from_le(o.count)).to_le();
            }
            return true;
        }
        return false;
    }

    /// List as many entries in this IBLT as can be extracted.
    pub fn list<F: FnMut(&[u8; 8])>(mut self, mut f: F) {
        let mut queue: Vec<u32> = Vec::with_capacity(self.map.len());

        for bi in 0..self.map.len() {
            if unsafe { self.map.get_unchecked(bi).is_singular() } {
                queue.push(bi as u32);
            }
        }

        loop {
            let b = queue.pop();
            let b = if b.is_some() {
                unsafe { self.map.get_unchecked_mut(b.unwrap() as usize) }
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
                    let b_idx = iteration_index % (self.map.len() as u64);
                    let b = unsafe { self.map.get_unchecked_mut(b_idx as usize) };
                    b.key_sum ^= key;
                    b.check_hash_sum ^= check_hash;
                    b.count = (i32::from_le(b.count) - 1).to_le();

                    if b.is_singular() {
                        if queue.len() > self.map.len() { // sanity check for invalid IBLT
                            return;
                        }
                        queue.push(b_idx as u32);
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
            let mut test: IBLT = IBLT::new(CAPACITY);
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
            let mut local: IBLT = IBLT::new(CAPACITY);
            let mut remote: IBLT = IBLT::new(CAPACITY);

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
            let bytes = local.as_bytes().len();
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
