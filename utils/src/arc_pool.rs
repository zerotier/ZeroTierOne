use std::fmt::{Debug, Display};
use std::marker::PhantomData;
use std::mem::{self, ManuallyDrop, MaybeUninit};
use std::num::NonZeroU64;
use std::ops::Deref;
use std::ptr::{self, NonNull};
use std::sync::{
    atomic::{AtomicPtr, AtomicU32, Ordering},
    Mutex, RwLock, RwLockReadGuard,
};

const DEFAULT_L: usize = 64;

union SlotState<T> {
    empty_next: *mut Slot<T>,
    full_obj: ManuallyDrop<T>,
}
struct Slot<T> {
    obj: SlotState<T>,
    free_lock: RwLock<()>,
    ref_count: AtomicU32,
    uid: u64,
}

struct PoolMem<T, const L: usize = DEFAULT_L> {
    mem: [MaybeUninit<Slot<T>>; L],
    pre: *mut PoolMem<T, L>,
}

/// A generic, *thread-safe*, fixed-sized memory allocator for instances of `T`.
/// New instances of `T` are packed together into arrays of size `L`, and allocated in bulk as one memory arena from the global allocator.
/// Arenas from the global allocator are not deallocated until the pool is dropped, and are re-used as instances of `T` are allocated and freed.
///
/// This specific datastructure also supports generational indexing, which means that an arbitrary number of non-owning references to allocated instances of `T` can be generated safely. These references can outlive the underlying `T` they reference, and will safely report upon dereference that the original underlying `T` is gone.
///
/// Atomic reference counting is also implemented allowing for exceedingly complex models of shared ownership. Multiple copies of both strong and weak references to the underlying `T` can be generated that are all memory safe and borrow-checked.
///
/// Allocating from a pool results in very little internal and external fragmentation in the global heap, thus saving significant amounts of memory from being used by one's program. Pools also allocate memory significantly faster on average than the global allocator. This specific pool implementation supports guaranteed constant time `alloc` and `free`.
pub struct Pool<T, const L: usize = DEFAULT_L>(Mutex<(*mut Slot<T>, u64, *mut PoolMem<T, L>, usize)>);
unsafe impl<T, const L: usize> Send for Pool<T, L> {}
unsafe impl<T, const L: usize> Sync for Pool<T, L> {}

impl<T, const L: usize> Pool<T, L> {
    pub const DEFAULT_L: usize = DEFAULT_L;

    /// Creates a new `Pool<T>` with packing length `L`. Packing length determines the number of instances of `T` that will fit in a page before it becomes full. Once all pages in a `Pool<T>` are full a new page is allocated from the LocalNode allocator. Larger values of `L` are generally faster, but the returns are diminishing and vary by platform.
    ///
    /// A `Pool<T>` cannot be interacted with directly, it requires a `impl StaticPool<T> for Pool<T>` implementation. See the `static_pool!` macro for automatically generated trait implementation.
    #[inline]
    pub const fn new() -> Self {
        Pool(Mutex::new((ptr::null_mut(), 1, ptr::null_mut(), usize::MAX)))
    }

    #[inline(always)]
    fn create_arr() -> [MaybeUninit<Slot<T>>; L] {
        unsafe { MaybeUninit::<[MaybeUninit<Slot<T>>; L]>::uninit().assume_init() }
    }

