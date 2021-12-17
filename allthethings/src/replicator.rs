/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::collections::HashMap;
use std::convert::TryInto;
use std::error::Error;
use std::hash::{Hash, Hasher};
use std::marker::PhantomData;
use std::mem::{size_of, transmute};
use std::sync::Arc;
use std::sync::atomic::{AtomicU64, Ordering};
use std::time::Duration;

use getrandom::getrandom;
use sha2::{Digest, Sha384};
use smol::{Executor, Task, Timer};
use smol::io::{AsyncReadExt, AsyncWriteExt, BufReader};
use smol::lock::Mutex;
use smol::net::{IpAddr, Ipv4Addr, Ipv6Addr, SocketAddrV4, SocketAddrV6, TcpListener, TcpStream, SocketAddr};
use smol::stream::StreamExt;

use crate::{IDENTITY_HASH_SIZE, ms_monotonic, ms_since_epoch, protocol};
use crate::store::{StoreObjectResult, Store};
use crate::varint;

const CONNECTION_TIMEOUT_SECONDS: u64 = 60;
const CONNECTION_SYNC_RESTART_TIMEOUT_SECONDS: u64 = 5;

static mut XORSHIFT64_STATE: u64 = 0;

/// Get a non-cryptographic random number.
fn xorshift64_random() -> u64 {
    let mut x = unsafe { XORSHIFT64_STATE };
    x ^= x.wrapping_shl(13);
    x ^= x.wrapping_shr(7);
    x ^= x.wrapping_shl(17);
    unsafe { XORSHIFT64_STATE = x };
    x
}

pub struct Config {
    /// Number of P2P connections desired.
    pub target_link_count: usize,

    /// Maximum allowed size of an object.
    pub max_object_size: usize,

    /// TCP port to which this should bind.
    pub tcp_port: u16,

    /// A name for this replicated data set. This is just used to prevent linking to peers replicating different data.
    pub domain: String,
}

#[derive(PartialEq, Eq, Clone)]
struct ConnectionKey {
    instance_id: [u8; 16],
    ip: IpAddr,
}

impl Hash for ConnectionKey {
    #[inline(always)]
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.instance_id.hash(state);
        self.ip.hash(state);
    }
}

struct Connection {
    remote_address: SocketAddr,
    last_receive: Arc<AtomicU64>,
    task: Task<()>
}

struct ReplicatorImpl<'ex> {
    executor: Arc<Executor<'ex>>,
    instance_id: [u8; 16],
    loopback_check_code_secret: [u8; 16],
    domain_hash: [u8; 48],
    store: Arc<dyn Store>,
    config: Config,
    connections: Mutex<HashMap<ConnectionKey, Connection>>,
    announced_objects_requested: Mutex<HashMap<[u8; IDENTITY_HASH_SIZE], u64>>,
}

pub struct Replicator<'ex> {
    v4_listener_task: Option<Task<()>>,
    v6_listener_task: Option<Task<()>>,
    service_task: Task<()>,
    _marker: PhantomData<std::cell::UnsafeCell<&'ex ()>>,
}

impl<'ex> Replicator<'ex> {
    /// Create a new replicator to replicate the contents of the provided store.
    /// All async tasks, sockets, and connections will be dropped if the replicator is dropped.
    pub async fn start(executor: &Arc<Executor<'ex>>, store: Arc<dyn Store>, config: Config) -> Result<Replicator<'ex>, Box<dyn Error>> {
        let _ = unsafe { getrandom(&mut *(&mut XORSHIFT64_STATE as *mut u64).cast::<[u8; 8]>()) };

        let listener_v4 = TcpListener::bind(SocketAddrV4::new(Ipv4Addr::UNSPECIFIED, config.tcp_port)).await;
        let listener_v6 = TcpListener::bind(SocketAddrV6::new(Ipv6Addr::UNSPECIFIED, config.tcp_port, 0, 0)).await;
        if listener_v4.is_err() && listener_v6.is_err() {
            return Err(Box::new(listener_v4.unwrap_err()));
        }

        let r = Arc::new(ReplicatorImpl::<'ex> {
            executor: executor.clone(),
            instance_id: {
                let mut tmp = [0_u8; 16];
                getrandom(&mut tmp).expect("getrandom failed");
                tmp
            },
            loopback_check_code_secret: {
                let mut tmp = [0_u8; 16];
                getrandom(&mut tmp).expect("getrandom failed");
                tmp
            },
            domain_hash: {
                let mut h = Sha384::new();
                h.update(config.domain.as_bytes());
                h.finalize().as_ref().try_into().unwrap()
            },
            config,
            store,
            connections: Mutex::new(HashMap::new()),
            announced_objects_requested: Mutex::new(HashMap::new())
        });

        Ok(Self {
            v4_listener_task: listener_v4.map_or(None, |listener_v4| Some(executor.spawn(r.clone().listener_task_main(listener_v4)))),
            v6_listener_task: listener_v6.map_or(None, |listener_v6| Some(executor.spawn(r.clone().listener_task_main(listener_v6)))),
            service_task: executor.spawn(r.service_main()),
            _marker: PhantomData::default(),
        })
    }
}

