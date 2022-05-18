/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

pub const VERSION_MAJOR: u8 = 1;
pub const VERSION_MINOR: u8 = 99;
pub const VERSION_REVISION: u8 = 1;

pub mod error;
pub mod util;
pub mod vl1;
pub mod vl2;

mod networkhypervisor;

pub use networkhypervisor::{Interface, NetworkHypervisor};
pub use vl1::protocol::{PacketBuffer, PooledPacketBuffer};
