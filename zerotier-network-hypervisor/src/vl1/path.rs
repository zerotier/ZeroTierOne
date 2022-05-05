/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::collections::HashMap;
use std::hash::{Hash, Hasher};
use std::num::NonZeroI64;
use std::sync::atomic::{AtomicI64, Ordering};

use lazy_static::lazy_static;
use metrohash::MetroHash128;
use parking_lot::Mutex;
use zerotier_core_crypto::random;

use crate::util::*;
use crate::vl1::fragmentedpacket::FragmentedPacket;
use crate::vl1::node::*;
use crate::vl1::protocol::*;
use crate::vl1::{endpoint, Endpoint};
use crate::PacketBuffer;

lazy_static! {
    static ref METROHASH_SEED: u64 = random::next_u64_secure();
}

/// A remote endpoint paired with a local socket and a local interface.
/// These are maintained in Node and canonicalized so that all unique paths have
/// one and only one unique path object. That enables statistics to be tracked
/// for them and uniform application of things like keepalives.
pub struct Path {
    pub endpoint: Endpoint,
    pub local_socket: Option<NonZeroI64>,
    pub local_interface: Option<NonZeroI64>,
    last_send_time_ticks: AtomicI64,
    last_receive_time_ticks: AtomicI64,
    fragmented_packets: Mutex<HashMap<u64, FragmentedPacket, U64NoOpHasher>>,
}

impl Path {
    /// Get a 128-bit key to look up this endpoint in the local node path map.
    pub(crate) fn local_lookup_key(endpoint: &Endpoint, local_socket: Option<NonZeroI64>, local_interface: Option<NonZeroI64>) -> u128 {
        let mut h = MetroHash128::with_seed(*METROHASH_SEED);
        h.write_u64(local_socket.map_or(0, |s| s.get() as u64));
        h.write_u64(local_interface.map_or(0, |s| s.get() as u64));
        match endpoint {
            Endpoint::Nil => h.write_u8(endpoint::TYPE_NIL),
            Endpoint::ZeroTier(_, fingerprint) => {
                h.write_u8(endpoint::TYPE_ZEROTIER);
                h.write(fingerprint);
            }
            Endpoint::Ethernet(m) => {
                h.write_u8(endpoint::TYPE_ETHERNET);
                h.write_u64(m.to_u64());
            }
            Endpoint::WifiDirect(m) => {
                h.write_u8(endpoint::TYPE_WIFIDIRECT);
                h.write_u64(m.to_u64());
            }
            Endpoint::Bluetooth(m) => {
                h.write_u8(endpoint::TYPE_BLUETOOTH);
                h.write_u64(m.to_u64());
            }
            Endpoint::Ip(ip) => {
                h.write_u8(endpoint::TYPE_IP);
                h.write(ip.ip_bytes());
            }
            Endpoint::IpUdp(ip) => {
                h.write_u8(endpoint::TYPE_IPUDP);
                ip.hash(&mut h);
            }
            Endpoint::IpTcp(ip) => {
                h.write_u8(endpoint::TYPE_IPTCP);
                ip.hash(&mut h);
            }
            Endpoint::Http(s) => {
                h.write_u8(endpoint::TYPE_HTTP);
                h.write(s.as_bytes());
            }
            Endpoint::WebRTC(b) => {
                h.write_u8(endpoint::TYPE_WEBRTC);
                h.write(b.as_slice());
            }
            Endpoint::ZeroTierEncap(_, fingerprint) => {
                h.write_u8(endpoint::TYPE_ZEROTIER_ENCAP);
                h.write(fingerprint);
            }
        }
        assert_eq!(std::mem::size_of::<(u64, u64)>(), std::mem::size_of::<u128>());
        unsafe { std::mem::transmute(h.finish128()) }
    }

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

    /// Receive a fragment and return a FragmentedPacket if the entire packet was assembled.
    /// This returns None if more fragments are needed to assemble the packet.
    pub(crate) fn receive_fragment(&self, packet_id: u64, fragment_no: u8, fragment_expecting_count: u8, packet: PacketBuffer, time_ticks: i64) -> Option<FragmentedPacket> {
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

        if fp.entry(packet_id).or_insert_with(|| FragmentedPacket::new(time_ticks)).add_fragment(packet, fragment_no, fragment_expecting_count) {
            fp.remove(&packet_id)
        } else {
            None
        }
    }

    /// Called when any packet is received.
    #[inline(always)]
    pub(crate) fn log_receive_anything(&self, time_ticks: i64) {
        self.last_receive_time_ticks.store(time_ticks, Ordering::Relaxed);
    }

    #[inline(always)]
    pub(crate) fn log_send_anything(&self, time_ticks: i64) {
        self.last_send_time_ticks.store(time_ticks, Ordering::Relaxed);
    }
}

impl BackgroundServicable for Path {
    const SERVICE_INTERVAL_MS: i64 = PATH_KEEPALIVE_INTERVAL;

    fn service<SI: SystemInterface>(&self, si: &SI, _: &Node, time_ticks: i64) -> bool {
        self.fragmented_packets.lock().retain(|_, frag| (time_ticks - frag.ts_ticks) < PACKET_FRAGMENT_EXPIRATION);
        if (time_ticks - self.last_send_time_ticks.load(Ordering::Relaxed)) >= PATH_KEEPALIVE_INTERVAL {
            self.last_send_time_ticks.store(time_ticks, Ordering::Relaxed);
            si.wire_send(&self.endpoint, self.local_socket, self.local_interface, &[&ZEROES[..1]], 0);
        }
        true
    }
}
