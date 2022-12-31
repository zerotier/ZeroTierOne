// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::sync::Arc;

use crate::protocol::PacketBuffer;
use crate::vl1::{ApplicationLayer, InnerProtocolLayer, Node, PacketHandlerResult, Path, Peer};

pub trait SwitchInterface: Sync + Send {}

pub struct Switch {}

#[allow(unused_variables)]
impl InnerProtocolLayer for Switch {
    fn handle_packet<Application: ApplicationLayer + ?Sized>(
        &self,
        app: &Application,
        node: &Node,
        source: &Arc<Peer>,
        source_path: &Arc<Path>,
        source_hops: u8,
        message_id: u64,
        verb: u8,
        payload: &PacketBuffer,
        cursor: usize,
    ) -> PacketHandlerResult {
        PacketHandlerResult::NotHandled
    }

    fn handle_error<Application: ApplicationLayer + ?Sized>(
        &self,
        app: &Application,
        node: &Node,
        source: &Arc<Peer>,
        source_path: &Arc<Path>,
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

    fn handle_ok<Application: ApplicationLayer + ?Sized>(
        &self,
        app: &Application,
        node: &Node,
        source: &Arc<Peer>,
        source_path: &Arc<Path>,
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

impl Switch {}
