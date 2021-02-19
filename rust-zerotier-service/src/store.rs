/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

use std::error::Error;
use std::io::{Read, Write};
use std::path::{Path, PathBuf};
use std::sync::Mutex;
use std::str::FromStr;
use std::ffi::CString;

use zerotier_core::{StateObjectType, NetworkId};

use crate::localconfig::LocalConfig;

/// In-filesystem data store for configuration and objects.
pub(crate) struct Store {
    pub base_path: Box<Path>,
    pub default_log_path: Box<Path>,
    prev_local_config: Mutex<String>,
    peers_path: Box<Path>,
    controller_path: Box<Path>,
    networks_path: Box<Path>,
    certs_path: Box<Path>,
}

/// Restrict file permissions using OS-specific code in osdep/OSUtils.cpp.
pub fn lock_down_file(path: &str) {
    let p = CString::new(path.as_bytes());
    if p.is_ok() {
        let p = p.unwrap();
        unsafe {
            crate::osdep::lockDownFile(p.as_ptr(), 0);
        }
    }
}

impl Store {
    const MAX_OBJECT_SIZE: usize = 262144; // sanity limit

    pub fn new(base_path: &str) -> std::io::Result<Store> {
        let bp = Path::new(base_path);
        let md = bp.metadata()?;
        if !md.is_dir() || md.permissions().readonly() {
            return Err(std::io::Error::new(std::io::ErrorKind::PermissionDenied, "base path does not exist or is not writable"));
        }

        let s = Store {
            base_path: bp.to_path_buf().into_boxed_path(),
            default_log_path: bp.join("service.log").into_boxed_path(),
            prev_local_config: Mutex::new(String::new()),
            peers_path: bp.join("peers.d").into_boxed_path(),
            controller_path: bp.join("controller.d").into_boxed_path(),
            networks_path: bp.join("networks.d").into_boxed_path(),
            certs_path: bp.join("certs.d").into_boxed_path(),
        };

        let _ = std::fs::create_dir_all(&s.peers_path);
        let _ = std::fs::create_dir_all(&s.controller_path);
        let _ = std::fs::create_dir_all(&s.networks_path);
        let _ = std::fs::create_dir_all(&s.certs_path);

        Ok(s)
    }

    fn make_obj_path(&self, obj_type: &StateObjectType, obj_id: &[u64]) -> Option<PathBuf> {
        match obj_type {
            StateObjectType::IdentityPublic => {
                Some(self.base_path.join("identity.public"))
            },
            StateObjectType::IdentitySecret => {
                Some(self.base_path.join("identity.secret"))
            },
            StateObjectType::Certificate => {
                if obj_id.len() < 6 {
                    None
                } else {
                    Some(self.certs_path.join(format!("{:0>16x}{:0>16x}{:0>16x}{:0>16x}{:0>16x}{:0>16x}.cert", obj_id[0], obj_id[1], obj_id[2], obj_id[3], obj_id[4], obj_id[5])))
                }
            },
            StateObjectType::TrustStore => {
                Some(self.base_path.join("truststore"))
            },
            StateObjectType::Locator => {
                Some(self.base_path.join("locator"))
            },
            StateObjectType::NetworkConfig => {
                if obj_id.len() < 1 {
                    None
                } else {
                    Some(self.networks_path.join(format!("{:0>16x}.conf", obj_id[0])))
                }
            },
            StateObjectType::Peer => {
                if obj_id.len() < 1 {
                    None
                } else {
                    Some(self.peers_path.join(format!("{:0>10x}.peer", obj_id[0])))
                }
            }
        }
    }

    /// Class-internal read function
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

