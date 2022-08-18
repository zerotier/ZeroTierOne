// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

mod multicastgroup;
mod networkid;

pub(crate) mod switch;

pub use multicastgroup::MulticastGroup;
pub use networkid::NetworkId;
pub use switch::SwitchInterface;
