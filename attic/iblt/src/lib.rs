/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2022 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::borrow::Cow;

#[inline(always)]
fn murmurhash32_mix32(mut x: u32) -> u32 {
    x ^= x.wrapping_shr(16);
    x = x.wrapping_mul(0x85ebca6b);
    x ^= x.wrapping_shr(13);
    x = x.wrapping_mul(0xc2b2ae35);
    x ^= x.wrapping_shr(16);
    x
}

#[inline(always)]
fn xor_with<const L: usize>(x: &mut [u8; L], y: &[u8; L]) {
    for i in 0..L {
        x[i] ^= y[i];
    }
}

/// An Invertible Bloom Lookup Table for set reconciliation.
///
/// Usage inspired by this paper:
///
/// https://dash.harvard.edu/bitstream/handle/1/14398536/GENTILI-SENIORTHESIS-2015.pdf
///
/// BUCKETS is the maximum capacity in buckets, while HASHES is the number of
/// "different" (differently seeded) hash functions to use. ITEM_BYTES is the size of
/// each set item in bytes.
///
/// NOTE: due to the small check hash and count used in this implementation, there is a
/// very small (less than one in a billion) chance of a spurious bogus result during list().
/// Most sync protocols should be tolerant of this, but be sure that it's okay if this
/// occurs as it may occur at scale.
///
/// The best value for HASHES seems to be 3 for an optimal fill of 75%.
#[repr(C)]
pub struct IBLT<const BUCKETS: usize, const ITEM_BYTES: usize, const HASHES: usize> {
    check_hash: [u32; BUCKETS],
    count: [i8; BUCKETS],
    key: [[u8; ITEM_BYTES]; BUCKETS],
}

impl<const BUCKETS: usize, const ITEM_BYTES: usize, const HASHES: usize> Clone for IBLT<BUCKETS, ITEM_BYTES, HASHES> {
    #[inline(always)]
    fn clone(&self) -> Self {
        // Intentionally designed to work on any platform even if unaligned. The default way
        // of implementing clone() may not since check_hash[] is u32.
        unsafe {
            let mut tmp: Self = std::mem::MaybeUninit::uninit().assume_init();
            std::ptr::copy_nonoverlapping((self as *const Self).cast::<u8>(), (&mut tmp as *mut Self).cast::<u8>(), Self::SIZE_BYTES);
            tmp
        }
    }
}

impl<const BUCKETS: usize, const ITEM_BYTES: usize, const HASHES: usize> IBLT<BUCKETS, ITEM_BYTES, HASHES> {
    /// Number of bytes each bucket consumes (not congituously, but doesn't matter).
    const BUCKET_SIZE_BYTES: usize = ITEM_BYTES + 4 + 1;

    /// Number of buckets in this IBLT.
    #[allow(unused)]
    pub const BUCKETS: usize = BUCKETS;

    /// Size of this IBLT in bytes.
    pub const SIZE_BYTES: usize = BUCKETS * Self::BUCKET_SIZE_BYTES;

    /// Create a new zeroed IBLT.
    #[inline(always)]
    pub fn new() -> Self {
        assert!(Self::SIZE_BYTES <= std::mem::size_of::<Self>());
        unsafe { std::mem::zeroed() }
    }

    /// Create a new zeroed IBLT on the heap.
    ///
    /// This is useful to create and use IBLT instances too large to fit on the stack.
    #[inline(always)]
    pub fn new_boxed() -> Box<Self> {
        assert!(Self::SIZE_BYTES <= std::mem::size_of::<Self>());
        unsafe { Box::from_raw(std::alloc::alloc_zeroed(std::alloc::Layout::new::<Self>()).cast()) }
    }

    /// Get this IBLT as a byte slice (free cast operation).
    /// The returned slice is always SIZE_BYTES in length.
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
            #[cfg(any(target_arch = "x86_64", target_arch = "x86", target_arch = "powerpc64", target_arch = "aarch64"))]
            {
                Some(Cow::Borrowed(unsafe { &*b.as_ptr().cast() }))
            }

