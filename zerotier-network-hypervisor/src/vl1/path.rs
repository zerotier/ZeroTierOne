/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::collections::HashMap;
use std::hash::Hasher;
use std::io::Write;
use std::num::NonZeroI64;
use std::sync::Arc;
use std::sync::atomic::{AtomicI64, Ordering};

use arc_swap::ArcSwap;
use highway::HighwayHash;
use parking_lot::Mutex;

use crate::PacketBuffer;
use crate::util::{highwayhasher, U64NoOpHasher};
use crate::vl1::Endpoint;
use crate::vl1::fragmentedpacket::FragmentedPacket;
use crate::vl1::node::VL1SystemInterface;
use crate::vl1::protocol::*;

/// Keepalive interval for paths in milliseconds.
pub(crate) const PATH_KEEPALIVE_INTERVAL: i64 = 20000;

lazy_static! {
    static mut RANDOM_64BIT_SALT_0: u64 = zerotier_core_crypto::random::next_u64_secure();
    static mut RANDOM_64BIT_SALT_1: u64 = zerotier_core_crypto::random::next_u64_secure();
    static mut RANDOM_64BIT_SALT_2: u64 = zerotier_core_crypto::random::next_u64_secure();
}

/// A remote endpoint paired with a local socket and a local interface.
/// These are maintained in Node and canonicalized so that all unique paths have
/// one and only one unique path object. That enables statistics to be tracked
/// for them and uniform application of things like keepalives.
pub struct Path {
    endpoint: ArcSwap<Endpoint>,
    local_socket: Option<NonZeroI64>,
    local_interface: Option<NonZeroI64>,
    last_send_time_ticks: AtomicI64,
    last_receive_time_ticks: AtomicI64,
    fragmented_packets: Mutex<HashMap<u64, FragmentedPacket, U64NoOpHasher>>,
}

impl Path {
    /// Get a 128-bit key to look up this endpoint in the local node path map.
    #[inline(always)]
    pub(crate) fn local_lookup_key(endpoint: &Endpoint, local_socket: Option<NonZeroI64>, local_interface: Option<NonZeroI64>) -> u128 {
        let local_socket = local_socket.map_or(0, |s| crate::util::hash64_noncrypt(RANDOM_64BIT_SALT_0 + s.get() as u64));
        let local_interface = local_interface.map_or(0, |s| crate::util::hash64_noncrypt(RANDOM_64BIT_SALT_1 + s.get() as u64));
        let lsi = (local_socket as u128).wrapping_shl(64) | (local_interface as u128);
        match endpoint {
            Endpoint::Nil => 0,
            Endpoint::ZeroTier(a) => a.to_u64() as u128,
            Endpoint::Ethernet(m) => (m.to_u64() | 0x0100000000000000) as u128 ^ lsi,
            Endpoint::WifiDirect(m) => (m.to_u64() | 0x0200000000000000) as u128 ^ lsi,
            Endpoint::Bluetooth(m) => (m.to_u64() | 0x0400000000000000) as u128 ^ lsi,
            Endpoint::Ip(ip) => ip.ip_as_native_u128().wrapping_sub(lsi), // naked IP has no port
            Endpoint::IpUdp(ip) => ip.ip_as_native_u128().wrapping_add(lsi), // UDP maintains one path per IP but merely learns the most recent port
            Endpoint::IpTcp(ip) => ip.ip_as_native_u128().wrapping_sub(crate::util::hash64_noncrypt((ip.port() as u64).wrapping_add(RANDOM_64BIT_SALT_2)) as u128).wrapping_sub(lsi),
            Endpoint::Http(s) => {
                let mut hh = highwayhasher();
                let _ = hh.write_all(s.as_bytes());
                let _ = hh.write_u64(local_socket);
                let _ = hh.write_u64(local_interface);
                u128::from_ne_bytes(unsafe { *hh.finalize128().as_ptr().cast() })
            }
            Endpoint::WebRTC(b) => {
                let mut hh = highwayhasher();
                let _ = hh.write_u64(local_socket);
                let _ = hh.write_u64(local_interface);
                let _ = hh.write_all(b.as_slice());
                u128::from_ne_bytes(unsafe { *hh.finalize128().as_ptr().cast() })
            }
        }
    }

    #[inline(always)]
    pub fn new(endpoint: Endpoint, local_socket: Option<NonZeroI64>, local_interface: Option<NonZeroI64>) -> Self {
        Self {
            endpoint: ArcSwap::new(Arc::new(endpoint)),
            local_socket,
            local_interface,
            last_send_time_ticks: AtomicI64::new(0),
            last_receive_time_ticks: AtomicI64::new(0),
            fragmented_packets: Mutex::new(HashMap::with_capacity_and_hasher(4, U64NoOpHasher::new())),
        }
    }

    #[inline(always)]
    pub fn endpoint(&self) -> Arc<Endpoint> { self.endpoint.load_full() }

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

        // This is optimized for the fragmented case because that's the most common when transferring data.
        if fp.entry(packet_id).or_insert_with(|| FragmentedPacket::new(time_ticks)).add_fragment(packet, fragment_no, fragment_expecting_count) {
            fp.remove(&packet_id)
        } else {
            None
        }
    }

    #[inline(always)]
    pub(crate) fn log_receive_anything(&self, time_ticks: i64) {
        self.last_receive_time_ticks.store(time_ticks, Ordering::Relaxed);
    }

    #[inline(always)]
    pub(crate) fn log_receive_authenticated_packet(&self, _bytes: usize, source_endpoint: &Endpoint) {
        let mut replace = false;
        match source_endpoint {
            Endpoint::IpUdp(ip) => {
                let ep = self.endpoint.load();
                match ep.as_ref() {
                    Endpoint::IpUdp(ip_orig) => {
                        debug_assert!(ip_orig.ip_bytes().eq(ip.ip_bytes()));
                        if ip_orig.port() != ip.port() {
                            replace = true;
                        }
                    },
                    _ => {}
                }
            },
            _ => {}
        }
        if replace {
            self.endpoint.swap(Arc::new(source_endpoint.clone()));
        }
    }

    #[inline(always)]
    pub(crate) fn log_send_anything(&self, time_ticks: i64) {
        self.last_send_time_ticks.store(time_ticks, Ordering::Relaxed);
    }

    pub(crate) const CALL_EVERY_INTERVAL_MS: i64 = PATH_KEEPALIVE_INTERVAL;

    #[inline(always)]
    pub(crate) fn call_every_interval<CI: VL1SystemInterface>(&self, ct: &CI, time_ticks: i64) {
        self.fragmented_packets.lock().retain(|packet_id, frag| (time_ticks - frag.ts_ticks) < PACKET_FRAGMENT_EXPIRATION);
    }
}
