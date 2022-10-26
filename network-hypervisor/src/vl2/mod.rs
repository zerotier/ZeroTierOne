// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

mod multicastgroup;
mod networkid;
mod switch;

pub mod networkconfig;
pub mod rule;
pub mod v1;

pub use multicastgroup::MulticastGroup;
pub use networkid::NetworkId;
pub use switch::{Switch, SwitchInterface};
