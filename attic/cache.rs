// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::collections::{BTreeMap, HashMap};
use std::error::Error;
use std::mem::replace;
use std::ops::Bound;
use std::sync::{Mutex, RwLock};

use crate::database::Database;
use crate::model::{Member, Network};

use zerotier_network_hypervisor::vl1::{Address, PartialAddress};
use zerotier_network_hypervisor::vl2::NetworkId;

/// Network and member cache used by database implementations to implement change detection.
///
/// Note: the database must ensure that calls to on_X_updated() methods are only performed
/// when a potentially newer version is committed. No-op calls when nothing has changed are
/// okay but calls out of order will result in extra updated events being generated for
/// movements forward and backward in time. Calls must be temporally ordered.
pub struct Cache {
    by_nwid: RwLock<HashMap<NetworkId, (Network, Mutex<BTreeMap<PartialAddress, Member>>)>>,
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
            if let Some(network) = db.get_network(&network_id).await? {
                let network_entry = by_nwid
                    .entry(network_id.clone())
                    .or_insert_with(|| (network, Mutex::new(BTreeMap::new())));
                let mut by_node_id = network_entry.1.lock().unwrap();
                let members = db.list_members(&network_id).await?;
                for node_id in members {
                    if let Some(member) = db.get_member(&network_id, &node_id).await? {
                        let _ = by_node_id.insert(node_id, member);
                    }
                }
            }
        }

        Ok(())
    }

    /// Update a network if changed, returning whether or not any update was made and the old version if any.
    ///
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
            assert!(by_nwid
                .insert(network.id.clone(), (network.clone(), Mutex::new(BTreeMap::new())))
                .is_none());
            (true, None)
        }
    }

    /// Update a member if changed, returning whether or not any update was made and the old version if any.
    ///
    /// A value of (true, None) indicates that there was no member with that ID and that it was added. If
    /// there is no network with the member's network ID (false, None) is returned and no action is taken.
    pub fn on_member_updated(&self, member: Member) -> (bool, Option<Member>) {
        let by_nwid = self.by_nwid.read().unwrap();
        if let Some(network) = by_nwid.get(&member.network_id) {
            let mut by_node_id = network.1.lock().unwrap();
            if let Some(exact_address_match) = by_node_id.get_mut(&member.node_id) {
                if !member.eq(exact_address_match) {
                    return (true, Some(std::mem::replace(exact_address_match, member)));
                }
            } else {
                let mut partial_address_match = None;
                for m in by_node_id.range_mut::<PartialAddress, (Bound<&PartialAddress>, Bound<&PartialAddress>)>((
                    Bound::Included(&member.node_id),
                    Bound::Unbounded,
                )) {
                    if m.0.matches_partial(&member.node_id) {
                        if partial_address_match.is_some() {
                            return (false, None);
                        }
                        let _ = partial_address_match.insert(m.1);
                    } else {
                        break;
                    }
                }

                if let Some(partial_address_match) = partial_address_match {
                    if !member.eq(partial_address_match) {
                        return (true, Some(std::mem::replace(partial_address_match, member)));
                    } else {
                        return (false, None);
                    }
                }

                let mut partial_address_match = None;
                for m in by_node_id
                    .range_mut::<PartialAddress, (Bound<&PartialAddress>, Bound<&PartialAddress>)>((
                        Bound::Unbounded,
                        Bound::Included(&member.node_id),
                    ))
                    .rev()
                {
                    if m.0.matches_partial(&member.node_id) {
                        if partial_address_match.is_some() {
                            return (false, None);
                        }
                        let _ = partial_address_match.insert(m.1);
                    } else {
                        break;
                    }
                }

                if let Some(partial_address_match) = partial_address_match {
                    if !member.eq(partial_address_match) {
                        return (true, Some(std::mem::replace(partial_address_match, member)));
                    } else {
                        return (false, None);
                    }
                }

                assert!(by_node_id.insert(member.node_id.clone(), member).is_none());
                return (true, None);
            }
        }
        return (false, None);
    }

    /// Delete a network, returning it if it existed.
    pub fn on_network_deleted(&self, network_id: NetworkId) -> Option<(Network, Vec<Member>)> {
        let mut by_nwid = self.by_nwid.write().unwrap();
        let network = by_nwid.remove(&network_id)?;
        let mut members = network.1.lock().unwrap();
        Some((network.0, members.values().cloned().collect()))
    }

    /// Delete a member, returning it if it existed.
    pub fn on_member_deleted(&self, network_id: NetworkId, node_id: Address) -> Option<Member> {
        let by_nwid = self.by_nwid.read().unwrap();
        let network = by_nwid.get(&network_id)?;
        let mut members = network.1.lock().unwrap();
        members.remove(&node_id.to_partial())
    }
}
