// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

pub mod buffer;
pub(crate) mod gate;
pub mod marshalable;
pub(crate) mod pool;

pub use zerotier_core_crypto::hex;
pub use zerotier_core_crypto::varint;

pub(crate) const ZEROES: [u8; 64] = [0_u8; 64];

#[cfg(target_feature = "debug_events")]
#[allow(unused_macros)]
macro_rules! debug_event {
    ($si:expr, $fmt:expr $(, $($arg:tt)*)?) => {
        $si.event(crate::Event::Debug(file!(), line!(), format!($fmt, $($($arg)*)?)));
    }
}

#[cfg(not(target_feature = "debug_events"))]
#[allow(unused_macros)]
macro_rules! debug_event {
    ($si:expr, $fmt:expr $(, $($arg:tt)*)?) => {};
}

#[allow(unused_imports)]
pub(crate) use debug_event;

/// Obtain a reference to a sub-array within an existing byte array.
#[inline(always)]
pub(crate) fn byte_array_range<const A: usize, const START: usize, const LEN: usize>(a: &[u8; A]) -> &[u8; LEN] {
    assert!((START + LEN) <= A);
    unsafe { &*a.as_ptr().add(START).cast::<[u8; LEN]>() }
}
