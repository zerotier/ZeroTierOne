use std::collections::Bound::Included;
use std::collections::BTreeMap;
use std::io::Write;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Mutex;

use smol::net::SocketAddr;

use zerotier_core_crypto::random::xorshift64_random;

use crate::{IDENTITY_HASH_SIZE, ms_since_epoch, Store, StorePutResult};

/// A Store that stores all objects in memory, mostly for testing.
pub struct MemoryStore(Mutex<BTreeMap<[u8; IDENTITY_HASH_SIZE], Vec<u8>>>, Mutex<Vec<SocketAddr>>, AtomicU64);

impl MemoryStore {
    pub fn new() -> Self { Self(Mutex::new(BTreeMap::new()), Mutex::new(Vec::new()), AtomicU64::new(u64::MAX)) }
}

impl Store for MemoryStore {
    fn get(&self, _reference_time: u64, identity_hash: &[u8; IDENTITY_HASH_SIZE], buffer: &mut Vec<u8>) -> bool {
        buffer.clear();
        self.0.lock().unwrap().get(identity_hash).map_or(false, |value| {
            let _ = buffer.write_all(value.as_slice());
            true
        })
    }

    fn put(&self, _reference_time: u64, identity_hash: &[u8; IDENTITY_HASH_SIZE], object: &[u8]) -> StorePutResult {
        let mut result = StorePutResult::Duplicate;
        let _ = self.0.lock().unwrap().entry(identity_hash.clone()).or_insert_with(|| {
            self.2.store(ms_since_epoch(), Ordering::Relaxed);
            result = StorePutResult::Ok;
            object.to_vec()
        });
        result
    }

    fn have(&self, _reference_time: u64, identity_hash: &[u8; IDENTITY_HASH_SIZE]) -> bool {
        self.0.lock().unwrap().contains_key(identity_hash)
    }

    fn total_count(&self, _reference_time: u64) -> Option<u64> {
        Some(self.0.lock().unwrap().len() as u64)
    }

    fn last_object_receive_time(&self) -> Option<u64> {
        let rt = self.2.load(Ordering::Relaxed);
        if rt == u64::MAX {
            None
        } else {
            Some(rt)
        }
    }

    fn count(&self, _reference_time: u64, start: &[u8; IDENTITY_HASH_SIZE], end: &[u8; IDENTITY_HASH_SIZE]) -> Option<u64> {
        if start.le(end) {
            Some(self.0.lock().unwrap().range((Included(*start), Included(*end))).count() as u64)
        } else {
            None
        }
    }

    fn save_remote_endpoint(&self, to_address: &SocketAddr) {
        let mut sv = self.1.lock().unwrap();
        if !sv.contains(to_address) {
            sv.push(to_address.clone());
        }
    }

    fn get_remote_endpoint(&self) -> Option<SocketAddr> {
        let sv = self.1.lock().unwrap();
        if sv.is_empty() {
            None
        } else {
            sv.get((xorshift64_random() as usize) % sv.len()).cloned()
        }
    }
}
