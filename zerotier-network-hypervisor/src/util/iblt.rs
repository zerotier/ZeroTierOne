/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::io::{Read, Write};
use std::mem::{size_of, transmute, zeroed};
use std::ptr::write_bytes;

use zerotier_core_crypto::varint;

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
fn calc_check_hash<const HS: usize>(key: &[u8]) -> u64 {
    xorshift64(u64::from_le_bytes((&key[(HS - 8)..HS]).try_into().unwrap()))
}

#[derive(Clone, PartialEq, Eq)]
struct IBLTEntry<const HS: usize> {
    key_sum: [u8; HS],
    check_hash_sum: u64,
    count: i64
}

impl<const HS: usize> IBLTEntry<HS> {
    #[inline(always)]
    fn is_singular(&self) -> bool {
        if self.count == 1 || self.count == -1 {
            calc_check_hash::<HS>(&self.key_sum) == self.check_hash_sum
        } else {
            false
        }
    }
}

/// An Invertible Bloom Lookup Table for set reconciliation.
///
/// This implementation assumes that hashes are random. Hashes must be
/// at least 8 bytes in size.
#[derive(Clone, PartialEq, Eq)]
pub struct IBLT<const HS: usize, const B: usize> {
    map: [IBLTEntry<HS>; B]
}

impl<const HS: usize, const B: usize> IBLT<HS, B> {
    pub const BUCKETS: usize = B;

    pub const HASH_SIZE: usize = HS;

    pub fn new() -> Self { unsafe { zeroed() } }

    #[inline(always)]
    pub fn clear(&mut self) {
        unsafe { write_bytes((self as *mut Self).cast::<u8>(), 0, size_of::<Self>()) };
    }

    pub fn read<R: Read>(&mut self, r: &mut R) -> std::io::Result<()> {
        let mut tmp = [0_u8; 8];
        let mut prev_c = 0_i64;
        for b in self.map.iter_mut() {
            r.read_exact(&mut b.key_sum)?;
            r.read_exact(&mut tmp)?;
            b.check_hash_sum = u64::from_le_bytes(tmp);
            let mut c = varint::read(r)?.0 as i64;
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

    pub fn write<W: Write>(&self, w: &mut W) -> std::io::Result<()> {
        let mut prev_c = 0_i64;
        for b in self.map.iter() {
            w.write_all(&b.key_sum)?;
            w.write_all(&b.check_hash_sum.to_le_bytes())?;
            let mut c = (b.count - prev_c).wrapping_shl(1);
            prev_c = b.count;
            if c < 0 {
                c = -c | 1;
            }
            varint::write(w, c as u64)?;
        }
        Ok(())
    }

    fn ins_rem(&mut self, key: &[u8], delta: i64) {
        assert!(HS >= 8);
        assert!(key.len() >= HS);
        let iteration_indices: [u64; 8] = unsafe { transmute(zerotier_core_crypto::hash::SHA512::hash(key)) };
        let check_hash = calc_check_hash::<HS>(&key);
        for i in 0..KEY_MAPPING_ITERATIONS {
            let b = unsafe { self.map.get_unchecked_mut((u64::from_le(iteration_indices[i]) as usize) % B) };
            for x in 0..HS {
                b.key_sum[x] ^= key[x];
            }
            b.check_hash_sum ^= check_hash;
            b.count += delta;
        }
    }

    #[inline(always)]
    pub fn insert(&mut self, key: &[u8]) { self.ins_rem(key, 1); }

    #[inline(always)]
    pub fn remove(&mut self, key: &[u8]) { self.ins_rem(key, -1); }

    pub fn subtract(&mut self, other: &Self) {
        for b in 0..B {
            let s = &mut self.map[b];
            let o = &other.map[b];
            for x in 0..HS {
                s.key_sum[x] ^= o.key_sum[x];
            }
            s.check_hash_sum ^= o.check_hash_sum;
            s.count += o.count;
        }
    }

    pub fn list<F: FnMut(&[u8; HS])>(mut self, mut f: F) -> bool {
        assert!(HS >= 8);
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
                let key = b.key_sum.clone();
                let iteration_indices: [u64; 8] = unsafe { transmute(zerotier_core_crypto::hash::SHA512::hash(&key)) };
                let check_hash = calc_check_hash::<HS>(&key);

                f(&key);

                for i in 0..KEY_MAPPING_ITERATIONS {
                    let b_idx = (u64::from_le(iteration_indices[i]) as usize) % B;
                    let b = &mut self.map[b_idx];
                    for x in 0..HS {
                        b.key_sum[x] ^= key[x];
                    }
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

    use zerotier_core_crypto::hash::SHA384;

    use crate::util::iblt::*;

    #[allow(unused_variables)]
    #[test]
    fn insert_and_list() {
        let mut e: HashSet<[u8; 48]> = HashSet::with_capacity(1024);
        for _ in 0..2 {
            for expected_cnt in 0..768 {
                let random_u64 = zerotier_core_crypto::random::xorshift64_random();
                let mut t: IBLT<48, 1152> = IBLT::new();
                e.clear();
                for i in 0..expected_cnt {
                    let k = SHA384::hash(&((i + random_u64) as u64).to_le_bytes());
                    t.insert(&k);
                    e.insert(k);
                }

                let t_backup = t.clone();
                assert!(t == t_backup);

                let mut cnt = 0;
                t.list(|k| {
                    assert!(e.contains(k));
                    cnt += 1;
                });
                assert_eq!(cnt, expected_cnt);

                let mut test_buf: Vec<u8> = Vec::new();
                assert!(t_backup.write(&mut test_buf).is_ok());
                let mut t_restore: IBLT<48, 1152> = IBLT::new();
                let mut test_read = test_buf.as_slice();
                assert!(t_restore.read(&mut test_read).is_ok());

                assert!(t_restore == t_backup);

                cnt = 0;
                t_restore.list(|k| {
                    assert!(e.contains(k));
                    cnt += 1;
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
            let mut alice: IBLT<48, 2048> = IBLT::new();
            let mut bob: IBLT<48, 2048> = IBLT::new();
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
            });
            // This is a probabilistic process so we tolerate a little bit of failure. The idea is that each
            // pass reconciles more and more differences.
            assert!(((alice_total + bob_total) - diff_total).abs() <= 128);
        }
    }
}
