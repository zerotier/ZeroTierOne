use std::collections::{BTreeMap, BTreeSet, HashMap};
use std::ops::Deref;
use std::str::FromStr;
use std::sync::{Arc, Mutex};

use async_trait::async_trait;
use pin_utils::pin_mut;
use serde::{Deserialize, Serialize};
use tokio_postgres::types::Type;
use tokio_postgres::{Client, Statement};

use zerotier_crypto::secure_eq;

use zerotier_network_hypervisor::vl1::{Address, InetAddress};
use zerotier_network_hypervisor::vl2::rule::Rule;
use zerotier_network_hypervisor::vl2::{IpRoute, NetworkId};

use zerotier_utils::futures_util::{Stream, StreamExt};
use zerotier_utils::tokio;
use zerotier_utils::tokio::runtime::Handle;
use zerotier_utils::tokio::sync::broadcast::{channel, Receiver, Sender};
use zerotier_utils::tokio::task::JoinHandle;

use crate::database::*;
use crate::model::{IpAssignmentPool, Member, Network, RequestLogItem};

const RECONNECT_RATE_LIMIT: tokio::time::Duration = tokio::time::Duration::from_millis(250);

/// Query for looking up a virtual network and all its relevant settings.
const GET_NETWORK_SQL: &'static str = "
SELECT
    n.capabilities,
    n.enable_broadcast,
    n.mtu,
    n.multicast_limit,
    n.name,
    n.private,
    n.rules,
    n.v4_assign_mode,
    n.v6_assign_mode,
    n.sso_enabled,
    (CASE WHEN n.sso_enabled THEN o.client_id ELSE NULL END) as client_id,
    (CASE WHEN n.sso_enabled THEN o.authorization_endpoint ELSE NULL END) as authorization_endpoint,
    d.domain,
    ARRAY_TO_STRING(d.servers, ',', '*'),
    ARRAY_TO_STRING(ARRAY(SELECT CONCAT(host(ip_range_start),'|', host(ip_range_end)) FROM ztc_network_assignment_pool WHERE network_id = n.id), ',', '*') AS assignment_pool,
    ARRAY_TO_STRING(ARRAY(SELECT CONCAT(host(address),'/',bits::text,'|',COALESCE(host(via), 'NULL')) FROM ztc_network_route WHERE network_id = n.id), ',', '*') AS routes
FROM
    ztc_network n
    LEFT OUTER JOIN ztc_org o ON o.owner_id = n.owner_id
    LEFT OUTER JOIN ztc_network_oidc_config noc ON noc.network_id = n.id
    LEFT OUTER JOIN ztc_oidc_config oc ON noc.client_id = oc.client_id AND o.org_id = oc.org_id
    LEFT OUTER JOIN ztc_network_dns d ON d.network_id = n.id
WHERE
    n.id = $1 AND
    n.deleted = false";

/// Query to get capabilities, a deprecated feature for small subsets of rules that is handled here automatically by
/// merging with the main rule set.
const GET_NETWORK_MEMBERS_WITH_CAPABILITIES_SQL: &'static str = "
SELECT
    m.id,
    m.capabilities
FROM
    ztc_member m
WHERE
    network_id = $1 AND
    authorized = true AND
    deleted = false AND
    capabilities IS NOT NULL AND
    capabilities != '[]' AND
    capabilities != 'null'";

struct PostgresConnection {
    s_list_networks: Statement,
    s_list_members: Statement,
    s_get_network: Statement,
    s_get_network_members_with_capabilities: Statement,
    client: Client,
    connection_task: JoinHandle<()>,
}

