/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::ffi::CString;
use std::io::{Read, Write};
use std::path::{Path, PathBuf};
use std::str::FromStr;
use std::sync::Mutex;

use crate::localconfig::LocalConfig;

use zerotier_network_hypervisor::vl1::identity::NetworkId;

const ZEROTIER_PID: &'static str = "zerotier.pid";
const ZEROTIER_URI: &'static str = "zerotier.uri";
const LOCAL_CONF: &'static str = "local.conf";
const AUTHTOKEN_SECRET: &'static str = "authtoken.secret";
const SERVICE_LOG: &'static str = "service.log";

#[derive(Clone, Copy)]
pub enum StateObjectType {
    IdentityPublic,
    IdentitySecret,
    NetworkConfig,
    Peer,
}

#[cfg(any(target_os = "macos", target_os = "ios"))]
pub fn platform_default_home_path() -> String {
    "/Library/Application Support/ZeroTier".into_string()
}

/// In-filesystem data store for configuration and objects.
pub(crate) struct Store {
    pub base_path: Box<Path>,
    pub default_log_path: Box<Path>,
    previous_local_config_on_disk: Mutex<String>,
    peers_path: Box<Path>,
    controller_path: Box<Path>,
    networks_path: Box<Path>,
    auth_token_path: Mutex<Box<Path>>,
    auth_token: Mutex<String>,
}

/// Restrict file permissions using OS-specific code in osdep/OSUtils.cpp.
pub fn lock_down_file(path: &str) {
    // TODO: need both Windows and Unix implementations
}

impl Store {
    const MAX_OBJECT_SIZE: usize = 262144; // sanity limit

    pub fn new(base_path: &str, auth_token_path_override: &Option<String>, auth_token_override: &Option<String>) -> std::io::Result<Store> {
        let bp = Path::new(base_path);
        let _ = std::fs::create_dir_all(bp);
        let md = bp.metadata()?;
        if !md.is_dir() || md.permissions().readonly() {
            return Err(std::io::Error::new(std::io::ErrorKind::PermissionDenied, "base path does not exist or is not writable"));
        }

        let s = Store {
            base_path: bp.to_path_buf().into_boxed_path(),
            default_log_path: bp.join(SERVICE_LOG).into_boxed_path(),
            previous_local_config_on_disk: Mutex::new(String::new()),
            peers_path: bp.join("peers.d").into_boxed_path(),
            controller_path: bp.join("controller.d").into_boxed_path(),
            networks_path: bp.join("networks.d").into_boxed_path(),
            auth_token_path: Mutex::new(auth_token_path_override.map_or_else(|| bp.join(AUTHTOKEN_SECRET).into_boxed_path(), |auth_token_path_override| PathBuf::from(auth_token_path_override).into_boxed_path())),
            auth_token: Mutex::new(auth_token_override.map_or_else(|| String::new(), |auth_token_override| auth_token_override)),
        };

        let _ = std::fs::create_dir_all(&s.peers_path);
        let _ = std::fs::create_dir_all(&s.controller_path);
        let _ = std::fs::create_dir_all(&s.networks_path);

        Ok(s)
    }

    fn make_obj_path_internal(&self, obj_type: StateObjectType, obj_id: &[u64]) -> Option<PathBuf> {
        match obj_type {
            StateObjectType::IdentityPublic => Some(self.base_path.join("identity.public")),
            StateObjectType::IdentitySecret => Some(self.base_path.join("identity.secret")),
            StateObjectType::NetworkConfig => {
                if obj_id.len() < 1 {
                    None
                } else {
                    Some(self.networks_path.join(format!("{:0>16x}.conf", obj_id[0])))
                }
            }
            StateObjectType::Peer => {
                if obj_id.len() < 1 {
                    None
                } else {
                    Some(self.peers_path.join(format!("{:0>10x}.peer", obj_id[0])))
                }
            }
        }
    }

    fn read_internal(&self, path: PathBuf) -> std::io::Result<Vec<u8>> {
        let fmd = path.metadata()?;
        if fmd.is_file() {
            let flen = fmd.len();
            if flen <= Store::MAX_OBJECT_SIZE as u64 {
                let mut f = std::fs::File::open(path)?;
                let mut buf: Vec<u8> = Vec::new();
                buf.reserve(flen as usize);
                let rs = f.read_to_end(&mut buf)?;
                buf.resize(rs as usize, 0);
                return Ok(buf);
            }
        }
        Err(std::io::Error::new(std::io::ErrorKind::NotFound, "does not exist or is not readable"))
    }

