/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::num::NonZeroI64;
use std::sync::Arc;
use std::time::Duration;

use crate::error::InvalidParameterError;
use crate::vl1::{Address, Identity, Endpoint, NodeInterface, Node};
use crate::vl2::{Switch, SwitchInterface};
use crate::{PacketBuffer, PacketBufferPool};

pub trait Interface: NodeInterface + SwitchInterface {}

pub struct NetworkHypervisor {
    vl1: Node,
    vl2: Switch,
}

impl NetworkHypervisor {
    pub fn new<CI: Interface>(ci: &CI, auto_generate_identity_type: Option<crate::vl1::identity::Type>) -> Result<NetworkHypervisor, InvalidParameterError> {
        Ok(NetworkHypervisor {
            vl1: Node::new(ci, auto_generate_identity_type)?,
            vl2: Switch::new(),
        })
    }

    /// Obtain a new packet buffer from the buffer pool.
    ///
    /// The returned object is a Pooled<Buffer<>> instance. The buffer is returned to the pool when the container is destroyed.
    #[inline(always)]
    pub fn get_packet_buffer(&self) -> PacketBuffer { self.vl1.get_packet_buffer() }

    /// Get a direct reference to the packet buffer pool.
    #[inline(always)]
    pub fn packet_buffer_pool(&self) -> &Arc<PacketBufferPool> { self.vl1.packet_buffer_pool() }

    #[inline(always)]
    pub fn address(&self) -> Address { self.vl1.address() }

    #[inline(always)]
    pub fn identity(&self) -> &Identity { self.vl1.identity() }

    pub fn do_background_tasks<CI: Interface>(&self, ci: &CI) -> Duration {
        self.vl1.do_background_tasks(ci)
    }

    #[inline(always)]
    pub fn wire_receive<CI: NodeInterface>(&self, ci: &CI, source_endpoint: &Endpoint, source_local_socket: Option<NonZeroI64>, source_local_interface: Option<NonZeroI64>, mut data: PacketBuffer) {
        self.vl1.wire_receive(ci, &self.vl2, source_endpoint, source_local_socket, source_local_interface, data)
    }
}
