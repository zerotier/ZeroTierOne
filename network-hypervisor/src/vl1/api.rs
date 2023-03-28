use std::hash::Hash;
use std::sync::Arc;

use super::endpoint::Endpoint;
use super::event::Event;
use super::identity::Identity;
use super::node::Node;
use super::path::Path;
use super::peer::Peer;

use crate::protocol::{PacketBuffer, PooledPacketBuffer};
use zerotier_crypto::typestate::Valid;

/// Interface trait to be implemented by code that's using the ZeroTier network hypervisor.
///
/// This is analogous to a C struct full of function pointers to callbacks along with some
/// associated type definitions.
pub trait ApplicationLayer: Sync + Send + 'static {
    /// Type for local system sockets.
    type LocalSocket: Sync + Send + Hash + PartialEq + Eq + Clone + ToString + Sized + 'static;

    /// Type for local system interfaces.
    type LocalInterface: Sync + Send + Hash + PartialEq + Eq + Clone + ToString + Sized + 'static;

    /// A VL1 level event occurred.
    fn event(&self, event: Event);

    /// Get a pooled packet buffer for internal use.
    fn get_buffer(&self) -> PooledPacketBuffer;

    /// Check a local socket for validity.
    ///
    /// This could return false if the socket's interface no longer exists, its port has been
    /// unbound, etc.
    fn local_socket_is_valid(&self, socket: &Self::LocalSocket) -> bool;

    /// Check if this node should respond to messages from a given peer at all.
    ///
    /// The default implementation always returns true. Typically this is what you want for a
    /// controller or a root but not a regular node (unless required for backward compatibility).
    #[allow(unused)]
    fn should_respond_to(&self, id: &Valid<Identity>) -> bool {
        true
    }

    /// Called to send a packet over the physical network (virtual -> physical).
    ///
    /// This sends with UDP-like semantics. It should do whatever best effort it can and return.
    ///
    /// If a local socket is specified the implementation should send from that socket or not
    /// at all (returning false). If a local interface is specified the implementation should
    /// send from all sockets on that interface. If neither is specified the packet may be
    /// sent on all sockets or a random subset.
    ///
    /// For endpoint types that support a packet TTL, the implementation may set the TTL
    /// if the 'ttl' parameter is not zero. If the parameter is zero or TTL setting is not
    /// supported, the default TTL should be used. This parameter is ignored for types that
    /// don't support it.
    fn wire_send(
        &self,
        endpoint: &Endpoint,
        local_socket: Option<&Self::LocalSocket>,
        local_interface: Option<&Self::LocalInterface>,
        data: &[u8],
        packet_ttl: u8,
    );

    /// Called to check and see if a physical address should be used for ZeroTier traffic to a node.
    ///
    /// The default implementation always returns true.
    #[allow(unused_variables)]
    fn should_use_physical_path<Application: ApplicationLayer>(
        &self,
        id: &Valid<Identity>,
        endpoint: &Endpoint,
        local_socket: Option<&Application::LocalSocket>,
        local_interface: Option<&Application::LocalInterface>,
    ) -> bool {
        true
    }

    /// Called to look up any statically defined or memorized paths to known nodes.
    ///
    /// The default implementation always returns None.
    #[allow(unused_variables)]
    fn get_path_hints<Application: ApplicationLayer>(
        &self,
        id: &Valid<Identity>,
    ) -> Option<Vec<(Endpoint, Option<Application::LocalSocket>, Option<Application::LocalInterface>)>> {
        None
    }

    /// Called to get the current time in milliseconds from the system monotonically increasing clock.
    /// This needs to be accurate to about 250 milliseconds resolution or better.
    fn time_ticks(&self) -> i64;

    /// Called to get the current time in milliseconds since epoch from the real-time clock.
    /// This needs to be accurate to about one second resolution or better.
    fn time_clock(&self) -> i64;
}

/// Result of a packet handler in the InnerProtocolLayer trait.
pub enum PacketHandlerResult {
    /// Packet was handled successfully.
    Ok,

    /// Packet was handled and an error occurred (malformed, authentication failure, etc.)
    Error,

    /// Packet was not handled by this handler.
    NotHandled,
}

/// Interface between VL1 and higher/inner protocol layers.
///
/// This is implemented by Switch in VL2. It's usually not used outside of VL2 in the core but
/// it could also be implemented for testing or "off label" use of VL1 to carry different protocols.
#[allow(unused)]
pub trait InnerProtocolLayer: Sync + Send {
    /// Handle a packet, returning true if it was handled by the next layer.
    ///
    /// Do not attempt to handle OK or ERROR. Instead implement handle_ok() and handle_error().
    /// The default version returns NotHandled.
    fn handle_packet<Application: ApplicationLayer>(
        &self,
        app: &Application,
        node: &Node<Application>,
        source: &Arc<Peer<Application>>,
        source_path: &Arc<Path<Application>>,
        source_hops: u8,
        message_id: u64,
        verb: u8,
        payload: &PacketBuffer,
        cursor: usize,
    ) -> PacketHandlerResult {
        PacketHandlerResult::NotHandled
    }

    /// Handle errors, returning true if the error was recognized.
    /// The default version returns NotHandled.
    fn handle_error<Application: ApplicationLayer>(
        &self,
        app: &Application,
        node: &Node<Application>,
        source: &Arc<Peer<Application>>,
        source_path: &Arc<Path<Application>>,
        source_hops: u8,
        message_id: u64,
        in_re_verb: u8,
        in_re_message_id: u64,
        error_code: u8,
        payload: &PacketBuffer,
        cursor: usize,
    ) -> PacketHandlerResult {
        PacketHandlerResult::NotHandled
    }

    /// Handle an OK, returning true if the OK was recognized.
    /// The default version returns NotHandled.
    fn handle_ok<Application: ApplicationLayer>(
        &self,
        app: &Application,
        node: &Node<Application>,
        source: &Arc<Peer<Application>>,
        source_path: &Arc<Path<Application>>,
        source_hops: u8,
        message_id: u64,
        in_re_verb: u8,
        in_re_message_id: u64,
        payload: &PacketBuffer,
        cursor: usize,
    ) -> PacketHandlerResult {
        PacketHandlerResult::NotHandled
    }
}
