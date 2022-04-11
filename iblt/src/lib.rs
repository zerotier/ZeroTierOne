/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2022 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::borrow::Cow;

/// Total memory overhead of each bucket in bytes.
const BUCKET_SIZE_BYTES: usize = 13; // u64 key + u32 check + i8 count

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

/// Based on xorshift64 with endian conversion for BE systems.
#[inline(always)]
fn get_check_hash(mut x: u64) -> u32 {
    x = u64::from_le(x);
    x ^= x.wrapping_shl(13);
    x ^= x.wrapping_shr(7);
    x ^= x.wrapping_shl(17);
    x.wrapping_add(x.wrapping_shr(32)).to_le() as u32
}

/// Called to get the next iteration index for each KEY_MAPPING_ITERATIONS table lookup.
/// A series of these implements the "series of different hashes" construct in IBLT.
#[inline(always)]
fn next_iteration_index(mut x: u64, hash_no: u64) -> u64 {
    x = x.wrapping_add(hash_no);
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
///
/// Note that an 8-bit counter that wraps is used instead of a much wider counter that
/// would probably never overflow. This saves space on the wire but means that there is
/// a very small (roughly 1/2^64) chance that this will list a value that is invalid in that
/// it was never added on either side. In our protocol that should not cause a problem as
/// it would just result in one key in a GetRecords query not being fulfilled, and in any
/// case it should be an extremely rare event.
///
/// BUCKETS is the maximum capacity in buckets, while HASHES is the number of
/// "different" (differently seeded) hash functions to use.
///
/// The best value for HASHES seems to be 3 for an optimal fill of 80%.
#[repr(C)]
pub struct IBLT<const BUCKETS: usize, const HASHES: usize> {
    total_count: i64, // always stored little-endian in memory
    key: [u64; BUCKETS],
    check_hash: [u32; BUCKETS],
    count: [i8; BUCKETS],
}

impl<const BUCKETS: usize, const HASHES: usize> Clone for IBLT<BUCKETS, HASHES> {
    #[inline(always)]
    fn clone(&self) -> Self {
        // NOTE: clone() is manually implemented here so it's tolerant of unaligned access on architectures not supporting it.
        unsafe {
            let mut tmp: Self = std::mem::MaybeUninit::uninit().assume_init();
            std::ptr::copy_nonoverlapping((self as *const Self).cast::<u8>(), (&mut tmp as *mut Self).cast::<u8>(), Self::SIZE_BYTES);
            tmp
        }
    }
}

impl<const BUCKETS: usize, const HASHES: usize> IBLT<BUCKETS, HASHES> {
    /// Number of buckets in this IBLT.
    #[allow(unused)]
    pub const BUCKETS: usize = BUCKETS;

    /// Size of this IBLT in bytes.
    pub const SIZE_BYTES: usize = 8 + (BUCKETS * BUCKET_SIZE_BYTES); // total_count + buckets

    /// Create a new zeroed IBLT.
    #[inline(always)]
    pub fn new() -> Self {
        assert!(Self::SIZE_BYTES <= std::mem::size_of::<Self>());
        assert!(BUCKETS < (i32::MAX as usize));
        unsafe { std::mem::zeroed() }
    }

    /// Get this IBLT as a byte slice (free cast operation).
    #[inline(always)]
    pub fn as_bytes(&self) -> &[u8] {
        unsafe { &*std::ptr::slice_from_raw_parts((self as *const Self).cast::<u8>(), Self::SIZE_BYTES) }
    }

    /// Obtain an IBLT from bytes in memory.
    ///
    /// If the architecture supports unaligned memory access or the memory is aligned, this returns a borrowed
    /// Cow to 'b' that is just a cast. If re-alignment is necessary it returns an owned Cow containing a properly
    /// aligned copy. This makes conversion a nearly free cast when alignment adjustment isn't needed.
    #[inline(always)]
    pub fn from_bytes<'a>(b: &'a [u8]) -> Option<Cow<'a, Self>> {
        if b.len() == Self::SIZE_BYTES {
            #[cfg(not(any(target_arch = "x86_64", target_arch = "x86", target_arch = "powerpc64", target_arch = "aarch64")))]
            {
                if b.as_ptr().align_offset(8) == 0 {
                    Some(Cow::Borrowed(unsafe { &*b.as_ptr().cast() }))
                } else {
                    // NOTE: clone() is implemented above using a raw copy so that alignment doesn't matter.
                    Some(Cow::Owned(unsafe { &*b.as_ptr().cast::<Self>() }.clone()))
                }
            }
            #[cfg(any(target_arch = "x86_64", target_arch = "x86", target_arch = "powerpc64", target_arch = "aarch64"))]
            {
                Some(Cow::Borrowed(unsafe { &*b.as_ptr().cast() }))
            }
        } else {
            None
        }
    }

    /// Zero this IBLT.
    #[inline(always)]
    pub fn reset(&mut self) {
        unsafe {
            std::ptr::write_bytes((self as *mut Self).cast::<u8>(), 0, std::mem::size_of::<Self>());
        }
    }

    /// Get the total number of set items that have been added to this IBLT.
    pub fn count(&self) -> u64 {
        i64::from_le(self.total_count).max(0) as u64
    }

    pub(crate) fn ins_rem(&mut self, key: u64, delta: i8) {
        self.total_count = i64::from_le(self.total_count).wrapping_add(delta as i64).to_le();
        let check_hash = get_check_hash(key);
        let mut iteration_index = u64::from_le(key);
        for k in 0..(HASHES as u64) {
            iteration_index = next_iteration_index(iteration_index, k);
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
        self.total_count = i64::from_le(self.total_count).wrapping_sub(i64::from_le(other.total_count.max(0))).max(0).to_le();
        self.key.iter_mut().zip(other.key.iter()).for_each(|(a, b)| *a ^= *b);
        self.check_hash.iter_mut().zip(other.check_hash.iter()).for_each(|(a, b)| *a ^= *b);
        self.count.iter_mut().zip(other.count.iter()).for_each(|(a, b)| *a = a.wrapping_sub(*b));
    }

    /// List as many entries in this IBLT as can be extracted.
    ///
    /// True is returned if the number of extracted items was exactly equal to the total number of items
    /// in this set summary. A return of false indicates incomplete extraction or an invalid IBLT.
    ///
    /// Due to the small check hash sizes used in this IBLT there is a very small chance this will list
    /// bogus items that were never added. This is not an issue with this protocol as it would just result
    /// in an unsatisfied record request.
    pub fn list<F: FnMut(u64)>(mut self, mut f: F) -> bool {
        let mut queue: Vec<u32> = Vec::with_capacity(BUCKETS);

        for i in 0..BUCKETS {
            let count = self.count[i];
            if (count == 1 || count == -1) && get_check_hash(self.key[i]) == self.check_hash[i] {
                queue.push(i as u32);
            }
        }

        let total_count = i64::from_le(self.total_count);
        let mut listed = 0;

        'list_main: while listed < total_count {
            let i = queue.pop();
            let i = if i.is_some() {
                i.unwrap() as usize
            } else {
                break 'list_main;
            };

            let key = self.key[i];
            let check_hash = self.check_hash[i];
            let count = self.count[i];
            if (count == 1 || count == -1) && check_hash == get_check_hash(key) {
                listed += 1;
                f(key);

                let mut iteration_index = u64::from_le(key);
                for k in 0..(HASHES as u64) {
                    iteration_index = next_iteration_index(iteration_index, k);
                    let i = (iteration_index as usize) % BUCKETS;
                    let key2 = self.key[i] ^ key;
                    let check_hash2 = self.check_hash[i] ^ check_hash;
                    let count2 = self.count[i].wrapping_sub(count);
                    self.key[i] = key2;
                    self.check_hash[i] = check_hash2;
                    self.count[i] = count2;
                    if (count2 == 1 || count2 == -1) && check_hash2 == get_check_hash(key2) {
                        if queue.len() > BUCKETS {
                            // sanity check, should be impossible
                            break 'list_main;
                        }
                        queue.push(i as u32);
                    }
                }
            }
        }

        listed == total_count
    }
}

