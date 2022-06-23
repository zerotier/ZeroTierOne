// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::sync::atomic::{AtomicUsize, Ordering};

use lazy_static::lazy_static;

lazy_static! {
    static ref CANONICAL_ID_COUNTER: AtomicUsize = AtomicUsize::new(0);
}

/// An object that implements equality such that each instance is globally unique in a runtime/process.
///
/// This is used to make canonicalized objects like Path and Peer implement eq() accordingly. A unique
/// ID assigned internally from a counter is used instead of the object's location in memory because
/// technically objects can move in Rust. Canonical objects are encased in Arc<> and unlikely to do so,
/// but this is "correct."
#[repr(transparent)]
pub struct CanonicalObject(usize);

impl CanonicalObject {
    #[inline(always)]
    pub fn new() -> Self {
        Self(CANONICAL_ID_COUNTER.fetch_add(1, Ordering::SeqCst))
    }

    #[inline(always)]
    pub fn canonical_instance_id(&self) -> usize {
        self.0
    }
}

impl PartialEq for CanonicalObject {
    #[inline(always)]
    fn eq(&self, other: &Self) -> bool {
        self.0 == other.0
    }
}

impl Eq for CanonicalObject {}
