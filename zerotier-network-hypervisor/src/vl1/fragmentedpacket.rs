/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use crate::vl1::protocol::*;
use crate::PacketBuffer;

/// Packet fragment re-assembler and container.
///
/// Performance note: PacketBuffer is Pooled<Buffer> which is NotNull<*mut Buffer>.
/// That means Option<PacketBuffer> is just a pointer, since NotNull permits the
/// compiler to optimize out any additional state in Option.
///
/// This will need to be modified if we ever support more than 8 fragments to increase
/// the size of frags[] and the number of bits in 'have' and 'expecting'.
pub(crate) struct FragmentedPacket {
    pub ts_ticks: i64,
    pub frags: [Option<PacketBuffer>; PACKET_FRAGMENT_COUNT_MAX],
    pub have: u8,
    pub expecting: u8,
}

impl FragmentedPacket {
    #[inline(always)]
    pub fn new(ts: i64) -> Self {
        debug_assert_eq!(PACKET_FRAGMENT_COUNT_MAX, 8);
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
            /*
             * This works by setting bit N in the 'have' bit mask and then setting X bits
             * in 'expecting' if the 'expecting' field is non-zero. Since the packet head
             * does not carry the expecting fragment count (it will be provided as zero) and
             * all subsequent fragments should have the same fragment count, this will yield
             * a 'have' of 1 and an 'expecting' of 0 after the head arrives. Then 'expecting'
             * will be set to the right bit pattern by the first fragment and 'true' will get
             * returned once all fragments have arrived and therefore all flags in 'have' are
             * set.
             *
             * Receipt of a four-fragment packet would look like:
             *
             * after head      : have == 0x01, expecting == 0x00 -> false
             * after fragment 1: have == 0x03, expecting == 0x0f -> false
             * after fragment 2: have == 0x07, expecting == 0x0f -> false
             * after fragment 3: have == 0x0f, expecting == 0x0f -> true (done!)
             *
             * This algorithm is just a few instructions in ASM and also correctly handles
             * duplicated packet fragments. If all fragments never arrive receipt eventually
             * times out and this is discarded.
             */

            let _ = entry.insert(frag);

            self.have |= 1_u8.wrapping_shl(no as u32);
            self.expecting |= 0xff_u8.wrapping_shr(8 - (expecting as u32));
            self.have == self.expecting
        })
    }
}
