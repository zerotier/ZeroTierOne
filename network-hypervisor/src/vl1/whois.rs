use std::collections::{HashMap, LinkedList};

use parking_lot::Mutex;

use crate::util::gate::IntervalGate;
use crate::vl1::Address;
use crate::vl1::constants::*;
use crate::vl1::fragmentedpacket::FragmentedPacket;
use crate::vl1::node::{Node, PacketBuffer, VL1CallerInterface};

/// Frequency for WHOIS retries
pub const WHOIS_RETRY_INTERVAL: i64 = 1000;

/// Maximum number of WHOIS retries
pub const WHOIS_RETRY_MAX: u16 = 3;

/// Maximum number of packets to queue up behind a WHOIS.
pub const WHOIS_MAX_WAITING_PACKETS: usize = 64;

pub(crate) enum QueuedPacket {
    Singular(PacketBuffer),
    Fragmented(FragmentedPacket)
}

struct WhoisQueueItem {
    packet_queue: LinkedList<QueuedPacket>,
    retry_gate: IntervalGate<{ WHOIS_RETRY_INTERVAL }>,
    retry_count: u16,
}

pub(crate) struct WhoisQueue {
    queue: Mutex<HashMap<Address, WhoisQueueItem>>
}

impl WhoisQueue {
    pub(crate) const INTERVAL: i64 = WHOIS_RETRY_INTERVAL;

    pub fn new() -> Self {
        Self {
            queue: Mutex::new(HashMap::new())
        }
    }

    pub fn query<CI: VL1CallerInterface>(&self, node: &Node, ci: &CI, target: Address, packet: Option<QueuedPacket>) {
        let mut q = self.queue.lock();

        let qi = q.entry(target).or_insert_with(|| WhoisQueueItem {
            packet_queue: LinkedList::new(),
            retry_gate: IntervalGate::new(0),
            retry_count: 0,
        });

        if qi.retry_gate.gate(ci.time_ticks()) {
            qi.retry_count += 1;
            if packet.is_some() {
                while qi.packet_queue.len() >= WHOIS_MAX_WAITING_PACKETS {
                    let _ = qi.packet_queue.pop_front();
                }
                qi.packet_queue.push_back(packet.unwrap());
            }
            self.send_whois(node, ci, &[target]);
        }
    }

    /// Remove a WHOIS request from the queue and call the supplied function for all queued packets.
    pub fn response_received_get_packets<F: FnMut(&mut QueuedPacket)>(&self, address: Address, packet_handler: F) {
        let mut qi = self.queue.lock().remove(&address);
        let _ = qi.map(|mut qi| qi.packet_queue.iter_mut().for_each(packet_handler));
    }

    /// Called every INTERVAL during background tasks.
    pub fn on_interval<CI: VL1CallerInterface>(&self, node: &Node, ci: &CI, time_ticks: i64) {
        let mut targets: Vec<Address> = Vec::new();
        self.queue.lock().retain(|target, qi| {
            if qi.retry_count < WHOIS_RETRY_MAX {
                if qi.retry_gate.gate(time_ticks) {
                    qi.retry_count += 1;
                    targets.push(*target);
                }
                true
            } else {
                false
            }
        });
        if !targets.is_empty() {
            self.send_whois(node, ci, targets.as_slice());
        }
    }

    fn send_whois<CI: VL1CallerInterface>(&self, node: &Node, ci: &CI, targets: &[Address]) {
        todo!()
    }
}