    /// Get a list of the network IDs to which this node is joined.
    /// This is used to recall networks on startup by enumerating networks.d
    /// and telling the core to (re)join them all.
    pub fn networks(&self) -> Vec<NetworkId> {
        let mut list: Vec<NetworkId> = Vec::new();
        let d = std::fs::read_dir(self.networks_path.as_ref());
        if d.is_ok() {
            for de in d.unwrap() {
                if de.is_ok() {
                    let nn = de.unwrap().file_name();
                    let n = nn.to_str().unwrap_or("");
                    if n.len() == 21 && n.ends_with(".conf") { // ################.conf
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

    /// Read a file located in the base ZeroTier home directory.
    pub fn read_file(&self, fname: &str) -> std::io::Result<Vec<u8>> {
        self.read_internal(self.base_path.join(fname))
    }

    /// Like read_file but also converts into a string.
    pub fn read_file_str(&self, fname: &str) -> std::io::Result<String> {
        let data = self.read_file(fname)?;
        let data = String::from_utf8(data);
        if data.is_err() {
            return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, data.err().unwrap().to_string()));
        }
        Ok(data.unwrap())
    }

    /// Write a file to the base ZeroTier home directory.
    /// Error code std::io::ErrorKind::Other is returned if skip_if_unchanged is true
    /// and there has been no change from the last read.
    pub fn write_file(&self, fname: &str, data: &[u8]) -> std::io::Result<()> {
        std::fs::OpenOptions::new().write(true).truncate(true).create(true).open(self.base_path.join(fname))?.write_all(data)
    }

    /// Reads local.conf and deserializes into a LocalConfig object.
    pub fn read_local_conf(&self, skip_if_unchanged: bool) -> std::io::Result<LocalConfig> {
        let data = self.read_file_str("local.conf")?;
        if skip_if_unchanged {
            let mut prev = self.prev_local_config.lock().unwrap();
            if prev.eq(&data) {
                return Err(std::io::Error::new(std::io::ErrorKind::Other, "unchangd"));
            }
            *prev = data.clone();
        } else {
            *(self.prev_local_config.lock().unwrap()) = data.clone();
        }
        let lc = LocalConfig::new_from_json(data.as_str());
        if lc.is_err() {
            return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, lc.err().unwrap()));
        }
        Ok(lc.unwrap())
    }

    /// Writes a LocalConfig object in JSON format to local.conf.
    pub fn write_local_conf(&self, lc: &LocalConfig) -> std::io::Result<()> {
        let json = lc.to_json();
        self.write_file("local.conf", json.as_bytes())
    }

    /// Writes the primary port number bound to zerotier.port.
    pub fn write_port(&self, port: u16) -> std::io::Result<()> {
        let ps = port.to_string();
        self.write_file("zerotier.port", ps.as_bytes())
    }

    /// Read zerotier.port and return port or 0 if not found or not readable.
    pub fn read_port(&self) -> u16 {
        self.read_file_str("zerotier.port").map_or_else(|_| {
            0_u16
        },|s| {
            u16::from_str(s.trim()).unwrap_or(0_u16)
        })
    }

    /// Reads the authtoken.secret file in the home directory.
    #[inline(always)]
    pub fn read_authtoken_secret(&self) -> std::io::Result<String> {
        Ok(self.read_file_str("authtoken.secret")?)
    }

    /// Write authtoken.secret and lock down file permissions.
    pub fn write_authtoken_secret(&self, sec: &str) -> std::io::Result<()> {
        let p = self.base_path.join("authtoken.secret");
        let _ = std::fs::OpenOptions::new().write(true).truncate(true).create(true).open(&p)?.write_all(sec.as_bytes())?;
        lock_down_file(p.to_str().unwrap());
        Ok(())
    }

    /// Write zerotier.pid file with current process's PID.
    #[cfg(unix)]
    pub fn write_pid(&self) -> std::io::Result<()> {
        let pid = unsafe { crate::osdep::getpid() }.to_string();
        self.write_file(self.base_path.join("zerotier.pid").to_str().unwrap(), pid.as_bytes())
    }

    /// Erase zerotier.pid if present.
    pub fn erase_pid(&self) {
        std::fs::remove_file(self.base_path.join("zerotier.pid"));
    }

    /// Load a ZeroTier core object.
    pub fn load_object(&self, obj_type: &StateObjectType, obj_id: &[u64]) -> std::io::Result<Vec<u8>> {
        let obj_path = self.make_obj_path(&obj_type, obj_id);
        if obj_path.is_some() {
            return self.read_internal(obj_path.unwrap());
        }
        Err(std::io::Error::new(std::io::ErrorKind::NotFound, "does not exist or is not readable"))
    }

    /// Erase a ZeroTier core object.
    pub fn erase_object(&self, obj_type: &StateObjectType, obj_id: &[u64]) {
        let obj_path = self.make_obj_path(obj_type, obj_id);
        if obj_path.is_some() {
            let _ = std::fs::remove_file(obj_path.unwrap());
        }
    }

    /// Store a ZeroTier core object.
    /// Permissions will also be restricted for some object types.
    pub fn store_object(&self, obj_type: &StateObjectType, obj_id: &[u64], obj_data: &[u8]) -> std::io::Result<()> {
        let obj_path = self.make_obj_path(obj_type, obj_id);
        if obj_path.is_some() {
            let obj_path = obj_path.unwrap();
            std::fs::OpenOptions::new().write(true).truncate(true).create(true).open(&obj_path)?.write_all(obj_data)?;

            if obj_type.eq(&StateObjectType::IdentitySecret) || obj_type.eq(&StateObjectType::TrustStore) {
                lock_down_file(obj_path.to_str().unwrap());
            }

            Ok(())
        } else {
            Err(std::io::Error::new(std::io::ErrorKind::InvalidInput, "object type or ID not valid"))
        }
    }
}