    /// Allocates uninitialized memory for an instance `T`. The returned pointer points to this memory. It is undefined what will be contained in this memory, it must be initiallized before being used. This pointer must be manually freed from the pool using `Pool::free_ptr` before being dropped, otherwise its memory will be leaked. If the pool is dropped before this pointer is freed, the destructor of `T` will not be run and this pointer will point to invalid memory.
    unsafe fn alloc_ptr(&self, obj: T) -> NonNull<Slot<T>> {
        let mut mutex = self.0.lock().unwrap();
        let (mut first_free, uid, mut head_arena, mut head_size) = *mutex;

        let slot_ptr = if let Some(mut slot_ptr) = NonNull::new(first_free) {
            let slot = slot_ptr.as_mut();
            let _announce_free = slot.free_lock.write().unwrap();
            debug_assert_eq!(slot.uid, 0);
            first_free = slot.obj.empty_next;
            slot.ref_count = AtomicU32::new(1);
            slot.uid = uid;
            slot.obj.full_obj = ManuallyDrop::new(obj);
            slot_ptr
        } else {
            if head_size >= L {
                let new = Box::leak(Box::new(PoolMem { pre: head_arena, mem: Self::create_arr() }));
                head_arena = new;
                head_size = 0;
            }
            let slot = Slot {
                obj: SlotState { full_obj: ManuallyDrop::new(obj) },
                free_lock: RwLock::new(()),
                ref_count: AtomicU32::new(1),
                uid,
            };
            let slot_ptr = &mut (*head_arena).mem[head_size];
            let slot_ptr = NonNull::new_unchecked(slot_ptr.write(slot));
            head_size += 1;
            // We do not have to hold the free lock since we know this slot has never been touched before and nothing external references it
            slot_ptr
        };

        *mutex = (first_free, uid.wrapping_add(1), head_arena, head_size);
        slot_ptr
    }
    /// Frees memory allocated from the pool by `Pool::alloc_ptr`. This must be called only once on only pointers returned by `Pool::alloc_ptr` from the same pool. Once memory is freed the content of the memory is undefined, it should not be read or written.
    ///
    /// `drop` will be called on the `T` pointed to, be sure it has not been called already.
    ///
    /// The free lock must be held by the caller.
    unsafe fn free_ptr(&self, mut slot_ptr: NonNull<Slot<T>>) {
        let slot = slot_ptr.as_mut();
        slot.uid = 0;
        ManuallyDrop::<T>::drop(&mut slot.obj.full_obj);
        //linked-list insert
        let mut mutex = self.0.lock().unwrap();

        slot.obj.empty_next = mutex.0;
        mutex.0 = slot_ptr.as_ptr();
    }
}
impl<T, const L: usize> Drop for Pool<T, L> {
    fn drop(&mut self) {
        let mutex = self.0.lock().unwrap();
        let (_, _, mut head_arena, _) = *mutex;
        unsafe {
            while !head_arena.is_null() {
                let mem = Box::from_raw(head_arena);
                head_arena = mem.pre;
                drop(mem);
            }
        }
        drop(mutex);
    }
}

pub trait StaticPool<T, const L: usize = DEFAULT_L> {
    /// Must return a pointer to an instance of a `Pool<T, L>` with a static lifetime. That pointer must be cast to a `*const ()` to make the borrow-checker happy.
    ///
    /// **Safety**: The returned pointer must have originally been a `&'static Pool<T, L>` reference. So it must have had a matching `T` and `L` and it must have the static lifetime.
    ///
    /// In order to borrow-split allocations from a `Pool<T, L>`, we need to force the borrow-checker to not associate the lifetime of an instance of `T` with the lifetime of the pool. Otherwise the borrow-checker would require every allocated `T` to have the `'static` lifetime, to match the pool's lifetime.
    /// The simplest way I have found to do this is to return the pointer to the static pool as an anonymous, lifetimeless `*const ()`. This introduces unnecessary safety concerns surrounding pointer casting unfortunately. If there is a better way to borrow-split from a pool I will gladly implement it.
    unsafe fn get_static_pool() -> *const ();

    /// Allocates memory for an instance `T` and puts its pointer behind a memory-safe Arc. This `PoolArc` automatically frees itself on drop, and will cause the borrow checker to complain if you attempt to drop the pool before you drop this box.
    ///
    /// This `PoolArc` supports the ability to generate weak, non-owning references to the allocated `T`.
    #[inline(always)]
    fn alloc(obj: T) -> PoolArc<T, Self, L>
    where
        Self: Sized,
    {
        unsafe {
            PoolArc {
                ptr: (*Self::get_static_pool().cast::<Pool<T, L>>()).alloc_ptr(obj),
                _p: PhantomData,
            }
        }
    }
}

