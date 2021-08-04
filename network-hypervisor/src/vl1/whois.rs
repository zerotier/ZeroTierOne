use std::collections::HashMap;

use parking_lot::Mutex;

use crate::util::gate::IntervalGate;
use crate::vl1::Address;
use crate::vl1::constants::*;
use crate::vl1::fragmentedpacket::FragmentedPacket;
use crate::vl1::node::{Node, PacketBuffer, VL1CallerInterface};

pub(crate) enum QueuedPacket {
    Singular(PacketBuffer),
    Fragmented(FragmentedPacket)
}

struct WhoisQueueItem {
    packet_queue: Vec<QueuedPacket>,
    retry_gate: IntervalGate<{ WHOIS_RETRY_INTERVAL }>,
    retry_count: u16,
}

pub(crate) struct WhoisQueue {
    queue: Mutex<HashMap<Address, WhoisQueueItem>>
}

impl WhoisQueue {
    pub const INTERVAL: i64 = WHOIS_RETRY_INTERVAL;

    pub fn new() -> Self {
        Self {
            queue: Mutex::new(HashMap::new())
        }
    }

    pub fn query<CI: VL1CallerInterface>(&self, node: &Node, ci: &CI, target: Address, packet: Option<QueuedPacket>) {
        let mut q = self.queue.lock();

        let qi = q.entry(target).or_insert_with(|| WhoisQueueItem {
            packet_queue: Vec::new(),
            retry_gate: IntervalGate::new(0),
            retry_count: 0,
        });

        if qi.retry_gate.gate(ci.time_ticks()) {
            qi.retry_count += 1;
            if packet.is_some() {
                qi.packet_queue.push(packet.unwrap());
            }
            self.send_whois(node, ci, &[target]);
        }
    }

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
