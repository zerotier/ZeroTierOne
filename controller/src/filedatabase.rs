use std::error::Error;
use std::path::{Path, PathBuf};

use async_trait::async_trait;

use serde::de::DeserializeOwned;
use serde::Serialize;

use zerotier_network_hypervisor::vl1::Address;
use zerotier_network_hypervisor::vl2::NetworkId;
use zerotier_utils::json::{json_patch, to_json_pretty};
use zerotier_utils::tokio::fs;
use zerotier_utils::tokio::io::ErrorKind;

use crate::database::Database;
use crate::model::*;

pub struct FileDatabase {
    base: PathBuf,
    live: PathBuf,
}

fn network_path(base: &PathBuf, network_id: NetworkId) -> PathBuf {
    base.join(network_id.to_string()).join("config.json")
}

fn member_path(base: &PathBuf, network_id: NetworkId, member_id: Address) -> PathBuf {
    base.join(network_id.to_string()).join(format!("m{}.json", member_id.to_string()))
}

impl FileDatabase {
    pub async fn new<P: AsRef<Path>>(base_path: P) -> Self {
        let base: PathBuf = base_path.as_ref().into();
        let live: PathBuf = base_path.as_ref().join("live");
        let _ = fs::create_dir_all(&live).await;
        Self { base, live }
    }

    async fn merge_with_live<O: Serialize + DeserializeOwned>(&self, live_path: PathBuf, changes: O) -> O {
        if let Ok(changes) = serde_json::to_value(&changes) {
            if let Ok(old_raw_json) = fs::read(&live_path).await {
                if let Ok(mut patched) = serde_json::from_slice::<serde_json::Value>(old_raw_json.as_slice()) {
                    json_patch(&mut patched, &changes, 64);
                    if let Ok(patched) = serde_json::from_value::<O>(patched) {
                        if let Ok(patched_json) = serde_json::to_vec(&patched) {
                            if let Ok(to_replace) = fs::read(&live_path).await {
                                if to_replace.as_slice().eq(patched_json.as_slice()) {
                                    return patched;
                                }
                            }
                            let _ = fs::write(live_path, patched_json.as_slice()).await;
                            return patched;
                        }
                    }
                }
            }
        }
        // TODO: report error
        return changes;
    }
}

#[async_trait]
impl Database for FileDatabase {
    type Error = Box<dyn Error>;

    async fn get_network(&self, id: NetworkId) -> Result<Option<Network>, Self::Error> {
        let r = fs::read(network_path(&self.base, id)).await;
        if let Ok(raw) = r {
            let r = serde_json::from_slice::<Network>(raw.as_slice());
            if let Ok(network) = r {
                return Ok(Some(self.merge_with_live(network_path(&self.live, id), network).await));
            } else {
                return Err(Box::new(r.err().unwrap()));
            }
        } else {
            let e = r.unwrap_err();
            if matches!(e.kind(), ErrorKind::NotFound) {
                let _ = fs::remove_dir_all(self.live.join(id.to_string())).await;
                return Ok(None);
            } else {
                return Err(Box::new(e));
            }
        }
    }

    async fn save_network(&self, obj: &Network) -> Result<(), Self::Error> {
        let _ = fs::create_dir_all(self.base.join(obj.id.to_string())).await;
        let _ = fs::create_dir_all(self.live.join(obj.id.to_string())).await;

        let base_network_path = network_path(&self.base, obj.id);
        if !fs::metadata(&base_network_path).await.is_ok() {
            fs::write(base_network_path, to_json_pretty(obj).as_bytes()).await?;
        }

        fs::write(network_path(&self.live, obj.id), serde_json::to_vec(obj)?.as_slice()).await?;

        Ok(())
    }

    async fn list_members(&self, network_id: NetworkId) -> Result<Vec<Address>, Self::Error> {
        let mut members = Vec::new();
        let mut dir = fs::read_dir(self.base.join(network_id.to_string())).await?;
        while let Ok(Some(ent)) = dir.next_entry().await {
            let osname = ent.file_name();
            let name = osname.to_string_lossy();
            if name.len() == (zerotier_network_hypervisor::protocol::ADDRESS_SIZE_STRING + 6)
                && name.starts_with("m")
                && name.ends_with(".json")
            {
                if let Ok(member_address) = u64::from_str_radix(&name[1..11], 16) {
                    if let Some(member_address) = Address::from_u64(member_address) {
                        members.push(member_address);
                    }
                }
            }
        }
        Ok(members)
    }

    async fn get_member(&self, network_id: NetworkId, node_id: Address) -> Result<Option<Member>, Self::Error> {
        let r = fs::read(member_path(&self.base, network_id, node_id)).await;
        if let Ok(raw) = r {
            let r = serde_json::from_slice::<Member>(raw.as_slice());
            if let Ok(member) = r {
                return Ok(Some(
                    self.merge_with_live(member_path(&self.live, network_id, node_id), member).await,
                ));
            } else {
                return Err(Box::new(r.err().unwrap()));
            }
        } else {
            let e = r.unwrap_err();
            if matches!(e.kind(), ErrorKind::NotFound) {
                let _ = fs::remove_file(member_path(&self.live, network_id, node_id)).await;
                return Ok(None);
            } else {
                return Err(Box::new(e));
            }
        }
    }

    async fn save_member(&self, obj: &Member) -> Result<(), Self::Error> {
        let base_member_path = member_path(&self.base, obj.network_id, obj.node_id);
        if !fs::metadata(&base_member_path).await.is_ok() {
            fs::write(base_member_path, to_json_pretty(obj).as_bytes()).await?;
        }

        fs::write(
            member_path(&self.live, obj.network_id, obj.node_id),
            serde_json::to_vec(obj)?.as_slice(),
        )
        .await?;
        Ok(())
    }
}
