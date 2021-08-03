use std::sync::Arc;
use std::sync::atomic::{AtomicI64, AtomicU64, AtomicU8};

use crate::vl1::{Identity, Path};
use crate::vl1::fragmentedpacket::FragmentedPacket;
use crate::vl1::protocol::{PacketID, PacketHeader};
use crate::vl1::node::{VL1CallerInterface, PacketBuffer, Node};

use parking_lot::Mutex;

const MAX_PATHS: usize = 16;

struct TxState {
    packet_iv_counter: u64,
    last_send_time_ticks: i64,
    paths: [Arc<Path>; MAX_PATHS],
}

struct RxState {
    last_receive_time_ticks: i64,
    remote_version: [u8; 4],
    remote_protocol_version: u8,
}

/// A remote peer known to this node.
/// Sending-related and receiving-related fields are locked separately since concurrent
/// send/receive is not uncommon.
pub struct Peer {
    identity: Identity,

    // Static shared secret computed from agreement with identity.
    identity_static_secret: [u8; 48],

    // State used primarily when sending to this peer.
    txs: Mutex<TxState>,

    // State used primarily when receiving from this peer.
    rxs: Mutex<RxState>,
}

impl Peer {
    pub(crate) fn receive_from_singular<CI: VL1CallerInterface>(&self, node: &Node, ci: &CI, header: &PacketHeader, packet: &PacketBuffer) {
    }

    pub(crate) fn receive_from_fragmented<CI: VL1CallerInterface>(&self, node: &Node, ci: CI, header: &PacketHeader, packet: &FragmentedPacket) {
    }
}
