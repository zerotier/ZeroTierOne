// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use async_trait::async_trait;

use crate::vl1::node::{InnerProtocolInterface, SystemInterface};
use crate::vl1::protocol::*;
use crate::vl1::{Identity, Path, Peer};

pub trait SwitchInterface: Sync + Send {}

pub struct Switch {}

#[async_trait]
impl InnerProtocolInterface for Switch {
    #[allow(unused)]
    async fn handle_packet<SI: SystemInterface>(&self, peer: &Peer<SI>, source_path: &Path<SI>, verb: u8, payload: &PacketBuffer) -> bool {
        false
    }

    #[allow(unused)]
    async fn handle_error<SI: SystemInterface>(
        &self,
        peer: &Peer<SI>,
        source_path: &Path<SI>,
        in_re_verb: u8,
        in_re_message_id: u64,
        error_code: u8,
        payload: &PacketBuffer,
        cursor: &mut usize,
    ) -> bool {
        false
    }

    #[allow(unused)]
    async fn handle_ok<SI: SystemInterface>(
        &self,
        peer: &Peer<SI>,
        source_path: &Path<SI>,
        in_re_verb: u8,
        in_re_message_id: u64,
        payload: &PacketBuffer,
        cursor: &mut usize,
    ) -> bool {
        false
    }

    #[allow(unused)]
    fn should_communicate_with(&self, id: &Identity) -> bool {
        true
    }
}

impl Switch {
    pub async fn new() -> Self {
        Self {}
    }
}
