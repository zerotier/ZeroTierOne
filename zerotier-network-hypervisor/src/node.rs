/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::sync::Arc;
use std::time::Duration;

use crate::vl1::node::VL1CallerInterface;
use crate::error::InvalidParameterError;
use crate::vl1::{PacketBuffer, PacketBufferPool, Address, Identity, Endpoint};

pub trait CallerInterface: VL1CallerInterface {
}

/// A complete ZeroTier node.
///
/// This is a composition of the VL1 node and the VL2 virtual switch.
pub struct Node {
    vl1: crate::vl1::node::Node,
    vl2: crate::vl2::switch::Switch,
}

impl Node {
    pub fn new<CI: CallerInterface>(ci: &CI, auto_generate_identity_type: Option<crate::vl1::identity::Type>) -> Result<Node, InvalidParameterError> {
        Ok(Node {
            vl1: crate::vl1::node::Node::new(ci, auto_generate_identity_type)?,
            vl2: crate::vl2::switch::Switch::new(),
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

    #[inline(always)]
    pub fn fips_mode(&self) -> bool { self.vl1.fips_mode() }

    pub fn do_background_tasks<CI: CallerInterface>(&self, ci: &CI) -> Duration {
        self.vl1.do_background_tasks(ci)
    }

    #[inline(always)]
    pub fn wire_receive<CI: VL1CallerInterface>(&self, ci: &CI, source_endpoint: &Endpoint, source_local_socket: i64, source_local_interface: i64, mut data: PacketBuffer) {
        self.vl1.wire_receive(ci, &self.vl2, source_endpoint, source_local_socket, source_local_interface, data)
    }
}