/// A rust-style RAII wrapper that drops and frees memory allocated from a pool automatically, the same as an `Arc<T>`. This will run the destructor of `T` in place within the pool before freeing it, correctly maintaining the invariants that the borrow checker and rust compiler expect of generic types.
pub struct PoolArc<T, OriginPool: StaticPool<T, L>, const L: usize = DEFAULT_L> {
    ptr: NonNull<Slot<T>>,
    _p: PhantomData<*const OriginPool>,
}

impl<T, OriginPool: StaticPool<T, L>, const L: usize> PoolArc<T, OriginPool, L> {
    /// Obtain a non-owning reference to the `T` contained in this `PoolArc`. This reference has the special property that the underlying `T` can be dropped from the pool while neither making this reference invalid or unsafe nor leaking the memory of `T`. Instead attempts to `grab` the reference will safely return `None`.
    ///
    /// `T` is guaranteed to be dropped when all `PoolArc<T>` are dropped, regardless of how many `PoolWeakRef<T>` still exist.
    #[inline]
    pub fn downgrade(&self) -> PoolWeakRef<T, OriginPool, L> {
        unsafe {
            // Since this is a Arc we know for certain the object has not been freed, so we don't have to hold the free lock
            PoolWeakRef {
                ptr: self.ptr,
                uid: NonZeroU64::new_unchecked(self.ptr.as_ref().uid),
                _p: PhantomData,
            }
        }
    }
    /// Returns a number that uniquely identifies this allocated `T` within this pool. No other instance of `T` may have this uid.
    pub fn uid(&self) -> NonZeroU64 {
        unsafe { NonZeroU64::new_unchecked(self.ptr.as_ref().uid) }
    }
}

impl<T, OriginPool: StaticPool<T, L>, const L: usize> Deref for PoolArc<T, OriginPool, L> {
    type Target = T;
    #[inline]
    fn deref(&self) -> &Self::Target {
        unsafe { &self.ptr.as_ref().obj.full_obj }
    }
}
impl<T, OriginPool: StaticPool<T, L>, const L: usize> Clone for PoolArc<T, OriginPool, L> {
    fn clone(&self) -> Self {
        unsafe {
            self.ptr.as_ref().ref_count.fetch_add(1, Ordering::Relaxed);
        }
        Self { ptr: self.ptr, _p: PhantomData }
    }
}
impl<T, OriginPool: StaticPool<T, L>, const L: usize> Drop for PoolArc<T, OriginPool, L> {
    #[inline]
    fn drop(&mut self) {
        unsafe {
            let slot = self.ptr.as_ref();
            if slot.ref_count.fetch_sub(1, Ordering::AcqRel) == 1 {
                let _announce_free = slot.free_lock.write().unwrap();
                // We have to check twice in case a weakref was upgraded before the lock was acquired
                if slot.ref_count.load(Ordering::Relaxed) == 0 {
                    (*OriginPool::get_static_pool().cast::<Pool<T, L>>()).free_ptr(self.ptr);
                }
            }
        }
    }
}
unsafe impl<T, OriginPool: StaticPool<T, L>, const L: usize> Send for PoolArc<T, OriginPool, L> where T: Send {}
unsafe impl<T, OriginPool: StaticPool<T, L>, const L: usize> Sync for PoolArc<T, OriginPool, L> where T: Sync {}
impl<T, OriginPool: StaticPool<T, L>, const L: usize> Debug for PoolArc<T, OriginPool, L>
where
    T: Debug,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_tuple("PoolArc").field(self.deref()).finish()
    }
}
impl<T, OriginPool: StaticPool<T, L>, const L: usize> Display for PoolArc<T, OriginPool, L>
where
    T: Display,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        self.deref().fmt(f)
    }
}

/// A non-owning reference to a `T` allocated by a pool. This reference has the special property that the underlying `T` can be dropped from the pool while neither making this reference invalid nor leaking the memory of `T`. Instead attempts to `grab` this reference will safely return `None` if the underlying `T` has been freed by any thread.
///
/// Due to their thread safety and low overhead a `PoolWeakRef` implements clone and copy.
///
/// The lifetime of this reference is tied to the lifetime of the pool it came from, because if it were allowed to live longer than its origin pool, it would no longer be safe to dereference and would most likely segfault. Instead the borrow-checker will enforce that this reference has a shorter lifetime that its origin pool.
///
/// For technical reasons a `RwLock<PoolWeakRef<T>>` will always be the fastest implementation of a `PoolWeakRefSwap`, which is why this library does not provide a `PoolWeakRefSwap` type.
pub struct PoolWeakRef<T, OriginPool: StaticPool<T, L>, const L: usize = DEFAULT_L> {
    /// A number that uniquely identifies this allocated `T` within this pool. No other instance of `T` may have this uid. This value is read-only.
    pub uid: NonZeroU64,
    ptr: NonNull<Slot<T>>,
    _p: PhantomData<*const OriginPool>,
}

