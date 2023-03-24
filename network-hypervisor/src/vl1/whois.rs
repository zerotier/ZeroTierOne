use std::collections::BTreeMap;
use std::convert::Infallible;
use std::ops::Bound;
use std::sync::{Mutex, Weak};

use super::address::PartialAddress;
use super::identity::Identity;
use super::node::{ApplicationLayer, InnerProtocolLayer, Node};
use super::path::Path;
use crate::debug_event;
use crate::protocol;

use zerotier_crypto::typestate::Valid;
use zerotier_utils::ringbuffer::RingBuffer;

pub(super) struct Whois<Application: ApplicationLayer + ?Sized> {
    whois_queue: Mutex<BTreeMap<PartialAddress, WhoisQueueItem<Application>>>,
}

struct WhoisQueueItem<Application: ApplicationLayer + ?Sized> {
    pending_v1_packets: RingBuffer<(Weak<Path<Application>>, protocol::PooledPacketBuffer), { protocol::WHOIS_MAX_WAITING_PACKETS }>,
    last_retry_time: i64,
    retry_count: u16,
}

impl<Application: ApplicationLayer + ?Sized> Whois<Application> {
    pub fn new() -> Self {
        Self { whois_queue: Mutex::new(BTreeMap::new()) }
    }

    pub fn query(
        &self,
        app: &Application,
        address: &PartialAddress,
        waiting_packet: Option<(Weak<Path<Application>>, protocol::PooledPacketBuffer)>,
        time_ticks: i64,
    ) {
    }

    pub fn handle_incoming_identity<Inner: InnerProtocolLayer + ?Sized>(
        &self,
        app: &Application,
        node: &Node<Application>,
        inner: &Inner,
        time_ticks: i64,
        identity: Valid<Identity>,
    ) {
        let mut queued_items = Vec::with_capacity(2);
        {
            // Iterate "up" the sorted list of pending requests since less specific addresses will be sorted
            // before more specific addresses. We keep going up until we find a non-matching address, matching
            // all partials that this full identity matches.
            let mut q = self.whois_queue.lock().unwrap();
            let mut to_delete = Vec::with_capacity(2);
            for qi in q.range((Bound::Unbounded, Bound::Included(identity.address.to_partial()))).rev() {
                if qi.0.matches(&identity.address) {
                    to_delete.push(qi.0.clone());
                    // TODO
                } else {
                    break;
                }
            }
            for a in to_delete {
                queued_items.push(q.remove(&a).unwrap());
            }
        }

        if let Some(peer) = node.peers.get_or_add(&node.identity_secret, &identity, time_ticks) {
            for qi in queued_items.iter() {
                for pkt in qi.pending_v1_packets.iter() {
                    if let Some(source_path) = pkt.0.upgrade() {
                        if let Ok(packet_header) = pkt.1.struct_at::<protocol::v1::PacketHeader>(0) {
                            peer.v1_proto_receive(node, app, inner, time_ticks, &source_path, packet_header, &pkt.1, &[]);
                        }
                    }
                }
            }
        }
    }

    pub fn retry_queued(&self) {}

    fn send_whois(&self, app: &Application, node: &Node<Application>, mut addresses: &[PartialAddress], time_ticks: i64) {
        debug_assert!(!addresses.is_empty());
        debug_event!(app, "[vl1] [v1] sending WHOIS for {}", {
            let mut tmp = String::new();
            for a in addresses.iter() {
                if !tmp.is_empty() {
                    tmp.push(',');
                }
                tmp.push_str(a.to_string().as_str());
            }
            tmp
        });

        if let Some(root) = node.best_root() {
            while !addresses.is_empty() {
                if !root
                    .send(app, node, None, time_ticks, |packet| -> Result<(), Infallible> {
                        assert!(packet.append_u8(protocol::message_type::VL1_WHOIS).is_ok());
                        while !addresses.is_empty() && (packet.len() + addresses[0].as_bytes().len()) <= protocol::UDP_DEFAULT_MTU {
                            debug_assert_eq!(addresses[0].as_bytes().len(), PartialAddress::LEGACY_SIZE_BYTES); // will need protocol work to support different partial sizes
                            assert!(packet.append_bytes(addresses[0].as_bytes()).is_ok());
                            addresses = &addresses[1..];
                        }
                        Ok(())
                    })
                    .is_some()
                {
                    break;
                }
            }
        }
    }
}