#[cfg(test)]
mod tests {
    use std::collections::HashSet;
    #[allow(unused_imports)]
    use std::time::SystemTime;

    use super::*;
    #[allow(unused_imports)]
    use super::{splitmix64, xorshift64};

    const HASHES: usize = 3;

    #[test]
    fn fill_list_performance() {
        const CAPACITY: usize = 4096;
        //let mut rn = xorshift64(SystemTime::now().duration_since(SystemTime::UNIX_EPOCH).unwrap().as_nanos() as u64);
        let mut rn = 31337;
        let mut expected: HashSet<u64> = HashSet::with_capacity(4096);
        let mut count = 64;
        while count <= CAPACITY {
            let mut test = IBLT::<CAPACITY, HASHES>::new();
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

            println!("inserted: {}\tlisted: {}\tcapacity: {}\tscore: {:.4}\tfill: {:.4}", count, list_count, CAPACITY, (list_count as f64) / (count as f64), (count as f64) / (CAPACITY as f64));
            count += 64;
        }
    }

    #[test]
    fn merge_sets() {
        const CAPACITY: usize = 16384;
        const REMOTE_SIZE: usize = 1024 * 1024 * 2;
        const STEP: usize = 1024;
        //let mut rn = xorshift64(SystemTime::now().duration_since(SystemTime::UNIX_EPOCH).unwrap().as_nanos() as u64);
        let mut rn = 31337;
        let mut missing_count = 1024;
        let mut missing: HashSet<u64> = HashSet::with_capacity(CAPACITY * 2);
        let mut all: HashSet<u64> = HashSet::with_capacity(REMOTE_SIZE);
        while missing_count <= CAPACITY {
            missing.clear();
            all.clear();
            let mut local = IBLT::<CAPACITY, HASHES>::new();
            let mut remote = IBLT::<CAPACITY, HASHES>::new();

            let mut k = 0;
            while k < REMOTE_SIZE {
                if all.insert(rn) {
                    if k >= missing_count {
                        local.insert(rn);
                    } else {
                        missing.insert(rn);
                    }
                    remote.insert(rn);
                    k += 1;
                }
                rn = splitmix64(rn);
            }

            local.subtract(&mut remote);
            let bytes = local.as_bytes().len();
            let mut cnt = 0;
            let all_success = local.list(|k| {
                assert!(missing.contains(&k));
                cnt += 1;
            });

            println!("total: {}  missing: {:5}  recovered: {:5}  size: {:5}  score: {:.4}  bytes/item: {:.2}  extract(fill): {:.4}  100%: {}", REMOTE_SIZE, missing.len(), cnt, bytes, (cnt as f64) / (missing.len() as f64), (bytes as f64) / (cnt as f64), (cnt as f64) / (CAPACITY as f64), all_success);

            missing_count += STEP;
        }
    }
}
