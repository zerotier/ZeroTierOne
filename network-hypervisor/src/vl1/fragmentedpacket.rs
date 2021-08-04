use std::sync::Arc;

use crate::vl1::node::PacketBuffer;
use crate::vl1::constants::FRAGMENT_COUNT_MAX;
use crate::vl1::Path;
use crate::vl1::protocol::PacketID;

/// Packet fragment re-assembler and container.
/// This is only used in the receive path.
pub(crate) struct FragmentedPacket {
    pub id: PacketID,
    pub ts_ticks: i64,
    pub frags: [Option<PacketBuffer>; FRAGMENT_COUNT_MAX],
    pub have: u8,
    pub expecting: u8,
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
    /// Return fragments to pool and reset id and ts_ticks to 0 and -1 respectively.
    #[inline(always)]
    pub fn clear(&mut self) {
        self.id = 0;
        self.ts_ticks = -1;
        self.frags.fill(None);
    }

    /// Initialize for a new packet and log the first fragment.
    /// This will panic if 'no' is out of bounds.
    #[inline(always)]
    pub fn first_fragment(&mut self, id: PacketID, ts_ticks: i64, frag: PacketBuffer, no: u8, expecting: u8) {
        self.id = id;
        self.ts_ticks = ts_ticks;
        let _ = self.frags[no as usize].replace(frag);
        self.have = 1;
        self.expecting = expecting;
    }

    /// Add a fragment to this fragment set and return true if the packet appears complete.
    /// This will panic if 'no' is out of bounds.
    #[inline(always)]
    pub fn add_fragment(&mut self, frag: PacketBuffer, no: u8, expecting: u8) -> bool {
        if self.frags[no as usize].replace(frag).is_none() {
            self.have = self.have.wrapping_add(1);
            self.expecting |= expecting; // in valid streams expecting is either 0 or the (same) total
            self.have == self.expecting
        } else {
            false
        }
    }
}
