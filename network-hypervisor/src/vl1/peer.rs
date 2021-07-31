use std::sync::Arc;
use std::sync::atomic::{AtomicI64, AtomicU64, AtomicU8};

use crate::vl1::protocol::PacketID;
use crate::vl1::node::PacketBuffer;
use crate::vl1::constants::{FRAGMENT_COUNT_MAX, PEER_DEFRAGMENT_MAX_PACKETS_IN_FLIGHT};
use crate::vl1::{Identity, Path};

use parking_lot::Mutex;

struct FragmentedPacket {
    pub id: PacketID,
    pub frag: [Option<PacketBuffer>; FRAGMENT_COUNT_MAX],
}

pub struct Peer {
    // This peer's identity.
    identity: Identity,

    // Primary static secret resulting from key agreement with identity.
    identity_static_secret: [u8; 48],

    // Outgoing packet IV counter used to generate packet IDs to this peer.
    packet_iv_counter: AtomicU64,

    // Paths sorted in ascending order of quality / preference.
    paths: Mutex<Vec<Arc<Path>>>,

    // Incoming fragmented packet defragment buffer.
    fragmented_packets: Mutex<[FragmentedPacket; PEER_DEFRAGMENT_MAX_PACKETS_IN_FLIGHT]>,

    // Last send and receive time in millisecond ticks (not wall clock).
    last_send_time_ticks: AtomicI64,
    last_receive_time_ticks: AtomicI64,

    // Most recent remote version (most to least significant bytes: major, minor, revision, build)
    remote_version: AtomicU64,

    // Most recent remote protocol version
    remote_protocol_version: AtomicU8,
}
