// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::hash::{Hash, Hasher};

use std::mem::MaybeUninit;

#[inline(always)]
fn xorshift64(mut x: u64) -> u64 {
    x ^= x.wrapping_shl(13);
    x ^= x.wrapping_shr(7);
    x ^= x.wrapping_shl(17);
    x
}

struct XorShiftHasher(u64);

impl XorShiftHasher {
    #[inline(always)]
    fn new(salt: u32) -> Self {
        Self(salt as u64)
    }
}

impl Hasher for XorShiftHasher {
    #[inline(always)]
    fn finish(&self) -> u64 {
        self.0
    }

    #[inline(always)]
    fn write(&mut self, mut bytes: &[u8]) {
        let mut x = self.0;
        while bytes.len() >= 8 {
            x = xorshift64(x.wrapping_add(u64::from_ne_bytes(unsafe { *bytes.as_ptr().cast::<[u8; 8]>() })));
            bytes = &bytes[8..];
        }
        while bytes.len() >= 4 {
            x = xorshift64(x.wrapping_add(u32::from_ne_bytes(unsafe { *bytes.as_ptr().cast::<[u8; 4]>() }) as u64));
            bytes = &bytes[4..];
        }
        for b in bytes.iter() {
            x = xorshift64(x.wrapping_add(*b as u64));
        }
        self.0 = x;
    }

    #[inline(always)]
    fn write_isize(&mut self, i: isize) {
        self.0 = xorshift64(self.0.wrapping_add(i as u64));
    }

    #[inline(always)]
    fn write_usize(&mut self, i: usize) {
        self.0 = xorshift64(self.0.wrapping_add(i as u64));
    }

    #[inline(always)]
    fn write_i32(&mut self, i: i32) {
        self.0 = xorshift64(self.0.wrapping_add(i as u64));
    }

    #[inline(always)]
    fn write_u32(&mut self, i: u32) {
        self.0 = xorshift64(self.0.wrapping_add(i as u64));
    }

    #[inline(always)]
    fn write_i64(&mut self, i: i64) {
        self.0 = xorshift64(self.0.wrapping_add(i as u64));
    }

    #[inline(always)]
    fn write_u64(&mut self, i: u64) {
        self.0 = xorshift64(self.0.wrapping_add(i));
    }
}

struct Entry<K: Eq + PartialEq + Hash + Clone, V> {
    key: MaybeUninit<K>,
    value: MaybeUninit<V>,
    bucket: i32, // which bucket is this in? -1 for none
    next: i32,   // next item in bucket's linked list, -1 for none
    prev: i32,   // previous entry to permit deletion of old entries from bucket lists
}

/// A hybrid between a circular buffer and a map.
///
/// The map has a finite capacity. If a new entry is added and there's no more room the oldest
/// entry is removed and overwritten. The same could be achieved by pairing a circular buffer
/// with a HashMap but that would be less efficient. This requires no memory allocations unless
/// the K or V types allocate memory and occupies a fixed amount of memory.
///
/// This is pretty basic and doesn't have a remove function. Old entries just roll off. This
/// only contains what is needed elsewhere in the project.
///
/// The C template parameter is the total capacity while the B parameter is the number of
/// buckets in the hash table.
pub struct RingBufferMap<K: Eq + PartialEq + Hash + Clone, V, const C: usize, const B: usize> {
    entries: [Entry<K, V>; C],
    buckets: [i32; B],
    entry_ptr: u32,
    salt: u32,
}

impl<K: Eq + PartialEq + Hash + Clone, V, const C: usize, const B: usize> RingBufferMap<K, V, C, B> {
    #[inline]
    pub fn new(salt: u32) -> Self {
        Self {
            entries: {
                let mut entries: [Entry<K, V>; C] = unsafe { MaybeUninit::uninit().assume_init() };
                for e in entries.iter_mut() {
                    e.bucket = -1;
                    e.next = -1;
                    e.prev = -1;
                }
                entries
            },
            buckets: [-1; B],
            entry_ptr: 0,
            salt,
        }
    }

