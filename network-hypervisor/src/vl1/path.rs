use std::sync::atomic::{AtomicI64, Ordering};

use crate::vl1::Endpoint;
use crate::vl1::constants::{FRAGMENT_COUNT_MAX, FRAGMENT_EXPIRATION};
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

    /// Receive a fragment and invoke the handler if a packet appears fully assembled.
    /// This also updates last receive time, etc.
    #[inline(always)]
    pub(crate) fn receive_fragment<F: FnOnce(&mut FragmentedPacket)>(&self, packet_id: PacketID, fragment_no: u8, fragment_expecting_count: u8, packet: PacketBuffer, time_ticks: i64, assembled_packet_handler: F) {
        if fragment_no < FRAGMENT_COUNT_MAX as u8 {
            let mut rxs = self.rxs.lock();
            rxs.last_receive_time_ticks = time_ticks;

            // In most situlations this algorithms runs right through and doesn't need to iterate.
            // If there are no fragments fpcnt will be 0 and the first loop will skip. If there are
            // no fragments then the second loop won't be needed either since the first slot will
            // be open. Looping only happens when there are multiple fragments in flight, which is
            // not a common scenario for peer-to-peer links. The maximum iteration count in the
            // worst case is only 2*FRAGMENT_COUNT_MAX and the loops are only doing integer
            // comparisons, so the worst case is still linear.

            let mut fragmented_packets_to_check = rxs.fragmented_packet_count;
            let mut i = 0;
            while fragmented_packets_to_check > 0 {
                let mut f = &mut rxs.fragmented_packets[i];
                if f.id == packet_id {
                    if f.add_fragment(packet, fragment_no, fragment_expecting_count) {
                        assembled_packet_handler(f);
                        f.clear();
                        rxs.fragmented_packet_count -= 1;
                    }
                    return;
                } else if f.ts_ticks >= 0 {
                    if (time_ticks - f.ts_ticks) > FRAGMENT_EXPIRATION {
                        f.clear();
                        rxs.fragmented_packet_count -= 1;
                    }
                    fragmented_packets_to_check -= 1;
                }
                i += 1;
            }

            let mut oldest_ts = &mut rxs.fragmented_packets[0];
            let mut oldest_ts_ticks = oldest_ts.ts_ticks;
            if oldest_ts_ticks >= 0 {
                for fidx in 1..FRAGMENT_COUNT_MAX {
                    let ts = &mut rxs.fragmented_packets[fidx];
                    let tst = ts.ts_ticks;
                    if tst < oldest_ts_ticks {
                        oldest_ts = ts;
                        oldest_ts_ticks = tst;
                        if tst < 0 {
                            break;
                        }
                    }
                }
            }

            if oldest_ts_ticks < 0 {
                rxs.fragmented_packet_count += 1;
            } else {
                oldest_ts.clear();
            }
            rxs.fragmented_packets[oldest_idx].init(packet_id, time_ticks, packet, fragment_no, fragment_expecting_count);
        }
    }

    /// Register receipt of "anything" else which right now includes unfragmented packets and keepalives.
    #[inline(always)]
    pub(crate) fn receive_other(&self, time_ticks: i64) {
        self.rxs.lock().last_receive_time_ticks = time_ticks;
    }
}

unsafe impl Send for Path {}

unsafe impl Sync for Path {}
