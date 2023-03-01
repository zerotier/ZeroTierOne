use std::borrow::Borrow;
use std::collections::{BTreeMap, HashMap};
use std::hash::Hash;

/// A store of keys that can be retrieved by key id.
pub trait Store {
    type Algorithm: ?Sized;

    fn get(&self, key_id: &str) -> Option<&Self::Algorithm>;
}

impl<K, A> Store for BTreeMap<K, A>
where
    K: Borrow<str> + Ord,
{
    type Algorithm = A;

    fn get(&self, key_id: &str) -> Option<&A> {
        BTreeMap::get(self, key_id)
    }
}

impl<K, A> Store for HashMap<K, A>
where
    K: Borrow<str> + Ord + Hash,
{
    type Algorithm = A;

    fn get(&self, key_id: &str) -> Option<&A> {
        HashMap::get(self, key_id)
    }
}