    #[inline]
    pub fn get(&self, key: &K) -> Option<&V> {
        let mut h = XorShiftHasher::new(self.salt);
        key.hash(&mut h);
        let mut e = self.buckets[(h.finish() as usize) % B];
        while e >= 0 {
            let ee = &self.entries[e as usize];
            debug_assert!(ee.bucket >= 0);
            if unsafe { ee.key.assume_init_ref().eq(key) } {
                return Some(unsafe { &ee.value.assume_init_ref() });
            }
            e = ee.next;
        }
        return None;
    }

    /// Get an entry, creating if not present.
    #[inline]
    pub fn get_or_create_mut<CF: FnOnce() -> V>(&mut self, key: &K, create: CF) -> &mut V {
        let mut h = XorShiftHasher::new(self.salt);
        key.hash(&mut h);
        let bucket = (h.finish() as usize) % B;

        let mut e = self.buckets[bucket];
        while e >= 0 {
            unsafe {
                let e_ptr = &mut *self.entries.as_mut_ptr().add(e as usize);
                debug_assert!(e_ptr.bucket >= 0);
                if e_ptr.key.assume_init_ref().eq(key) {
                    return e_ptr.value.assume_init_mut();
                }
                e = e_ptr.next;
            }
        }

        return self.internal_add(bucket, key.clone(), create());
    }

    /// Set a value or create a new entry if not found.
    #[inline]
    pub fn set(&mut self, key: K, value: V) {
        let mut h = XorShiftHasher::new(self.salt);
        key.hash(&mut h);
        let bucket = (h.finish() as usize) % B;

        let mut e = self.buckets[bucket];
        while e >= 0 {
            let e_ptr = &mut self.entries[e as usize];
            debug_assert!(e_ptr.bucket >= 0);
            if unsafe { e_ptr.key.assume_init_ref().eq(&key) } {
                unsafe { *e_ptr.value.assume_init_mut() = value };
                return;
            }
            e = e_ptr.next;
        }

        self.internal_add(bucket, key, value);
    }

    #[inline]
    fn internal_add(&mut self, bucket: usize, key: K, value: V) -> &mut V {
        let e = (self.entry_ptr as usize) % C;
        self.entry_ptr = self.entry_ptr.wrapping_add(1);
        let e_ptr = unsafe { &mut *self.entries.as_mut_ptr().add(e) };

        if e_ptr.bucket >= 0 {
            if e_ptr.prev >= 0 {
                self.entries[e_ptr.prev as usize].next = e_ptr.next;
            } else {
                self.buckets[e_ptr.bucket as usize] = e_ptr.next;
            }
            unsafe {
                e_ptr.key.assume_init_drop();
                e_ptr.value.assume_init_drop();
            }
        }

        e_ptr.key.write(key);
        e_ptr.value.write(value);
        e_ptr.bucket = bucket as i32;
        e_ptr.next = self.buckets[bucket];
        if e_ptr.next >= 0 {
            self.entries[e_ptr.next as usize].prev = e as i32;
        }
        self.buckets[bucket] = e as i32;
        e_ptr.prev = -1;
        unsafe { e_ptr.value.assume_init_mut() }
    }
}

impl<K: Eq + PartialEq + Hash + Clone, V, const C: usize, const B: usize> Drop for RingBufferMap<K, V, C, B> {
    #[inline]
    fn drop(&mut self) {
        for e in self.entries.iter_mut() {
            if e.bucket >= 0 {
                unsafe {
                    e.key.assume_init_drop();
                    e.value.assume_init_drop();
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::RingBufferMap;

    #[test]
    fn finite_map() {
        let mut m = RingBufferMap::<usize, usize, 128, 17>::new(1);
        for i in 0..64 {
            m.set(i, i);
        }
        for i in 0..64 {
            assert_eq!(*m.get(&i).unwrap(), i);
        }

        for i in 0..256 {
            m.set(i, i);
        }
        for i in 0..128 {
            assert!(m.get(&i).is_none());
        }
        for i in 128..256 {
            assert_eq!(*m.get(&i).unwrap(), i);
        }

        m.set(1000, 1000);
        assert!(m.get(&128).is_none());
        assert_eq!(*m.get(&129).unwrap(), 129);
        assert_eq!(*m.get(&1000).unwrap(), 1000);
    }
}
