use std::path::{Path, PathBuf};
use std::sync::{Arc, Mutex, Weak};

use async_trait::async_trait;
use notify::{RecursiveMode, Watcher};
use serde::de::DeserializeOwned;

use zerotier_network_hypervisor::vl1::{Address, Identity, Valid};
use zerotier_network_hypervisor::vl2::NetworkId;
use zerotier_utils::reaper::Reaper;
use zerotier_utils::tokio::fs;
use zerotier_utils::tokio::runtime::Handle;
use zerotier_utils::tokio::sync::broadcast::{channel, Receiver, Sender};
use zerotier_utils::tokio::task::JoinHandle;
use zerotier_utils::tokio::time::{sleep, Duration, Instant};
use zerotier_vl1_service::datadir::{load_node_identity, save_node_identity};
use zerotier_vl1_service::VL1DataStorage;

use crate::cache::Cache;
use crate::database::{Change, Database, Error};
use crate::model::*;

const EVENT_HANDLER_TASK_TIMEOUT: Duration = Duration::from_secs(10);

/// An in-filesystem database that permits live editing.
///
/// A cache is maintained that contains the actual objects. When an object is live edited,
/// once it successfully reads and loads it is merged with the cached object and saved to
/// the cache. The cache will also contain any ephemeral data, generated data, etc.
///
/// The file format is YAML instead of JSON for better human friendliness and the layout
/// is different from V1 so it'll need a converter to use with V1 FileDb controller data.
pub struct FileDatabase {
    base_path: PathBuf,
    local_identity: Valid<Identity>,
    change_sender: Sender<Change>,
    tasks: Reaper,
    cache: Cache,
    daemon: JoinHandle<()>,
}

// TODO: should cache at least hashes and detect changes in the filesystem live.

