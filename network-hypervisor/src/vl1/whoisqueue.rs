// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::collections::{HashMap, LinkedList};

use parking_lot::Mutex;

use crate::protocol::{PooledPacketBuffer, WHOIS_MAX_WAITING_PACKETS, WHOIS_RETRY_INTERVAL, WHOIS_RETRY_MAX};
use crate::util::gate::IntervalGate;
use crate::vl1::fragmentedpacket::FragmentedPacket;
use crate::vl1::node::{HostSystem, Node};
use crate::vl1::Address;

pub(crate) const SERVICE_INTERVAL_MS: i64 = WHOIS_RETRY_INTERVAL;

pub(crate) enum QueuedPacket {
    Unfragmented(PooledPacketBuffer),
    Fragmented(FragmentedPacket),
}

struct WhoisQueueItem {
    packet_queue: LinkedList<QueuedPacket>,
    retry_gate: IntervalGate<{ WHOIS_RETRY_INTERVAL }>,
    retry_count: u16,
}

pub(crate) struct WhoisQueue(Mutex<HashMap<Address, WhoisQueueItem>>);

impl WhoisQueue {
    pub fn new() -> Self {
        Self(Mutex::new(HashMap::new()))
    }

    /// Launch or renew a WHOIS query and enqueue a packet to be processed when (if) it is received.
    pub fn query<SI: HostSystem>(&self, node: &Node<SI>, si: &SI, target: Address, packet: Option<QueuedPacket>) {
        let mut q = self.0.lock();

        let qi = q.entry(target).or_insert_with(|| WhoisQueueItem {
            packet_queue: LinkedList::new(),
            retry_gate: IntervalGate::new(0),
            retry_count: 0,
        });

        if qi.retry_gate.gate(si.time_ticks()) {
            qi.retry_count += 1;
            if packet.is_some() {
                while qi.packet_queue.len() >= WHOIS_MAX_WAITING_PACKETS {
                    let _ = qi.packet_queue.pop_front();
                }
                qi.packet_queue.push_back(packet.unwrap());
            }
            self.send_whois(node, si, &[target]);
        }
    }

    /// Remove a WHOIS request from the queue and call the supplied function for all queued packets.
    #[allow(unused)]
    pub fn response_received_get_packets<F: FnMut(&mut QueuedPacket)>(&self, address: Address, packet_handler: F) {
        let mut qi = self.0.lock().remove(&address);
        let _ = qi.map(|mut qi| qi.packet_queue.iter_mut().for_each(packet_handler));
    }

    #[allow(unused)]
    fn send_whois<SI: HostSystem>(&self, node: &Node<SI>, si: &SI, targets: &[Address]) {
        todo!()
    }

    pub(crate) fn service<SI: HostSystem>(&self, si: &SI, node: &Node<SI>, time_ticks: i64) {
        let mut targets: Vec<Address> = Vec::new();
        self.0.lock().retain(|target, qi| {
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
            self.send_whois(node, si, targets.as_slice());
        }
    }
}
