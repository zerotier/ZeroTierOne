// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::path::{Path, PathBuf};
use std::str::FromStr;
use std::sync::{Arc, Mutex, RwLock};

use serde::de::DeserializeOwned;
use serde::Serialize;

use zerotier_crypto::random::next_u32_secure;
use zerotier_network_hypervisor::vl1::{Identity, Valid};
use zerotier_utils::io::{fs_restrict_permissions, read_limit, DEFAULT_FILE_IO_READ_LIMIT};
use zerotier_utils::json::to_json_pretty;

use crate::vl1service::VL1DataStorage;

pub const AUTH_TOKEN_FILENAME: &'static str = "authtoken.secret";
pub const IDENTITY_PUBLIC_FILENAME: &'static str = "identity.public";
pub const IDENTITY_SECRET_FILENAME: &'static str = "identity.secret";
pub const CONFIG_FILENAME: &'static str = "local.conf";

const AUTH_TOKEN_DEFAULT_LENGTH: usize = 48;
const AUTH_TOKEN_POSSIBLE_CHARS: &'static str = "0123456789abcdefghijklmnopqrstuvwxyz";

pub fn load_node_identity(base_path: &Path) -> Option<Valid<Identity>> {
    let id_data = read_limit(base_path.join(IDENTITY_SECRET_FILENAME), 4096);
    if id_data.is_err() {
        return None;
    }
    let id_data = Identity::from_str(String::from_utf8_lossy(id_data.unwrap().as_slice()).as_ref());
    if id_data.is_err() {
        return None;
    }
    Some(Valid::mark_valid(id_data.unwrap()))
}

pub fn save_node_identity(base_path: &Path, id: &Valid<Identity>) -> bool {
    assert!(id.secret.is_some());
    let id_secret_str = id.to_secret_string();
    let id_public_str = id.to_string();
    let secret_path = base_path.join(IDENTITY_SECRET_FILENAME);
    if std::fs::write(&secret_path, id_secret_str.as_bytes()).is_err() {
        return false;
    }
    assert!(fs_restrict_permissions(&secret_path));
    return std::fs::write(base_path.join(IDENTITY_PUBLIC_FILENAME), id_public_str.as_bytes()).is_ok();
}

pub struct DataDir<Config: PartialEq + Eq + Clone + Send + Sync + Default + Serialize + DeserializeOwned + 'static> {
    pub base_path: PathBuf,
    config: RwLock<Arc<Config>>,
    authtoken: Mutex<String>,
}

impl<Config: PartialEq + Eq + Clone + Send + Sync + Default + Serialize + DeserializeOwned + 'static> VL1DataStorage for DataDir<Config> {
    fn load_node_identity(&self) -> Option<Valid<Identity>> {
        load_node_identity(self.base_path.as_path())
    }

    fn save_node_identity(&self, id: &Valid<Identity>) -> bool {
        save_node_identity(self.base_path.as_path(), id)
    }
}

impl<Config: PartialEq + Eq + Clone + Send + Sync + Default + Serialize + DeserializeOwned + 'static> DataDir<Config> {
    pub fn open<P: AsRef<Path>>(path: P) -> std::io::Result<Self> {
        let base_path = path.as_ref().to_path_buf();
        if !base_path.is_dir() {
            let _ = std::fs::create_dir_all(&base_path);
            if !base_path.is_dir() {
                return Err(std::io::Error::new(
                    std::io::ErrorKind::NotFound,
                    "base path not found and cannot be created",
                ));
            }
        }

        let config_path = base_path.join(CONFIG_FILENAME);
        let config_data = read_limit(&config_path, DEFAULT_FILE_IO_READ_LIMIT);
        let config = RwLock::new(Arc::new(if config_data.is_ok() {
            let c = serde_json::from_slice::<Config>(config_data.unwrap().as_slice());
            if c.is_err() {
                return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, c.err().unwrap()));
            }
            c.unwrap()
        } else {
            if config_path.is_file() {
                return Err(std::io::Error::new(std::io::ErrorKind::PermissionDenied, "local.conf not readable"));
            } else {
                Config::default()
            }
        }));

        return Ok(Self { base_path, config, authtoken: Mutex::new(String::new()) });
    }

    /// Get authorization token for local API, creating and saving if it does not exist.
    pub fn authtoken(&self) -> std::io::Result<String> {
        let authtoken = self.authtoken.lock().unwrap().clone();
        if authtoken.is_empty() {
            let authtoken_path = self.base_path.join(AUTH_TOKEN_FILENAME);
            let authtoken_bytes = read_limit(&authtoken_path, 4096);
            if authtoken_bytes.is_err() {
                let mut tmp = String::with_capacity(AUTH_TOKEN_DEFAULT_LENGTH);
                for _ in 0..AUTH_TOKEN_DEFAULT_LENGTH {
                    tmp.push(AUTH_TOKEN_POSSIBLE_CHARS.as_bytes()[(next_u32_secure() as usize) % AUTH_TOKEN_POSSIBLE_CHARS.len()] as char);
                }
                std::fs::write(&authtoken_path, tmp.as_bytes())?;
                assert!(fs_restrict_permissions(&authtoken_path));
                *self.authtoken.lock().unwrap() = tmp;
            } else {
                *self.authtoken.lock().unwrap() = String::from_utf8_lossy(authtoken_bytes.unwrap().as_slice()).into();
            }
        }
        Ok(authtoken)
    }

    /// Get a readable locked reference to this node's configuration.
    ///
    /// Use clone() to get a copy of the configuration if you want to modify it. Then use
    /// save_config() to save the modified configuration and update the internal copy in
    /// this structure.
    pub fn config(&self) -> Arc<Config> {
        self.config.read().unwrap().clone()
    }

    /// Save a modified copy of the configuration and replace the internal copy in this structure (if it's actually changed).
    pub fn save_config(&self, modified_config: Config) -> std::io::Result<()> {
        if !modified_config.eq(&self.config.read().unwrap()) {
            let config_data = to_json_pretty(&modified_config);
            std::fs::write(self.base_path.join(CONFIG_FILENAME), config_data.as_bytes())?;
            *self.config.write().unwrap() = Arc::new(modified_config);
        }
        Ok(())
    }
}