impl<T, OriginPool: StaticPool<T, L>, const L: usize> PoolWeakRef<T, OriginPool, L> {
    /// Obtains a lock that allows the `T` contained in this `PoolWeakRef` to be dereferenced in a thread-safe manner. This lock does not prevent other threads from accessing `T` at the same time, so `T` ought to use interior mutability if it needs to be mutated in a thread-safe way. What this lock does guarantee is that `T` cannot be destructed and freed while it is being held.
    ///
    /// Do not attempt from within the same thread to drop the `PoolArc` that owns this `T` before dropping this lock, or else the thread will deadlock. Rust makes this quite hard to do accidentally but it's not strictly impossible.
    #[inline]
    pub fn grab<'b>(&self) -> Option<PoolGuard<'b, T>> {
        unsafe {
            let slot = self.ptr.as_ref();
            let prevent_free_lock = slot.free_lock.read().unwrap();
            if slot.uid == self.uid.get() {
                Some(PoolGuard(prevent_free_lock, &slot.obj.full_obj))
            } else {
                None
            }
        }
    }
    /// Attempts to create an owning `PoolArc` from this `PoolWeakRef` of the underlying `T`. Will return `None` if the underlying `T` has already been dropped.
    pub fn upgrade(&self) -> Option<PoolArc<T, OriginPool, L>> {
        unsafe {
            let slot = self.ptr.as_ref();
            let _prevent_free_lock = slot.free_lock.read().unwrap();
            if slot.uid == self.uid.get() {
                self.ptr.as_ref().ref_count.fetch_add(1, Ordering::Relaxed);
                Some(PoolArc { ptr: self.ptr, _p: PhantomData })
            } else {
                None
            }
        }
    }
}
impl<T, OriginPool: StaticPool<T, L>, const L: usize> Clone for PoolWeakRef<T, OriginPool, L> {
    fn clone(&self) -> Self {
        Self { uid: self.uid, ptr: self.ptr, _p: PhantomData }
    }
}
impl<T, OriginPool: StaticPool<T, L>, const L: usize> Copy for PoolWeakRef<T, OriginPool, L> {}
unsafe impl<T, OriginPool: StaticPool<T, L>, const L: usize> Send for PoolWeakRef<T, OriginPool, L> where T: Send {}
unsafe impl<T, OriginPool: StaticPool<T, L>, const L: usize> Sync for PoolWeakRef<T, OriginPool, L> where T: Sync {}
impl<T, OriginPool: StaticPool<T, L>, const L: usize> Debug for PoolWeakRef<T, OriginPool, L>
where
    T: Debug,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let inner = self.grab();
        f.debug_tuple("PoolWeakRef").field(&inner).finish()
    }
}
impl<T, OriginPool: StaticPool<T, L>, const L: usize> Display for PoolWeakRef<T, OriginPool, L>
where
    T: Display,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if let Some(inner) = self.grab() {
            inner.fmt(f)
        } else {
            f.write_str("Empty")
        }
    }
}

/// A multithreading lock guard that prevents another thread from freeing the underlying `T` while it is held. It does not prevent other threads from accessing the underlying `T`.
///
/// If the same thread that holds this guard attempts to free `T` before dropping the guard, it will deadlock.
pub struct PoolGuard<'a, T>(RwLockReadGuard<'a, ()>, &'a T);
impl<'a, T> Deref for PoolGuard<'a, T> {
    type Target = T;
    #[inline]
    fn deref(&self) -> &Self::Target {
        &*self.1
    }
}
impl<'a, T> Debug for PoolGuard<'a, T>
where
    T: Debug,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_tuple("PoolGuard").field(self.deref()).finish()
    }
}
impl<'a, T> Display for PoolGuard<'a, T>
where
    T: Display,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        self.deref().fmt(f)
    }
}

