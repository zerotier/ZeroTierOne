/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::collections::HashMap;
use std::num::NonZeroI64;
use std::sync::atomic::{AtomicI64, Ordering};

use parking_lot::Mutex;

use crate::PacketBuffer;
use crate::util::U64NoOpHasher;
use crate::vl1::Endpoint;
use crate::vl1::fragmentedpacket::FragmentedPacket;
use crate::vl1::node::NodeInterface;
use crate::vl1::protocol::*;

/// Keepalive interval for paths in milliseconds.
pub(crate) const PATH_KEEPALIVE_INTERVAL: i64 = 20000;

/// A remote endpoint paired with a local socket and a local interface.
/// These are maintained in Node and canonicalized so that all unique paths have
/// one and only one unique path object. That enables statistics to be tracked
/// for them and uniform application of things like keepalives.
pub struct Path {
    endpoint: Endpoint,
    local_socket: Option<NonZeroI64>,
    local_interface: Option<NonZeroI64>,
    last_send_time_ticks: AtomicI64,
    last_receive_time_ticks: AtomicI64,
    fragmented_packets: Mutex<HashMap<u64, FragmentedPacket, U64NoOpHasher>>,
}

impl Path {
    #[inline(always)]
    pub fn new(endpoint: Endpoint, local_socket: Option<NonZeroI64>, local_interface: Option<NonZeroI64>) -> Self {
        Self {
            endpoint,
            local_socket,
            local_interface,
            last_send_time_ticks: AtomicI64::new(0),
            last_receive_time_ticks: AtomicI64::new(0),
            fragmented_packets: Mutex::new(HashMap::with_capacity_and_hasher(4, U64NoOpHasher::new())),
        }
    }

    #[inline(always)]
    pub fn endpoint(&self) -> &Endpoint { &self.endpoint }

    #[inline(always)]
    pub fn local_socket(&self) -> Option<NonZeroI64> { self.local_socket }

    #[inline(always)]
    pub fn local_interface(&self) -> Option<NonZeroI64> { self.local_interface }

    #[inline(always)]
    pub fn last_send_time_ticks(&self) -> i64 { self.last_send_time_ticks.load(Ordering::Relaxed) }

    #[inline(always)]
    pub fn last_receive_time_ticks(&self) -> i64 { self.last_receive_time_ticks.load(Ordering::Relaxed) }

    /// Receive a fragment and return a FragmentedPacket if the entire packet was assembled.
    /// This returns None if more fragments are needed to assemble the packet.
    #[inline(always)]
    pub(crate) fn receive_fragment(&self, packet_id: PacketID, fragment_no: u8, fragment_expecting_count: u8, packet: PacketBuffer, time_ticks: i64) -> Option<FragmentedPacket> {
        let mut fp = self.fragmented_packets.lock();

        // Discard some old waiting packets if the total incoming fragments for a path exceeds a
        // sanity limit. This is to prevent memory exhaustion DOS attacks.
        let fps = fp.len();
        if fps > PACKET_FRAGMENT_MAX_INBOUND_PACKETS_PER_PATH {
            let mut entries: Vec<(i64, u64)> = Vec::new();
            entries.reserve(fps);
            for f in fp.iter() {
                entries.push((f.1.ts_ticks, *f.0));
            }
            entries.sort_unstable_by(|a, b| (*a).0.cmp(&(*b).0));
            for i in 0..(fps / 3) {
                let _ = fp.remove(&(*entries.get(i).unwrap()).1);
            }
        }

        // This is optimized for the fragmented case because that's the most common when transferring data.
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

    /// Desired period between calls to call_every_interval().
    pub(crate) const CALL_EVERY_INTERVAL_MS: i64 = PATH_KEEPALIVE_INTERVAL;

    /// Called every INTERVAL during background tasks.
    #[inline(always)]
    pub(crate) fn call_every_interval<CI: NodeInterface>(&self, ct: &CI, time_ticks: i64) {
        self.fragmented_packets.lock().retain(|packet_id, frag| (time_ticks - frag.ts_ticks) < PACKET_FRAGMENT_EXPIRATION);
    }
}
