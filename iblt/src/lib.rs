/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2022 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use zerocopy::{AsBytes, FromBytes};

#[cfg(not(any(target_arch = "aarch64", target_arch = "x86", target_arch = "x86_64", target_arch = "powerpc64")))]
#[inline(always)]
fn xor_with<T>(x: &mut T, y: &T)
where
    T: FromBytes + AsBytes + Sized,
{
    x.as_bytes_mut().iter_mut().zip(y.as_bytes().iter()).for_each(|(a, b)| *a ^= *b);
}

#[cfg(any(target_arch = "aarch64", target_arch = "x86", target_arch = "x86_64", target_arch = "powerpc64"))]
#[inline(always)]
fn xor_with<T>(x: &mut T, y: &T)
where
    T: FromBytes + AsBytes + Sized,
{
    let size = std::mem::size_of::<T>();

    if size >= 16 {
        for i in 0..(size / 16) {
            unsafe { *x.as_bytes_mut().as_mut_ptr().cast::<u128>().add(i) ^= *y.as_bytes().as_ptr().cast::<u128>().add(i) };
        }
        for i in (size - (size % 16))..size {
            unsafe { *x.as_bytes_mut().as_mut_ptr().add(i) ^= *y.as_bytes().as_ptr().add(i) };
        }
    } else {
        for i in 0..(size / 8) {
            unsafe { *x.as_bytes_mut().as_mut_ptr().cast::<u64>().add(i) ^= *y.as_bytes().as_ptr().cast::<u64>().add(i) };
        }
        for i in (size - (size % 8))..size {
            unsafe { *x.as_bytes_mut().as_mut_ptr().add(i) ^= *y.as_bytes().as_ptr().add(i) };
        }
    }
}

