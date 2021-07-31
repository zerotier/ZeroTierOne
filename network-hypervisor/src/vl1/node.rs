use std::sync::Arc;
use std::str::FromStr;
use std::time::Duration;
use std::marker::PhantomData;
use std::hash::Hash;

use crate::crypto::random::SecureRandom;
use crate::error::InvalidParameterError;
use crate::util::pool::{Pool, Pooled};
use crate::vl1::{Address, Identity, Endpoint, Locator};
use crate::vl1::buffer::Buffer;
use crate::vl1::constants::{PACKET_SIZE_MAX, FRAGMENT_COUNT_MAX};
use crate::vl1::path::Path;
use crate::vl1::peer::Peer;

use parking_lot::Mutex;
use dashmap::DashMap;

/// Standard packet buffer type including pool container.
pub type PacketBuffer = Pooled<Buffer<{ PACKET_SIZE_MAX }>>;

/// Callback interface and call context for calls to the node (for VL1).
/// Every non-trivial call takes a reference to this, which it passes all the way through
/// the call stack. This can be used to call back into the caller to send packets, get or
/// store data, report events, etc.
pub trait VL1CallerInterface {
    /// Node is up and ready for operation.
    fn event_node_is_up(&self);

    /// Node is shutting down.
    fn event_node_is_down(&self);

    /// A root signaled an identity collision.
    /// This should cause the external code to shut down this node, delete its identity, and recreate.
    fn event_identity_collision(&self);

    /// Node has gone online or offline.
    fn event_online_status_change(&self, online: bool);

    /// A USER_MESSAGE packet was received.
    fn event_user_message(&self, source: &Identity, message_type: u64, message: &[u8]);

    /// Load this node's identity from the data store.
    fn load_identity(&self) -> Option<&[u8]>;

    /// Save this node's identity.
    /// Note that this is only called on first startup (after up) and after identity_changed.
    fn save_identity(&self, id: &Identity, public: &[u8], secret: &[u8]);

    /// Load this node's latest locator.
    fn load_locator(&self) -> Option<&[u8]>;

    /// Save this node's latest locator.
    fn save_locator(&self, locator: &[u8]);

    /// Load a peer's latest saved state. (A remote peer, not this one.)
    fn load_peer(&self, address: Address) -> Option<&[u8]>;

    /// Save a peer's state.
    /// The state contains the identity, so there's no need to save that separately.
    /// It's just supplied for the address and if the external code wants it.
    fn save_peer(&self, id: &Identity, peer: &[u8]);

    /// Load network configuration.
    fn load_network_config(&self, id: u64) -> Option<&[u8]>;

    /// Save network configuration.
    fn save_network_config(&self, id: u64, config: &[u8]);

    /// Called to send a packet over the physical network (virtual -> physical).
    ///
    /// This may return false if the send definitely failed, and may return true if the send
    /// succeeded or may have succeeded (in the case of UDP and similar).
    ///
    /// If local socket and/or local interface are None, the sending code should make its
    /// own decision about what local socket or interface to use. It may send on a random
    /// one, the best fit, or all at once.
    ///
    /// If packet TTL is non-zero it should be used to set the packet TTL for outgoing packets
    /// for supported protocols such as UDP, but otherwise it can be ignored. It can also be
    /// ignored if the platform does not support setting the TTL.
    fn wire_send(&self, endpoint: &Endpoint, local_socket: Option<i64>, local_interface: Option<i64>, data: PacketBuffer, packet_ttl: u8) -> bool;

    /// Called to check and see if a physical address should be used for ZeroTier traffic to a node.
    fn check_path(&self, id: &Identity, endpoint: &Endpoint, local_socket: Option<i64>, local_interface: Option<i64>) -> bool;

    /// Called to look up a path to a known node.
    /// If a path is found, this returns a tuple of an endpoint and optional local socket and local
    /// interface IDs. If these are None they will be None when this is sent with wire_send.
    fn get_path_hints(&self, id: &Identity) -> Option<&[(&Endpoint, Option<i64>, Option<i64>)]>;

    /// Called to get the current time in milliseconds from the system monotonically increasing clock.
    fn time_ticks(&self) -> i64;

    /// Called to get the current time in milliseconds since epoch from the real-time clock.
    fn time_clock(&self) -> i64;
}

pub struct Node {
    identity: Identity,
    locator: Mutex<Option<Locator>>,
    paths_by_inaddr: DashMap<u128, Arc<Path>>,
    peers: DashMap<Address, Arc<Peer>>,
    peer_vec: Mutex<Vec<Arc<Peer>>>, // for rapid iteration through all peers
    buffer_pool: Pool<Buffer<{ PACKET_SIZE_MAX }>>,
    secure_prng: SecureRandom,
}

impl Node {
    /// Create a new Node.
    /// If the auto-generate identity type is not None, a new identity will be generated if
    /// no identity is currently stored in the data store.
    pub fn new<CI: VL1CallerInterface>(ci: &CI, auto_generate_identity_type: Option<crate::vl1::identity::Type>) -> Result<Self, InvalidParameterError> {
        crate::crypto::init(); // make sure this is initialized, okay to call more than once

        let id = {
            let id_str = ci.load_identity();
            if id_str.is_none() {
                if auto_generate_identity_type.is_none() {
                    return Err(InvalidParameterError("no identity found and auto-generate not specified"));
                } else {
                    let id = Identity::generate(auto_generate_identity_type.unwrap());
                    ci.save_identity(&id, id.to_string().as_bytes(), id.to_secret_string().as_bytes());
                    id
                }
            } else {
                let id_str = String::from_utf8_lossy(id_str.unwrap());
                let id = Identity::from_str(id_str.as_ref());
                if id.is_err() {
                    return Err(InvalidParameterError("invalid identity"));
                } else {
                    id.unwrap()
                }
            }
        };

        Ok(Self {
            identity: id,
            locator: Mutex::new(None),
            paths_by_inaddr: DashMap::new(),
            peers: DashMap::new(),
            peer_vec: Mutex::new(Vec::new()),
            buffer_pool: Pool::new(64),
            secure_prng: SecureRandom::get(),
        })
    }

    /// Get address, short for .identity().address()
    #[inline(always)]
    pub fn address(&self) -> Address {
        self.identity.address()
    }

    /// Get identity, which includes secret keys.
    #[inline(always)]
    pub fn identity(&self) -> &Identity {
        &self.identity
    }

    /// Get a reusable packet buffer.
    /// The buffer will automatically be returned to the pool if it is dropped.
    #[inline(always)]
    pub fn get_packet_buffer(&self) -> PacketBuffer {
        self.buffer_pool.get()
    }

    /// Run background tasks and return desired delay until next call in milliseconds.
    /// This should only be called once at a time. It technically won't hurt anything to
    /// call concurrently but it will waste CPU cycles.
    pub fn do_background_tasks<CI: VL1CallerInterface>(&self, ci: &CI) -> Duration {
        Duration::from_millis(1000)
    }

    /// Called when a packet is received on the physical wire.
    pub fn wire_receive<CI: VL1CallerInterface>(&self, ci: &CI, endpoint: &Endpoint, local_socket: i64, local_interface: i64, data: PacketBuffer) {
    }
}

unsafe impl Send for Node {}

unsafe impl Sync for Node {}
