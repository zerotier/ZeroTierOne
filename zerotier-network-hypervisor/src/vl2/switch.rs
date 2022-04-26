/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::sync::Arc;

use crate::util::buffer::Buffer;
use crate::vl1::node::InnerProtocolInterface;
use crate::vl1::protocol::*;
use crate::vl1::{Identity, Path, Peer};

pub trait SwitchInterface: Sync + Send {}

pub struct Switch {}

impl InnerProtocolInterface for Switch {
    fn handle_packet(&self, peer: &Peer, source_path: &Arc<Path>, forward_secrecy: bool, extended_authentication: bool, verb: u8, payload: &Buffer<{ PACKET_SIZE_MAX }>) -> bool {
        false
    }

    fn handle_error(&self, peer: &Peer, source_path: &Arc<Path>, forward_secrecy: bool, extended_authentication: bool, in_re_verb: u8, in_re_message_id: u64, error_code: u8, payload: &Buffer<{ PACKET_SIZE_MAX }>, cursor: &mut usize) -> bool {
        false
    }

    fn handle_ok(&self, peer: &Peer, source_path: &Arc<Path>, forward_secrecy: bool, extended_authentication: bool, in_re_verb: u8, in_re_message_id: u64, payload: &Buffer<{ PACKET_SIZE_MAX }>, cursor: &mut usize) -> bool {
        false
    }

    fn has_trust_relationship(&self, id: &Identity) -> bool {
        true
    }
}

impl Switch {
    pub fn new() -> Self {
        Self {}
    }
}
