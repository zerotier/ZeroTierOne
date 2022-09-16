// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::path::{Path, PathBuf};
use std::str::FromStr;
use std::sync::Arc;

use crate::localconfig::Config;
use crate::utils::{read_limit, DEFAULT_FILE_IO_READ_LIMIT};

use async_trait::async_trait;

use parking_lot::{Mutex, RwLock};

use zerotier_crypto::random::next_u32_secure;
use zerotier_network_hypervisor::vl1::{Identity, Storage};

const AUTH_TOKEN_DEFAULT_LENGTH: usize = 48;
const AUTH_TOKEN_POSSIBLE_CHARS: &'static str = "0123456789abcdefghijklmnopqrstuvwxyz";
const AUTH_TOKEN_FILENAME: &'static str = "authtoken.secret";
const IDENTITY_PUBLIC_FILENAME: &'static str = "identity.public";
const IDENTITY_SECRET_FILENAME: &'static str = "identity.secret";
const CONFIG_FILENAME: &'static str = "local.conf";

/// Abstraction around ZeroTier's home data directory.
pub struct DataDir {
    pub base_path: PathBuf,
    config: RwLock<Arc<Config>>,
    authtoken: Mutex<String>,
}

#[async_trait]
impl Storage for DataDir {
    async fn load_node_identity(&self) -> Option<Identity> {
        let id_data = read_limit(self.base_path.join(IDENTITY_SECRET_FILENAME), 4096).await;
        if id_data.is_err() {
            return None;
        }
        let id_data = Identity::from_str(String::from_utf8_lossy(id_data.unwrap().as_slice()).as_ref());
        if id_data.is_err() {
            return None;
        }
        Some(id_data.unwrap())
    }

    async fn save_node_identity(&self, id: &Identity) {
        assert!(id.secret.is_some());
        let id_secret_str = id.to_secret_string();
        let id_public_str = id.to_string();
        let secret_path = self.base_path.join(IDENTITY_SECRET_FILENAME);
        // TODO: handle errors
        let _ = tokio::fs::write(&secret_path, id_secret_str.as_bytes()).await;
        assert!(crate::utils::fs_restrict_permissions(&secret_path));
        let _ = tokio::fs::write(self.base_path.join(IDENTITY_PUBLIC_FILENAME), id_public_str.as_bytes()).await;
    }
}

impl DataDir {
    pub async fn open<P: AsRef<Path>>(path: P) -> std::io::Result<Self> {
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
        let config_data = read_limit(&config_path, DEFAULT_FILE_IO_READ_LIMIT).await;
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
    pub async fn authtoken(&self) -> std::io::Result<String> {
        let authtoken = self.authtoken.lock().clone();
        if authtoken.is_empty() {
            let authtoken_path = self.base_path.join(AUTH_TOKEN_FILENAME);
            let authtoken_bytes = read_limit(&authtoken_path, 4096).await;
            if authtoken_bytes.is_err() {
                let mut tmp = String::with_capacity(AUTH_TOKEN_DEFAULT_LENGTH);
                for _ in 0..AUTH_TOKEN_DEFAULT_LENGTH {
                    tmp.push(AUTH_TOKEN_POSSIBLE_CHARS.as_bytes()[(next_u32_secure() as usize) % AUTH_TOKEN_POSSIBLE_CHARS.len()] as char);
                }
                tokio::fs::write(&authtoken_path, tmp.as_bytes()).await?;
                assert!(crate::utils::fs_restrict_permissions(&authtoken_path));
                *self.authtoken.lock() = tmp;
            } else {
                *self.authtoken.lock() = String::from_utf8_lossy(authtoken_bytes.unwrap().as_slice()).into();
            }
        }
        Ok(authtoken)
    }

    /// Get a readable locked reference to this node's configuration.
    ///
    /// Use clone() to get a copy of the configuration if you want to modify it. Then use
    /// save_config() to save the modified configuration and update the internal copy in
    /// this structure.
    pub async fn config(&self) -> Arc<Config> {
        self.config.read().clone()
    }

    /// Save a modified copy of the configuration and replace the internal copy in this structure (if it's actually changed).
    pub async fn save_config(&self, modified_config: Config) -> std::io::Result<()> {
        if !modified_config.eq(&self.config.read()) {
            let config_data = crate::utils::to_json_pretty(&modified_config);
            tokio::fs::write(self.base_path.join(CONFIG_FILENAME), config_data.as_bytes()).await?;
            *self.config.write() = Arc::new(modified_config);
        }
        Ok(())
    }
}
