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
use std::path::{Path, PathBuf};
use zerotier_core::StateObjectType;
use std::io::{Read, Write};

pub struct Store {
    pub base_path: Box<Path>,
    pub peers_path: Box<Path>,
    pub controller_path: Box<Path>,
    pub networks_path: Box<Path>,
    pub certs_path: Box<Path>,
}

impl Store {
    const MAX_OBJECT_SIZE: usize = 131072; // sanity limit

    pub fn new(base_path: &str) -> Result<Store, std::io::Error> {
        let bp = Path::new(base_path);
        let md = bp.metadata()?;
        if !md.is_dir() || md.permissions().readonly() {
            return Err(std::io::Error::new(std::io::ErrorKind::PermissionDenied, "base path does not exist or is not writable"));
        }
        Ok(Store{
            base_path: bp.to_path_buf().into_boxed_path(),
            peers_path: bp.join("peers.d").into_boxed_path(),
            controller_path: bp.join("controller.d").into_boxed_path(),
            networks_path: bp.join("networks.d").into_boxed_path(),
            certs_path: bp.join("certs.d").into_boxed_path(),
        })
    }

    fn make_obj_path(&self, obj_type: StateObjectType, obj_id: &[u64]) -> Option<PathBuf> {
        Some(match obj_type {
            StateObjectType::IdentityPublic => self.base_path.join("identity.public"),
            StateObjectType::IdentitySecret => self.base_path.join("identity.secret"),
            StateObjectType::Certificate => {
                if obj_id.len() < 6 {
                    return None;
                }
                self.certs_path.join(format!("{:0>16x}{:0>16x}{:0>16x}{:0>16x}{:0>16x}{:0>16x}.cert",obj_id[0],obj_id[1],obj_id[2],obj_id[3],obj_id[4],obj_id[5]))
            },
            StateObjectType::TrustStore => self.base_path.join("truststore"),
            StateObjectType::Locator => self.base_path.join("locator"),
            StateObjectType::NetworkConfig => {
                if obj_id.len() < 1 {
                    return None;
                }
                self.networks_path.join(format!("{:0>16x}.conf", obj_id[0]))
            },
            StateObjectType::Peer => {
                if obj_id.len() < 1 {
                    return None;
                }
                self.peers_path.join(format!("{:0>10x}.peer", obj_id[0]))
            }
        })
    }

    pub fn load(&self, obj_type: StateObjectType, obj_id: &[u64]) -> std::io::Result<Box<[u8]>> {
        let obj_path = self.make_obj_path(obj_type, obj_id);
        if obj_path.is_some() {
            let obj_path = obj_path.unwrap();
            let fmd = obj_path.metadata()?;
            if fmd.is_file() {
                let flen = fmd.len();
                if flen <= Store::MAX_OBJECT_SIZE as u64 {
                    let mut f = std::fs::File::open(obj_path)?;
                    let mut buf: Vec<u8> = Vec::new();
                    buf.reserve(flen as usize);
                    let rs = f.read_to_end(&mut buf)?;
                    buf.resize(rs as usize, 0);
                    return Ok(buf.into_boxed_slice());
                }
            }
        }
        Err(std::io::Error::new(std::io::ErrorKind::NotFound, "does not exist or is not readable"))
    }

    pub fn erase(&self, obj_type: StateObjectType, obj_id: &[u64]) {
        let obj_path = self.make_obj_path(obj_type, obj_id);
        if obj_path.is_some() {
            let _ = std::fs::remove_file(obj_path.unwrap());
        }
    }

    pub fn store(&self, obj_type: StateObjectType, obj_id: &[u64], obj_data: &[u8]) -> std::io::Result<()> {
        let obj_path = self.make_obj_path(obj_type, obj_id);
        if obj_path.is_some() {
            std::fs::OpenOptions::new().write(true).truncate(true).create(true).open(obj_path.unwrap())?.write_all(obj_data)
        } else {
            Err(std::io::Error::new(std::io::ErrorKind::InvalidInput, "object ID not valid"))
        }
    }
}
