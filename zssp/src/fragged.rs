use std::cell::UnsafeCell;
use std::mem::{needs_drop, size_of, zeroed, MaybeUninit};
use std::ptr::slice_from_raw_parts;
use std::sync::RwLock;
use std::sync::atomic::{AtomicU64, Ordering};

/// Fast packet defragmenter
pub struct Fragged<Fragment, const MAX_FRAGMENTS: usize> {
    counter_want: RwLock<(u64, u64)>,
    have: AtomicU64,
    frags: UnsafeCell<[MaybeUninit<Fragment>; MAX_FRAGMENTS]>,
}

pub struct Assembled<Fragment, const MAX_FRAGMENTS: usize>([MaybeUninit<Fragment>; MAX_FRAGMENTS], usize);

impl<Fragment, const MAX_FRAGMENTS: usize> AsRef<[Fragment]> for Assembled<Fragment, MAX_FRAGMENTS> {
    #[inline(always)]
    fn as_ref(&self) -> &[Fragment] {
        unsafe { &*slice_from_raw_parts(self.0.as_ptr().cast::<Fragment>(), self.1) }
    }
}

impl<Fragment, const MAX_FRAGMENTS: usize> Drop for Assembled<Fragment, MAX_FRAGMENTS> {
    #[inline(always)]
    fn drop(&mut self) {
        for i in 0..self.1 {
            unsafe {
                self.0.get_unchecked_mut(i).assume_init_drop();
            }
        }
    }
}

impl<Fragment, const MAX_FRAGMENTS: usize> Fragged<Fragment, MAX_FRAGMENTS> {
    #[inline(always)]
    pub fn new() -> Self {
        // These assertions should be optimized out at compile time and check to make sure
        // that the array of MaybeUninit<Fragment> can be freely cast into an array of
        // Fragment. They also check that the maximum number of fragments is not too large
        // for the fact that we use bits in a u64 to track which fragments are received.
        assert!(MAX_FRAGMENTS <= 64);
        assert_eq!(size_of::<MaybeUninit<Fragment>>(), size_of::<Fragment>());
        assert_eq!(
            size_of::<[MaybeUninit<Fragment>; MAX_FRAGMENTS]>(),
            size_of::<[Fragment; MAX_FRAGMENTS]>()
        );
        Self {
            counter_want: RwLock::new((0, 0)),
            have: AtomicU64::new(0),
            frags: unsafe {zeroed()},
        }
    }

    /// Add a fragment and return an assembled packet container if all fragments have been received.
    ///
    /// When a fully assembled packet is returned the internal state is reset and this object can
    /// be reused to assemble another packet.
    #[inline(always)]
    pub fn assemble(&self, counter: u64, fragment: Fragment, fragment_no: u8, fragment_count: u8) -> Option<Assembled<Fragment, MAX_FRAGMENTS>> {
        if fragment_no < fragment_count && (fragment_count as usize) <= MAX_FRAGMENTS {
            let r = self.counter_want.read().unwrap();
            let (mut r_counter, mut r_want) = *r;
            let mut _r_guard = Some(r);
            let mut _w_guard = None;

            // If the counter has changed, reset the structure to receive a new packet.
            if counter != r_counter {
                _r_guard.take();
                let mut w = self.counter_want.write().unwrap();
                let (w_counter, w_want) = *w;
                if counter != w_counter {
                    if needs_drop::<Fragment>() {
                        let mut have = self.have.load(Ordering::Relaxed);
                        if have != w_want {
                            let mut i = 0;
                            while have != 0 {
                                if (have & 1) != 0 {
                                    debug_assert!(i < MAX_FRAGMENTS);
                                    unsafe { (*self.frags.get()).get_unchecked_mut(i).assume_init_drop() };
                                }
                                have = have.wrapping_shr(1);
                                i += 1;
                            }
                        }
                    }
                    r_counter = counter;
                    r_want = 1u64.wrapping_shl(fragment_count as u32) - 1;
                    *w = (r_counter, r_want);
                    self.have.store(0, Ordering::Relaxed);
                }
                _w_guard = Some(w);
            }

            let got = 1u64.wrapping_shl(fragment_no as u32);
            if got & r_want > 0 && 1u64.wrapping_shl(fragment_count as u32) - 1 == r_want {
                let have = self.have.fetch_or(got, Ordering::Relaxed);
                if have & got == 0 {

                    unsafe {
                        (*self.frags.get()).get_unchecked_mut(fragment_no as usize).write(fragment);
                    }
                    if have | got == r_want {
                        // Setting 'have' to 0 resets the state of this object, and the fragments
                        // are effectively moved into the Assembled<> container and returned. That
                        // container will drop them when it is dropped.
                        return Some(Assembled(unsafe { std::mem::transmute_copy(&self.frags) }, fragment_count as usize));
                    }
                }
            }
        }
        return None;
    }
}

impl<Fragment, const MAX_FRAGMENTS: usize> Drop for Fragged<Fragment, MAX_FRAGMENTS> {
    #[inline(always)]
    fn drop(&mut self) {
        if needs_drop::<Fragment>() {
            let mut have = self.have.load(Ordering::Relaxed);
            let r = self.counter_want.read().unwrap();
            let (_, r_want) = *r;
            if have != r_want {
                let mut i = 0;
                while have != 0 {
                    if (have & 1) != 0 {
                        debug_assert!(i < MAX_FRAGMENTS);
                        unsafe { (*self.frags.get()).get_unchecked_mut(i).assume_init_drop() };
                    }
                    have = have.wrapping_shr(1);
                    i += 1;
                }
            }
        }
    }
}
