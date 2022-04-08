/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2022 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::collections::BTreeMap;
use std::io::{stdout, Write};
use std::net::{Ipv4Addr, SocketAddr, SocketAddrV4};
use std::ops::Bound::Included;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;
use std::time::{Duration, Instant, SystemTime};

use async_trait::async_trait;

use sha2::digest::Digest;
use sha2::Sha512;

use syncwhole::datastore::*;
use syncwhole::host::*;
use syncwhole::node::*;
use syncwhole::utils::*;

const TEST_NODE_COUNT: usize = 8;
const TEST_PORT_RANGE_START: u16 = 21384;
const TEST_STARTING_RECORDS_PER_NODE: usize = 16;

static mut RANDOM_CTR: u128 = 0;

fn get_random_bytes(mut buf: &mut [u8]) {
    // This is only for testing and is not really secure.
    let mut ctr = unsafe { RANDOM_CTR };
    if ctr == 0 {
        ctr = SystemTime::now().duration_since(SystemTime::UNIX_EPOCH).unwrap().as_nanos() * (1 + Instant::now().elapsed().as_nanos());
    }
    while !buf.is_empty() {
        let l = buf.len().min(64);
        ctr = ctr.wrapping_add(1);
        buf[..l].copy_from_slice(&Sha512::digest(&ctr.to_ne_bytes()).as_slice()[..l]);
        buf = &mut buf[l..];
    }
    unsafe { RANDOM_CTR = ctr };
}

pub struct TestNodeHost {
    pub name: String,
    pub config: Config,
    pub records: tokio::sync::Mutex<BTreeMap<[u8; 64], [u8; 64]>>,
}

impl TestNodeHost {
    pub fn new_random(test_no: usize) -> Self {
        let mut s = BTreeMap::new();
        for _ in 0..TEST_STARTING_RECORDS_PER_NODE {
            let mut v = [0_u8; 64];
            get_random_bytes(&mut v);
            let k = Self::sha512(&[&v]);
            s.insert(k, v);
        }
        Self {
            name: test_no.to_string(),
            config: Config::default(),
            records: tokio::sync::Mutex::new(s),
        }
    }
}

impl Host for TestNodeHost {
    fn node_config(&self) -> Config {
        self.config.clone()
    }

    fn on_connect_attempt(&self, _address: &SocketAddr) {
        //println!("{:5}: connecting to {}", self.name, _address.to_string());
    }

    fn on_connect(&self, info: &RemoteNodeInfo) {
        //println!("{:5}: connected to {} ({}, {})", self.name, info.remote_address.to_string(), info.node_name.as_ref().map_or("null", |s| s.as_str()), if info.inbound { "inbound" } else { "outbound" });
    }

    fn on_connection_closed(&self, info: &RemoteNodeInfo, reason: String) {
        //println!("{:5}: closed connection to {}: {} ({}, {})", self.name, info.remote_address.to_string(), reason, if info.inbound { "inbound" } else { "outbound" }, if info.initialized { "initialized" } else { "not initialized" });
    }

    fn get_secure_random(&self, buf: &mut [u8]) {
        // This is only for testing and is not really secure.
        get_random_bytes(buf);
    }
}

#[async_trait]
impl DataStore for TestNodeHost {
    type ValueRef = [u8; 64];

    const MAX_VALUE_SIZE: usize = 1024;

    fn clock(&self) -> i64 {
        ms_since_epoch()
    }

    fn domain(&self) -> &str {
        "test"
    }

    async fn load(&self, _: i64, key: &[u8]) -> Option<Self::ValueRef> {
        let key = key.try_into();
        if key.is_ok() {
            let key: [u8; 64] = key.unwrap();
            let records = self.records.lock().await;
            let value = records.get(&key);
            if value.is_some() {
                return Some(value.unwrap().clone());
            }
        }
        return None;
    }

    async fn store(&self, key: &[u8], value: &[u8]) -> StoreResult {
        let key = key.try_into();
        if key.is_ok() && value.len() == 64 {
            let key: [u8; 64] = key.unwrap();
            let value: [u8; 64] = value.try_into().unwrap();
            if key == Self::sha512(&[&value]) {
                if self.records.lock().await.insert(key, value).is_none() {
                    StoreResult::Ok
                } else {
                    StoreResult::Duplicate
                }
            } else {
                StoreResult::Rejected
            }
        } else {
            StoreResult::Rejected
        }
    }

    async fn count(&self, _: i64, key_range_start: &[u8], key_range_end: &[u8]) -> u64 {
        let start: [u8; 64] = key_range_start.try_into().unwrap();
        let end: [u8; 64] = key_range_end.try_into().unwrap();
        self.records.lock().await.range((Included(start), Included(end))).count() as u64
    }

    async fn total_count(&self) -> u64 {
        self.records.lock().await.len() as u64
    }

    async fn for_each<F: Send + FnMut(&[u8], &Self::ValueRef) -> bool>(&self, _reference_time: i64, key_range_start: &[u8], key_range_end: &[u8], mut f: F) {
        let start: [u8; 64] = key_range_start.try_into().unwrap();
        let end: [u8; 64] = key_range_end.try_into().unwrap();
        let records = self.records.lock().await;
        for (k, v) in records.range((Included(start), Included(end))) {
            if !f(k, v) {
                break;
            }
        }
    }
}

fn main() {
    tokio::runtime::Builder::new_current_thread().enable_all().build().unwrap().block_on(async {
        println!("Running syncwhole local self-test network with {} nodes starting at 127.0.0.1:{}", TEST_NODE_COUNT, TEST_PORT_RANGE_START);
        println!();

        println!("Starting nodes on 127.0.0.1...");
        let mut nodes: Vec<Node<TestNodeHost, TestNodeHost>> = Vec::with_capacity(TEST_NODE_COUNT);
        for port in TEST_PORT_RANGE_START..(TEST_PORT_RANGE_START + (TEST_NODE_COUNT as u16)) {
            let mut peers: Vec<SocketAddr> = Vec::with_capacity(TEST_NODE_COUNT);
            for port2 in TEST_PORT_RANGE_START..(TEST_PORT_RANGE_START + (TEST_NODE_COUNT as u16)) {
                if port != port2 {
                    peers.push(SocketAddr::V4(SocketAddrV4::new(Ipv4Addr::LOCALHOST, port2)));
                }
            }
            let mut th = TestNodeHost::new_random(port as usize);
            th.config.anchors = peers;
            th.config.name = port.to_string();
            let nh = Arc::new(th);
            //println!("Starting node on 127.0.0.1:{}...", port, nh.db.lock().unwrap().len());
            nodes.push(Node::new(nh.clone(), nh.clone(), SocketAddr::V4(SocketAddrV4::new(Ipv4Addr::LOCALHOST, port))).await.unwrap());
        }

        print!("Waiting for all connections to be established...");
        let _ = stdout().flush();
        loop {
            tokio::time::sleep(Duration::from_secs(1)).await;
            let mut count = 0;
            for n in nodes.iter() {
                count += n.connection_count().await;
            }
            if count == (TEST_NODE_COUNT * (TEST_NODE_COUNT - 1)) {
                println!(" {} connections up.", count);
                break;
            } else {
                print!(".");
                let _ = stdout().flush();
            }
        }

        loop {
            tokio::time::sleep(Duration::from_secs(1)).await;
        }
    });
}
