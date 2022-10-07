// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::error::Error;
use std::sync::Arc;

use tokio::time::{Duration, Instant};

use zerotier_network_hypervisor::protocol::{verbs, PacketBuffer, DEFAULT_MULTICAST_LIMIT, ZEROTIER_VIRTUAL_NETWORK_DEFAULT_MTU};
use zerotier_network_hypervisor::vl1::{HostSystem, Identity, InnerProtocol, Node, PacketHandlerResult, Path, PathFilter, Peer};
use zerotier_network_hypervisor::vl2::{CertificateOfMembership, CertificateOfOwnership, NetworkConfig, NetworkId, Tag};
use zerotier_utils::dictionary::Dictionary;
use zerotier_utils::error::UnexpectedError;
use zerotier_utils::ms_since_epoch;
use zerotier_utils::reaper::Reaper;
use zerotier_utils::tokio;

use crate::database::Database;
use crate::model::{AuthorizationResult, Member, CREDENTIAL_WINDOW_SIZE_DEFAULT};

const REQUEST_TIMEOUT: Duration = Duration::from_secs(10);

pub struct Handler<DatabaseImpl: Database> {
    reaper: Reaper,
    runtime: tokio::runtime::Handle,
    inner: Arc<Inner<DatabaseImpl>>,
}

impl<DatabaseImpl: Database> Handler<DatabaseImpl> {
    pub fn new(database: Arc<DatabaseImpl>, runtime: tokio::runtime::Handle, local_identity: Identity) -> Arc<Self> {
        assert!(local_identity.secret.is_some());
        Arc::new(Self {
            reaper: Reaper::new(&runtime),
            runtime,
            inner: Arc::new(Inner::<DatabaseImpl> { database, local_identity }),
        })
    }
}

impl<DatabaseImpl: Database> PathFilter for Handler<DatabaseImpl> {
    fn should_use_physical_path<HostSystemImpl: HostSystem>(
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

impl<DatabaseImpl: Database> InnerProtocol for Handler<DatabaseImpl> {
    fn handle_packet<HostSystemImpl: HostSystem>(
        &self,
        _node: &Node<HostSystemImpl>,
        source: &Arc<Peer<HostSystemImpl>>,
        source_path: &Arc<Path<HostSystemImpl>>,
        message_id: u64,
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

                // Launch handler as an async background task.
                let (inner, source2, source_path2) = (self.inner.clone(), source.clone(), source_path.clone());
                self.reaper.add(
                    self.runtime.spawn(async move {
                        // TODO: log errors
                        let _ = inner.handle_network_config_request(
                            source2,
                            source_path2,
                            message_id,
                            network_id,
                            meta_data,
                            have_revision,
                            have_timestamp,
                        );
                    }),
                    Instant::now().checked_add(REQUEST_TIMEOUT).unwrap(),
                );

                PacketHandlerResult::Ok
            }
            _ => PacketHandlerResult::NotHandled,
        }
    }

