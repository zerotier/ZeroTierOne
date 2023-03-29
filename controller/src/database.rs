use async_trait::async_trait;

use zerotier_crypto::secure_eq;
use zerotier_network_hypervisor::vl1::{InetAddress, PartialAddress};
use zerotier_network_hypervisor::vl2::NetworkId;
use zerotier_utils::tokio::sync::broadcast::Receiver;

use crate::model::*;

pub type Error = Box<dyn std::error::Error + Send + Sync>;

/// Database change relevant to the controller and that was NOT initiated by the controller.
#[derive(Clone, Debug)]
pub enum Change {
    NetworkCreated(Network),
    NetworkChanged(Network, Network),
    NetworkDeleted(Network, Vec<Member>),
    MemberCreated(Member),
    MemberChanged(Member, Member),
    MemberDeleted(Member),
}

#[async_trait]
pub trait Database: Sync + Send + 'static {
    /// List networks on this controller.
    async fn list_networks(&self) -> Result<Vec<NetworkId>, Error>;

    /// Get a network by network ID.
    async fn get_network(&self, id: &NetworkId) -> Result<Option<Network>, Error>;

    /// Save a network.
    ///
    /// Note that unlike members the network ID is not automatically promoted from legacy to full
    /// ID format.
    async fn save_network(&self, obj: Network, generate_change_notification: bool) -> Result<(), Error>;

    /// List members of a network.
    async fn list_members(&self, network_id: &NetworkId) -> Result<Vec<PartialAddress>, Error>;

    /// Get a member of network.
    ///
    /// If node_id is not a complete address, the best unique match should be returned. None should
    /// be returned not only if the member is not found but if node_id is ambiguous (would match more
    /// than one member).
    async fn get_member(&self, network_id: &NetworkId, node_id: &PartialAddress) -> Result<Option<Member>, Error>;

    /// Save a modified member to a network.
    ///
    /// Note that member modifications can include the automatic replacement of a less specific address
    /// in node_id with a fully specific address. This happens the first time a member added with an
    /// incomplete address is actually seen. In that case the implementation must correctly find the
    /// best matching existing member and replace it with a member identified by the fully specified
    /// address, removing and re-adding if needed.
    ///
    /// This must also handle the (rare) case when someone may try to save a member with a less
    /// specific address than the one currently in the database. In that case the "old" more specific
    /// address should replace the less specific address in the node_id field. This can only happen if
    /// an external user manually does this. The controller won't do this automatically.
    async fn save_member(&self, obj: Member, generate_change_notification: bool) -> Result<(), Error>;

    /// Save a log entry for a request this controller has handled.
    async fn log_request(&self, obj: RequestLogItem) -> Result<(), Error>;

    /// Get a receiver that can be used to receive changes made to networks and members, if supported.
    ///
    /// The receiver returned is a broadcast receiver. This can be called more than once if there are
    /// multiple parts of the controller that listen.
    ///
    /// The default implementation returns None indicating that change following is not supported.
    /// Change following is required for instant deauthorization with revocations and other instant
    /// changes in response to modifications to network and member configuration.
    async fn changes(&self) -> Option<Receiver<Change>> {
        None
    }

    /// List members deauthorized after a given time (milliseconds since epoch).
    ///
    /// The default trait implementation uses a brute force method. This should be reimplemented if a
    /// more efficient way is available.
    async fn list_members_deauthorized_after(&self, network_id: &NetworkId, cutoff: i64) -> Result<Vec<PartialAddress>, Error> {
        let mut v = Vec::new();
        let members = self.list_members(network_id).await?;
        for a in members.iter() {
            if let Some(m) = self.get_member(network_id, a).await? {
                if m.last_deauthorized_time.unwrap_or(i64::MIN) >= cutoff {
                    v.push(m.node_id);
                }
            }
        }
        Ok(v)
    }

    /// Check if any member of a network has a given static IP assignment.
    ///
    /// The default trait implementation uses a brute force method. This should be reimplemented if a
    /// more efficient way is available.
    async fn is_ip_assigned(&self, network_id: &NetworkId, ip: &InetAddress) -> Result<bool, Error> {
        let members = self.list_members(network_id).await?;
        for a in members.iter() {
            if let Some(m) = self.get_member(network_id, a).await? {
                if m.ip_assignments.iter().any(|ip2| secure_eq(ip2.ip_bytes(), ip.ip_bytes())) {
                    return Ok(true);
                }
            }
        }
        return Ok(false);
    }
}
