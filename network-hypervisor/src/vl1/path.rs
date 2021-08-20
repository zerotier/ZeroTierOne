use std::collections::HashMap;
use std::sync::atomic::{AtomicI64, Ordering};

use parking_lot::Mutex;

use crate::util::U64PassThroughHasher;
use crate::vl1::Endpoint;
use crate::vl1::fragmentedpacket::FragmentedPacket;
use crate::vl1::node::{PacketBuffer, VL1CallerInterface};
use crate::vl1::protocol::*;

/// Keepalive interval for paths in milliseconds.
pub(crate) const PATH_KEEPALIVE_INTERVAL: i64 = 20000;

/// A remote endpoint paired with a local socket and a local interface.
/// These are maintained in Node and canonicalized so that all unique paths have
/// one and only one unique path object. That enables statistics to be tracked
/// for them and uniform application of things like keepalives.
pub struct Path {
    pub(crate) endpoint: Endpoint,
    pub(crate) local_socket: i64,
    pub(crate) local_interface: i64,
    last_send_time_ticks: AtomicI64,
    last_receive_time_ticks: AtomicI64,
    fragmented_packets: Mutex<HashMap<u64, FragmentedPacket, U64PassThroughHasher>>,
}

impl Path {
    pub(crate) const INTERVAL: i64 = PATH_KEEPALIVE_INTERVAL;

    #[inline(always)]
    pub fn new(endpoint: Endpoint, local_socket: i64, local_interface: i64) -> Self {
        Self {
            endpoint,
            local_socket,
            local_interface,
            last_send_time_ticks: AtomicI64::new(0),
            last_receive_time_ticks: AtomicI64::new(0),
            fragmented_packets: Mutex::new(HashMap::with_capacity_and_hasher(8, U64PassThroughHasher::new())),
        }
    }

    #[inline(always)]
    pub fn endpoint(&self) -> &Endpoint { &self.endpoint }

    #[inline(always)]
    pub fn local_socket(&self) -> i64 { self.local_socket }

    #[inline(always)]
    pub fn local_interface(&self) -> i64 { self.local_interface }

    #[inline(always)]
    pub fn last_send_time_ticks(&self) -> i64 { self.last_send_time_ticks.load(Ordering::Relaxed) }

    #[inline(always)]
    pub fn last_receive_time_ticks(&self) -> i64 { self.last_receive_time_ticks.load(Ordering::Relaxed) }

    /// Receive a fragment and return a FragmentedPacket if the entire packet was assembled.
    /// This returns None if more fragments are needed to assemble the packet.
    #[inline(always)]
    pub(crate) fn receive_fragment(&self, packet_id: PacketID, fragment_no: u8, fragment_expecting_count: u8, packet: PacketBuffer, time_ticks: i64) -> Option<FragmentedPacket> {
        let mut fp = self.fragmented_packets.lock();

        // This is mostly a defense against denial of service attacks or broken peers. It will
        // trim off about 1/3 of waiting packets if the total is over the limit.
        let fps = fp.len();
        if fps > FRAGMENT_MAX_INBOUND_PACKETS_PER_PATH {
            let mut entries: Vec<(i64, u64)> = Vec::new();
            entries.reserve(fps);
            for f in fp.iter() {
                entries.push((f.1.ts_ticks, *f.0));
            }
            entries.sort_unstable_by(|a, b| (*a).0.cmp(&(*b).0));
            for i in 0..(fps / 3) {
                let _ = fp.remove(&(*unsafe { entries.get_unchecked(i) }).1);
            }
        }

        if fp.entry(packet_id).or_insert_with(|| FragmentedPacket::new(time_ticks)).add_fragment(packet, fragment_no, fragment_expecting_count) {
            fp.remove(&packet_id)
        } else {
            None
        }
    }

    #[inline(always)]
    pub(crate) fn log_receive(&self, time_ticks: i64) {
        self.last_receive_time_ticks.store(time_ticks, Ordering::Relaxed);
    }

    #[inline(always)]
    pub(crate) fn log_send(&self, time_ticks: i64) {
        self.last_send_time_ticks.store(time_ticks, Ordering::Relaxed);
    }

    /// Called every INTERVAL during background tasks.
    #[inline(always)]
    pub(crate) fn on_interval<CI: VL1CallerInterface>(&self, ct: &CI, time_ticks: i64) {
        self.fragmented_packets.lock().retain(|packet_id, frag| (time_ticks - frag.ts_ticks) < FRAGMENT_EXPIRATION);
    }
}
