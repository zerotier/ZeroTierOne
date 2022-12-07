// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::any::TypeId;
use std::mem::{forget, size_of, MaybeUninit};
use std::ptr::{drop_in_place, read, write};

/// A statically sized container that acts like Box<dyn Any> but without heap overhead.
///
/// This is used in a couple places to avoid what would otherwise be an explosion of cascading
/// template parameters to support externally defined small objects. It does so with virtually
/// no overhead, unlike Box<>, and could be no_std compatible.
///
/// This will panic if the capacity is too small. If that occurs, it must be enlarged. It will
/// also panic if any of the accessors (other than the try_ versions) are used to try to get
/// a type other than the one it was constructed with.
#[repr(C)]
pub struct Thing<const CAPACITY: usize> {
    storage: [u8; CAPACITY],
    dropper: fn(*mut u8),
    data_type: TypeId,
}

impl<const CAPACITY: usize> Thing<CAPACITY> {
    #[inline(always)]
    pub fn new<T: Sized + 'static>(x: T) -> Self {
        assert!(size_of::<T>() <= CAPACITY);
        let mut p = Self {
            storage: unsafe { MaybeUninit::uninit().assume_init() },
            dropper: |s: *mut u8| unsafe {
                drop_in_place::<T>((*s.cast::<Self>()).storage.as_mut_ptr().cast());
            },
            data_type: TypeId::of::<T>(),
        };
        unsafe { write(p.storage.as_mut_ptr().cast(), x) };
        p
    }

    #[inline(always)]
    pub fn get<T: Sized + 'static>(&self) -> &T {
        assert_eq!(TypeId::of::<T>(), self.data_type);
        unsafe { &*self.storage.as_ptr().cast() }
    }

    #[inline(always)]
    pub fn get_mut<T: Sized + 'static>(&mut self) -> &mut T {
        assert_eq!(TypeId::of::<T>(), self.data_type);
        unsafe { &mut *self.storage.as_mut_ptr().cast() }
    }

    #[inline(always)]
    pub fn try_get<T: Sized + 'static>(&self) -> Option<&T> {
        if TypeId::of::<T>() == self.data_type {
            Some(unsafe { &*self.storage.as_ptr().cast() })
        } else {
            None
        }
    }

    #[inline(always)]
    pub fn try_get_mut<T: Sized + 'static>(&mut self) -> Option<&mut T> {
        if TypeId::of::<T>() == self.data_type {
            Some(unsafe { &mut *self.storage.as_mut_ptr().cast() })
        } else {
            None
        }
    }

    #[inline(always)]
    pub fn unwrap<T: Sized + 'static>(self) -> T {
        assert_eq!(TypeId::of::<T>(), self.data_type);
        let x = unsafe { read(self.storage.as_ptr().cast()) };
        forget(self);
        x
    }
}

impl<T: Sized + 'static, const CAPACITY: usize> AsRef<T> for Thing<CAPACITY> {
    #[inline(always)]
    fn as_ref(&self) -> &T {
        assert_eq!(TypeId::of::<T>(), self.data_type);
        unsafe { &*self.storage.as_ptr().cast() }
    }
}

impl<T: Sized + 'static, const CAPACITY: usize> AsMut<T> for Thing<CAPACITY> {
    #[inline(always)]
    fn as_mut(&mut self) -> &mut T {
        assert_eq!(TypeId::of::<T>(), self.data_type);
        unsafe { &mut *self.storage.as_mut_ptr().cast() }
    }
}

impl<const CAPACITY: usize> Drop for Thing<CAPACITY> {
    #[inline(always)]
    fn drop(&mut self) {
        (self.dropper)((self as *mut Self).cast());
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::rc::Rc;

    #[test]
    fn typing_and_life_cycle() {
        let test_obj = Rc::new(1i32);
        assert_eq!(Rc::strong_count(&test_obj), 1);
        let a = Thing::<32>::new(test_obj.clone());
        let b = Thing::<32>::new(test_obj.clone());
        let c = Thing::<32>::new(test_obj.clone());
        assert!(a.get::<Rc<i32>>().eq(b.get()));
        assert!(a.try_get::<Rc<i32>>().is_some());
        assert!(a.try_get::<Rc<usize>>().is_none());
        assert_eq!(Rc::strong_count(&test_obj), 4);
        drop(a);
        assert_eq!(Rc::strong_count(&test_obj), 3);
        drop(b);
        assert_eq!(Rc::strong_count(&test_obj), 2);
        let c = c.unwrap::<Rc<i32>>();
        assert_eq!(Rc::strong_count(&test_obj), 2);
        drop(c);
        assert_eq!(Rc::strong_count(&test_obj), 1);
    }
}
