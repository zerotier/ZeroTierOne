// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::ops::{Deref, DerefMut};
use std::ptr::NonNull;
use std::sync::{Arc, Mutex, Weak};

/// Each pool requires a factory that creates and resets (for re-use) pooled objects.
pub trait PoolFactory<O> {
    fn create(&self) -> O;
    fn reset(&self, obj: &mut O);
}

/// Container for pooled objects that have been checked out of the pool.
///
/// Objects are automagically returned to the pool when Pooled<> is dropped if the pool still exists.
/// If the pool itself is gone objects are freed. Two methods for conversion to/from raw pointers are
/// available for interoperation with foreign APIs.
#[repr(transparent)]
pub struct Pooled<O, F: PoolFactory<O>>(NonNull<PoolEntry<O, F>>);

#[repr(C)]
struct PoolEntry<O, F: PoolFactory<O>> {
    obj: O, // must be first
    return_pool: Weak<PoolInner<O, F>>,
}

impl<O, F: PoolFactory<O>> Pooled<O, F> {
    /// Create a pooled object wrapper around an object but with no pool to return it to.
    /// The object will be freed when this pooled container is dropped.
    #[inline]
    pub fn naked(o: O) -> Self {
        unsafe {
            Self(NonNull::new_unchecked(Box::into_raw(Box::new(PoolEntry::<O, F> {
                obj: o,
                return_pool: Weak::new(),
            }))))
        }
    }

    /// Get a raw pointer to the object wrapped by this pooled object container.
    ///
    /// The returned pointer MUST be returned to the pooling system with from_raw() or memory
    /// will leak.
    #[inline]
    pub unsafe fn into_raw(self) -> *mut O {
        // Verify that the structure is not padded before 'obj'.
        assert_eq!(
            (&self.0.as_ref().obj as *const O).cast::<u8>(),
            (self.0.as_ref() as *const PoolEntry<O, F>).cast::<u8>()
        );

        let ptr = self.0.as_ptr().cast::<O>();
        std::mem::forget(self);
        ptr
    }

    /// Restore a raw pointer from into_raw() into a Pooled object.
    ///
    /// The supplied pointer MUST have been obtained from a Pooled object. None is returned
    /// if the pointer is null.
    #[inline]
    pub unsafe fn from_raw(raw: *mut O) -> Option<Self> {
        if !raw.is_null() {
            Some(Self(NonNull::new_unchecked(raw.cast())))
        } else {
            None
        }
    }
}

impl<O, F: PoolFactory<O>> Clone for Pooled<O, F>
where
    O: Clone,
{
    #[inline]
    fn clone(&self) -> Self {
        let internal = unsafe { &mut *self.0.as_ptr() };
        if let Some(p) = internal.return_pool.upgrade() {
            if let Some(o) = p.pool.lock().unwrap().pop() {
                let mut o = Self(o);
                *o.as_mut() = self.as_ref().clone();
                o
            } else {
                Pooled::<O, F>(unsafe {
                    NonNull::new_unchecked(Box::into_raw(Box::new(PoolEntry::<O, F> {
                        obj: self.as_ref().clone(),
                        return_pool: Arc::downgrade(&p),
                    })))
                })
            }
        } else {
            Self::naked(self.as_ref().clone())
        }
    }
}

unsafe impl<O, F: PoolFactory<O>> Send for Pooled<O, F> where O: Send {}
unsafe impl<O, F: PoolFactory<O>> Sync for Pooled<O, F> where O: Sync {}

impl<O, F: PoolFactory<O>> Deref for Pooled<O, F> {
    type Target = O;

    #[inline(always)]
    fn deref(&self) -> &Self::Target {
        unsafe { &self.0.as_ref().obj }
    }
}

impl<O, F: PoolFactory<O>> DerefMut for Pooled<O, F> {
    #[inline(always)]
    fn deref_mut(&mut self) -> &mut Self::Target {
        unsafe { &mut self.0.as_mut().obj }
    }
}