impl FileDatabase {
    pub async fn new<P: AsRef<Path>>(runtime: Handle, base_path: P) -> Result<Arc<Self>, Error> {
        let base_path: PathBuf = base_path.as_ref().into();

        let (change_sender, _) = channel(256);
        let db_weak_tmp: Arc<Mutex<Weak<Self>>> = Arc::new(Mutex::new(Weak::default()));
        let db_weak = db_weak_tmp.clone();
        let runtime2 = runtime.clone();

        let local_identity =
            load_node_identity(base_path.as_path()).ok_or(std::io::Error::new(std::io::ErrorKind::NotFound, "identity.secret not found"))?;
        let controller_address = local_identity.address;

        let db = Arc::new(Self {
            base_path: base_path.clone(),
            local_identity,
            change_sender,
            tasks: Reaper::new(&runtime2),
            cache: Cache::new(),
            daemon: runtime2.spawn(async move {
                let mut watcher = notify::recommended_watcher(move |event: notify::Result<notify::event::Event>| {
                    if let Ok(event) = event {
                        match event.kind {
                            notify::EventKind::Create(_) | notify::EventKind::Modify(_) | notify::EventKind::Remove(_) => {
                                if let Some(db) = db_weak.lock().unwrap().upgrade() {
                                    db.clone().tasks.add(
                                        runtime.spawn(async move {
                                            if let Some(path0) = event.paths.first() {
                                                if let Some((record_type, network_id, node_id)) =
                                                    Self::record_type_from_path(controller_address, path0.as_path())
                                                {
                                                    // Paths to objects that were deleted or changed. Changed includes adding new objects.
                                                    let mut deleted = None;
                                                    let mut changed = None;

                                                    match event.kind {
                                                        notify::EventKind::Create(create_kind) => match create_kind {
                                                            notify::event::CreateKind::File => {
                                                                changed = Some(path0.as_path());
                                                            }
                                                            _ => {}
                                                        },
                                                        notify::EventKind::Modify(modify_kind) => match modify_kind {
                                                            notify::event::ModifyKind::Data(_) => {
                                                                changed = Some(path0.as_path());
                                                            }
                                                            notify::event::ModifyKind::Name(rename_mode) => match rename_mode {
                                                                notify::event::RenameMode::Both => {
                                                                    if event.paths.len() >= 2 {
                                                                        if let Some(path1) = event.paths.last() {
                                                                            deleted = Some(path0.as_path());
                                                                            changed = Some(path1.as_path());
                                                                        }
                                                                    }
                                                                }
                                                                notify::event::RenameMode::From => {
                                                                    deleted = Some(path0.as_path());
                                                                }
                                                                notify::event::RenameMode::To => {
                                                                    changed = Some(path0.as_path());
                                                                }
                                                                _ => {}
                                                            },
                                                            _ => {}
                                                        },
                                                        notify::EventKind::Remove(remove_kind) => match remove_kind {
                                                            notify::event::RemoveKind::File => {
                                                                deleted = Some(path0.as_path());
                                                            }
                                                            _ => {}
                                                        },
                                                        _ => {}
                                                    }

                                                    if deleted.is_some() {
                                                        match record_type {
                                                            RecordType::Network => {
                                                                if let Some((network, members)) = db.cache.on_network_deleted(network_id) {
                                                                    let _ = db.change_sender.send(Change::NetworkDeleted(network, members));
                                                                }
                                                            }
                                                            RecordType::Member => {
                                                                if let Some(node_id) = node_id {
                                                                    if let Some(member) = db.cache.on_member_deleted(network_id, node_id) {
                                                                        let _ = db.change_sender.send(Change::MemberDeleted(member));
                                                                    }
                                                                }
                                                            }
                                                            _ => {}
                                                        }
                                                    }

                                                    if let Some(changed) = changed {
                                                        match record_type {
                                                            RecordType::Network => {
                                                                if let Ok(Some(new_network)) = Self::load_object::<Network>(changed).await {
                                                                    match db.cache.on_network_updated(new_network.clone()) {
                                                                        (true, Some(old_network)) => {
                                                                            let _ = db
                                                                                .change_sender
                                                                                .send(Change::NetworkChanged(old_network, new_network));
                                                                        }
                                                                        (true, None) => {
                                                                            let _ = db.change_sender.send(Change::NetworkCreated(new_network));
                                                                        }
                                                                        _ => {}
                                                                    }
                                                                }
                                                            }
                                                            RecordType::Member => {
                                                                if let Ok(Some(new_member)) = Self::load_object::<Member>(changed).await {
                                                                    match db.cache.on_member_updated(new_member.clone()) {
                                                                        (true, Some(old_member)) => {
                                                                            let _ =
                                                                                db.change_sender.send(Change::MemberChanged(old_member, new_member));
                                                                        }
                                                                        (true, None) => {
                                                                            let _ = db.change_sender.send(Change::MemberCreated(new_member));
                                                                        }
                                                                        _ => {}
                                                                    }
                                                                }
                                                            }
                                                            _ => {}
                                                        }
                                                    }
                                                }
                                            }
                                        }),
                                        Instant::now().checked_add(EVENT_HANDLER_TASK_TIMEOUT).unwrap(),
                                    );
                                }
                            }
                            _ => {}
                        }
                    }
                })
                .expect("FATAL: unable to start filesystem change listener");
                let _ = watcher.configure(
                    notify::Config::default()
                        .with_compare_contents(true)
                        .with_poll_interval(std::time::Duration::from_secs(2)),
                );
                watcher
                    .watch(&base_path, RecursiveMode::Recursive)
                    .expect("FATAL: unable to watch base path");

                loop {
                    // Any periodic background stuff can be put here. Adjust timing as needed.
                    sleep(Duration::from_secs(10)).await;
                }
            }),
        });

        db.cache.load_all(db.as_ref()).await?;
        *db_weak_tmp.lock().unwrap() = Arc::downgrade(&db); // this starts the daemon tasks and starts watching for file changes

        Ok(db)
    }

    fn network_path(&self, network_id: NetworkId) -> PathBuf {
        self.base_path.join(format!("N{:06x}", network_id.network_no())).join("config.yaml")
    }

    fn member_path(&self, network_id: NetworkId, member_id: Address) -> PathBuf {
        self.base_path
            .join(format!("N{:06x}", network_id.network_no()))
            .join(format!("M{}.yaml", member_id.to_string()))
    }

