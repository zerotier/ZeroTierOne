/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::collections::Bound::Included;
use std::collections::BTreeMap;
use std::sync::Arc;

use parking_lot::Mutex;

use crate::{IDENTITY_HASH_SIZE, ms_monotonic, ms_since_epoch, Store, StorePutResult};

/// A Store that stores all objects in memory, mostly for testing.
pub struct MemoryStore(Mutex<BTreeMap<[u8; IDENTITY_HASH_SIZE], (Arc<[u8]>, u64)>>);

impl MemoryStore {
    pub fn new() -> Self { Self(Mutex::new(BTreeMap::new())) }
}

impl Store for MemoryStore {
    type Object = Arc<[u8]>;

    #[inline(always)]
    fn clock(&self) -> u64 { ms_since_epoch() }

    #[inline(always)]
    fn monotonic_clock(&self) -> u64 { ms_monotonic() }

    fn get(&self, reference_time: u64, identity_hash: &[u8; IDENTITY_HASH_SIZE]) -> Option<Self::Object> {
        self.0.lock().get(identity_hash).and_then(|o| {
            if (*o).1 <= reference_time {
                Some((*o).0.clone())
            } else {
                None
            }
        })
    }

    fn put(&self, identity_hash: &[u8; IDENTITY_HASH_SIZE], object: &[u8]) -> StorePutResult {
        let mut result = StorePutResult::Duplicate;
        let _ = self.0.lock().entry(identity_hash.clone()).or_insert_with(|| {
            result = StorePutResult::Ok;
            (object.to_vec().into(), ms_since_epoch())
        });
        result
    }

    fn have(&self, identity_hash: &[u8; IDENTITY_HASH_SIZE]) -> bool {
        self.0.lock().contains_key(identity_hash)
    }

    fn total_count(&self, reference_time: u64) -> Option<u64> {
        let mut tc = 0_u64;
        for e in self.0.lock().iter() {
            tc += ((*e.1).1 <= reference_time) as u64;
        }
        Some(tc)
    }

    fn for_each<F: FnMut(&[u8], &Arc<[u8]>) -> bool>(&self, reference_time: u64, start: &[u8; IDENTITY_HASH_SIZE], end: &[u8; IDENTITY_HASH_SIZE], mut f: F) {
        let mut tmp: Vec<([u8; IDENTITY_HASH_SIZE], Arc<[u8]>)> = Vec::with_capacity(1024);
        for e in self.0.lock().range((Included(*start), Included(*end))).into_iter() {
            if (*e.1).1 <= reference_time {
                tmp.push((e.0.clone(), (*e.1).0.clone()));
            }
        }
        for e in tmp.iter() {
            if !f(&(*e).0, &(*e).1) {
                break;
            }
        }
    }

    fn for_each_identity_hash<F: FnMut(&[u8]) -> bool>(&self, reference_time: u64, start: &[u8; IDENTITY_HASH_SIZE], end: &[u8; IDENTITY_HASH_SIZE], mut f: F) {
        let mut tmp: Vec<[u8; IDENTITY_HASH_SIZE]> = Vec::with_capacity(1024);
        for e in self.0.lock().range((Included(*start), Included(*end))).into_iter() {
            if (*e.1).1 <= reference_time {
                tmp.push(e.0.clone());
            }
        }
        for e in tmp.iter() {
            if !f(e) {
                break;
            }
        }
    }

    fn count(&self, reference_time: u64, start: &[u8; IDENTITY_HASH_SIZE], end: &[u8; IDENTITY_HASH_SIZE]) -> Option<u64> {
        let mut tc = 0_u64;
        for e in self.0.lock().range((Included(*start), Included(*end))).into_iter() {
            tc += ((*e.1).1 <= reference_time) as u64;
        }
        Some(tc)
    }
}
