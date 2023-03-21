use std::collections::HashMap;
use std::sync::{Arc, RwLock};

use crate::vl1::address::{Address, LegacyAddress};
use crate::vl1::node::ApplicationLayer;
use crate::vl1::Peer;

use zerotier_utils::oneormore::OneOrMore;

/// Mapping of addresses (and short legacy addresses) to peers.
///
/// Collisions in the legacy 40-bit address space are very rare, so the OneOrMore<> optimization is
/// used to allow lookups to almost always happen by way of a simple u64 key.
pub struct PeerMap<Application: ApplicationLayer + ?Sized> {
    peers: RwLock<HashMap<LegacyAddress, OneOrMore<Arc<Peer<Application>>>>>,
}

impl<Application: ApplicationLayer + ?Sized> PeerMap<Application> {
    pub fn new() -> Self {
        Self { peers: RwLock::new(HashMap::new()) }
    }

    pub fn each<F: FnMut(&Arc<Peer<Application>>)>(&self, mut f: F) {
        let peers = self.peers.read().unwrap();
        for (_, pl) in peers.iter() {
            for p in pl.iter() {
                f(p);
            }
        }
    }

    pub fn remove(&self, address: &Address) {
        let peers = self.peers.write().unwrap();
        if let Some(list) = peers.get_mut(&address.legacy_address()) {
            list.remove_if(|peer| peer.identity.address.eq(address));
            if list.is_empty() {
                peers.remove(&address.legacy_address());
            }
        }
    }

    pub fn get(&self, address: &Address) -> Option<Arc<Peer<Application>>> {
        self.peers.read().unwrap().get(&address.legacy_address()).and_then(|list| {
            for p in list.iter() {
                if p.identity.address.eq(address) {
                    return Some(p.clone());
                }
            }
            return None;
        })
    }

    /// Get a peer by only its short 40-bit address.
    ///
    /// This is only used in V1 compatibility mode to look up peers by V1 address. The rule here
    /// is that only one V1 peer can map to one V1 address.
    pub(crate) fn get_legacy(&self, legacy_address: &LegacyAddress) -> Option<Arc<Peer<Application>>> {
        self.peers.read().unwrap().get(legacy_address).and_then(|list| {
            // First, get the matching peer whose identity is of the legacy x25519-only type.
            for p in list.iter() {
                if p.identity.p384.is_none() {
                    return Some(p.clone());
                }
            }
            // Then, if that doesn't exist, get the first matching peer with the same short address.
            return list.front().cloned();
        })
    }

    /// Insert the supplied peer if it is in fact unique.
    ///
    /// This returns either the new peer or the existing one if the new peer is a duplicate. True is returned
    /// for the second return value if the new peer is new or false if it was a duplicate.
    ///
    /// Short 40-bit addresses are unique within the domain of peers with V1 identities, meaning identities
    /// that lack P-384 keys. Otherwise the full 384-bit key space is used.
    pub fn insert_if_unique(&self, peer: Arc<Peer<Application>>) -> (Arc<Peer<Application>>, bool) {
        let peers = self.peers.write().unwrap();
        if let Some(list) = peers.get(&peer.identity.address.legacy_address()) {
            for p in list.iter() {
                if (p.identity.p384.is_none()
                    && peer.identity.p384.is_none()
                    && p.identity.address.legacy_address() == peer.identity.address.legacy_address())
                    || p.identity.address.eq(&peer.identity.address)
                {
                    return (p.clone(), false);
                }
            }
            list.push_front(peer.clone());
        } else {
            peers.insert(peer.identity.address.legacy_address(), OneOrMore::new_one(peer.clone()));
        }
        return (peer, true);
    }
}
