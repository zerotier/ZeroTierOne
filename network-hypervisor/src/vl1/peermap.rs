use std::collections::BTreeMap;
use std::ops::Bound;
use std::sync::{Arc, RwLock};

use super::address::{Address, PartialAddress};
use super::identity::{Identity, IdentitySecret};
use super::node::ApplicationLayer;
use super::peer::Peer;

use zerotier_crypto::typestate::Valid;

pub struct PeerMap<Application: ApplicationLayer + ?Sized> {
    maps: [RwLock<BTreeMap<Address, Arc<Peer<Application>>>>; 256],
}

impl<Application: ApplicationLayer + ?Sized> PeerMap<Application> {
    pub fn new() -> Self {
        Self { maps: std::array::from_fn(|_| RwLock::new(BTreeMap::new())) }
    }

    pub fn each<F: FnMut(&Arc<Peer<Application>>)>(&self, mut f: F) {
        for m in self.maps.iter() {
            let mm = m.read().unwrap();
            for (_, p) in mm.iter() {
                f(p);
            }
        }
    }

    pub fn remove(&self, address: &Address) -> Option<Arc<Peer<Application>>> {
        self.maps[address.0[0] as usize].write().unwrap().remove(address)
    }

    /// Get an exact match for a full specificity address.
    /// This always returns None if the address provided does not have 384 bits of specificity.
    pub fn get_exact(&self, address: &Address) -> Option<Arc<Peer<Application>>> {
        self.maps[address.0[0] as usize].read().unwrap().get(address).cloned()
    }

    /// Get a matching peer for a partial address of any specificity, but return None if the match is ambiguous.
    pub fn get_unambiguous(&self, address: &PartialAddress) -> Option<Arc<Peer<Application>>> {
        let mm = self.maps[address.0 .0[0] as usize].read().unwrap();
        let matches = mm.range::<[u8; 48], (Bound<&[u8; 48]>, Bound<&[u8; 48]>)>((Bound::Included(&address.0 .0), Bound::Unbounded));
        let mut r = None;
        for m in matches {
            if address.matches(m.0) {
                if r.is_none() {
                    r.insert(m.1);
                } else {
                    return None;
                }
            } else {
                break;
            }
        }
        return r.cloned();
    }

    /// Insert the supplied peer if it is in fact new, otherwise return the existing peer with the same address.
    pub fn add(&self, peer: Arc<Peer<Application>>) -> (Arc<Peer<Application>>, bool) {
        let mm = self.maps[peer.identity.address.0[0] as usize].write().unwrap();
        let p = mm.entry(peer.identity.address).or_insert(peer.clone());
        if Arc::ptr_eq(p, &peer) {
            (peer, true)
        } else {
            (p.clone(), false)
        }
    }

    /// Get a peer or create one if not found.
    /// This should be used when the peer will almost always be new, such as on OK(WHOIS).
    pub fn get_or_add(&self, this_node_identity: &IdentitySecret, peer_identity: Valid<Identity>, time_ticks: i64) -> Option<Arc<Peer<Application>>> {
        let peer = Arc::new(Peer::new(this_node_identity, peer_identity, time_ticks)?);
        Some(
            self.maps[peer_identity.address.0[0] as usize]
                .write()
                .unwrap()
                .entry(peer.identity.address)
                .or_insert(peer)
                .clone(),
        )
    }
}
