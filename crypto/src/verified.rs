// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::ops::{Deref, DerefMut};

/// A zero-overhead wrapper that signals that a credential is verified.
///
/// This is used when a function expects to receive an object that is already verified to
/// make code more self-documenting and make it semantically harder to accidentally use
/// an untrusted object.
#[derive(Clone, PartialEq, Eq, PartialOrd, Ord)]
#[repr(transparent)]
pub struct Verified<T>(pub T);

impl<T> Deref for Verified<T> {
    type Target = T;

    #[inline(always)]
    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl<T> DerefMut for Verified<T> {
    #[inline(always)]
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

impl<T> Verified<T> {
    #[inline(always)]
    pub fn unwrap(self) -> T {
        self.0
    }
}
