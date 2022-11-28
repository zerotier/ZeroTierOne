// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::fmt::Debug;
use std::hash::Hash;
use std::ops::{Deref, DerefMut};

/// A zero-overhead typestate indicating that a credential has been verified as valid.
///
/// What this means is obviously specific to the credential.
///
/// The purpose of this is to make code more self-documenting and make it harder to accidentally
/// use an unverified/unvalidated credential (or other security critical object) where a verified
/// one is required.
#[repr(transparent)]
pub struct Verified<T>(T);

impl<T> AsRef<T> for Verified<T> {
    #[inline(always)]
    fn as_ref(&self) -> &T {
        &self.0
    }
}

impl<T> AsMut<T> for Verified<T> {
    #[inline(always)]
    fn as_mut(&mut self) -> &mut T {
        &mut self.0
    }
}

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

impl<T> Clone for Verified<T>
where
    T: Clone,
{
    #[inline(always)]
    fn clone(&self) -> Self {
        Self(self.0.clone())
    }
}

impl<T> PartialEq for Verified<T>
where
    T: PartialEq,
{
    #[inline(always)]
    fn eq(&self, other: &Self) -> bool {
        self.0.eq(&other.0)
    }
}

impl<T> Eq for Verified<T> where T: Eq {}

impl<T> Ord for Verified<T>
where
    T: Ord,
{
    #[inline(always)]
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.0.cmp(&other.0)
    }
}

impl<T> PartialOrd for Verified<T>
where
    T: PartialOrd,
{
    #[inline(always)]
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        self.0.partial_cmp(&other.0)
    }
}

impl<T> Hash for Verified<T>
where
    T: Hash,
{
    #[inline(always)]
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.0.hash(state);
    }
}

impl<T> Debug for Verified<T>
where
    T: Debug,
{
    #[inline]
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_tuple("Verified").field(&self.0).finish()
    }
}

impl<T> Verified<T> {
    /// Strip the Verified typestate off this object.
    #[inline(always)]
    pub fn unwrap(self) -> T {
        self.0
    }

    /// Set Verified typestate on an object.
    #[inline(always)]
    pub fn assume_verified(o: T) -> Self {
        Self(o)
    }
}