    async fn load_object<O: DeserializeOwned>(path: &Path) -> Result<Option<O>, Error> {
        if let Ok(raw) = fs::read(path).await {
            return Ok(Some(serde_yaml::from_slice::<O>(raw.as_slice())?));
        } else {
            return Ok(None);
        }
    }

    /// Get record type and also the number after it: network number or address.
    fn record_type_from_path(controller_address: Address, p: &Path) -> Option<(RecordType, NetworkId, Option<Address>)> {
        let parent = p.parent()?.file_name()?.to_string_lossy();
        if parent.len() == 7 && (parent.starts_with("N") || parent.starts_with('n')) {
            let network_id = NetworkId::from_controller_and_network_no(controller_address, u64::from_str_radix(&parent[1..], 16).ok()?)?;
            if let Some(file_name) = p.file_name().map(|p| p.to_string_lossy().to_lowercase()) {
                if file_name.eq("config.yaml") {
                    return Some((RecordType::Network, network_id, None));
                } else if file_name.len() == 16 && file_name.starts_with("m") && file_name.ends_with(".yaml") {
                    return Some((
                        RecordType::Member,
                        network_id,
                        Some(Address::from_u64(u64::from_str_radix(&file_name.as_str()[1..11], 16).unwrap_or(0))?),
                    ));
                }
            }
        }
        return None;
    }
}

impl Drop for FileDatabase {
    fn drop(&mut self) {
        self.daemon.abort();
    }
}

impl VL1DataStorage for FileDatabase {
    fn load_node_identity(&self) -> Option<Valid<Identity>> {
        load_node_identity(self.base_path.as_path())
    }

    fn save_node_identity(&self, id: &Valid<Identity>) -> bool {
        save_node_identity(self.base_path.as_path(), id)
    }
}

#[async_trait]
impl Database for FileDatabase {
    async fn list_networks(&self) -> Result<Vec<NetworkId>, Error> {
        let mut networks = Vec::new();
        let controller_address_shift24 = u64::from(self.local_identity.address).wrapping_shl(24);
        let mut dir = fs::read_dir(&self.base_path).await?;
        while let Ok(Some(ent)) = dir.next_entry().await {
            if ent.file_type().await.map_or(false, |t| t.is_dir()) {
                let osname = ent.file_name();
                let name = osname.to_string_lossy();
                if name.len() == 7 && name.starts_with("N") {
                    if fs::metadata(ent.path().join("config.yaml")).await.is_ok() {
                        if let Ok(nwid_last24bits) = u64::from_str_radix(&name[1..], 16) {
                            if let Some(nwid) = NetworkId::from_u64(controller_address_shift24 | nwid_last24bits) {
                                networks.push(nwid);
                            }
                        }
                    }
                }
            }
        }
        Ok(networks)
    }

    async fn get_network(&self, id: NetworkId) -> Result<Option<Network>, Error> {
        let mut network = Self::load_object::<Network>(self.network_path(id).as_path()).await?;
        if let Some(network) = network.as_mut() {
            // FileDatabase stores networks by their "network number" and automatically adapts their IDs
            // if the controller's identity changes. This is done to make it easy to just clone networks,
            // including storing them in "git."
            let network_id_should_be = network.id.change_network_controller(self.local_identity.address);
            if network.id != network_id_should_be {
                network.id = network_id_should_be;
                let _ = self.save_network(network.clone(), false).await?;
            }
        }
        Ok(network)
    }

    async fn save_network(&self, obj: Network, generate_change_notification: bool) -> Result<(), Error> {
        if !generate_change_notification {
            let _ = self.cache.on_network_updated(obj.clone());
        }
        let base_network_path = self.network_path(obj.id);
        let _ = fs::create_dir_all(base_network_path.parent().unwrap()).await;
        let _ = fs::write(base_network_path, serde_yaml::to_string(&obj)?.as_bytes()).await?;
        return Ok(());
    }

