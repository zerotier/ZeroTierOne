use std::mem::size_of;
use std::ops::{Deref, DerefMut};
use std::sync::{Arc, Weak};

use parking_lot::Mutex;

/// Trait for objects that can be used with Pool.
pub trait Reusable: Default + Sized {
    fn reset(&mut self);
}

struct PoolEntry<O: Reusable> {
    obj: O,
    return_pool: Weak<PoolInner<O>>,
}

type PoolInner<O> = Mutex<Vec<*mut PoolEntry<O>>>;

/// Container for pooled objects that have been checked out of the pool.
///
/// When this is dropped the object is returned to the pool or if the pool or is
/// dropped if the pool has been dropped. There is also an into_raw() and from_raw()
/// functionality that allows conversion to/from naked pointers to O for
/// interoperation with C/C++ APIs.
///
/// Note that pooled objects are not clonable. If you want to share them use Rc<>
/// or Arc<>.
#[repr(transparent)]
pub struct Pooled<O: Reusable>(*mut PoolEntry<O>);

impl<O: Reusable> Pooled<O> {
    /// Get a raw pointer to the object wrapped by this pooled object container.
    /// The returned raw pointer MUST be restored into a Pooled instance with
    /// from_raw() or memory will leak.
    #[inline(always)]
    pub unsafe fn into_raw(self) -> *mut O {
        debug_assert!(!self.0.is_null());
        debug_assert_eq!(self.0.cast::<u8>(), (&mut (*self.0).obj as *mut O).cast::<u8>());
        std::mem::forget(self);
        self.0.cast()
    }

    /// Restore a raw pointer from into_raw() into a Pooled object.
    /// The supplied pointer MUST have been obtained from a Pooled object or
    /// undefined behavior will occur. Pointers from other sources can't be used
    /// here. None is returned if the pointer is null.
    #[inline(always)]
    pub unsafe fn from_raw(raw: *mut O) -> Option<Self> {
        if !raw.is_null() {
            Some(Self(raw.cast()))
        } else {
            None
        }
    }
}

impl<O: Reusable> Deref for Pooled<O> {
    type Target = O;

    #[inline(always)]
    fn deref(&self) -> &Self::Target {
        debug_assert!(!self.0.is_null());
        unsafe { &(*self.0).obj }
    }
}

impl<O: Reusable> AsRef<O> for Pooled<O> {
    #[inline(always)]
    fn as_ref(&self) -> &O {
        debug_assert!(!self.0.is_null());
        unsafe { &(*self.0).obj }
    }
}

impl<O: Reusable> DerefMut for Pooled<O> {
    #[inline(always)]
    fn deref_mut(&mut self) -> &mut Self::Target {
        debug_assert!(!self.0.is_null());
        unsafe { &mut (*self.0).obj }
    }
}

impl<O: Reusable> AsMut<O> for Pooled<O> {
    #[inline(always)]
    fn as_mut(&mut self) -> &mut O {
        debug_assert!(!self.0.is_null());
        unsafe { &mut (*self.0).obj }
    }
}

impl<O: Reusable> Drop for Pooled<O> {
    fn drop(&mut self) {
        unsafe {
            Weak::upgrade(&(*self.0).return_pool).map_or_else(|| {
                drop(Box::from_raw(self.0))
            }, |p| {
                (*self.0).obj.reset();
                p.lock().push(self.0)
            })
        }
    }
}

/// An object pool for Reusable objects.
/// The pool is safe in that checked out objects return automatically when their Pooled
/// transparent container is dropped, or deallocate if the pool has been dropped.
pub struct Pool<O: Reusable>(Arc<PoolInner<O>>);

impl<O: Reusable> Pool<O> {
    pub fn new(initial_stack_capacity: usize) -> Self {
        Self(Arc::new(Mutex::new(Vec::with_capacity(initial_stack_capacity))))
    }

    /// Get a pooled object, or allocate one if the pool is empty.
    pub fn get(&self) -> Pooled<O> {
        Pooled::<O>(self.0.lock().pop().map_or_else(|| {
            Box::into_raw(Box::new(PoolEntry::<O> {
                obj: O::default(),
                return_pool: Arc::downgrade(&self.0),
            }))
        }, |obj| {
            debug_assert!(!obj.is_null());
            obj
        }))
    }

    /// Get approximate memory use in bytes (does not include checked out objects).
    #[inline(always)]
    pub fn pool_memory_bytes(&self) -> usize {
        self.0.lock().len() * (size_of::<PoolEntry<O>>() + size_of::<usize>())
    }

    /// Dispose of all pooled objects, freeing any memory they use.
    /// If get() is called after this new objects will be allocated, and any outstanding
    /// objects will still be returned on drop unless the pool itself is dropped. This can
    /// be done to free some memory if there has been a spike in memory use.
    pub fn purge(&self) {
        let mut p = self.0.lock();
        for obj in p.iter() {
            drop(unsafe { Box::from_raw(*obj) });
        }
        p.clear();
    }
}

impl<O: Reusable> Drop for Pool<O> {
    fn drop(&mut self) {
        self.purge();
    }
}

unsafe impl<O: Reusable> Sync for Pool<O> {}

unsafe impl<O: Reusable> Send for Pool<O> {}

#[cfg(test)]
mod tests {
    use std::sync::atomic::{AtomicUsize, Ordering};
    use std::ops::DerefMut;
    use std::time::Duration;

    use crate::util::pool::{Reusable, Pool};
    use std::sync::Arc;

    struct ReusableTestObject(usize);

    impl Default for ReusableTestObject {
        fn default() -> Self {
            Self(0)
        }
    }

    impl Reusable for ReusableTestObject {
        fn reset(&mut self) {}
    }

    #[test]
    fn threaded_pool_use() {
        let p: Arc<Pool<ReusableTestObject>> = Arc::new(Pool::new(2));
        let ctr = Arc::new(AtomicUsize::new(0));
        for _ in 0..64 {
            let p2 = p.clone();
            let ctr2 = ctr.clone();
            let _ = std::thread::spawn(move || {
                for _ in 0..16384 {
                    let mut o1 = p2.get();
                    o1.deref_mut().0 += 1;
                    let mut o2 = p2.get();
                    drop(o1);
                    o2.deref_mut().0 += 1;
                    ctr2.fetch_add(1, Ordering::Relaxed);
                }
            });
        }
        loop {
            std::thread::sleep(Duration::from_millis(100));
            if ctr.load(Ordering::Relaxed) >= 16384 * 64 {
                break;
            }
        }
        //println!("pool memory size: {}", p.pool_memory_bytes());
    }
}
