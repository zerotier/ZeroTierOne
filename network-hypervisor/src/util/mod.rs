// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

pub mod buffer;
pub(crate) mod canonicalobject;
pub(crate) mod gate;
pub mod marshalable;
pub(crate) mod pool;

/// A value for ticks that indicates that something never happened, and is thus very long before zero ticks.
pub(crate) const NEVER_HAPPENED_TICKS: i64 = -2147483648;

#[cfg(test)]
pub mod testutil;

#[cfg(feature = "debug_events")]
#[allow(unused_macros)]
macro_rules! debug_event {
    ($si:expr, $fmt:expr $(, $($arg:tt)*)?) => {
        $si.event(crate::Event::Debug(file!(), line!(), format!($fmt, $($($arg)*)?)));
    }
}

#[cfg(not(feature = "debug_events"))]
#[allow(unused_macros)]
macro_rules! debug_event {
    ($si:expr, $fmt:expr $(, $($arg:tt)*)?) => {};
}

#[allow(unused_imports)]
pub(crate) use debug_event;

/// Obtain a view into a byte array cast as another byte array.
#[inline(always)]
pub(crate) fn byte_array_range<const A: usize, const START: usize, const LEN: usize>(a: &[u8; A]) -> &[u8; LEN] {
    assert!((START + LEN) <= A);
    unsafe { &*a.as_ptr().add(START).cast::<[u8; LEN]>() }
}

/// View a flat (Copy) object as a byte array.
#[inline(always)]
pub(crate) fn flat_object_as_bytes<T: Copy>(t: &T) -> &[u8] {
    unsafe { &*std::ptr::slice_from_raw_parts((t as *const T).cast::<u8>(), std::mem::size_of::<T>()) }
}

/// Trait that annotates a type as being alignment neutral, such as a packed struct of all bytes and byte arrays.
pub(crate) unsafe trait AlignmentNeutral: Copy {}

/// View a byte array as a flat (Copy) object.
/// To be safe this can only be used with annotated alignment-neutral structs.
#[inline(always)]
pub(crate) fn bytes_as_flat_object<T: Copy + AlignmentNeutral>(b: &[u8]) -> &T {
    assert!(b.len() >= std::mem::size_of::<T>());
    unsafe { &*b.as_ptr().cast() }
}
