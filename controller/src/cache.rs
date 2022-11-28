// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::collections::HashMap;
use std::error::Error;
use std::mem::replace;
use std::sync::{Mutex, RwLock};

use crate::database::Database;
use crate::model::{Member, Network};

use zerotier_network_hypervisor::vl1::Address;
use zerotier_network_hypervisor::vl2::NetworkId;

/// Network and member cache used by database implementations to implement change detection.
pub struct Cache {
    by_nwid: RwLock<HashMap<NetworkId, (Network, Mutex<HashMap<Address, Member>>)>>,
}

impl Cache {
    pub fn new() -> Self {
        Self { by_nwid: RwLock::new(HashMap::new()) }
    }

    /// Load (or reload) the entire cache from a database.
    pub async fn load_all<DatabaseImpl: Database>(&self, db: &DatabaseImpl) -> Result<(), Box<dyn Error + Send + Sync>> {
        let mut by_nwid = self.by_nwid.write().unwrap();
        by_nwid.clear();

        let networks = db.list_networks().await?;
        for network_id in networks {
            if let Some(network) = db.get_network(network_id).await? {
                let network_entry = by_nwid.entry(network_id).or_insert_with(|| (network, Mutex::new(HashMap::new())));
                let mut by_node_id = network_entry.1.lock().unwrap();
                let members = db.list_members(network_id).await?;
                for node_id in members {
                    if let Some(member) = db.get_member(network_id, node_id).await? {
                        let _ = by_node_id.insert(node_id, member);
                    }
                }
            }
        }

        Ok(())
    }

    /// Update a network if changed, returning whether or not any update was made and the old version if any.
    /// A value of (true, None) indicates that there was no network by that ID in which case it is added.
    pub fn on_network_updated(&self, network: Network) -> (bool, Option<Network>) {
        let mut by_nwid = self.by_nwid.write().unwrap();
        if let Some(prev_network) = by_nwid.get_mut(&network.id) {
            if !prev_network.0.eq(&network) {
                (true, Some(replace(&mut prev_network.0, network)))
            } else {
                (false, None)
            }
        } else {
            let _ = by_nwid.insert(network.id, (network.clone(), Mutex::new(HashMap::new())));
            (true, None)
        }
    }

    /// Update a member if changed, returning whether or not any update was made and the old version if any.
    /// A value of (true, None) indicates that there was no member with that ID. If there is no network with
    /// the member's network ID (false, None) is returned and no action is taken.
    pub fn on_member_updated(&self, member: Member) -> (bool, Option<Member>) {
        let by_nwid = self.by_nwid.read().unwrap();
        if let Some(network) = by_nwid.get(&member.network_id) {
            let mut by_node_id = network.1.lock().unwrap();
            if let Some(prev_member) = by_node_id.get_mut(&member.node_id) {
                if !member.eq(prev_member) {
                    (true, Some(replace(prev_member, member)))
                } else {
                    (false, None)
                }
            } else {
                let _ = by_node_id.insert(member.node_id, member);
                (true, None)
            }
        } else {
            (false, None)
        }
    }

    /// Delete a network, returning it if it existed.
    pub fn on_network_deleted(&self, network_id: NetworkId) -> Option<(Network, Vec<Member>)> {
        let mut by_nwid = self.by_nwid.write().unwrap();
        let network = by_nwid.remove(&network_id)?;
        let mut members = network.1.lock().unwrap();
        Some((network.0, members.drain().map(|(_, v)| v).collect()))
    }

    /// Delete a member, returning it if it existed.
    pub fn on_member_deleted(&self, network_id: NetworkId, node_id: Address) -> Option<Member> {
        let by_nwid = self.by_nwid.read().unwrap();
        let network = by_nwid.get(&network_id)?;
        let mut members = network.1.lock().unwrap();
        members.remove(&node_id)
    }
}