            #[cfg(not(any(target_arch = "x86_64", target_arch = "x86", target_arch = "powerpc64", target_arch = "aarch64")))]
            {
                if b.as_ptr().align_offset(4) == 0 {
                    Some(Cow::Borrowed(unsafe { &*b.as_ptr().cast() }))
                } else {
                    // NOTE: clone() is implemented above using a raw copy so that alignment doesn't matter.
                    Some(Cow::Owned(unsafe { &*b.as_ptr().cast::<Self>() }.clone()))
                }
            }
        } else {
            None
        }
    }

    /// Zero this IBLT.
    #[inline(always)]
    pub fn reset(&mut self) {
        unsafe { std::ptr::write_bytes((self as *mut Self).cast::<u8>(), 0, std::mem::size_of::<Self>()) };
    }

    pub(crate) fn ins_rem(&mut self, key: &[u8; ITEM_BYTES], delta: i8) {
        let check_hash = crc32fast::hash(key);
        let mut iteration_index = u32::from_le(check_hash).wrapping_add(1);
        for _ in 0..(HASHES as u64) {
            iteration_index = murmurhash32_mix32(iteration_index);
            let i = (iteration_index as usize) % BUCKETS;
            self.check_hash[i] ^= check_hash;
            self.count[i] = self.count[i].wrapping_add(delta);
            xor_with(&mut self.key[i], key);
        }
    }

    /// Insert a set item into this set.
    /// This will panic if the slice is smaller than ITEM_BYTES.
    #[inline(always)]
    pub fn insert(&mut self, key: &[u8]) {
        assert!(key.len() >= ITEM_BYTES);
        self.ins_rem(unsafe { &*key.as_ptr().cast() }, 1);
    }

    /// Insert a set item into this set.
    /// This will panic if the slice is smaller than ITEM_BYTES.
    #[inline(always)]
    pub fn remove(&mut self, key: &[u8]) {
        assert!(key.len() >= ITEM_BYTES);
        self.ins_rem(unsafe { &*key.as_ptr().cast() }, -1);
    }

    /// Subtract another IBLT from this one to get a set difference.
    pub fn subtract(&mut self, other: &Self) {
        self.check_hash.iter_mut().zip(other.check_hash.iter()).for_each(|(a, b)| *a ^= *b);
        self.count.iter_mut().zip(other.count.iter()).for_each(|(a, b)| *a = a.wrapping_sub(*b));
        self.key.iter_mut().zip(other.key.iter()).for_each(|(a, b)| xor_with(a, b));
    }

    /// List as many entries in this IBLT as can be extracted.
    ///
    /// The second boolean argument supplied to the function is true if this item belongs to the
    /// "local" IBLT and false if it belongs to the "remote." This is meaningful after subtract()
    /// where true would indicate an item we have and false would indicate an item that only exists
    /// on the other side.
    ///
    /// This function returns true if all entries were successfully listed and false if the listing
    /// is incomplete due to an over-saturated or corrupt IBLT. An incomplete list will still yield
    /// some valid entries, so that does not mean results that were generated were invalid.
    ///
    /// Due to the small check hash sizes used in this IBLT there is a very small chance this will list
    /// bogus items that were never added. This is not an issue with this protocol as it would just result
    /// in an unsatisfied record request.
    pub fn list<F: FnMut([u8; ITEM_BYTES], bool)>(mut self, mut f: F) -> bool {
        assert!(BUCKETS <= (u32::MAX as usize));
        let mut queue: Vec<u32> = Vec::with_capacity(BUCKETS);

        for i in 0..BUCKETS {
            let count = self.count[i];
            if (count == 1 || count == -1) && crc32fast::hash(&self.key[i]) == self.check_hash[i] {
                queue.push(i as u32);
            }
        }

        'list_main: loop {
            let i = queue.pop();
            let i = if i.is_some() {
                i.unwrap() as usize
            } else {
                break 'list_main;
            };

            let check_hash = self.check_hash[i];
            let count = self.count[i];
            let key = &self.key[i];
            if (count == 1 || count == -1) && check_hash == crc32fast::hash(key) {
                let key = key.clone();

                let mut iteration_index = u32::from_le(check_hash).wrapping_add(1);
                for _ in 0..(HASHES as u64) {
                    iteration_index = murmurhash32_mix32(iteration_index);
                    let i2 = (iteration_index as usize) % BUCKETS;
                    let check_hash2 = self.check_hash[i2] ^ check_hash;
                    let count2 = self.count[i2].wrapping_sub(count);
                    let key2 = &mut self.key[i2];
                    self.check_hash[i2] = check_hash2;
                    self.count[i2] = count2;
                    xor_with(key2, &key);
                    if (count2 == 1 || count2 == -1) && check_hash2 == crc32fast::hash(key2) {
                        if queue.len() >= BUCKETS {
                            // sanity check, should be impossible
                            break 'list_main;
                        }
                        queue.push(i2 as u32);
                    }
                }

                f(key, count == 1);
            }
        }

        self.check_hash.iter().all(|x| *x == 0) && self.count.iter().all(|x| *x == 0)
    }
}

impl<const BUCKETS: usize, const ITEM_BYTES: usize, const HASHES: usize> PartialEq for IBLT<BUCKETS, ITEM_BYTES, HASHES> {
    #[inline(always)]
    fn eq(&self, other: &Self) -> bool {
        self.as_bytes().eq(other.as_bytes())
    }
}

impl<const BUCKETS: usize, const ITEM_BYTES: usize, const HASHES: usize> Eq for IBLT<BUCKETS, ITEM_BYTES, HASHES> {}

