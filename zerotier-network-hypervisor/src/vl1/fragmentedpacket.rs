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
    pub frags: [Option<PacketBuffer>; PACKET_FRAGMENT_COUNT_MAX],
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

    /// Add a fragment to this fragment set and return true if all fragments are present.
    #[inline(always)]
    pub fn add_fragment(&mut self, frag: PacketBuffer, no: u8, expecting: u8) -> bool {
        self.frags.get_mut(no as usize).map_or(false, |entry| {
            // Note that a duplicate fragment just gets silently replaced. This shouldn't happen
            // unless a dupe occurred at the network level, in which case this is usually a
            // no-op event. There is no security implication since the whole packet gets MAC'd
            // after assembly.
            if entry.replace(frag).is_none() {
                self.have += 1;
                self.expecting |= expecting; // expecting is either 0 or the expected total
                self.have == self.expecting
            } else {
                false
            }
        })
    }
}
