/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use crate::PacketBuffer;
use crate::vl1::protocol::*;

/// Packet fragment re-assembler and container.
///
/// Performance note: PacketBuffer is Pooled<Buffer> which is NotNull<*mut Buffer>.
/// That means Option<PacketBuffer> is just a pointer, since NotNull permits the
/// compiler to optimize out any additional state in Option.
pub(crate) struct FragmentedPacket {
    pub ts_ticks: i64,
    pub frags: [Option<PacketBuffer>; FRAGMENT_COUNT_MAX],
    pub have: u8,
    pub expecting: u8,
}

impl FragmentedPacket {
    #[inline(always)]
    pub fn new(ts: i64) -> Self {
        Self {
            ts_ticks: ts,
            frags: [None, None, None, None, None, None, None, None],
            have: 0,
            expecting: 0,
        }
    }

    /// Add a fragment to this fragment set and return true if the packet appears complete.
    /// This will panic if 'no' is out of bounds.
    #[inline(always)]
    pub fn add_fragment(&mut self, frag: PacketBuffer, no: u8, expecting: u8) -> bool {
        if no < FRAGMENT_COUNT_MAX as u8 {
            if self.frags[no as usize].replace(frag).is_none() {
                self.have = self.have.wrapping_add(1);
                self.expecting |= expecting; // in valid streams expecting is either 0 or the (same) total
                return self.have == self.expecting && self.have < FRAGMENT_COUNT_MAX as u8;
            }
        }
        false
    }
}
