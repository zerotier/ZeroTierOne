use std::collections::HashMap;
use std::sync::{Arc, Mutex, RwLock};

use crate::protocol;
use crate::protocol::PacketBuffer;
use crate::vl1::{Address, HostSystem, Identity, PacketHandlerResult, Peer};
use crate::vl2::{MulticastGroup, NetworkId};

/// Handler implementations for VL2_MULTICAST_LIKE and VL2_MULTICAST_GATHER.
///
/// Both controllers and roots will want to handle these, with the latter supporting them for legacy
/// reasons only. Regular nodes may also want to handle them in the future. So, break this out to allow
/// easy code reuse. To integrate call the appropriate method when the appropriate message type is
/// received and pass in a function to check whether specific network/identity combinations should be
/// processed. The GATHER implementation will send reply packets to the source peer.
pub struct MulticastAuthority {
    subscriptions: RwLock<HashMap<(NetworkId, MulticastGroup), Mutex<HashMap<Address, i64>>>>,
}

impl MulticastAuthority {
    fn handle_vl2_multicast_like<HostSystemImpl: HostSystem + ?Sized, Authenticator: Fn(NetworkId, &Identity) -> bool>(
        &self,
        auth: Authenticator,
        host_system: &HostSystemImpl,
        source: &Arc<Peer>,
        message_id: u64,
        payload: &PacketBuffer,
        mut cursor: usize,
    ) -> PacketHandlerResult {
        PacketHandlerResult::Ok
    }

    fn handle_vl2_multicast_gather<HostSystemImpl: HostSystem + ?Sized, Authenticator: Fn(NetworkId, &Identity) -> bool>(
        &self,
        auth: Authenticator,
        host_system: &HostSystemImpl,
        source: &Arc<Peer>,
        message_id: u64,
        payload: &PacketBuffer,
        mut cursor: usize,
    ) -> PacketHandlerResult {
        PacketHandlerResult::Ok
    }
}
