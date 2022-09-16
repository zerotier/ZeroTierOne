// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

pub mod buffer;
pub mod dictionary;
pub(crate) mod gate;
pub mod marshalable;

/// A value for ticks that indicates that something never happened, and is thus very long before zero ticks.
pub(crate) const NEVER_HAPPENED_TICKS: i64 = -2147483648;

#[cfg(feature = "debug_events")]
#[allow(unused_macros)]
macro_rules! debug_event {
    ($si:expr, $fmt:expr $(, $($arg:tt)*)?) => {
        $si.event(crate::vl1::Event::Debug(file!(), line!(), format!($fmt, $($($arg)*)?)));
    }
}

#[cfg(not(feature = "debug_events"))]
#[allow(unused_macros)]
macro_rules! debug_event {
    ($si:expr, $fmt:expr $(, $($arg:tt)*)?) => {};
}

#[allow(unused_imports)]
pub(crate) use debug_event;