unsafe impl<'ex> Send for Replicator<'ex> {}

unsafe impl<'ex> Sync for Replicator<'ex> {}

impl<'ex> ReplicatorImpl<'ex> {
    async fn service_main(self: Arc<ReplicatorImpl<'ex>>) {
        let mut timer = smol::Timer::interval(Duration::from_secs(5));
        loop {
            timer.next().await;
            let now_mt = ms_monotonic();
            self.announced_objects_requested.lock().await.retain(|_, ts| now_mt.saturating_sub(*ts) < (CONNECTION_TIMEOUT_SECONDS * 1000));
            self.connections.lock().await.retain(|_, c| (now_mt.saturating_sub(c.last_receive.load(Ordering::Relaxed))) < (CONNECTION_TIMEOUT_SECONDS * 1000));
        }
    }

    async fn listener_task_main(self: Arc<ReplicatorImpl<'ex>>, listener: TcpListener) {
        loop {
            let stream = listener.accept().await;
            if stream.is_ok() {
                let (stream, remote_address) = stream.unwrap();
                self.handle_new_connection(stream, remote_address, false).await;
            }
        }
    }

    async fn handle_new_connection(self: &Arc<ReplicatorImpl<'ex>>, mut stream: TcpStream, remote_address: SocketAddr, outgoing: bool) {
        stream.set_nodelay(true);

        let mut loopback_check_code_salt = [0_u8; 8];
        getrandom(&mut loopback_check_code_salt).expect("getrandom failed");

        let mut h = Sha384::new();
        h.update(&loopback_check_code_salt);
        h.update(&self.loopback_check_code_secret);
        let loopback_check_code: [u8; 48] = h.finalize().as_ref().try_into().unwrap();

        let hello = protocol::Hello {
            hello_size: size_of::<protocol::Hello>() as u8,
            protocol_version: protocol::PROTOCOL_VERSION,
            flags: [0_u8; 4],
            clock: ms_since_epoch().to_le_bytes(),
            data_set_size: self.store.total_size().to_le_bytes(),
            domain_hash: self.domain_hash.clone(),
            instance_id: self.instance_id.clone(),
            loopback_check_code_salt,
            loopback_check_code: (&loopback_check_code[0..16]).try_into().unwrap()
        };
        let hello: [u8; size_of::<protocol::Hello>()] = unsafe { transmute(hello) };

        if stream.write_all(&hello).await.is_ok() {
            let mut hello_buf = [0_u8; size_of::<protocol::Hello>()];
            if stream.read_exact(&mut hello_buf).await.is_ok() {
                let hello: protocol::Hello = unsafe { transmute(hello_buf) };

                // Sanity check HELLO packet. In the future we may support different versions and sizes.
                if hello.hello_size == size_of::<protocol::Hello>() as u8 && hello.protocol_version == protocol::PROTOCOL_VERSION {
                    // If this hash's first 16 bytes are equal to the one in the HELLO, this connection is
                    // from this node and should be dropped.
                    let mut h = Sha384::new();
                    h.update(&hello.loopback_check_code_salt);
                    h.update(&self.loopback_check_code_secret);
                    let loopback_if_equal: [u8; 48] = h.finalize().as_ref().try_into().unwrap();

                    if !loopback_if_equal[0..16].eq(&hello.loopback_check_code) {
                        let k = ConnectionKey {
                            instance_id: hello.instance_id.clone(),
                            ip: remote_address.ip()
                        };
                        let mut connections = self.connections.lock().await;
                        let _ = connections.entry(k).or_insert_with(move || {
                            stream.set_nodelay(false);
                            let last_receive = Arc::new(AtomicU64::new(ms_monotonic()));
                            Connection {
                                remote_address,
                                last_receive: last_receive.clone(),
                                task: self.executor.spawn(self.clone().connection_io_task_main(stream, hello.instance_id, last_receive))
                            }
                        });
                    }
                }
            }
        }
    }

