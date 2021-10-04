use std::collections::BTreeMap;
use std::ops::Bound::Included;
use std::sync::Arc;

use parking_lot::Mutex;

/// Trait to be implemented by any data store to be replicated.
pub trait Store {
    fn load(&self, key: &[u8]) -> Option<Arc<[u8]>>;
    fn store(&self, key: &[u8], value: &[u8]) -> bool;
    fn for_each_range<F: FnMut(&[u8], &Arc<[u8]>)>(&self, starting_key: &[u8], ending_key: &[u8], f: F);
    fn count(&self, starting_key: &[u8], ending_key: &[u8]) -> Option<u64>;
}

/// A simple BTreeMap backed Store, mostly for testing as it does not persist anything.
#[derive(Clone)]
pub struct BTreeStore(Mutex<BTreeMap<[u8], Arc<[u8]>>>);

impl BTreeStore {
    pub fn new() -> Self { Self(Mutex::new(BTreeMap::new())) }
}

impl Store for BTreeStore {
    fn load(&self, key: &[u8]) -> Option<Arc<[u8]>> {
        let db = self.0.lock();
        db.get(key).map(|v| v.clone())
    }

    fn store(&self, key: &[u8], value: &[u8]) -> bool {
        let mut db = self.0.lock();
        let _ = db.insert(*key, Arc::from(value));
        true
    }

    fn for_each_range<F: FnMut((&[u8], &Arc<[u8]>))>(&self, starting_key: &[u8], ending_key: &[u8], f: F) {
        let db = self.0.lock();
        db.range((Included(starting_key), Included(ending_key))).for_each(f)
    }

    fn count(&self, starting_key: &[u8], ending_key: &[u8]) -> Option<u64> {
        let db = self.0.lock();
        Some(db.range((Included(starting_key), Included(ending_key))).count() as u64)
    }
}
