// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

pub mod dictionary;
pub(crate) mod gate;
pub mod marshalable;

/// A value for ticks that indicates that something never happened, and is thus very long before zero ticks.
pub(crate) const NEVER_HAPPENED_TICKS: i64 = -2147483648;
