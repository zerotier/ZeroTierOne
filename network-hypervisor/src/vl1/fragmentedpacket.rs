// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use crate::protocol::*;

/// Packet fragment re-assembler and container.
///
/// Performance note: PacketBuffer is Pooled<Buffer> which is NotNull<*mut Buffer>.
/// That means Option<PacketBuffer> is just a pointer, since NotNull permits the
/// compiler to optimize out any additional state in Option.
pub(crate) struct FragmentedPacket {
    pub ts_ticks: i64,
    pub frags: [Option<PooledPacketBuffer>; v1::FRAGMENT_COUNT_MAX],
    pub have: u8,
    pub expecting: u8,
}

impl FragmentedPacket {
    pub fn new(ts: i64) -> Self {
        // 'have' and 'expecting' must be expanded if this is >8
        debug_assert!(v1::FRAGMENT_COUNT_MAX <= 8);

        Self {
            ts_ticks: ts,
            frags: Default::default(),
            have: 0,
            expecting: 0,
        }
    }

    /// Add a fragment to this fragment set and return true if all fragments are present.
    #[inline(always)]
    pub fn add_fragment(&mut self, frag: PooledPacketBuffer, no: u8, expecting: u8) -> bool {
        if let Some(entry) = self.frags.get_mut(no as usize) {
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
        } else {
            false
        }
    }
}
