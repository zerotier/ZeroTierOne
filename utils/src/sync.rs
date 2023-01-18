/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::sync::{RwLock, RwLockReadGuard, RwLockWriteGuard};

/// Variant version of lock for RwLock with automatic conversion to a write lock as needed.
pub enum RMaybeWLockGuard<'a, T> {
    R(Option<RwLockReadGuard<'a, T>>),
    W(RwLockWriteGuard<'a, T>),
}

impl<'a, T> RMaybeWLockGuard<'a, T> {
    #[inline(always)]
    pub fn new_read(l: &'a RwLock<T>) -> Self {
        Self::R(Some(l.read().unwrap()))
    }

    /// Get a readable reference to the object.
    #[inline]
    pub fn read(&self) -> &T {
        match self {
            Self::R(r) => &*(r.as_ref().unwrap()),
            Self::W(w) => &*w,
        }
    }

    /// Get a writable reference to the object, converting this to a write lock if needed.
    #[inline]
    pub fn write(&mut self, l: &'a RwLock<T>) -> &mut T {
        match self {
            Self::R(r) => {
                let _ = r.take();
                *self = Self::W(l.write().unwrap());
                match self {
                    Self::W(w) => &mut *w,
                    _ => panic!(),
                }
            }
            Self::W(w) => &mut *w,
        }
    }
}
