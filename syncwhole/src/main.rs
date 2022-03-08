extern crate core;

use std::collections::BTreeMap;
use std::net::{Ipv4Addr, SocketAddr, SocketAddrV4};
use std::ops::Bound::Included;
use std::sync::{Arc, Mutex};
use std::time::{Duration, SystemTime};

use sha2::digest::Digest;
use sha2::Sha512;

use syncwhole::datastore::{DataStore, LoadResult, StoreResult};
use syncwhole::host::Host;
use syncwhole::ms_since_epoch;
use syncwhole::node::{Node, RemoteNodeInfo};

const TEST_NODE_COUNT: usize = 16;
const TEST_PORT_RANGE_START: u16 = 21384;

struct TestNodeHost {
    name: String,
    peers: Vec<SocketAddr>,
    db: Mutex<BTreeMap<[u8; 64], Arc<[u8]>>>,
}

impl Host for TestNodeHost {
    fn fixed_peers(&self) -> &[SocketAddr] { self.peers.as_slice() }

    fn name(&self) -> Option<&str> { Some(self.name.as_str()) }

    fn on_connect_attempt(&self, _address: &SocketAddr) {
        //println!("{:5}: connecting to {}", self.name, _address.to_string());
    }

    fn on_connect(&self, info: &RemoteNodeInfo) {
        println!("{:5}: connected to {} ({}, {})", self.name, info.remote_address.to_string(), info.node_name.as_ref().map_or("null", |s| s.as_str()), if info.inbound { "inbound" } else { "outbound" });
    }

    fn on_connection_closed(&self, info: &RemoteNodeInfo, reason: String) {
        println!("{:5}: closed connection to {}: {} ({}, {})", self.name, info.remote_address.to_string(), reason, if info.inbound { "inbound" } else { "outbound" }, if info.initialized { "initialized" } else { "not initialized" });
    }

    fn get_secure_random(&self, mut buf: &mut [u8]) {
        // This is only for testing and is not really secure.
        let mut ctr = SystemTime::now().duration_since(SystemTime::UNIX_EPOCH).unwrap().as_nanos();
        while !buf.is_empty() {
            let l = buf.len().min(64);
            ctr = ctr.wrapping_add(1);
            buf[0..l].copy_from_slice(&Self::sha512(&[&ctr.to_ne_bytes()])[0..l]);
            buf = &mut buf[l..];
        }
    }
}

impl DataStore for TestNodeHost {
    type LoadResultValueType = Arc<[u8]>;

    const KEY_SIZE: usize = 64;
    const MAX_VALUE_SIZE: usize = 1024;
    const KEY_IS_COMPUTED: bool = true;

    fn key_from_value(&self, value: &[u8], key_buffer: &mut [u8]) {
        key_buffer.copy_from_slice(Sha512::digest(value).as_slice());
    }

    fn clock(&self) -> i64 { ms_since_epoch() }

    fn domain(&self) -> &str { "test" }

    fn load(&self, _: i64, key: &[u8]) -> LoadResult<Self::LoadResultValueType> {
        self.db.lock().unwrap().get(key).map_or(LoadResult::NotFound, |r| LoadResult::Ok(r.clone()))
    }

    fn store(&self, key: &[u8], value: &[u8]) -> StoreResult {
        assert_eq!(key.len(), 64);
        let mut res = StoreResult::Ok;
        self.db.lock().unwrap().entry(key.try_into().unwrap()).and_modify(|e| {
            if e.as_ref().eq(value) {
                res = StoreResult::Duplicate;
            } else {
                *e = Arc::from(value)
            }
        }).or_insert_with(|| {
            Arc::from(value)
        });
        res
    }

    fn total_count(&self) -> u64 { self.db.lock().unwrap().len() as u64 }

    fn for_each<F: FnMut(&[u8], &[u8]) -> bool>(&self, _: i64, key_prefix: &[u8], mut f: F) {
        let mut r_start = [0_u8; Self::KEY_SIZE];
        let mut r_end = [0xff_u8; Self::KEY_SIZE];
        (&mut r_start[0..key_prefix.len()]).copy_from_slice(key_prefix);
        (&mut r_end[0..key_prefix.len()]).copy_from_slice(key_prefix);
        for (k, v) in self.db.lock().unwrap().range((Included(r_start), Included(r_end))) {
            if !f(k, v.as_ref()) {
                break;
            }
        }
    }
}

fn main() {
    tokio::runtime::Builder::new_current_thread().enable_all().build().unwrap().block_on(async {
        println!("Running syncwhole local self-test network with {} nodes starting at 127.0.0.1:{}", TEST_NODE_COUNT, TEST_PORT_RANGE_START);
        println!();

        let mut nodes: Vec<Node<TestNodeHost, TestNodeHost>> = Vec::with_capacity(TEST_NODE_COUNT);
        for port in TEST_PORT_RANGE_START..(TEST_PORT_RANGE_START + (TEST_NODE_COUNT as u16)) {
            let mut peers: Vec<SocketAddr> = Vec::with_capacity(TEST_NODE_COUNT);
            for port2 in TEST_PORT_RANGE_START..(TEST_PORT_RANGE_START + (TEST_NODE_COUNT as u16)) {
                if port != port2 {
                    peers.push(SocketAddr::V4(SocketAddrV4::new(Ipv4Addr::LOCALHOST, port2)));
                }
            }
            let nh = Arc::new(TestNodeHost {
                name: format!("{}", port),
                peers,
                db: Mutex::new(BTreeMap::new())
            });
            println!("Starting node on 127.0.0.1:{} with {} records in data store...", port, nh.db.lock().unwrap().len());
            nodes.push(Node::new(nh.clone(), nh.clone(), SocketAddr::V4(SocketAddrV4::new(Ipv4Addr::LOCALHOST, port))).await.unwrap());
        }
        println!();

        loop {
            tokio::time::sleep(Duration::from_secs(1)).await;
            let mut count = 0;
            for n in nodes.iter() {
                count += n.connection_count().await;
            }
            println!("{}", count);
        }
    });
}
