use std::sync::atomic::{AtomicI64, Ordering};

use crate::vl1::Endpoint;
use crate::vl1::constants::FRAGMENT_COUNT_MAX;
use crate::vl1::fragmentedpacket::FragmentedPacket;
use crate::vl1::protocol::{FragmentHeader, PacketID};
use crate::vl1::node::PacketBuffer;

use parking_lot::Mutex;

struct RxState {
    last_receive_time_ticks: i64,
    fragmented_packet_count: usize,
    fragmented_packets: [FragmentedPacket; FRAGMENT_COUNT_MAX],
}

pub struct Path {
    pub(crate) endpoint: Endpoint,
    pub(crate) local_socket: i64,
    pub(crate) local_interface: i64,
    last_send_time_ticks: AtomicI64,
    rxs: Mutex<RxState>,
}

impl Path {
    #[inline(always)]
    pub fn new(endpoint: Endpoint, local_socket: i64, local_interface: i64) -> Self {
        Self {
            endpoint,
            local_socket,
            local_interface,
            last_send_time_ticks: AtomicI64::new(0),
            rxs: Mutex::new(RxState {
                last_receive_time_ticks: 0,
                fragmented_packet_count: 0,
                fragmented_packets: [FragmentedPacket::default(); FRAGMENT_COUNT_MAX]
            })
        }
    }

    #[inline(always)]
    pub fn last_send_time_ticks(&self) -> i64 {
        self.last_send_time_ticks.load(Ordering::Relaxed)
    }

    #[inline(always)]
    pub fn send_receive_time_ticks(&self) -> i64 {
        self.rxs.lock().last_receive_time_ticks
    }

    #[inline(always)]
    pub(crate) fn receive_fragment<F: FnOnce(&mut FragmentedPacket)>(&self, packet_id: PacketID, fragment_no: u8, fragment_expecting_count: u8, packet: PacketBuffer, time_ticks: i64, assembled_packet_handler: F) {
        if fragment_no < FRAGMENT_COUNT_MAX as u8 {
            let mut rxs = self.rxs.lock();
            rxs.last_receive_time_ticks = time_ticks;

            let mut fpcnt = rxs.fragmented_packet_count;
            let mut fidx = 0;
            while fpcnt > 0 {
                let mut f = &mut rxs.fragmented_packets[fidx];
                if f.id == packet_id {
                    if f.add(packet, fragment_no, fragment_expecting_count) {
                        assembled_packet_handler(f);
                        f.reset();
                        rxs.fragmented_packet_count = rxs.fragmented_packet_count.wrapping_sub(1);
                    }
                    return;
                } else if f.ts_ticks >= 0 {
                    fpcnt = fpcnt.wrapping_sub(1);
                }
                fidx = fidx.wrapping_add(1);
            }

            let mut oldest_ts = rxs.fragmented_packets[0].ts_ticks;
            let mut oldest_idx = 0;
            if oldest_ts >= 0 {
                for fidx in 1..FRAGMENT_COUNT_MAX {
                    let ts = rxs.fragmented_packets[fidx].ts_ticks;
                    if ts < oldest_ts {
                        oldest_ts = ts;
                        oldest_idx = fidx;
                    }
                }
            }

            let mut f = &mut rxs.fragmented_packets[oldest_idx];
            f.init(packet_id, time_ticks);
            let _ = f.add(packet, fragment_no, fragment_expecting_count);
        }
    }
}

unsafe impl Send for Path {}

unsafe impl Sync for Path {}
