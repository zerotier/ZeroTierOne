use std::error::Error;

use async_trait::async_trait;

use zerotier_network_hypervisor::vl1::{Address, InetAddress, NodeStorage};
use zerotier_network_hypervisor::vl2::NetworkId;

use crate::model::*;

#[async_trait]
pub trait Database: Sync + Send + NodeStorage + 'static {
    async fn get_network(&self, id: NetworkId) -> Result<Option<Network>, Box<dyn Error>>;
    async fn save_network(&self, obj: Network) -> Result<(), Box<dyn Error>>;

    async fn list_members(&self, network_id: NetworkId) -> Result<Vec<Address>, Box<dyn Error>>;
    async fn get_member(&self, network_id: NetworkId, node_id: Address) -> Result<Option<Member>, Box<dyn Error>>;
    async fn save_member(&self, obj: Member) -> Result<(), Box<dyn Error>>;

    /// List members deauthorized after a given time (milliseconds since epoch).
    ///
    /// The default trait implementation uses a brute force method. This should be reimplemented if a
    /// more efficient way is available.
    async fn list_members_deauthorized_after(&self, network_id: NetworkId, cutoff: i64) -> Result<Vec<Address>, Box<dyn Error>> {
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

    /// Check if any member of a network has a given static IP assignment.
    ///
    /// The default trait implementation uses a brute force method. This should be reimplemented if a
    /// more efficient way is available.
    async fn is_ip_assigned(&self, network_id: NetworkId, ip: &InetAddress) -> Result<bool, Box<dyn Error>> {
        let members = self.list_members(network_id).await?;
        for a in members.iter() {
            if let Some(m) = self.get_member(network_id, *a).await? {
                if m.ip_assignments.iter().any(|ip2| ip2.ip_bytes().eq(ip.ip_bytes())) {
                    return Ok(true);
                }
            }
        }
        return Ok(false);
    }

    async fn log_request(&self, obj: &RequestLogItem) -> Result<(), Box<dyn Error>>;
}
