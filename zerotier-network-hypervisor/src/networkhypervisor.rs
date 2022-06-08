// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::time::Duration;

use crate::error::InvalidParameterError;
use crate::util::buffer::Buffer;
use crate::util::marshalable::Marshalable;
use crate::vl1::node::*;
use crate::vl1::protocol::PooledPacketBuffer;
use crate::vl1::*;
use crate::vl2::switch::*;

pub trait Interface: SystemInterface + SwitchInterface {}

pub struct NetworkHypervisor<I: Interface> {
    vl1: Node<I>,
    vl2: Switch,
}

impl<I: Interface> NetworkHypervisor<I> {
    pub fn new(ii: &I, auto_generate_identity: bool, auto_upgrade_identity: bool) -> Result<Self, InvalidParameterError> {
        Ok(NetworkHypervisor {
            vl1: Node::new(ii, auto_generate_identity, auto_upgrade_identity)?,
            vl2: Switch::new(),
        })
    }

    #[inline(always)]
    pub fn get_packet_buffer(&self) -> PooledPacketBuffer {
        self.vl1.get_packet_buffer()
    }

    #[inline(always)]
    pub fn address(&self) -> Address {
        self.vl1.identity.address
    }

    #[inline(always)]
    pub fn identity(&self) -> &Identity {
        &self.vl1.identity
    }

    /// Run background tasks and return desired delay until next call in milliseconds.
    ///
    /// This shouldn't be called concurrently by more than one loop. Doing so would be harmless
    /// but would be a waste of compute cycles.
    #[inline(always)]
    pub fn do_background_tasks(&self, ii: &I) -> Duration {
        self.vl1.do_background_tasks(ii)
    }

    #[inline(always)]
    pub fn handle_incoming_physical_packet(&self, ii: &I, source_endpoint: &Endpoint, source_local_socket: &I::LocalSocket, source_local_interface: &I::LocalInterface, data: PooledPacketBuffer) {
        self.vl1.handle_incoming_physical_packet(ii, &self.vl2, source_endpoint, source_local_socket, source_local_interface, data)
    }

    /// Add or update a root set.
    ///
    /// If no root set exists by this name, a new root set is added. If one already
    /// exists it's checked against the new one and updated if the new set is valid
    /// and should supersede it.
    ///
    /// Changes will take effect within a few seconds when root sets are next
    /// examined and synchronized with peer and root list state.
    ///
    /// This returns true if the new root set was accepted and false otherwise.
    #[inline(always)]
    pub fn add_update_root_set(&self, rs: RootSet) -> bool {
        self.vl1.add_update_root_set(rs)
    }

    /// Add or update the compiled-in default ZeroTier RootSet.
    ///
    /// This is equivalent to unmarshaling default-rootset/root.zerotier.com.bin and then
    /// calling add_update_root_set().
    pub fn add_update_default_root_set(&self) -> bool {
        let mut buf: Buffer<4096> = Buffer::new();
        buf.set_to(include_bytes!("../default-rootset/root.zerotier.com.bin"));
        let mut cursor = 0;
        self.add_update_root_set(RootSet::unmarshal(&buf, &mut cursor).unwrap())
    }

    /// Call add_update_default_root_set if there are no roots defined, otherwise do nothing and return false.
    pub fn add_update_default_root_set_if_none(&self) -> bool {
        if self.vl1.has_roots_defined() {
            false
        } else {
            self.add_update_default_root_set()
        }
    }
}
