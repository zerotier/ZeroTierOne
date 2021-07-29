// This just defines a ConcurrentMap type, selecting standard locked HashMap for smaller systems
// or DashMap on larger ones where it would be faster. It also defines some wrappers for read
// and write locking that do nothing for DashMap and return a lock guard for RwLock<HashMap<>>.

#[allow(unused_imports)]
use std::sync::{Arc, RwLock, RwLockReadGuard, RwLockWriteGuard};
#[allow(unused_imports)]
use std::collections::HashMap;
#[cfg(any(target_arch = "aarch64", target_arch = "x86_64", target_arch = "powerpc64"))]
use dashmap::DashMap;

#[cfg(not(any(target_arch = "aarch64", target_arch = "x86_64", target_arch = "powerpc64")))]
pub type ConcurrentMap<K, V> = RwLock<HashMap<K, V>>;

#[cfg(any(target_arch = "aarch64", target_arch = "x86_64", target_arch = "powerpc64"))]
pub type ConcurrentMap<K, V> = DashMap<K, V>;

/// Wrapper to get a read lock guard on a concurrent map.
#[cfg(not(any(target_arch = "aarch64", target_arch = "x86_64", target_arch = "powerpc64")))]
#[inline(always)]
pub fn read<K, V>(m: &Arc<ConcurrentMap<K, V>>) -> RwLockReadGuard<HashMap<K, V>> {
    m.read().unwrap()
}

/// Wrapper to get a read lock guard on a concurrent map.
#[cfg(any(target_arch = "aarch64", target_arch = "x86_64", target_arch = "powerpc64"))]
#[inline(always)]
pub fn read<K, V>(m: &Arc<ConcurrentMap<K, V>>) -> &ConcurrentMap<K, V> {
    m.as_ref()
}

/// Wrapper to get a write lock guard on a concurrent map.
#[cfg(not(any(target_arch = "aarch64", target_arch = "x86_64", target_arch = "powerpc64")))]
#[inline(always)]
pub fn write<K, V>(m: &Arc<ConcurrentMap<K, V>>) -> RwLockWriteGuard<HashMap<K, V>> {
    m.write().unwrap()
}

/// Wrapper to get a write lock guard on a concurrent map.
#[cfg(any(target_arch = "aarch64", target_arch = "x86_64", target_arch = "powerpc64"))]
#[inline(always)]
pub fn write<K, V>(m: &Arc<ConcurrentMap<K, V>>) -> &ConcurrentMap<K, V> {
    m.as_ref()
}