/// Allows for the Atomic Swapping and Loading of a `PoolArc<T>`, similar to how a `RwLock<Arc<T>>` would function, but much faster and less verbose.
pub struct PoolArcSwap<T, OriginPool: StaticPool<T, L>, const L: usize = DEFAULT_L> {
    ptr: AtomicPtr<Slot<T>>,
    reads: AtomicU32,
    _p: PhantomData<*const OriginPool>,
}
impl<T, OriginPool: StaticPool<T, L>, const L: usize> PoolArcSwap<T, OriginPool, L> {
    /// Creates a new `PoolArcSwap`, consuming `arc` in the process.
    pub fn new(mut arc: PoolArc<T, OriginPool, L>) -> Self {
        unsafe {
            let ret = Self {
                ptr: AtomicPtr::new(arc.ptr.as_mut()),
                reads: AtomicU32::new(0),
                _p: arc._p,
            };
            // Suppress reference decrement on new
            mem::forget(arc);
            ret
        }
    }
    /// Atomically swaps the currently stored `PoolArc` with a new one, returning the previous one.
    pub fn swap(&self, arc: PoolArc<T, OriginPool, L>) -> PoolArc<T, OriginPool, L> {
        unsafe {
            let pre_ptr = self.ptr.swap(arc.ptr.as_ptr(), Ordering::Relaxed);

            while self.reads.load(Ordering::Acquire) > 0 {
                std::hint::spin_loop()
            }

            mem::forget(arc);
            PoolArc { ptr: NonNull::new_unchecked(pre_ptr), _p: self._p }
        }
    }

    /// Atomically loads and clones the currently stored `PoolArc`, guaranteeing that the underlying `T` cannot be freed while the clone is held.
    pub fn load(&self) -> PoolArc<T, OriginPool, L> {
        unsafe {
            self.reads.fetch_add(1, Ordering::Acquire);
            let ptr = self.ptr.load(Ordering::Relaxed);
            (*ptr).ref_count.fetch_add(1, Ordering::Relaxed);
            self.reads.fetch_sub(1, Ordering::Release);
            PoolArc { ptr: NonNull::new_unchecked(ptr), _p: self._p }
        }
    }
}
impl<T, OriginPool: StaticPool<T, L>, const L: usize> Drop for PoolArcSwap<T, OriginPool, L> {
    #[inline]
    fn drop(&mut self) {
        unsafe {
            let pre = self.ptr.load(Ordering::SeqCst);
            PoolArc { _p: self._p, ptr: NonNull::new_unchecked(pre) };
        }
    }
}
unsafe impl<T, OriginPool: StaticPool<T, L>, const L: usize> Send for PoolArcSwap<T, OriginPool, L> where T: Send {}
unsafe impl<T, OriginPool: StaticPool<T, L>, const L: usize> Sync for PoolArcSwap<T, OriginPool, L> where T: Sync {}
impl<T, OriginPool: StaticPool<T, L>, const L: usize> Debug for PoolArcSwap<T, OriginPool, L>
where
    T: Debug,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_tuple("PoolArcSwap").field(&self.load()).finish()
    }
}
impl<T, OriginPool: StaticPool<T, L>, const L: usize> Display for PoolArcSwap<T, OriginPool, L>
where
    T: Display,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        (&self.load()).fmt(f)
    }
}

/// Another implementation of a `PoolArcSwap` utalizing a RwLock instead of atomics.
/// This implementation has slower a `load` but a faster `swap` than the previous implementation of `PoolArcSwap`.
/// If you plan on swapping way more often than loading, this may be a better choice.
pub struct PoolArcSwapRw<T, OriginPool: StaticPool<T, L>, const L: usize = DEFAULT_L> {
    ptr: RwLock<NonNull<Slot<T>>>,
    _p: PhantomData<*const OriginPool>,
}