impl PostgresConnection {
    async fn new(runtime: &Handle, postgres_path: &str) -> Result<Box<Self>, Error> {
        let (client, connection) = tokio_postgres::connect(postgres_path, tokio_postgres::NoTls).await?;
        Ok(Box::new(Self {
            s_list_networks: client
                .prepare_typed("SELECT id FROM ztc_network WHERE controller_id = $1 AND deleted = false", &[Type::TEXT])
                .await?,
            s_list_members: client
                .prepare_typed("SELECT id FROM ztc_member WHERE network_id = $1 AND deleted = false", &[Type::TEXT])
                .await?,
            s_get_network: client.prepare_typed(GET_NETWORK_SQL, &[Type::TEXT]).await?,
            s_get_network_members_with_capabilities: client.prepare_typed(GET_NETWORK_MEMBERS_WITH_CAPABILITIES_SQL, &[Type::TEXT]).await?,
            client,
            connection_task: runtime.spawn(async move {
                if let Err(e) = connection.await {
                    eprintln!("ERROR: postgresql connection error: {}", e.to_string());
                }
            }),
        }))
    }
}

impl Drop for PostgresConnection {
    fn drop(&mut self) {
        self.connection_task.abort();
    }
}

struct ConnectionHolder<'a>(Option<Box<PostgresConnection>>, &'a PostgresDatabase);

impl<'a> Deref for ConnectionHolder<'a> {
    type Target = PostgresConnection;

    #[inline(always)]
    fn deref(&self) -> &Self::Target {
        &self.0.as_ref().unwrap()
    }
}

impl<'a> Drop for ConnectionHolder<'a> {
    fn drop(&mut self) {
        let mut connections = self.1.connections.lock().unwrap();
        connections.0.push(self.0.take().unwrap());
        let _ = connections.1.send(()); // unblock any waiting get_connection() requests
    }
}

pub struct PostgresDatabase {
    local_controller: Address,
    connections: Mutex<(Vec<Box<PostgresConnection>>, Sender<()>)>,
    postgres_path: String,
    runtime: Handle,
}

impl PostgresDatabase {
    pub async fn new(runtime: Handle, postgres_path: String, num_connections: usize) -> Result<Arc<Self>, Error> {
        assert!(num_connections > 0);
        let (sender, _) = channel(4096);
        let mut connections = Vec::with_capacity(num_connections);
        for _ in 0..num_connections {
            connections.push(PostgresConnection::new(&runtime, postgres_path.as_str()).await?);
        }
        Ok(Arc::new(Self {
            local_controller_id_str: local_identity.address.to_string(),
            connections: Mutex::new((connections, sender)),
            postgres_path,
            runtime,
        }))
    }

    async fn get_connection(&self) -> Result<ConnectionHolder, Error> {
        loop {
            let mut receiver = {
                let mut connections = self.connections.lock().unwrap();
                if let Some(c) = connections.0.pop() {
                    if c.client.is_closed() {
                        break;
                    } else {
                        return Ok(ConnectionHolder(Some(c), self));
                    }
                }
                connections.1.subscribe()
            };
            let _ = receiver.recv().await; // wait for a connection to be returned
        }
        tokio::time::sleep(RECONNECT_RATE_LIMIT).await; // rate limit reconnection attempts
        return Ok(ConnectionHolder(
            Some(PostgresConnection::new(&self.runtime, self.postgres_path.as_str()).await?),
            self,
        ));
    }
}

#[async_trait]
impl Database for PostgresDatabase {
    async fn list_networks(&self) -> Result<Vec<NetworkId>, Error> {
        let c = self.get_connection().await?;
        let rs = c.client.query_raw(&c.s_list_networks, &[&self.local_controller_id_str]).await?;
        pin_mut!(rs);
        let mut r = Vec::with_capacity(rs.size_hint().0.min(64));
        while let Some(Ok(row)) = rs.next().await {
            r.push(NetworkId::from_str(row.get(0))?);
        }
        Ok(r)
    }