impl<O, F: PoolFactory<O>> AsRef<O> for Pooled<O, F> {
    #[inline(always)]
    fn as_ref(&self) -> &O {
        unsafe { &self.0.as_ref().obj }
    }
}

impl<O, F: PoolFactory<O>> AsMut<O> for Pooled<O, F> {
    #[inline(always)]
    fn as_mut(&mut self) -> &mut O {
        unsafe { &mut self.0.as_mut().obj }
    }
}

impl<O, F: PoolFactory<O>> Drop for Pooled<O, F> {
    #[inline]
    fn drop(&mut self) {
        let internal = unsafe { &mut *self.0.as_ptr() };
        if let Some(p) = internal.return_pool.upgrade() {
            p.factory.reset(&mut internal.obj);
            p.pool.lock().unwrap().push(self.0);
        } else {
            drop(unsafe { Box::from_raw(self.0.as_ptr()) });
        }
    }
}

/// An object pool for Reusable objects.
/// Checked out objects are held by a guard object that returns them when dropped if
/// the pool still exists or drops them if the pool has itself been dropped.
pub struct Pool<O, F: PoolFactory<O>>(Arc<PoolInner<O, F>>);

struct PoolInner<O, F: PoolFactory<O>> {
    factory: F,
    pool: Mutex<Vec<NonNull<PoolEntry<O, F>>>>,
}

impl<O, F: PoolFactory<O>> Pool<O, F> {
    #[inline]
    pub fn new(initial_stack_capacity: usize, factory: F) -> Self {
        Self(Arc::new(PoolInner::<O, F> {
            factory,
            pool: Mutex::new(Vec::with_capacity(initial_stack_capacity)),
        }))
    }

    /// Get a pooled object, or allocate one if the pool is empty.
    #[inline]
    pub fn get(&self) -> Pooled<O, F> {
        if let Some(o) = self.0.pool.lock().unwrap().pop() {
            return Pooled::<O, F>(o);
        }
        return Pooled::<O, F>(unsafe {
            NonNull::new_unchecked(Box::into_raw(Box::new(PoolEntry::<O, F> {
                obj: self.0.factory.create(),
                return_pool: Arc::downgrade(&self.0),
            })))
        });
    }

    /// Dispose of all pooled objects, freeing any memory they use.
    ///
    /// If get() is called after this new objects will be allocated, and any outstanding
    /// objects will still be returned on drop unless the pool itself is dropped. This can
    /// be done to free some memory if there has been a spike in memory use.
    #[inline]
    pub fn purge(&self) {
        for o in self.0.pool.lock().unwrap().drain(..) {
            drop(unsafe { Box::from_raw(o.as_ptr()) })
        }
    }
}

impl<O, F: PoolFactory<O>> Drop for Pool<O, F> {
    #[inline(always)]
    fn drop(&mut self) {
        self.purge();
    }
}

unsafe impl<O: Send, F: PoolFactory<O>> Send for Pool<O, F> {}
unsafe impl<O: Send, F: PoolFactory<O>> Sync for Pool<O, F> {}

#[cfg(test)]
mod tests {
    use std::sync::atomic::{AtomicUsize, Ordering};
    use std::sync::Arc;
    use std::time::Duration;

    use super::*;

    struct TestPoolFactory;

    impl PoolFactory<String> for TestPoolFactory {
        fn create(&self) -> String {
            String::new()
        }

        fn reset(&self, obj: &mut String) {
            obj.clear();
        }
    }

    #[test]
    fn threaded_pool_use() {
        let p: Arc<Pool<String, TestPoolFactory>> = Arc::new(Pool::new(2, TestPoolFactory {}));
        let ctr = Arc::new(AtomicUsize::new(0));
        for _ in 0..64 {
            let p2 = p.clone();
            let ctr2 = ctr.clone();
            let _ = std::thread::spawn(move || {
                for _ in 0..16384 {
                    let mut o1 = p2.get();
                    o1.push('a');
                    let o2 = p2.get();
                    drop(o1);
                    let mut o2 = unsafe { Pooled::<String, TestPoolFactory>::from_raw(o2.into_raw()).unwrap() };
                    o2.push('b');
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
    }
}
