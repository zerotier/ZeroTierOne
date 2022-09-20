use async_trait::async_trait;

use zerotier_network_hypervisor::vl1::{Address, NodeStorage};
use zerotier_network_hypervisor::vl2::NetworkId;

use crate::model::*;

#[async_trait]
pub trait Database: NodeStorage + Sync + Send + 'static {
    type Error;

    async fn get_network(&self, id: NetworkId) -> Result<Option<Network>, Self::Error>;
    async fn save_network(&self, obj: Network) -> Result<(), Self::Error>;

    async fn get_network_members(&self, id: NetworkId) -> Result<Vec<Address>, Self::Error>;

    async fn get_member(&self, network_id: NetworkId, node_id: Address) -> Result<Option<Member>, Self::Error>;
    async fn save_member(&self, network_id: NetworkId, node_id: Address) -> Result<Option<Member>, Self::Error>;
}