    #[allow(unused_variables)]
    async fn get_network(&self, id: NetworkId) -> Result<Option<Network>, Error> {
        let (nw, with_caps) = {
            let c = self.get_connection().await?;
            let network_id_string = id.to_string();
            if let Some(r) = c.client.query_opt(&c.s_get_network, &[&network_id_string]).await? {
                if let Ok(with_caps) = c.client.query(&c.s_get_network_members_with_capabilities, &[&network_id_string]).await {
                    (r, with_caps)
                } else {
                    (r, Vec::new())
                }
            } else {
                return Ok(None);
            }
        };

        let mut capabilities: Option<&str> = nw.get(0);
        let enable_broadcast: bool = nw.get(1);
        let mtu: i32 = nw.get(2);
        let multicast_limit: i64 = nw.get(3);
        let name: &str = nw.get(4);
        let private: bool = nw.get(5);
        let mut rules: Option<&str> = nw.get(6);
        let v4_assign_mode: &str = nw.get(7);
        let v6_assign_mode: &str = nw.get(8);
        let sso_enabled: bool = nw.get(9);
        let mut client_id: Option<&str> = nw.get(10);
        let mut authorization_endpoint: Option<&str> = nw.get(11);
        let mut domain: Option<&str> = nw.get(12);
        let mut servers_csv: Option<&str> = nw.get(13);
        let mut assignment_pool_csv: Option<&str> = nw.get(14);
        let mut routes_csv: Option<&str> = nw.get(15);

        filter_null_string(&mut capabilities);
        filter_null_string(&mut rules);
        filter_null_string(&mut client_id);
        filter_null_string(&mut authorization_endpoint);
        filter_null_string(&mut domain);
        filter_null_string(&mut servers_csv);
        filter_null_string(&mut assignment_pool_csv);
        filter_null_string(&mut routes_csv);

        Ok(Some(Network {
            id,
            name: name.to_string(),
            multicast_limit: if multicast_limit < 0 {
                None
            } else {
                Some(multicast_limit.min(u32::MAX as i64) as u32)
            },
            enable_broadcast: Some(enable_broadcast),
            v4_assign_mode: Some(serde_json::from_str(v4_assign_mode)?),
            v6_assign_mode: Some(serde_json::from_str(v6_assign_mode)?),
            ip_assignment_pools: {
                let mut ip_assignment_pools = BTreeSet::new();
                if let Some(assignment_pool_csv) = assignment_pool_csv {
                    for p in assignment_pool_csv.split(',') {
                        if let Some((start, end)) = p.split_once('|') {
                            if let Ok(start) = InetAddress::from_str(start) {
                                if let Ok(end) = InetAddress::from_str(end) {
                                    ip_assignment_pools.insert(IpAssignmentPool { ip_range_start: start, ip_range_end: end });
                                }
                            }
                        }
                    }
                }
                ip_assignment_pools
            },
            ip_routes: {
                let mut ip_routes = BTreeSet::new();
                if let Some(routes_csv) = routes_csv {
                    for r in routes_csv.split(',') {
                        if let Some((cidr, via)) = r.split_once('|') {
                            if let Ok(cidr) = InetAddress::from_str(cidr) {
                                ip_routes.insert(IpRoute {
                                    target: cidr,
                                    via: if via == "NULL" {
                                        None
                                    } else {
                                        if let Ok(via) = InetAddress::from_str(via) {
                                            Some(via)
                                        } else {
                                            None
                                        }
                                    },
                                    flags: None,
                                    metric: None,
                                });
                            }
                        }
                    }
                }
                ip_routes
            },
            dns: {
                let mut dns = BTreeMap::new();
                if let Some(domain) = domain {
                    if let Some(servers_csv) = servers_csv {
                        let mut servers = BTreeSet::new();
                        for s in servers_csv.split(',') {
                            if let Ok(s) = InetAddress::from_str(s) {
                                servers.insert(s);
                            }
                        }
                        dns.insert(domain.trim().to_string(), servers);
                    }
                }
                dns
            },
            rules: if let Some(rules) = rules {
                let mut rules = serde_json::from_str::<Vec<Rule>>(rules)?;

                // Capabilities are being deprecated in V2 as they are complex and rarely used. To handle networks
                // that have them configured (there aren't many) we translate them into special portions of the
                // general rule set that match on the capability owner's address.
                if let Some(capabilities) = capabilities {
                    let capabilities_vec = serde_json::from_str::<Vec<V1Capability>>(capabilities)?;
                    let mut capabilities = HashMap::with_capacity(capabilities_vec.len());
                    for c in capabilities_vec.iter() {
                        capabilities.insert(c.id, c);
                    }
                    let mut members_by_cap = HashMap::with_capacity(with_caps.len());
                    for wc in with_caps.iter() {
                        if let Ok(member_id) = Address::from_str(wc.get(0)) {
                            if let Ok(cap_ids) = serde_json::from_str::<Vec<u32>>(wc.get(1)) {
                                for cap_id in cap_ids.iter() {
                                    members_by_cap.entry(*cap_id).or_insert_with(|| Vec::with_capacity(4)).push(member_id);
                                }
                            }
                        }
                    }
                    if !members_by_cap.is_empty() {
                        let mut base_rules = rules.clone();
                        rules.clear();

                        for (cap_id, member_ids) in members_by_cap.iter() {
                            if let Some(cap) = capabilities.get(cap_id) {
                                let mut or = false;
                                for m in member_ids.iter() {
                                    rules.push(Rule::match_source_zerotier_address(false, or, *m));
                                    or = true;
                                }
                                for r in cap.rules.iter() {
                                    rules.push(r.clone());
                                }
                                rules.push(Rule::action_accept());
                            }
                        }

                        for r in base_rules.drain(..) {
                            rules.push(r);
                        }
                    }
                }

                Some(rules)
            } else {
                None
            },
            credential_ttl: None,
            min_supported_version: None,
            mtu: if mtu < 0 || mtu > (u16::MAX as i32) {
                None
            } else {
                Some(mtu as u16)
            },
            private,
            learn_members: Some(true),
        }))
    }

