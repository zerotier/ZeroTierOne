/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use crate::vl1::node::{InnerProtocolInterface, SystemInterface};
use crate::vl1::protocol::*;
use crate::vl1::{Identity, Path, Peer};

pub trait SwitchInterface: Sync + Send {}

pub struct Switch {}

impl InnerProtocolInterface for Switch {
    fn handle_packet<SI: SystemInterface>(&self, peer: &Peer<SI>, source_path: &Path<SI>, forward_secrecy: bool, extended_authentication: bool, verb: u8, payload: &PacketBuffer) -> bool {
        false
    }

    fn handle_error<SI: SystemInterface>(&self, peer: &Peer<SI>, source_path: &Path<SI>, forward_secrecy: bool, extended_authentication: bool, in_re_verb: u8, in_re_message_id: u64, error_code: u8, payload: &PacketBuffer, cursor: &mut usize) -> bool {
        false
    }

    fn handle_ok<SI: SystemInterface>(&self, peer: &Peer<SI>, source_path: &Path<SI>, forward_secrecy: bool, extended_authentication: bool, in_re_verb: u8, in_re_message_id: u64, payload: &PacketBuffer, cursor: &mut usize) -> bool {
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