impl<T, OriginPool: StaticPool<T, L>, const L: usize> PoolArcSwapRw<T, OriginPool, L> {
    /// Creates a new `PoolArcSwap`, consuming `arc` in the process.
    pub fn new(arc: PoolArc<T, OriginPool, L>) -> Self {
        let ret = Self { ptr: RwLock::new(arc.ptr), _p: arc._p };
        mem::forget(arc);
        ret
    }

    /// Atomically swaps the currently stored `PoolArc` with a new one, returning the previous one.
    pub fn swap(&self, arc: PoolArc<T, OriginPool, L>) -> PoolArc<T, OriginPool, L> {
        let mut w = self.ptr.write().unwrap();
        let pre = PoolArc { ptr: *w, _p: self._p };
        *w = arc.ptr;
        mem::forget(arc);
        pre
    }

    /// Atomically loads and clones the currently stored `PoolArc`, guaranteeing that the underlying `T` cannot be freed while the clone is held.
    pub fn load(&self) -> PoolArc<T, OriginPool, L> {
        let r = self.ptr.read().unwrap();
        unsafe {
            r.as_ref().ref_count.fetch_add(1, Ordering::Relaxed);
        }
        let pre = PoolArc { ptr: *r, _p: self._p };
        pre
    }
}
impl<T, OriginPool: StaticPool<T, L>, const L: usize> Drop for PoolArcSwapRw<T, OriginPool, L> {
    #[inline]
    fn drop(&mut self) {
        let w = self.ptr.write().unwrap();
        PoolArc { ptr: *w, _p: self._p };
    }
}
unsafe impl<T, OriginPool: StaticPool<T, L>, const L: usize> Send for PoolArcSwapRw<T, OriginPool, L> where T: Send {}
unsafe impl<T, OriginPool: StaticPool<T, L>, const L: usize> Sync for PoolArcSwapRw<T, OriginPool, L> where T: Sync {}
impl<T, OriginPool: StaticPool<T, L>, const L: usize> Debug for PoolArcSwapRw<T, OriginPool, L>
where
    T: Debug,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_tuple("PoolArcSwapRw").field(&self.load()).finish()
    }
}
impl<T, OriginPool: StaticPool<T, L>, const L: usize> Display for PoolArcSwapRw<T, OriginPool, L>
where
    T: Display,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        (&self.load()).fmt(f)
    }
}

/// Automatically generates valid implementations of `StaticPool<T, L>` onto a chosen identifier, allowing this module to allocate instances of `T` with `alloc`. Users have to generate implementations clientside because rust does not allow for generic globals.
///
/// The chosen identifier is declared to be a struct with no fields, and instead contains a static global `Pool` for every implementation of `StaticPool<T, L>` requested.
///
/// # Example
/// ```
/// use zerotier_utils::arc_pool::{static_pool, StaticPool, Pool, PoolArc};
///
/// static_pool!(pub StaticPool MyPools {
///     Pool<u32>, Pool<&u32, 12>
/// });
///
/// struct Container {
///     item: PoolArc<u32, MyPools>
/// }
///
/// let object = 1u32;
/// let arc_object = MyPools::alloc(object);
/// let arc_ref = MyPools::alloc(&object);
/// let arc_container = Container {item: MyPools::alloc(object)};
///
/// assert_eq!(*arc_object, **arc_ref);
/// assert_eq!(*arc_object, *arc_container.item);
/// ```
#[macro_export]
macro_rules! __static_pool__ {
    ($m:ident $s:ident { $($($p:ident)::+<$t:ty$(, $l:tt)?>),+ $(,)?}) => {
        struct $s {}
        $(
            impl $m<$t$(, $l)?> for $s {
                #[inline(always)]
                unsafe fn get_static_pool() -> *const () {
                    static POOL: $($p)::+<$t$(, $l)?> = $($p)::+::new();
                    (&POOL as *const $($p)::+<$t$(, $l)?>).cast()
                }
            }
        )*
    };
    ($m:ident::$n:ident $s:ident { $($($p:ident)::+<$t:ty$(, $l:tt)?>),+ $(,)?}) => {
        struct $s {}
        $(
            impl $m::$n<$t$(, $l)?> for $s {
                #[inline(always)]
                unsafe fn get_static_pool() -> *const () {
                    static POOL: $($p)::+<$t$(, $l)?> = $($p)::+::new();
                    (&POOL as *const $($p)::+<$t$(, $l)?>).cast()
                }
            }
        )*
    };
    (pub $m:ident $s:ident { $($($p:ident)::+<$t:ty$(, $l:tt)?>),+ $(,)?}) => {
        pub struct $s {}
        $(
            impl $m<$t$(, $l)?> for $s {
                #[inline(always)]
                unsafe fn get_static_pool() -> *const () {
                    static POOL: $($p)::+<$t$(, $l)?> = $($p)::+::new();
                    (&POOL as *const $($p)::+<$t$(, $l)?>).cast()
                }
            }
        )*
    };
    (pub $m:ident::$n:ident $s:ident { $($($p:ident)::+<$t:ty$(, $l:tt)?>),+ $(,)?}) => {
        pub struct $s {}
        $(
            impl $m::$n<$t$(, $l)?> for $s {
                #[inline(always)]
                unsafe fn get_static_pool() -> *const () {
                    static POOL: $($p)::+<$t$(, $l)?> = $($p)::+::new();
                    (&POOL as *const $($p)::+<$t$(, $l)?>).cast()
                }
            }
        )*
    };
}
pub use __static_pool__ as static_pool;

