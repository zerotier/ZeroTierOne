use std::error::Error;
use std::path::{Path, PathBuf};
use std::str::FromStr;

use async_trait::async_trait;

use zerotier_network_hypervisor::vl1::{Address, Identity, NodeStorage};
use zerotier_network_hypervisor::vl2::NetworkId;

use zerotier_utils::io::{fs_restrict_permissions, read_limit};
use zerotier_utils::json::to_json_pretty;
use zerotier_utils::tokio::fs;
use zerotier_utils::tokio::io::AsyncWriteExt;
use zerotier_utils::tokio::sync::Mutex;

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
    old_log: Mutex<fs::File>,
}

fn network_path(base: &PathBuf, network_id: NetworkId) -> PathBuf {
    base.join(network_id.to_string()).join(format!("n{}.json", network_id.to_string()))
}

fn member_path(base: &PathBuf, network_id: NetworkId, member_id: Address) -> PathBuf {
    base.join(network_id.to_string()).join(format!("m{}.json", member_id.to_string()))
}

impl FileDatabase {
    pub async fn new<P: AsRef<Path>>(base_path: P) -> Result<Self, Box<dyn Error>> {
        let base: PathBuf = base_path.as_ref().into();
        let changelog = base.join("_old");
        let _ = fs::create_dir_all(&base).await?;
        Ok(Self {
            base_path: base,
            old_log: Mutex::new(fs::OpenOptions::new().append(true).create(true).open(changelog).await?),
        })
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
            Ok(Some(serde_json::from_slice::<Network>(raw.as_slice())?))
        } else {
            Ok(None)
        }
    }

    async fn save_network(&self, obj: Network) -> Result<(), Box<dyn Error>> {
        let base_network_path = network_path(&self.base_path, obj.id);
        let _ = fs::create_dir_all(base_network_path.parent().unwrap()).await;

        let prev = self.get_network(obj.id).await?;
        if let Some(prev) = prev {
            if obj == prev {
                return Ok(());
            }
            let mut j = zerotier_utils::json::to_json(&prev);
            j.push('\n');
            let _ = self.old_log.lock().await.write_all(j.as_bytes()).await?;
        }

        let _ = fs::write(base_network_path, to_json_pretty(&obj).as_bytes()).await?;
        return Ok(());
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
            Ok(Some(serde_json::from_slice::<Member>(raw.as_slice())?))
        } else {
            Ok(None)
        }
    }

    async fn save_member(&self, obj: Member) -> Result<(), Box<dyn Error>> {
        let base_member_path = member_path(&self.base_path, obj.network_id, obj.node_id);
        let _ = fs::create_dir_all(base_member_path.parent().unwrap()).await;

        let prev = self.get_member(obj.network_id, obj.node_id).await?;
        if let Some(prev) = prev {
            if obj == prev {
                return Ok(());
            }
            let mut j = zerotier_utils::json::to_json(&prev);
            j.push('\n');
            let _ = self.old_log.lock().await.write_all(j.as_bytes()).await?;
        }

        let _ = fs::write(base_member_path, to_json_pretty(&obj).as_bytes()).await?;
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
    fn test_db() {
        if let Ok(tokio_runtime) = zerotier_utils::tokio::runtime::Builder::new_current_thread().enable_all().build() {
            let _ = tokio_runtime.block_on(async {
                let node_id = Address::from_u64(0xdeadbeefu64).unwrap();
                let network_id = NetworkId::from_u64(0xfeedbeefcafebabeu64).unwrap();

                let test_dir = std::env::temp_dir().join("zt_filedatabase_test");
                println!("test filedatabase is in: {}", test_dir.as_os_str().to_str().unwrap());

                let _ = std::fs::remove_dir_all(&test_dir);

                let db = FileDatabase::new(test_dir).await.expect("new db");
                let mut test_member = Member::new_without_identity(node_id, network_id);

                for x in 0..3 {
                    test_member.name = x.to_string();
                    db.save_member(test_member.clone()).await.expect("member save ok");

                    let test_member2 = db.get_member(network_id, node_id).await.unwrap().unwrap();
                    //println!("{}", test_member.to_string());
                    //println!("{}", test_member2.to_string());
                    assert!(test_member == test_member2);
                }
            });
        }
    }
}