    async fn list_members(&self, network_id: NetworkId) -> Result<Vec<Address>, Error> {
        let mut members = Vec::new();
        let mut dir = fs::read_dir(self.base_path.join(format!("N{:06x}", network_id.network_no()))).await?;
        while let Ok(Some(ent)) = dir.next_entry().await {
            if ent.file_type().await.map_or(false, |t| t.is_file() || t.is_symlink()) {
                let osname = ent.file_name();
                let name = osname.to_string_lossy();
                if name.len() == (zerotier_network_hypervisor::protocol::ADDRESS_SIZE_STRING + 6) && name.starts_with("M") && name.ends_with(".yaml")
                {
                    if let Ok(member_address) = u64::from_str_radix(&name[1..11], 16) {
                        if let Some(member_address) = Address::from_u64(member_address) {
                            members.push(member_address);
                        }
                    }
                }
            }
        }
        Ok(members)
    }

    async fn get_member(&self, network_id: NetworkId, node_id: Address) -> Result<Option<Member>, Error> {
        let mut member = Self::load_object::<Member>(self.member_path(network_id, node_id).as_path()).await?;
        if let Some(member) = member.as_mut() {
            if member.network_id != network_id {
                // Also auto-update member network IDs, see get_network().
                member.network_id = network_id;
                self.save_member(member.clone(), false).await?;
            }
        }
        Ok(member)
    }

    async fn save_member(&self, obj: Member, generate_change_notification: bool) -> Result<(), Error> {
        if !generate_change_notification {
            let _ = self.cache.on_member_updated(obj.clone());
        }
        let base_member_path = self.member_path(obj.network_id, obj.node_id);
        let _ = fs::create_dir_all(base_member_path.parent().unwrap()).await;
        let _ = fs::write(base_member_path, serde_yaml::to_string(&obj)?.as_bytes()).await?;
        Ok(())
    }

    async fn changes(&self) -> Option<Receiver<Change>> {
        Some(self.change_sender.subscribe())
    }

    async fn log_request(&self, obj: RequestLogItem) -> Result<(), Error> {
        println!("{}", obj.to_string());
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    #[allow(unused_imports)]
    use super::*;
    use std::sync::atomic::{AtomicUsize, Ordering};

    #[allow(unused)]
    #[test]
    fn test_db() {
        if let Ok(tokio_runtime) = zerotier_utils::tokio::runtime::Builder::new_current_thread().enable_all().build() {
            let _ = tokio_runtime.block_on(async {
                let node_id = Address::from_u64(0xdeadbeefu64).unwrap();
                let network_id = NetworkId::from_u64(0xfeedbeefcafebabeu64).unwrap();

                let test_dir = std::env::temp_dir().join("zt_filedatabase_test");
                println!("test filedatabase is in: {}", test_dir.as_os_str().to_str().unwrap());

                let _ = std::fs::remove_dir_all(&test_dir);
                let controller_id = Identity::generate();

                assert!(fs::create_dir_all(&test_dir).await.is_ok());
                assert!(save_node_identity(test_dir.as_path(), &controller_id));
                let db = Arc::new(FileDatabase::new(tokio_runtime.handle().clone(), test_dir).await.expect("new db"));

                let change_count = Arc::new(AtomicUsize::new(0));

                let db2 = db.clone();
                let change_count2 = change_count.clone();
                tokio_runtime.spawn(async move {
                    let mut change_receiver = db2.changes().await.unwrap();
                    loop {
                        if let Ok(change) = change_receiver.recv().await {
                            change_count2.fetch_add(1, Ordering::SeqCst);
                            //println!("[FileDatabase] {:#?}", change);
                        } else {
                            break;
                        }
                    }
                });

                let mut test_network = Network::new(network_id);
                db.save_network(test_network.clone(), true).await.expect("network save error");

                let mut test_member = Member::new_without_identity(node_id, network_id);
                for x in 0..3 {
                    test_member.name = x.to_string();
                    db.save_member(test_member.clone(), true).await.expect("member save error");

                    zerotier_utils::tokio::task::yield_now().await;
                    sleep(Duration::from_millis(100)).await;
                    zerotier_utils::tokio::task::yield_now().await;

                    let test_member2 = db.get_member(network_id, node_id).await.unwrap().unwrap();
                    assert!(test_member == test_member2);
                }
            });
        }
    }
}
