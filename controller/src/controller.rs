use std::sync::Arc;

use crate::database::Database;

use async_trait::async_trait;

use zerotier_network_hypervisor::protocol::{verbs, PacketBuffer};
use zerotier_network_hypervisor::util::dictionary::Dictionary;
use zerotier_network_hypervisor::util::marshalable::MarshalUnmarshalError;
use zerotier_network_hypervisor::vl1::{HostSystem, Identity, InnerProtocol, Path, Peer};
use zerotier_network_hypervisor::vl2::NetworkId;

pub struct Controller<DatabaseImpl: Database> {
    pub database: Arc<DatabaseImpl>,
}

impl<DatabaseImpl: Database> Controller<DatabaseImpl> {
    pub async fn new(database: Arc<DatabaseImpl>) -> Arc<Self> {
        Arc::new(Self { database })
    }

    async fn handle_network_config_request<HostSystemImpl: HostSystem>(
        &self,
        source: &Peer<HostSystemImpl>,
        source_path: &Path<HostSystemImpl>,
        payload: &PacketBuffer,
    ) -> Result<(), MarshalUnmarshalError> {
        let mut cursor = 0;
        let network_id = NetworkId::from_u64(payload.read_u64(&mut cursor)?);
        if network_id.is_none() {
            return Err(MarshalUnmarshalError::InvalidData);
        }
        let network_id = network_id.unwrap();
        let meta_data = if cursor < payload.len() {
            let meta_data_len = payload.read_u16(&mut cursor)?;
            let d = Dictionary::from_bytes(payload.read_bytes(meta_data_len as usize, &mut cursor)?);
            if d.is_none() {
                return Err(MarshalUnmarshalError::InvalidData);
            }
            d.unwrap()
        } else {
            Dictionary::new()
        };
        let (have_revision, have_timestamp) = if cursor < payload.len() {
            let r = payload.read_u64(&mut cursor)?;
            let t = payload.read_u64(&mut cursor)?;
            (Some(r), Some(t))
        } else {
            (None, None)
        };

        if let Ok(Some(network)) = self.database.get_network(network_id).await {}

        return Ok(());
    }
}

#[async_trait]
impl<DatabaseImpl: Database> InnerProtocol for Controller<DatabaseImpl> {
    async fn handle_packet<HostSystemImpl: HostSystem>(
        &self,
        source: &Peer<HostSystemImpl>,
        source_path: &Path<HostSystemImpl>,
        verb: u8,
        payload: &PacketBuffer,
    ) -> bool {
        match verb {
            verbs::VL2_VERB_NETWORK_CONFIG_REQUEST => {
                let _ = self.handle_network_config_request(source, source_path, payload).await;
                // TODO: display/log errors
                true
            }
            _ => false,
        }
    }

    async fn handle_error<HostSystemImpl: HostSystem>(
        &self,
        source: &Peer<HostSystemImpl>,
        source_path: &Path<HostSystemImpl>,
        in_re_verb: u8,
        in_re_message_id: u64,
        error_code: u8,
        payload: &PacketBuffer,
        cursor: &mut usize,
    ) -> bool {
        false
    }

    async fn handle_ok<HostSystemImpl: HostSystem>(
        &self,
        source: &Peer<HostSystemImpl>,
        source_path: &Path<HostSystemImpl>,
        in_re_verb: u8,
        in_re_message_id: u64,
        payload: &PacketBuffer,
        cursor: &mut usize,
    ) -> bool {
        false
    }

    fn should_communicate_with(&self, _: &Identity) -> bool {
        true
    }
}