#[inline(always)]
fn murmurhash32_mix32(mut x: u32) -> u32 {
    x ^= x.wrapping_shr(16);
    x = x.wrapping_mul(0x85ebca6b);
    x ^= x.wrapping_shr(13);
    x = x.wrapping_mul(0xc2b2ae35);
    x ^= x.wrapping_shr(16);
    x
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
pub struct IBLT<T, const BUCKETS: usize, const HASHES: usize>
where
    T: FromBytes + AsBytes + Default + Sized + Clone,
{
    check_hash: Box<Vec<u32>>,
    count: Box<Vec<i8>>,
    key: Box<Vec<T>>,
}

impl<T, const BUCKETS: usize, const HASHES: usize> Clone for IBLT<T, BUCKETS, HASHES>
where
    T: FromBytes + AsBytes + Default + Sized + Clone,
{
    fn clone(&self) -> Self {
        unsafe {
            let mut tmp = Self::new();
            std::ptr::copy_nonoverlapping(self.check_hash.as_ptr(), tmp.check_hash.as_mut_ptr(), BUCKETS);
            std::ptr::copy_nonoverlapping(self.count.as_ptr(), tmp.count.as_mut_ptr(), BUCKETS);
            std::ptr::copy_nonoverlapping(self.key.as_ptr(), tmp.key.as_mut_ptr(), BUCKETS);
            tmp
        }
    }
}

impl<T, const BUCKETS: usize, const HASHES: usize> IBLT<T, BUCKETS, HASHES>
where
    T: FromBytes + AsBytes + Default + Sized + Clone,
{
    /// Number of bytes each bucket consumes (not congituously, but doesn't matter).
    const BUCKET_SIZE_BYTES: usize = std::mem::size_of::<T>() + 4 + 1;

    /// Number of buckets in this IBLT.
    pub const BUCKETS: usize = BUCKETS;

    /// Size of this IBLT in bytes.
    pub const SIZE_BYTES: usize = BUCKETS * Self::BUCKET_SIZE_BYTES;

    /// Create a new zeroed IBLT.
    #[inline(always)]
    pub fn new() -> Self {
        assert!(BUCKETS < (i32::MAX as usize));

        let mut s = Self {
            check_hash: Box::new(Vec::with_capacity(BUCKETS)),
            count: Box::new(Vec::with_capacity(BUCKETS)),
            key: Box::new(Vec::with_capacity(BUCKETS)),
        };

        s.reset();
        s
    }

    #[inline(always)]
    pub fn as_bytes(&self) -> Box<Vec<u8>> {
        let check_hash_len = BUCKETS * 4;
        let t_len = BUCKETS * std::mem::size_of::<T>();
        let len = check_hash_len + BUCKETS + t_len;

        let mut buf = Box::new(Vec::with_capacity(len));
        buf.resize(len, 0);

        let byt = buf.as_bytes_mut();

        byt[0..check_hash_len].copy_from_slice(self.check_hash.as_bytes());
        byt[check_hash_len..BUCKETS + check_hash_len].copy_from_slice(self.count.as_bytes());
        byt[len - t_len..len].copy_from_slice(self.key.as_bytes());

        buf
    }

    /// Obtain an IBLT from bytes in memory.
    ///
    /// If the architecture supports unaligned memory access or the memory is aligned, this returns a borrowed
    /// Cow to 'b' that is just a cast. If re-alignment is necessary it returns an owned Cow containing a properly
    /// aligned copy. This makes conversion a nearly free cast when alignment adjustment isn't needed.
    #[inline(always)]
    pub fn from_bytes(b: Box<Vec<u8>>) -> Option<Self> {
        if b.len() == Self::SIZE_BYTES {
            // FIXME I commented this out because I do not have access to the architectures needed.
            // #[cfg(not(any(target_arch = "x86_64", target_arch = "x86", target_arch = "powerpc64", target_arch = "aarch64")))]
            // {
            //     if b.as_ptr().align_offset(8) == 0 {
            //         Some(Cow::Borrowed(unsafe { &*b.as_ptr().cast() }))
            //     } else {
            //         // NOTE: clone() is implemented above using a raw copy so that alignment doesn't matter.
            //         Some(Cow::Owned(unsafe { &*b.as_ptr().cast::<Self>() }.clone()))
            //     }
            // }
            #[cfg(any(target_arch = "x86_64", target_arch = "x86", target_arch = "powerpc64", target_arch = "aarch64"))]
            {
                let mut tmp = Self::new();

                let mut i = 0;

                tmp.check_hash.as_bytes_mut().copy_from_slice(&b[0..BUCKETS * 4]);
                i += BUCKETS * 4;

                tmp.count.as_bytes_mut().copy_from_slice(&b[i..i + BUCKETS]);
                i += BUCKETS;

                tmp.key.as_bytes_mut().copy_from_slice(&b[i..i + std::mem::size_of::<T>() * BUCKETS]);

                Some(tmp)
            }
        } else {
            None
        }
    }

    /// Zero this IBLT.
    #[inline(always)]
    pub fn reset(&mut self) {
        self.check_hash.clear();
        self.count.clear();
        self.key.clear();
        self.check_hash.resize(BUCKETS, 0);
        self.count.resize(BUCKETS, 0);
        self.key.resize(BUCKETS, Default::default());
    }

    pub(crate) fn ins_rem(&mut self, key: &T, delta: i8) {
        let check_hash = crc32fast::hash(key.as_bytes());
        let mut iteration_index = u32::from_le(check_hash).wrapping_add(1);
        for _ in 0..(HASHES as u64) {
            iteration_index = murmurhash32_mix32(iteration_index);
            let i = (iteration_index as usize) % BUCKETS;
            self.check_hash[i] ^= check_hash;
            self.count[i] = self.count[i].wrapping_add(delta);
            xor_with(&mut self.key[i], &key);
        }
    }

    /// Insert a set item into this set.
    /// This will panic if the slice is smaller than ITEM_BYTES.
    #[inline(always)]
    pub fn insert(&mut self, key: &T) {
        self.ins_rem(key, 1);
    }

    /// Insert a set item into this set.
    /// This will panic if the slice is smaller than ITEM_BYTES.
    #[inline(always)]
    pub fn remove(&mut self, key: &T) {
        self.ins_rem(key, -1);
    }

    /// Subtract another IBLT from this one to get a set difference.
    pub fn subtract(&mut self, other: &Self) {
        self.check_hash.iter_mut().zip(other.check_hash.iter()).for_each(|(a, b)| *a ^= *b);
        self.count.iter_mut().zip(other.count.iter()).for_each(|(a, b)| *a = a.wrapping_sub(*b));
        self.key.iter_mut().zip(other.key.iter()).for_each(|(a, b)| xor_with(a, &b));
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
    pub fn list<F: FnMut(T, bool)>(&mut self, mut f: F) -> bool {
        let mut queue: Box<Vec<u32>> = Box::new(Vec::with_capacity(BUCKETS));

        for i in 0..BUCKETS {
            let count = self.count[i];
            if (count == 1 || count == -1) && crc32fast::hash(&self.key[i].as_bytes()) == self.check_hash[i] {
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
            if (count == 1 || count == -1) && check_hash == crc32fast::hash(key.as_bytes()) {
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
                    if (count2 == 1 || count2 == -1) && check_hash2 == crc32fast::hash(key2.as_bytes()) {
                        if queue.len() > BUCKETS {
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

impl<T, const BUCKETS: usize, const HASHES: usize> PartialEq for IBLT<T, BUCKETS, HASHES>
where
    T: AsBytes + FromBytes + Default + Clone,
{
    #[inline(always)]
    fn eq(&self, other: &Self) -> bool {
        self.as_bytes().eq(&other.as_bytes())
    }
}

impl<T, const BUCKETS: usize, const HASHES: usize> Eq for IBLT<T, BUCKETS, HASHES> where T: AsBytes + FromBytes + Default + Clone {}

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

    fn check_xor_with2<const L: usize>() {
        let with = [17_u8; L];
        let mut expected = [69_u8; L];
        let mut actual = [69_u8; L];
        expected.iter_mut().zip(with.iter()).for_each(|(a, b)| *a ^= *b);
        xor_with(&mut actual, &with);
        assert!(actual.eq(&expected));
    }

    fn typical_iblt() -> IBLT<[u8; 32], 16, 3> {
        // Typical case
        let mut tmp = IBLT::<[u8; 32], 16, 3>::new();
        tmp.check_hash.fill(0x01010101);
        tmp.count.fill(1);
        tmp.key.iter_mut().for_each(|x| x.fill(1));
        tmp
    }

    #[test]
    fn check_xor_with() {
        check_xor_with2::<128>();
        check_xor_with2::<65>();
        check_xor_with2::<64>();
        check_xor_with2::<63>();
        check_xor_with2::<33>();
        check_xor_with2::<32>();
        check_xor_with2::<31>();
        check_xor_with2::<17>();
        check_xor_with2::<16>();
        check_xor_with2::<15>();
        check_xor_with2::<9>();
        check_xor_with2::<8>();
        check_xor_with2::<7>();
        check_xor_with2::<6>();
        check_xor_with2::<5>();
        check_xor_with2::<4>();
        check_xor_with2::<3>();
        check_xor_with2::<2>();
        check_xor_with2::<1>();
    }

    #[test]
    fn struct_packing() {
        let tmp = typical_iblt();
        assert!(tmp.as_bytes().iter().all(|x| *x == 1));

        // Pathological alignment case #1
        let mut tmp = IBLT::<[u8; 17], 13, 3>::new();
        tmp.check_hash.fill(0x01010101);
        tmp.count.fill(1);
        tmp.key.iter_mut().for_each(|x| x.fill(1));
        assert!(tmp.as_bytes().iter().all(|x| *x == 1));

        // Pathological alignment case #2
        let mut tmp = IBLT::<[u8; 17], 8, 3>::new();
        tmp.check_hash.fill(0x01010101);
        tmp.count.fill(1);
        tmp.key.iter_mut().for_each(|x| x.fill(1));
        assert!(tmp.as_bytes().iter().all(|x| *x == 1));

        // Pathological alignment case #3
        let mut tmp = IBLT::<[u8; 16], 7, 3>::new();
        tmp.check_hash.fill(0x01010101);
        tmp.count.fill(1);
        tmp.key.iter_mut().for_each(|x| x.fill(1));
        assert!(tmp.as_bytes().iter().all(|x| *x == 1));
    }

    #[test]
    fn fill_list_performance() {
        const LENGTH: usize = 16;
        const CAPACITY: usize = 4096;
        let mut rn: u128 = 0xd3b07384d113edec49eaa6238ad5ff00;
        let mut expected: HashSet<u128> = HashSet::with_capacity(CAPACITY);
        let mut count = LENGTH;
        while count <= CAPACITY {
            let mut test = IBLT::<[u8; LENGTH], CAPACITY, HASHES>::new();
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
            count += LENGTH;
        }
    }

    #[test]
    fn merge_sets() {
        const CAPACITY: usize = 4096; // previously 16384;
        const REMOTE_SIZE: usize = 1024 * 1024 * 2;
        const STEP: usize = 1024;
        const LENGTH: usize = 16;
        let mut rn: u128 = 0xd3b07384d113edec49eaa6238ad5ff00;
        let mut missing_count = 1024;
        let mut missing: HashSet<u128> = HashSet::with_capacity(CAPACITY * 2);
        let mut all: HashSet<u128> = HashSet::with_capacity(REMOTE_SIZE);
        while missing_count <= CAPACITY {
            missing.clear();
            all.clear();
            let mut local = IBLT::<[u8; LENGTH], CAPACITY, HASHES>::new();
            let mut remote = IBLT::<[u8; LENGTH], CAPACITY, HASHES>::new();

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

    #[derive(Eq, PartialEq, Clone, AsBytes, FromBytes, Debug)]
    #[repr(C)]
    struct TestType {
        thing: [u8; 256],
        other_thing: [u8; 32],
    }

    impl Default for TestType {
        fn default() -> Self {
            Self::zeroed()
        }
    }

    impl TestType {
        pub fn zeroed() -> Self {
            unsafe { std::mem::zeroed() }
        }

        pub fn new() -> Self {
            let mut newtype = Self::zeroed();
            newtype.thing.fill_with(|| rand::random());
            newtype.other_thing.fill_with(|| rand::random());
            newtype
        }
    }

    #[test]
    fn test_polymorphism() {
        const CAPACITY: usize = 4096;
        let mut full = Box::new(IBLT::<TestType, CAPACITY, HASHES>::new());
        let mut zero = Box::new(IBLT::<TestType, CAPACITY, HASHES>::new());

        for _ in 0..CAPACITY {
            zero.insert(&TestType::zeroed());
            full.insert(&TestType::new());
        }

        zero.subtract(&full);

        zero.list(|item, new| {
            if new {
                assert_eq!(item, TestType::zeroed());
            } else {
                assert_ne!(item, TestType::zeroed());
            }
        });

        zero.reset();
        full.reset();

        for _ in 0..CAPACITY {
            zero.insert(&TestType::zeroed());
            full.insert(&TestType::new());
        }

        full.subtract(&zero);
        full.list(|item, new| {
            if new {
                assert_ne!(item, TestType::zeroed());
            } else {
                assert_eq!(item, TestType::zeroed());
            }
        });
    }

    #[test]
    fn test_to_from_bytes() {
        let tmp = typical_iblt();
        let mut tmp2 = IBLT::<[u8; 32], 16, 3>::from_bytes(tmp.as_bytes()).unwrap();

        tmp2.subtract(&tmp);
        tmp2.list(|_, new| assert!(!new));
    }

    #[test]
    fn test_clone() {
        let tmp = typical_iblt();
        let mut tmp2 = tmp.clone();

        tmp2.subtract(&tmp);
        tmp2.list(|_, new| assert!(!new));
    }
}