    fn handle_error<HostSystemImpl: HostSystem>(
        &self,
        _node: &Node<HostSystemImpl>,
        _source: &Arc<Peer<HostSystemImpl>>,
        _source_path: &Arc<Path<HostSystemImpl>>,
        _message_id: u64,
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
        _node: &Node<HostSystemImpl>,
        _source: &Arc<Peer<HostSystemImpl>>,
        _source_path: &Arc<Path<HostSystemImpl>>,
        _message_id: u64,
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

struct Inner<DatabaseImpl: Database> {
    database: Arc<DatabaseImpl>,
    local_identity: Identity,
}

impl<DatabaseImpl: Database> Inner<DatabaseImpl> {
    async fn handle_network_config_request<HostSystemImpl: HostSystem>(
        self: Arc<Self>,
        source: Arc<Peer<HostSystemImpl>>,
        _source_path: Arc<Path<HostSystemImpl>>,
        _message_id: u64,
        network_id: NetworkId,
        _meta_data: Dictionary,
        _have_revision: Option<u64>,
        _have_timestamp: Option<u64>,
    ) -> Result<(AuthorizationResult, Option<NetworkConfig>), Box<dyn Error>> {
        let network = self.database.get_network(network_id).await?;
        if network.is_none() {
            // TODO: send error
            return Ok((AuthorizationResult::Rejected, None));
        }
        let network = network.unwrap();

        let mut member = self.database.get_member(network_id, source.identity.address).await?;
        let mut member_changed = false;
        let legacy_v1 = source.identity.p384.is_none();

        // If we have a member object and a pinned identity, check to make sure it matches.
        if let Some(member) = member.as_ref() {
            if let Some(pinned_identity) = member.identity.as_ref() {
                if !pinned_identity.eq(&source.identity) {
                    return Ok((AuthorizationResult::RejectedIdentityMismatch, None));
                }
            }
        }

        let now = ms_since_epoch();

        let mut authorization_result = AuthorizationResult::Rejected;
        let mut authorized = member.as_ref().map_or(false, |m| m.authorized());
        if !authorized {
            if member.is_none() {
                if network.learn_members {
                    let _ = member.insert(Member::new_with_identity(source.identity.clone(), network_id));
                    member_changed = true;
                } else {
                    return Ok((AuthorizationResult::Rejected, None));
                }
            }

            if !network.private {
                authorization_result = AuthorizationResult::ApprovedOnPublicNetwork;
                authorized = true;
                member.as_mut().unwrap().last_authorized_time = Some(now);
                member_changed = true;
            }
        }

        let mut member = member.unwrap();

        let nc: Option<NetworkConfig> = if authorized {
            // ====================================================================================
            // Authorized requests are handled here
            // ====================================================================================

            // TODO: check SSO

            // Figure out time bounds for the certificate to generate.
            let max_delta = network.credential_window_size.unwrap_or(CREDENTIAL_WINDOW_SIZE_DEFAULT);

            // Get a list of all network members that were deauthorized but are still within the time window.
            // These will be issued revocations to remind the node not to speak to them until they fall off.
            let deauthed_members_still_in_window = self.database.list_members_deauthorized_after(network.id, now - max_delta).await;

            // Check and if necessary auto-assign static IPs for this member.
            member_changed |= network.check_zt_ip_assignments(self.database.as_ref(), &mut member).await;

            let mut nc = NetworkConfig::new(network_id, source.identity.address);

            nc.name = member.name.clone();
            nc.private = network.private;
            nc.timestamp = now;
            nc.max_delta = max_delta;
            nc.revision = now as u64;
            nc.mtu = network.mtu.unwrap_or(ZEROTIER_VIRTUAL_NETWORK_DEFAULT_MTU as u16);
            nc.multicast_limit = network.multicast_limit.unwrap_or(DEFAULT_MULTICAST_LIMIT as u32);
            nc.routes = network.ip_routes;
            nc.static_ips = member.ip_assignments.clone();
            nc.rules = network.rules;
            nc.dns = network.dns;

            nc.certificate_of_membership = Some(
                CertificateOfMembership::new(&self.local_identity, network_id, &source.identity, now, max_delta, legacy_v1)
                    .ok_or(UnexpectedError)?,
            );

            let mut coo = CertificateOfOwnership::new(network_id, now, source.identity.address, legacy_v1);
            for ip in nc.static_ips.iter() {
                coo.add_ip(ip);
            }
            if !coo.sign(&self.local_identity, &source.identity) {
                return Err(Box::new(UnexpectedError));
            }
            nc.certificates_of_ownership.push(coo);

            for (id, value) in member.tags.iter() {
                let _ = nc.tags.insert(
                    *id,
                    Tag::new(*id, *value, &self.local_identity, network_id, &source.identity, now, legacy_v1).ok_or(UnexpectedError)?,
                );
            }

            // TODO: node info, which isn't supported in v1 so not needed yet

            // TODO: revocations!

            Some(nc)
            // ====================================================================================
        } else {
            None
        };

        if member_changed {
            self.database.save_member(member).await?;
        }

        Ok((authorization_result, nc))
    }
}
