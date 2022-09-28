use std::error::Error;

use async_trait::async_trait;

use zerotier_network_hypervisor::vl1::{Address, NodeStorage};
use zerotier_network_hypervisor::vl2::NetworkId;

use crate::model::*;

#[async_trait]
pub trait Database: Sync + Send + NodeStorage + 'static {
    async fn get_network(&self, id: NetworkId) -> Result<Option<Network>, Box<dyn Error>>;
    async fn save_network(&self, obj: &Network) -> Result<(), Box<dyn Error>>;

    async fn list_members(&self, network_id: NetworkId) -> Result<Vec<Address>, Box<dyn Error>>;
    async fn get_member(&self, network_id: NetworkId, node_id: Address) -> Result<Option<Member>, Box<dyn Error>>;
    async fn save_member(&self, obj: &Member) -> Result<(), Box<dyn Error>>;

    async fn log_request(&self, obj: &RequestLogItem) -> Result<(), Box<dyn Error>>;
}