    pub fn auth_token(&self, generate_if_missing: bool) -> std::io::Result<String> {
        let mut token = self.auth_token.lock().unwrap();
        if token.is_empty() {
            let p = self.auth_token_path.lock().unwrap();
            let ps = p.to_str().unwrap();

            let token2 = self.read_file(ps).map_or(String::new(), |sb| String::from_utf8(sb).unwrap_or(String::new()).trim().to_string());
            if token2.is_empty() {
                if generate_if_missing {
                    let mut rb = [0_u8; 32];
                    zerotier_core_crypto::random::fill_bytes_secure(&mut rb);
                    token.reserve(rb.len());
                    for b in rb.iter() {
                        if *b > 127_u8 {
                            token.push((65 + (*b % 26)) as char); // A..Z
                        } else {
                            token.push((97 + (*b % 26)) as char); // a..z
                        }
                    }
                    let res = self.write_file(ps, token.as_bytes());
                    if res.is_err() {
                        token.clear();
                        Err(res.err().unwrap())
                    } else {
                        lock_down_file(ps);
                        Ok(token.clone())
                    }
                } else {
                    Err(std::io::Error::new(std::io::ErrorKind::NotFound, ""))
                }
            } else {
                *token = token2;
                Ok(token.clone())
            }
        } else {
            Ok(token.clone())
        }
    }

    pub fn list_joined_networks(&self) -> Vec<NetworkId> {
        let mut list: Vec<NetworkId> = Vec::new();
        let d = std::fs::read_dir(self.networks_path.as_ref());
        if d.is_ok() {
            for de in d.unwrap() {
                if de.is_ok() {
                    let nn = de.unwrap().file_name();
                    let n = nn.to_str().unwrap_or("");
                    if n.len() == 21 && n.ends_with(".conf") {
                        // ################.conf
                        let nwid = u64::from_str_radix(&n[0..16], 16);
                        if nwid.is_ok() {
                            list.push(NetworkId(nwid.unwrap()));
                        }
                    }
                }
            }
        }
        list
    }

    pub fn read_file(&self, fname: &str) -> std::io::Result<Vec<u8>> {
        self.read_internal(self.base_path.join(fname))
    }

    pub fn read_file_str(&self, fname: &str) -> std::io::Result<String> {
        let data = self.read_file(fname)?;
        let data = String::from_utf8(data);
        if data.is_err() {
            return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, data.err().unwrap().to_string()));
        }
        Ok(data.unwrap())
    }

    pub fn write_file(&self, fname: &str, data: &[u8]) -> std::io::Result<()> {
        std::fs::OpenOptions::new().write(true).truncate(true).create(true).open(self.base_path.join(fname))?.write_all(data)
    }

    pub fn read_local_conf(&self, skip_if_unchanged: bool) -> Option<std::io::Result<LocalConfig>> {
        let data = self.read_file_str(LOCAL_CONF);
        if data.is_err() {
            return Some(Err(data.err().unwrap()));
        }
        let data = data.unwrap();
        if skip_if_unchanged {
            let mut prev = self.previous_local_config_on_disk.lock().unwrap();
            if prev.eq(&data) {
                return None;
            }
            *prev = data.clone();
        } else {
            *(self.previous_local_config_on_disk.lock().unwrap()) = data.clone();
        }
        let lc = serde_json::from_str::<LocalConfig>(data.as_str());
        if lc.is_err() {
            return Some(Err(std::io::Error::new(std::io::ErrorKind::InvalidData, lc.err().unwrap())));
        }
        Some(Ok(lc.unwrap()))
    }

    pub fn read_local_conf_or_default(&self) -> LocalConfig {
        let lc = self.read_local_conf(false);
        if lc.is_some() {
            let lc = lc.unwrap();
            if lc.is_ok() {
                return lc.unwrap();
            }
        }
        LocalConfig::default()
    }

    pub fn write_local_conf(&self, lc: &LocalConfig) -> std::io::Result<()> {
        let json = serde_json::to_string(lc).unwrap();
        self.write_file(LOCAL_CONF, json.as_bytes())
    }

    pub fn write_pid(&self) -> std::io::Result<()> {
        let pid = unsafe { libc::getpid() }.to_string();
        self.write_file(ZEROTIER_PID, pid.as_bytes())
    }

    pub fn erase_pid(&self) {
        let _ = std::fs::remove_file(self.base_path.join(ZEROTIER_PID));
    }

    pub fn load_object(&self, obj_type: StateObjectType, obj_id: &[u64]) -> std::io::Result<Vec<u8>> {
        let obj_path = self.make_obj_path_internal(obj_type, obj_id);
        if obj_path.is_some() {
            return self.read_internal(obj_path.unwrap());
        }
        Err(std::io::Error::new(std::io::ErrorKind::NotFound, "does not exist or is not readable"))
    }

    pub fn erase_object(&self, obj_type: StateObjectType, obj_id: &[u64]) {
        let obj_path = self.make_obj_path_internal(obj_type, obj_id);
        if obj_path.is_some() {
            let _ = std::fs::remove_file(obj_path.unwrap());
        }
    }

    pub fn store_object(&self, obj_type: StateObjectType, obj_id: &[u64], obj_data: &[u8]) -> std::io::Result<()> {
        let obj_path = self.make_obj_path_internal(obj_type, obj_id);
        if obj_path.is_some() {
            let obj_path = obj_path.unwrap();
            std::fs::OpenOptions::new().write(true).truncate(true).create(true).open(&obj_path)?.write_all(obj_data)?;

            if obj_type.is_secret() {
                lock_down_file(obj_path.to_str().unwrap());
            }

            Ok(())
        } else {
            Err(std::io::Error::new(std::io::ErrorKind::InvalidInput, "object type or ID not valid"))
        }
    }
}
