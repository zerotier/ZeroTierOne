use std::sync::Arc;

use crate::vl1::node::PacketBuffer;
use crate::vl1::constants::FRAGMENT_COUNT_MAX;
use crate::vl1::Path;
use crate::vl1::protocol::PacketID;

pub struct FragmentedPacket {
    pub id: PacketID,
    pub ts_ticks: i64,
    frags: [Option<PacketBuffer>; FRAGMENT_COUNT_MAX],
    have: u8,
    expecting: u8,
}

impl Default for FragmentedPacket {
    fn default() -> Self {
        Self {
            id: 0,
            ts_ticks: -1,
            frags: [None; FRAGMENT_COUNT_MAX],
            have: 0,
            expecting: 0,
        }
    }
}

impl FragmentedPacket {
    /// Reset this fragmented packet for re-use.
    #[inline(always)]
    pub fn reset(&mut self) {
        self.id = 0;
        self.ts_ticks = -1;
        self.frags.fill(None);
        self.have = 0;
        self.expecting = 0;
    }

    /// Initialize for a new packet.
    #[inline(always)]
    pub fn init(&mut self, id: PacketID, ts_ticks: i64) {
        self.id = id;
        self.ts_ticks = ts_ticks;
    }

    /// Add a fragment to this fragment set and return true if the packet appears complete.
    #[inline(always)]
    pub fn add(&mut self, frag: PacketBuffer, no: u8, expecting: u8) -> bool {
        if self.frags[no].replace(frag).is_none() {
            self.have = self.have.wrapping_add(1);
            self.expecting |= expecting;
            self.have == self.expecting
        } else {
            false
        }
    }
}
