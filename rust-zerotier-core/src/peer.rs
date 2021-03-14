/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

use serde::{Deserialize, Serialize};
use crate::*;
use crate::capi as ztcore;

#[derive(Serialize, Deserialize, Clone)]
pub struct Peer {
    pub address: Address,
    pub identity: Identity,
    pub fingerprint: Fingerprint,
    #[serde(rename = "versionMajor")]
    pub version_major: i32,
    #[serde(rename = "versionMinor")]
    pub version_minor: i32,
    #[serde(rename = "versionRev")]
    pub version_rev: i32,
    #[serde(rename = "versionProto")]
    pub version_proto: i32,
    pub latency: i32,
    pub root: bool,
    pub networks: Vec<NetworkId>,
    pub paths: Vec<Path>,
    pub locator: Option<Locator>
}

impl Peer {
    pub(crate) fn new_from_capi(p: &ztcore::ZT_Peer) -> Peer {
        unsafe {
            let mut networks: Vec<NetworkId> = Vec::new();
            for i in 0..(p.networkCount as isize) {
                networks.push(NetworkId(*p.networks.offset(i)));
            }
            let mut paths: Vec<Path> = Vec::new();
            for i in 0..(p.pathCount as isize) {
                paths.push(Path::new_from_capi(&(*p.paths.offset(i))));
            }
            return Peer {
                address: Address(p.address),
                identity: Identity::new_from_capi(p.identity, false).clone(), // clone to get a copy independent of 'p'
                fingerprint: Fingerprint::new_from_capi(&(*p.fingerprint)),
                version_major: p.versionMajor as i32,
                version_minor: p.versionMinor as i32,
                version_rev: p.versionRev as i32,
                version_proto: p.versionProto as i32,
                latency: p.latency as i32,
                root: p.root != 0,
                networks,
                paths,
                locator: if p.locator.is_null() { None } else { Some(Locator::new_from_capi(p.locator, false).clone() )}
            }
        }
    }
}
