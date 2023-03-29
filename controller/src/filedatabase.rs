use std::collections::BTreeMap;
use std::mem::replace;
use std::path::{Path, PathBuf};
use std::sync::{Arc, Weak};

use serde::{Deserialize, Serialize};

use async_trait::async_trait;
use zerotier_utils::tokio::io::AsyncWriteExt;

use crate::database;
use crate::database::Change;
use crate::model::{Member, Network, RequestLogItem};

use zerotier_network_hypervisor::vl1::PartialAddress;
use zerotier_network_hypervisor::vl2::NetworkId;
use zerotier_utils::tokio;
use zerotier_utils::tokio::sync::{broadcast, mpsc};

pub struct FileDatabase {
    db_path: PathBuf,
    log: Option<tokio::sync::Mutex<tokio::fs::File>>,
    data: tokio::sync::Mutex<(BTreeMap<NetworkId, FileDbNetwork>, bool)>,
    change_sender: broadcast::Sender<Change>,
    file_write_notify_sender: mpsc::Sender<()>,
    file_writer: tokio::task::JoinHandle<()>,
}

#[derive(Serialize, Deserialize)]
struct FileDbNetwork {
    pub config: Network,
    pub members: BTreeMap<PartialAddress, Member>,
}

impl FileDatabase {
    pub async fn new(db_path: &Path, log_path: Option<&Path>) -> Result<Arc<Self>, Box<dyn std::error::Error + Send + Sync>> {
        let data_bytes = tokio::fs::read(db_path).await;
        let mut data: BTreeMap<NetworkId, FileDbNetwork> = BTreeMap::new();
        if let Err(e) = data_bytes {
            if !matches!(e.kind(), tokio::io::ErrorKind::NotFound) {
                return Err(Box::new(e));
            }
        } else {
            data = serde_json::from_slice(data_bytes.as_ref().unwrap().as_slice())?;
        }

        let log = if let Some(log_path) = log_path {
            Some(tokio::sync::Mutex::new(
                tokio::fs::OpenOptions::new().append(true).create(true).mode(0o600).open(log_path).await?,
            ))
        } else {
            None
        };

        let (file_write_notify_sender, mut file_write_notify_receiver) = mpsc::channel(16);
        let db = Arc::new_cyclic(|self_weak: &Weak<FileDatabase>| {
            let self_weak = self_weak.clone();
            Self {
                db_path: db_path.to_path_buf(),
                log,
                data: tokio::sync::Mutex::new((data, false)),
                change_sender: broadcast::channel(16).0,
                file_write_notify_sender,
                file_writer: tokio::task::spawn(async move {
                    loop {
                        file_write_notify_receiver.recv().await;
                        if let Some(db) = self_weak.upgrade() {
                            let mut data = db.data.lock().await;
                            if data.1 {
                                let json = zerotier_utils::json::to_json_pretty(&data.0);
                                if let Err(e) = tokio::fs::write(db.db_path.as_path(), json.as_bytes()).await {
                                    eprintln!(
                                        "WARNING: controller changes not persisted! unable to write file database to '{}': {}",
                                        db.db_path.to_string_lossy(),
                                        e.to_string()
                                    );
                                } else {
                                    data.1 = false;
                                }
                            }
                        } else {
                            break;
                        }
                    }
                }),
            }
        });

        Ok(db)
    }
}

impl Drop for FileDatabase {
    fn drop(&mut self) {
        self.file_writer.abort();
    }
}

#[async_trait]
impl database::Database for FileDatabase {
    async fn list_networks(&self) -> Result<Vec<NetworkId>, database::Error> {
        Ok(self.data.lock().await.0.keys().cloned().collect())
    }

    async fn get_network(&self, id: &NetworkId) -> Result<Option<Network>, database::Error> {
        Ok(self.data.lock().await.0.get(id).map(|x| x.config.clone()))
    }

    async fn save_network(&self, obj: Network, generate_change_notification: bool) -> Result<(), database::Error> {
        let mut data = self.data.lock().await;
        if let Some(nw) = data.0.get_mut(&obj.id) {
            if !nw.config.eq(&obj) {
                let old = replace(&mut nw.config, obj);
                if generate_change_notification {
                    let _ = self.change_sender.send(Change::NetworkChanged(old, nw.config.clone()));
                }
                let _ = self.file_write_notify_sender.send(()).await;
            }
        } else {
            data.0
                .insert(obj.id.clone(), FileDbNetwork { config: obj.clone(), members: BTreeMap::new() });
            if generate_change_notification {
                let _ = self.change_sender.send(Change::NetworkCreated(obj));
            }
            let _ = self.file_write_notify_sender.send(()).await;
        }
        return Ok(());
    }

    async fn list_members(&self, network_id: &NetworkId) -> Result<Vec<PartialAddress>, database::Error> {
        Ok(self
            .data
            .lock()
            .await
            .0
            .get(network_id)
            .map_or_else(|| Vec::new(), |x| x.members.keys().cloned().collect()))
    }

    async fn get_member(&self, network_id: &NetworkId, node_id: &PartialAddress) -> Result<Option<Member>, database::Error> {
        Ok(self
            .data
            .lock()
            .await
            .0
            .get_mut(network_id)
            .and_then(|x| node_id.find_unique_match(&x.members).cloned()))
    }

    async fn save_member(&self, mut obj: Member, generate_change_notification: bool) -> Result<(), database::Error> {
        let mut data = self.data.lock().await;
        if let Some(nw) = data.0.get_mut(&obj.network_id) {
            if let Some(member) = obj.node_id.find_unique_match_mut(&mut nw.members) {
                if !obj.eq(member) {
                    if member.node_id.specificity_bytes() != obj.node_id.specificity_bytes() {
                        // If the specificity of the node_id has changed we have to delete and re-add the entry.

                        let old_node_id = member.node_id.clone();
                        let old = nw.members.remove(&old_node_id);

                        if old_node_id.specificity_bytes() > obj.node_id.specificity_bytes() {
                            obj.node_id = old_node_id;
                        }

                        nw.members.insert(obj.node_id.clone(), obj.clone());

                        if generate_change_notification {
                            let _ = self.change_sender.send(Change::MemberChanged(old.unwrap(), obj));
                        }
                    } else {
                        let old = replace(member, obj);
                        if generate_change_notification {
                            let _ = self.change_sender.send(Change::MemberChanged(old, member.clone()));
                        }
                    }
                    let _ = self.file_write_notify_sender.send(()).await;
                }
            } else {
                let _ = nw.members.insert(obj.node_id.clone(), obj.clone());
                if generate_change_notification {
                    let _ = self.change_sender.send(Change::MemberCreated(obj));
                }
                let _ = self.file_write_notify_sender.send(()).await;
            }
        }
        return Ok(());
    }

    async fn log_request(&self, obj: RequestLogItem) -> Result<(), database::Error> {
        if let Some(log) = self.log.as_ref() {
            let mut json_line = zerotier_utils::json::to_json(&obj);
            json_line.push('\n');
            let _ = log.lock().await.write_all(json_line.as_bytes()).await;
        }
        Ok(())
    }

    async fn changes(&self) -> Option<broadcast::Receiver<Change>> {
        Some(self.change_sender.subscribe())
    }
}
