use std::error::Error;
use std::path::{Path, PathBuf};
use std::str::FromStr;

use async_trait::async_trait;

use serde::de::DeserializeOwned;
use serde::Serialize;

use zerotier_network_hypervisor::vl1::{Address, Identity, NodeStorage};
use zerotier_network_hypervisor::vl2::NetworkId;

use zerotier_utils::io::{fs_restrict_permissions, read_limit};
use zerotier_utils::json::{json_patch, to_json_pretty};
use zerotier_utils::tokio::fs;
use zerotier_utils::tokio::io::ErrorKind;

use crate::database::Database;
use crate::model::*;

const IDENTITY_SECRET_FILENAME: &'static str = "identity.secret";

/// An in-filesystem database that permits live editing.
///
/// A cache is maintained that contains the actual objects. When an object is live edited,
/// once it successfully reads and loads it is merged with the cached object and saved to
/// the cache. The cache will also contain any ephemeral data, generated data, etc.
pub struct FileDatabase {
    base_path: PathBuf,
    cache_path: PathBuf,
}

fn network_path(base: &PathBuf, network_id: NetworkId) -> PathBuf {
    base.join(network_id.to_string()).join(format!("n{}.json", network_id.to_string()))
}

fn member_path(base: &PathBuf, network_id: NetworkId, member_id: Address) -> PathBuf {
    base.join(network_id.to_string()).join(format!("m{}.json", member_id.to_string()))
}

impl FileDatabase {
    pub async fn new<P: AsRef<Path>>(base_path: P) -> Self {
        let base: PathBuf = base_path.as_ref().into();
        let cache: PathBuf = base_path.as_ref().join("cache");
        let _ = fs::create_dir_all(&cache).await;
        Self { base_path: base, cache_path: cache }
    }

    /// Merge an object with its cached instance and save the result to the 'cache' path.
    async fn merge_with_cache<O: Serialize + DeserializeOwned>(
        &self,
        object_path_in_cache: PathBuf,
        changes: O,
    ) -> Result<O, Box<dyn Error>> {
        let changes = serde_json::to_value(&changes)?;
        let cached_json = fs::read(&object_path_in_cache).await?;

        let mut patched = serde_json::from_slice::<serde_json::Value>(cached_json.as_slice())?;
        json_patch(&mut patched, &changes, 64);
        let patched = serde_json::from_value::<O>(patched)?;

        let patched_json = serde_json::to_vec(&patched)?;
        if !cached_json.as_slice().eq(patched_json.as_slice()) {
            let _ = fs::write(object_path_in_cache, patched_json.as_slice()).await;
        }

        return Ok(patched);
    }
}

impl NodeStorage for FileDatabase {
    fn load_node_identity(&self) -> Option<Identity> {
        let id_data = read_limit(self.base_path.join(IDENTITY_SECRET_FILENAME), 4096);
        if id_data.is_err() {
            return None;
        }
        let id_data = Identity::from_str(String::from_utf8_lossy(id_data.unwrap().as_slice()).as_ref());
        if id_data.is_err() {
            return None;
        }
        Some(id_data.unwrap())
    }

    fn save_node_identity(&self, id: &Identity) {
        assert!(id.secret.is_some());
        let id_secret_str = id.to_secret_string();
        let secret_path = self.base_path.join(IDENTITY_SECRET_FILENAME);
        assert!(std::fs::write(&secret_path, id_secret_str.as_bytes()).is_ok());
        assert!(fs_restrict_permissions(&secret_path));
    }
}

#[async_trait]
impl Database for FileDatabase {
    async fn get_network(&self, id: NetworkId) -> Result<Option<Network>, Box<dyn Error>> {
        let r = fs::read(network_path(&self.base_path, id)).await;
        if let Ok(raw) = r {
            let r = serde_json::from_slice::<Network>(raw.as_slice());
            if let Ok(network) = r {
                return Ok(Some(self.merge_with_cache(network_path(&self.cache_path, id), network).await?));
            } else {
                return Err(Box::new(r.err().unwrap()));
            }
        } else {
            let e = r.unwrap_err();
            if matches!(e.kind(), ErrorKind::NotFound) {
                let _ = fs::remove_dir_all(self.cache_path.join(id.to_string())).await;
                return Ok(None);
            } else {
                return Err(Box::new(e));
            }
        }
    }

    async fn save_network(&self, obj: &Network) -> Result<(), Box<dyn Error>> {
        let _ = fs::create_dir_all(self.base_path.join(obj.id.to_string())).await;
        let _ = fs::create_dir_all(self.cache_path.join(obj.id.to_string())).await;

        let base_network_path = network_path(&self.base_path, obj.id);
        if !fs::metadata(&base_network_path).await.is_ok() {
            fs::write(base_network_path, to_json_pretty(obj).as_bytes()).await?;
        }

        fs::write(network_path(&self.cache_path, obj.id), serde_json::to_vec(obj)?.as_slice()).await?;

        Ok(())
    }

    async fn list_members(&self, network_id: NetworkId) -> Result<Vec<Address>, Box<dyn Error>> {
        let mut members = Vec::new();
        let mut dir = fs::read_dir(self.base_path.join(network_id.to_string())).await?;
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

    async fn get_member(&self, network_id: NetworkId, node_id: Address) -> Result<Option<Member>, Box<dyn Error>> {
        let r = fs::read(member_path(&self.base_path, network_id, node_id)).await;
        if let Ok(raw) = r {
            let r = serde_json::from_slice::<Member>(raw.as_slice());
            if let Ok(member) = r {
                return Ok(Some(
                    self.merge_with_cache(member_path(&self.cache_path, network_id, node_id), member)
                        .await?,
                ));
            } else {
                return Err(Box::new(r.err().unwrap()));
            }
        } else {
            let e = r.unwrap_err();
            if matches!(e.kind(), ErrorKind::NotFound) {
                let _ = fs::remove_file(member_path(&self.cache_path, network_id, node_id)).await;
                return Ok(None);
            } else {
                return Err(Box::new(e));
            }
        }
    }

    async fn save_member(&self, obj: &Member) -> Result<(), Box<dyn Error>> {
        let base_member_path = member_path(&self.base_path, obj.network_id, obj.node_id);
        if !fs::metadata(&base_member_path).await.is_ok() {
            fs::write(base_member_path, to_json_pretty(obj).as_bytes()).await?;
        }

        fs::write(
            member_path(&self.cache_path, obj.network_id, obj.node_id),
            serde_json::to_vec(obj)?.as_slice(),
        )
        .await?;
        Ok(())
    }

    async fn log_request(&self, obj: &RequestLogItem) -> Result<(), Box<dyn Error>> {
        println!("{}", obj.to_string());
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    #[allow(unused_imports)]
    use super::*;

    #[test]
    fn test_db() {}
}