    async fn connection_io_task_main(self: Arc<ReplicatorImpl<'ex>>, stream: TcpStream, remote_instance_id: [u8; 16], last_receive: Arc<AtomicU64>) {
        let mut reader = BufReader::with_capacity(65536, stream.clone());
        let writer = Arc::new(Mutex::new(stream));

        let writer2 = writer.clone();
        let _sync_search_init_task = self.executor.spawn(async {
            let writer = writer2;
            let mut periodic_timer = Timer::interval(Duration::from_secs(1));
            loop {
                let _ = periodic_timer.next().await;
            }
        });

        let mut tmp_mem = Vec::new();
        tmp_mem.resize(self.config.max_object_size, 0);
        let tmp = tmp_mem.as_mut_slice();
        'main_io_loop: loop {
            if reader.read_exact(&mut tmp[0..1]).await.is_err() {
                break 'main_io_loop;
            }
            let message_type = tmp[0];

            last_receive.store(ms_monotonic(), Ordering::Relaxed);

            match message_type {
                protocol::MESSAGE_TYPE_NOP => {},

                protocol::MESSAGE_TYPE_HAVE_NEW_OBJECT => {
                    if reader.read_exact(&mut tmp[0..IDENTITY_HASH_SIZE]).await.is_err() {
                        break 'main_io_loop;
                    }
                    let identity_hash: [u8; 48] = (&tmp[0..IDENTITY_HASH_SIZE]).try_into().unwrap();
                    let mut announced_objects_requested = self.announced_objects_requested.lock().await;
                    if !announced_objects_requested.contains_key(&identity_hash) && !self.store.have(&identity_hash) {
                        announced_objects_requested.insert(identity_hash.clone(), ms_monotonic());
                        drop(announced_objects_requested); // release mutex

                        tmp[0] = protocol::MESSAGE_TYPE_GET_OBJECTS;
                        tmp[1] = 0;
                        tmp[2] = varint::ONE;
                        tmp[3..(3 + IDENTITY_HASH_SIZE)].copy_from_slice(&identity_hash);
                        if !writer.lock().await.write_all(&tmp).await.is_err() {
                            break 'main_io_loop;
                        }
                    }
                }

                protocol::MESSAGE_TYPE_OBJECT => {
                    let object_size = varint::async_read(&mut reader).await;
                    if object_size.is_err() {
                        break 'main_io_loop;
                    }
                    let object_size = object_size.unwrap();
                    if object_size > self.config.max_object_size as u64 {
                        break 'main_io_loop;
                    }

                    let object = &mut tmp[0..(object_size as usize)];
                    if reader.read_exact(object).await.is_err() {
                        break 'main_io_loop;
                    }

                    let identity_hash: [u8; 48] = Sha384::digest(object.as_ref()).as_ref().try_into().unwrap();
                    match self.store.put(&identity_hash, object) {
                        StoreObjectResult::Invalid => {
                            break 'main_io_loop;
                        },
                        StoreObjectResult::Ok | StoreObjectResult::Duplicate => {
                            if self.announced_objects_requested.lock().await.remove(&identity_hash).is_some() {
                                // TODO: propagate rumor if we requested this object in response to a HAVE message.
                            }
                        },
                        _ => {
                            let _ = self.announced_objects_requested.lock().await.remove(&identity_hash);
                        }
                    }
                },

                protocol::MESSAGE_TYPE_GET_OBJECTS => {
                    // Read common prefix if the requester is requesting a set of hashes with the same beginning.
                    // A common prefix length of zero means they're requesting by full hash.
                    if reader.read_exact(&mut tmp[0..1]).await.is_err() {
                        break 'main_io_loop;
                    }
                    let common_prefix_length = tmp[0] as usize;
                    if common_prefix_length >= IDENTITY_HASH_SIZE {
                        break 'main_io_loop;
                    }
                    if reader.read_exact(&mut tmp[0..common_prefix_length]).await.is_err() {
                        break 'main_io_loop;
                    }

                    // Get the number of hashes being requested.
                    let hash_count = varint::async_read(&mut reader).await;
                    if hash_count.is_err() {
                        break 'main_io_loop;
                    }
                    let hash_count = hash_count.unwrap();

                    // Step through each suffix of the common prefix and send the object if found.
                    for _ in 0..hash_count {
                        if reader.read_exact(&mut tmp[common_prefix_length..IDENTITY_HASH_SIZE]).await.is_err() {
                            break 'main_io_loop;
                        }
                        let identity_hash: [u8; IDENTITY_HASH_SIZE] = (&tmp[0..IDENTITY_HASH_SIZE]).try_into().unwrap();
                        let object = self.store.get(&identity_hash);
                        if object.is_some() {
                            let object2 = object.unwrap();
                            let object = object2.as_slice();
                            let mut w = writer.lock().await;
                            if varint::async_write(&mut *w, object.len() as u64).await.is_err() {
                                break 'main_io_loop;
                            }
                            if w.write_all(object).await.is_err() {
                                break 'main_io_loop;
                            }
                        }
                    }
                },

                _ => {
                    break 'main_io_loop;
                }
            }
        }
    }
}

unsafe impl<'ex> Send for ReplicatorImpl<'ex> {}

unsafe impl<'ex> Sync for ReplicatorImpl<'ex> {}
