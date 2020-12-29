use crate::*;
use crate::bindings::capi as ztcore;
use serde::{Deserialize, Serialize};

#[allow(non_snake_case)]
#[derive(Serialize, Deserialize)]
pub struct Peer {
    address: Address,
    identity: Identity,
    fingerprint: Fingerprint,
    versionMajor: i32,
    versionMinor: i32,
    versionRev: i32,
    versionProto: i32,
    latency: i32,
    root: bool,
    networks: Vec<NetworkId>,
    paths: Vec<Path>,
    // locator: Locator
}

impl Peer {
    #[inline]
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
                versionMajor: p.versionMajor as i32,
                versionMinor: p.versionMinor as i32,
                versionRev: p.versionRev as i32,
                versionProto: p.versionProto as i32,
                latency: p.latency as i32,
                root: p.root != 0,
                networks: networks,
                paths: paths
            }
        }
    }
}
