// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::sync::Arc;

use crate::protocol::PacketBuffer;
use crate::vl1::node::{HostSystem, InnerProtocol, PacketHandlerResult};
use crate::vl1::{Identity, Path, Peer};

pub trait SwitchInterface: Sync + Send {}

pub struct Switch {}

impl InnerProtocol for Switch {
    fn handle_packet<HostSystemImpl: HostSystem>(
        &self,
        source: &Arc<Peer<HostSystemImpl>>,
        source_path: &Arc<Path<HostSystemImpl>>,
        verb: u8,
        payload: &PacketBuffer,
    ) -> PacketHandlerResult {
        PacketHandlerResult::NotHandled
    }

    fn handle_error<HostSystemImpl: HostSystem>(
        &self,
        source: &Arc<Peer<HostSystemImpl>>,
        source_path: &Arc<Path<HostSystemImpl>>,
        in_re_verb: u8,
        in_re_message_id: u64,
        error_code: u8,
        payload: &PacketBuffer,
        cursor: &mut usize,
    ) -> PacketHandlerResult {
        PacketHandlerResult::NotHandled
    }

    fn handle_ok<HostSystemImpl: HostSystem>(
        &self,
        source: &Arc<Peer<HostSystemImpl>>,
        source_path: &Arc<Path<HostSystemImpl>>,
        in_re_verb: u8,
        in_re_message_id: u64,
        payload: &PacketBuffer,
        cursor: &mut usize,
    ) -> PacketHandlerResult {
        PacketHandlerResult::NotHandled
    }

    fn should_communicate_with(&self, id: &Identity) -> bool {
        true
    }
}

impl Switch {}