#[cfg(test)]
mod tests {
    use super::*;
    use std::{
        sync::{atomic::AtomicU64, Arc},
        thread,
    };

    fn rand(r: &mut u32) -> u32 {
        /* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
        *r ^= *r << 13;
        *r ^= *r >> 17;
        *r ^= *r << 5;
        *r
    }
    const fn prob(p: u64) -> u32 {
        (p * (u32::MAX as u64) / 100) as u32
    }
    fn rand_idx<'a, T>(v: &'a [T], r: &mut u32) -> Option<&'a T> {
        if v.len() > 0 {
            Some(&v[(rand(r) as usize) % v.len()])
        } else {
            None
        }
    }
    fn rand_i<'a, T>(v: &'a [T], r: &mut u32) -> Option<usize> {
        if v.len() > 0 {
            Some((rand(r) as usize) % v.len())
        } else {
            None
        }
    }

    struct Item {
        a: u32,
        count: &'static AtomicU64,
        b: u32,
    }
    impl Item {
        fn new(r: u32, count: &'static AtomicU64) -> Item {
            count.fetch_add(1, Ordering::Relaxed);
            Item { a: r, count, b: r }
        }
        fn check(&self, id: u32) {
            assert_eq!(self.a, self.b);
            assert_eq!(self.a, id);
        }
    }
    impl Drop for Item {
        fn drop(&mut self) {
            let _a = self.count.fetch_sub(1, Ordering::Relaxed);
            assert_eq!(self.a, self.b);
        }
    }

    const POOL_U32_LEN: usize = (5 * 12) << 2;
    static_pool!(StaticPool TestPools {
        Pool<u32, POOL_U32_LEN>, Pool<Item>
    });

    #[test]
    fn usage() {
        let num1 = TestPools::alloc(1u32);
        let num2 = TestPools::alloc(2u32);
        let num3 = TestPools::alloc(3u32);
        let num4 = TestPools::alloc(4u32);
        let num2_weak = num2.downgrade();

        assert_eq!(*num2_weak.grab().unwrap(), 2);
        drop(num2);

        assert_eq!(*num1, 1);
        assert_eq!(*num3, 3);
        assert_eq!(*num4, 4);
        assert!(num2_weak.grab().is_none());
    }
    #[test]
    fn single_thread() {
        let mut history = Vec::new();

        let num1 = TestPools::alloc(1u32);
        let num2 = TestPools::alloc(2u32);
        let num3 = TestPools::alloc(3u32);
        let num4 = TestPools::alloc(4u32);
        let num2_weak = num2.downgrade();

        for i in 0..1000 {
            history.push(TestPools::alloc(i as u32));
        }
        for i in 0..100 {
            let arc = history.remove((i * 10) % history.len());
            assert!(*arc < 1000);
        }
        for i in 0..1000 {
            history.push(TestPools::alloc(i as u32));
        }

        assert_eq!(*num2_weak.grab().unwrap(), 2);
        drop(num2);

        assert_eq!(*num1, 1);
        assert_eq!(*num3, 3);
        assert_eq!(*num4, 4);
        assert!(num2_weak.grab().is_none());
    }

    #[test]
    fn multi_thread() {
        const N: usize = 12345;
        static COUNT: AtomicU64 = AtomicU64::new(0);

        let mut joins = Vec::new();
        for i in 0..32 {
            joins.push(thread::spawn(move || {
                let r = &mut (i + 1234);

                let mut items_dup = Vec::new();
                let mut items = Vec::new();
                for _ in 0..N {
                    let p = rand(r);
                    if p < prob(30) {
                        let id = rand(r);
                        let s = TestPools::alloc(Item::new(id, &COUNT));
                        items.push((id, s.clone(), s.downgrade()));
                        s.check(id);
                    } else if p < prob(60) {
                        if let Some((id, s, w)) = rand_idx(&items, r) {
                            items_dup.push((*id, s.clone(), (*w).clone()));
                            s.check(*id);
                        }
                    } else if p < prob(80) {
                        if let Some(i) = rand_i(&items, r) {
                            let (id, s, w) = items.swap_remove(i);
                            w.grab().unwrap().check(id);
                            s.check(id);
                        }
                    } else if p < prob(100) {
                        if let Some(i) = rand_i(&items_dup, r) {
                            let (id, s, w) = items_dup.swap_remove(i);
                            w.grab().unwrap().check(id);
                            s.check(id);
                        }
                    }
                }
                for (id, s, w) in items_dup {
                    s.check(id);
                    w.grab().unwrap().check(id);
                }
                for (id, s, w) in items {
                    s.check(id);
                    w.grab().unwrap().check(id);
                    drop(s);
                    assert!(w.grab().is_none())
                }
            }));
        }
        for j in joins {
            j.join().unwrap();
        }
        assert_eq!(COUNT.load(Ordering::Relaxed), 0);
    }

    #[test]
    fn multi_thread_swap() {
        const N: usize = 1234;
        static COUNT: AtomicU64 = AtomicU64::new(0);

        let s = Arc::new(PoolArcSwap::new(TestPools::alloc(Item::new(0, &COUNT))));

        for _ in 0..123 {
            let mut joins = Vec::new();
            for _ in 0..8 {
                let swaps = s.clone();
                joins.push(thread::spawn(move || {
                    let r = &mut 1474;
                    let mut new = TestPools::alloc(Item::new(rand(r), &COUNT));
                    for _ in 0..N {
                        new = swaps.swap(new);
                    }
                }));
            }
            for j in joins {
                j.join().unwrap();
            }
        }
        drop(s);
        assert_eq!(COUNT.load(Ordering::Relaxed), 0);
    }

    #[test]
    fn multi_thread_swap_load() {
        const N: usize = 12345;
        static COUNT: AtomicU64 = AtomicU64::new(0);

        let s: Arc<[_; 8]> = Arc::new(std::array::from_fn(|i| PoolArcSwap::new(TestPools::alloc(Item::new(i as u32, &COUNT)))));

        let mut joins = Vec::new();

        for i in 0..4 {
            let swaps = s.clone();
            joins.push(thread::spawn(move || {
                let r = &mut (i + 2783);
                for _ in 0..N {
                    if let Some(s) = rand_idx(&swaps[..], r) {
                        let new = TestPools::alloc(Item::new(rand(r), &COUNT));
                        let _a = s.swap(new);
                    }
                }
            }));
        }
        for i in 0..28 {
            let swaps = s.clone();
            joins.push(thread::spawn(move || {
                let r = &mut (i + 4136);
                for _ in 0..N {
                    if let Some(s) = rand_idx(&swaps[..], r) {
                        let _a = s.load();
                        assert_eq!(_a.a, _a.b);
                    }
                }
            }));
        }
        for j in joins {
            j.join().unwrap();
        }
        drop(s);
        assert_eq!(COUNT.load(Ordering::Relaxed), 0);
    }
}