    async fn save_network(&self, _obj: Network, _generate_change_notification: bool) -> Result<(), Error> {
        todo!()
    }

    async fn list_members(&self, network_id: NetworkId) -> Result<Vec<Address>, Error> {
        let network_id_string = network_id.to_string();
        let c = self.get_connection().await?;
        let rs = c.client.query_raw(&c.s_list_members, &[&network_id_string]).await?;
        pin_mut!(rs);
        let mut r = Vec::with_capacity(rs.size_hint().0.min(64));
        while let Some(Ok(row)) = rs.next().await {
            r.push(Address::from_str(row.get(0))?);
        }
        Ok(r)
    }

    async fn get_member(&self, _network_id: NetworkId, _node_id: Address) -> Result<Option<Member>, Error> {
        todo!()
    }

    async fn save_member(&self, _obj: Member, _generate_change_notification: bool) -> Result<(), Error> {
        todo!()
    }

    async fn changes(&self) -> Option<Receiver<Change>> {
        // TODO
        None
    }

    async fn list_members_deauthorized_after(&self, network_id: NetworkId, cutoff: i64) -> Result<Vec<Address>, Error> {
        let mut v = Vec::new();
        let members = self.list_members(network_id).await?;
        for a in members.iter() {
            if let Some(m) = self.get_member(network_id, *a).await? {
                if m.last_deauthorized_time.unwrap_or(i64::MIN) >= cutoff {
                    v.push(m.node_id);
                }
            }
        }
        Ok(v)
    }

    async fn is_ip_assigned(&self, network_id: NetworkId, ip: &InetAddress) -> Result<bool, Error> {
        let members = self.list_members(network_id).await?;
        for a in members.iter() {
            if let Some(m) = self.get_member(network_id, *a).await? {
                if m.ip_assignments.iter().any(|ip2| secure_eq(ip2.ip_bytes(), ip.ip_bytes())) {
                    return Ok(true);
                }
            }
        }
        return Ok(false);
    }

    async fn log_request(&self, _obj: RequestLogItem) -> Result<(), Error> {
        todo!()
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct V1Capability {
    pub id: u32,
    pub rules: Vec<Rule>,
}

fn filter_null_string(s: &mut Option<&str>) {
    if let Some(ss) = s.as_ref() {
        let ss = (*ss).trim();
        if ss.is_empty() || ss == "null" || ss == "NULL" {
            let _ = s.take();
        }
    }
}
