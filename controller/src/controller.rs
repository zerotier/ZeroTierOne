// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::sync::Arc;

use tokio::time::{Duration, Instant};
use zerotier_utils::tokio;

use zerotier_network_hypervisor::protocol::{verbs, PacketBuffer};
use zerotier_network_hypervisor::vl1::{HostSystem, Identity, InnerProtocol, PacketHandlerResult, Path, PathFilter, Peer};
use zerotier_network_hypervisor::vl2::NetworkId;

use zerotier_utils::dictionary::Dictionary;
use zerotier_utils::reaper::Reaper;

use crate::database::Database;

const REQUEST_TIMEOUT: Duration = Duration::from_secs(10);

pub struct Controller<DatabaseImpl: Database> {
    database: Arc<DatabaseImpl>,
    reaper: Reaper,
    runtime: tokio::runtime::Handle,
}

impl<DatabaseImpl: Database> Controller<DatabaseImpl> {
    pub fn new(database: Arc<DatabaseImpl>, runtime: tokio::runtime::Handle) -> Arc<Self> {
        Arc::new(Self { database, reaper: Reaper::new(&runtime), runtime })
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
        println!(
            "handle_network_config_request {} {} {}",
            source.identity.to_string(),
            source_path.endpoint.to_string(),
            network_id.to_string()
        );
        if let Ok(Some(network)) = database.get_network(network_id).await {}
    }
}

impl<DatabaseImpl: Database> PathFilter for Controller<DatabaseImpl> {
    fn check_path<HostSystemImpl: HostSystem>(
        &self,
        _id: &Identity,
        _endpoint: &zerotier_network_hypervisor::vl1::Endpoint,
        _local_socket: Option<&HostSystemImpl::LocalSocket>,
        _local_interface: Option<&HostSystemImpl::LocalInterface>,
    ) -> bool {
        true
    }

    fn get_path_hints<HostSystemImpl: HostSystem>(
        &self,
        _id: &Identity,
    ) -> Option<
        Vec<(
            zerotier_network_hypervisor::vl1::Endpoint,
            Option<HostSystemImpl::LocalSocket>,
            Option<HostSystemImpl::LocalInterface>,
        )>,
    > {
        None
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
                let mut cursor = 1;

                let network_id = payload.read_u64(&mut cursor);
                if network_id.is_err() {
                    return PacketHandlerResult::Error;
                }
                let network_id = NetworkId::from_u64(network_id.unwrap());
                if network_id.is_none() {
                    return PacketHandlerResult::Error;
                }
                let network_id = network_id.unwrap();

                let meta_data = if (cursor + 2) < payload.len() {
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

                let (have_revision, have_timestamp) = if (cursor + 16) <= payload.len() {
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
                        self.runtime.spawn(Self::handle_network_config_request(
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
                    eprintln!("WARNING: Instant::now() + REQUEST_TIMEOUT overflowed! should be impossible.");
                }

                PacketHandlerResult::Ok
            }
            _ => PacketHandlerResult::NotHandled,
        }
    }

    fn handle_error<HostSystemImpl: HostSystem>(
        &self,
        _source: &Arc<Peer<HostSystemImpl>>,
        _source_path: &Arc<Path<HostSystemImpl>>,
        _in_re_verb: u8,
        _in_re_message_id: u64,
        _error_code: u8,
        _payload: &PacketBuffer,
        _cursor: &mut usize,
    ) -> PacketHandlerResult {
        PacketHandlerResult::NotHandled
    }

    fn handle_ok<HostSystemImpl: HostSystem>(
        &self,
        _source: &Arc<Peer<HostSystemImpl>>,
        _source_path: &Arc<Path<HostSystemImpl>>,
        _in_re_verb: u8,
        _in_re_message_id: u64,
        _payload: &PacketBuffer,
        _cursor: &mut usize,
    ) -> PacketHandlerResult {
        PacketHandlerResult::NotHandled
    }

    fn should_communicate_with(&self, _: &Identity) -> bool {
        true
    }
}
