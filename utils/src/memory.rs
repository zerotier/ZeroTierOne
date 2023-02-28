/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

#[allow(unused_imports)]
use std::mem::{needs_drop, size_of, MaybeUninit};

#[allow(unused_imports)]
use std::ptr::copy_nonoverlapping;

/// Store a raw object to a byte array (for architectures known not to care about unaligned access).
/// This will panic if the slice is too small or the object requires drop.
#[cfg(any(target_arch = "x86", target_arch = "x86_64", target_arch = "aarch64", target_arch = "powerpc64"))]
#[inline(always)]
pub fn store_raw<T: Copy>(o: T, dest: &mut [u8]) {
    assert!(!std::mem::needs_drop::<T>());
    assert!(dest.len() >= size_of::<T>());
    unsafe { *dest.as_mut_ptr().cast() = o };
}

/// Store a raw object to a byte array (portable).
/// This will panic if the slice is too small or the object requires drop.
#[cfg(not(any(target_arch = "x86", target_arch = "x86_64", target_arch = "aarch64", target_arch = "powerpc64")))]
#[inline(always)]
pub fn store_raw<T: Copy>(o: T, dest: &mut [u8]) {
    assert!(!std::mem::needs_drop::<T>());
    assert!(dest.len() >= size_of::<T>());
    unsafe { copy_nonoverlapping((&o as *const T).cast(), dest.as_mut_ptr(), size_of::<T>()) };
}

/// Load a raw object from a byte array (for architectures known not to care about unaligned access).
/// This will panic if the slice is too small or the object requires drop.
#[cfg(any(target_arch = "x86", target_arch = "x86_64", target_arch = "aarch64", target_arch = "powerpc64"))]
#[inline(always)]
pub fn load_raw<T: Copy>(src: &[u8]) -> T {
    assert!(!std::mem::needs_drop::<T>());
    assert!(src.len() >= size_of::<T>());
    unsafe { *src.as_ptr().cast() }
}

/// Load a raw object from a byte array (portable).
/// This will panic if the slice is too small or the object requires drop.
#[cfg(not(any(target_arch = "x86", target_arch = "x86_64", target_arch = "aarch64", target_arch = "powerpc64")))]
#[inline(always)]
pub fn load_raw<T: Copy>(src: &[u8]) -> T {
    assert!(!std::mem::needs_drop::<T>());
    assert!(src.len() >= size_of::<T>());
    unsafe {
        let mut tmp: T = MaybeUninit::uninit().assume_init();
        copy_nonoverlapping(src.as_ptr(), (&mut tmp as *mut T).cast(), size_of::<T>());
        tmp
    }
}

/// Obtain a view into an array cast as another array.
/// This will panic if the template parameters would result in out of bounds access.
#[inline(always)]
pub fn array_range<T: Copy, const S: usize, const START: usize, const LEN: usize>(a: &[T; S]) -> &[T; LEN] {
    assert!((START + LEN) <= S);
    unsafe { &*a.as_ptr().add(START).cast::<[T; LEN]>() }
}

/// Get a reference to a raw object as a byte array.
/// The template parameter S must equal the size of the object in bytes or this will panic.
#[inline(always)]
pub fn as_byte_array<T: Copy, const S: usize>(o: &T) -> &[u8; S] {
    assert_eq!(S, size_of::<T>());
    unsafe { &*(o as *const T).cast() }
}

/// Get a reference to a raw object as a byte array.
/// The template parameter S must equal the size of the object in bytes or this will panic.
#[inline(always)]
pub fn as_byte_array_mut<T: Copy, const S: usize>(o: &mut T) -> &mut [u8; S] {
    assert_eq!(S, size_of::<T>());
    unsafe { &mut *(o as *mut T).cast() }
}

/// Transmute an object to a byte array.
/// The template parameter S must equal the size of the object in bytes or this will panic.
#[inline(always)]
pub fn to_byte_array<T: Copy, const S: usize>(o: T) -> [u8; S] {
    assert_eq!(S, size_of::<T>());
    assert!(!std::mem::needs_drop::<T>());
    unsafe { *(&o as *const T).cast() }
}