#[cfg(test)]
mod tests {
    #[inline(always)]
    pub fn splitmix64(mut x: u64) -> u64 {
        x ^= x.wrapping_shr(30);
        x = x.wrapping_mul(0xbf58476d1ce4e5b9);
        x ^= x.wrapping_shr(27);
        x = x.wrapping_mul(0x94d049bb133111eb);
        x ^= x.wrapping_shr(31);
        x
    }

    use std::collections::HashSet;

    use super::*;

    const HASHES: usize = 3;

    #[test]
    fn struct_packing() {
        // Typical case
        let mut tmp = IBLT::<64, 16, 3>::new();
        tmp.check_hash.fill(0x01010101);
        tmp.count.fill(1);
        tmp.key.iter_mut().for_each(|x| x.fill(1));
        assert!(tmp.as_bytes().iter().all(|x| *x == 1));

        // Pathological alignment case #1
        let mut tmp = IBLT::<17, 13, 3>::new();
        tmp.check_hash.fill(0x01010101);
        tmp.count.fill(1);
        tmp.key.iter_mut().for_each(|x| x.fill(1));
        assert!(tmp.as_bytes().iter().all(|x| *x == 1));

        // Pathological alignment case #2
        let mut tmp = IBLT::<17, 8, 3>::new();
        tmp.check_hash.fill(0x01010101);
        tmp.count.fill(1);
        tmp.key.iter_mut().for_each(|x| x.fill(1));
        assert!(tmp.as_bytes().iter().all(|x| *x == 1));

        // Pathological alignment case #3
        let mut tmp = IBLT::<16, 7, 3>::new();
        tmp.check_hash.fill(0x01010101);
        tmp.count.fill(1);
        tmp.key.iter_mut().for_each(|x| x.fill(1));
        assert!(tmp.as_bytes().iter().all(|x| *x == 1));
    }

    #[test]
    fn fill_list_performance() {
        const CAPACITY: usize = 4096;
        let mut rn: u128 = 0xd3b07384d113edec49eaa6238ad5ff00;
        let mut expected: HashSet<u128> = HashSet::with_capacity(4096);
        let mut count = 64;
        while count <= CAPACITY {
            let mut test = IBLT::<CAPACITY, 16, HASHES>::new_boxed();
            expected.clear();

            for _ in 0..count {
                rn = rn.wrapping_add(splitmix64(rn as u64) as u128);
                expected.insert(rn);
                test.insert(&rn.to_le_bytes());
            }

            let mut list_count = 0;
            test.list(|x, d| {
                list_count += 1;
                assert!(expected.contains(&u128::from_le_bytes(x)));
                assert!(d);
            });

            println!("inserted: {}\tlisted: {}\tcapacity: {}\tscore: {:.4}\tfill: {:.4}", count, list_count, CAPACITY, (list_count as f64) / (count as f64), (count as f64) / (CAPACITY as f64));
            count += 32;
        }
    }

    #[test]
    fn merge_sets() {
        const CAPACITY: usize = 16384;
        const REMOTE_SIZE: usize = 1024 * 1024 * 2;
        const STEP: usize = 1024;
        let mut rn: u128 = 0xd3b07384d113edec49eaa6238ad5ff00;
        let mut missing_count = 1024;
        let mut missing: HashSet<u128> = HashSet::with_capacity(CAPACITY * 2);
        let mut all: HashSet<u128> = HashSet::with_capacity(REMOTE_SIZE);
        while missing_count <= CAPACITY {
            missing.clear();
            all.clear();
            let mut local = IBLT::<CAPACITY, 16, HASHES>::new_boxed();
            let mut remote = IBLT::<CAPACITY, 16, HASHES>::new_boxed();

            let mut k = 0;
            while k < REMOTE_SIZE {
                rn = rn.wrapping_add(splitmix64(rn as u64) as u128);
                if all.insert(rn) {
                    if k >= missing_count {
                        local.insert(&rn.to_le_bytes());
                    } else {
                        missing.insert(rn);
                    }
                    remote.insert(&rn.to_le_bytes());
                    k += 1;
                }
            }

            let mut remote2 = remote.clone();
            remote2.subtract(&local);
            remote2.list(|_, d| {
                assert!(d);
            });
            local.subtract(&remote);
            let bytes = local.as_bytes().len();
            let mut cnt = 0;
            let all_success = local.list(|x, d| {
                assert!(missing.contains(&u128::from_le_bytes(x)));
                assert!(!d);
                cnt += 1;
            });

            println!("total: {}  missing: {:5}  recovered: {:5}  size: {:5}  score: {:.4}  bytes/item: {:.2}  extract(fill): {:.4}  100%: {}", REMOTE_SIZE, missing.len(), cnt, bytes, (cnt as f64) / (missing.len() as f64), (bytes as f64) / (cnt as f64), (cnt as f64) / (CAPACITY as f64), all_success);

            missing_count += STEP;
        }
    }
}
