use crate::database::Database;

use std::sync::Arc;

use tokio::time::{Duration, Instant};

use zerotier_network_hypervisor::protocol::{verbs, PacketBuffer};
use zerotier_network_hypervisor::util::dictionary::Dictionary;
use zerotier_network_hypervisor::vl1::{HostSystem, Identity, InnerProtocol, PacketHandlerResult, Path, Peer};
use zerotier_network_hypervisor::vl2::NetworkId;

use zerotier_utils::reaper::Reaper;

const REQUEST_TIMEOUT: Duration = Duration::from_secs(10);

pub struct Controller<DatabaseImpl: Database> {
    database: Arc<DatabaseImpl>,
    reaper: Reaper,
}

impl<DatabaseImpl: Database> Controller<DatabaseImpl> {
    pub async fn new(database: Arc<DatabaseImpl>) -> Arc<Self> {
        Arc::new(Self { database, reaper: Reaper::new() })
    }

    async fn handle_network_config_request<HostSystemImpl: HostSystem>(
        database: Arc<DatabaseImpl>,
        source: Arc<Peer<HostSystemImpl>>,
        source_path: Arc<Path<HostSystemImpl>>,
        network_id: NetworkId,
        meta_data: Dictionary,
        have_revision: Option<u64>,
        have_timestamp: Option<u64>,
    ) {
        if let Ok(Some(network)) = database.get_network(network_id).await {}
    }
}

impl<DatabaseImpl: Database> InnerProtocol for Controller<DatabaseImpl> {
    fn handle_packet<HostSystemImpl: HostSystem>(
        &self,
        source: &Arc<Peer<HostSystemImpl>>,
        source_path: &Arc<Path<HostSystemImpl>>,
        verb: u8,
        payload: &PacketBuffer,
    ) -> PacketHandlerResult {
        match verb {
            verbs::VL2_VERB_NETWORK_CONFIG_REQUEST => {
                let mut cursor = 0;
                let network_id = payload.read_u64(&mut cursor);
                if network_id.is_err() {
                    return PacketHandlerResult::Error;
                }
                let network_id = NetworkId::from_u64(network_id.unwrap());
                if network_id.is_none() {
                    return PacketHandlerResult::Error;
                }
                let network_id = network_id.unwrap();
                let meta_data = if cursor < payload.len() {
                    let meta_data_len = payload.read_u16(&mut cursor);
                    if meta_data_len.is_err() {
                        return PacketHandlerResult::Error;
                    }
                    if let Ok(d) = payload.read_bytes(meta_data_len.unwrap() as usize, &mut cursor) {
                        let d = Dictionary::from_bytes(d);
                        if d.is_none() {
                            return PacketHandlerResult::Error;
                        }
                        d.unwrap()
                    } else {
                        return PacketHandlerResult::Error;
                    }
                } else {
                    Dictionary::new()
                };
                let (have_revision, have_timestamp) = if cursor < payload.len() {
                    let r = payload.read_u64(&mut cursor);
                    let t = payload.read_u64(&mut cursor);
                    if r.is_err() || t.is_err() {
                        return PacketHandlerResult::Error;
                    }
                    (Some(r.unwrap()), Some(t.unwrap()))
                } else {
                    (None, None)
                };

                if let Some(deadline) = Instant::now().checked_add(REQUEST_TIMEOUT) {
                    self.reaper.add(
                        tokio::spawn(Self::handle_network_config_request(
                            self.database.clone(),
                            source.clone(),
                            source_path.clone(),
                            network_id,
                            meta_data,
                            have_revision,
                            have_timestamp,
                        )),
                        deadline,
                    );
                } else {
                    eprintln!("WARNING: instant + REQUEST_TIMEOUT overflowed! should be impossible.");
                }

                PacketHandlerResult::Ok
            }
            _ => PacketHandlerResult::NotHandled,
        }
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

    fn should_communicate_with(&self, _: &Identity) -> bool {
        true
    }
}
