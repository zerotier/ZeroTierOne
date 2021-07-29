use std::sync::Arc;

use crate::vl1::{Address, Identity};
use crate::vl1::concurrentmap::ConcurrentMap;
use crate::vl1::path::Path;
use crate::vl1::peer::Peer;

pub struct Node {
    identity: Identity,
    paths: ConcurrentMap<[u64; 4], Arc<Path>>,
    peers: ConcurrentMap<Address, Arc<Peer>>,
}
