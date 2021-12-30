/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */



/*
use std::collections::HashMap;
use std::convert::TryInto;
use std::error::Error;
use std::hash::{Hash, Hasher};
use std::mem::{size_of, transmute};
use std::sync::Arc;
use std::sync::atomic::{AtomicU64, Ordering};
use std::time::Duration;

use smol::{Executor, Task, Timer};
use smol::io::{AsyncReadExt, AsyncWriteExt, BufReader};
use smol::lock::Mutex;
use smol::net::*;
use smol::stream::StreamExt;

use zerotier_core_crypto::hash::SHA384;
use zerotier_core_crypto::random;

use crate::{IDENTITY_HASH_SIZE, ms_monotonic, ms_since_epoch, protocol, Config};
use crate::store::{Store, StorePutResult};
use crate::varint;

const CONNECTION_TIMEOUT_SECONDS: u64 = 60;
const CONNECTION_SYNC_RESTART_TIMEOUT_SECONDS: u64 = 5;

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
    task: Task<()>,
}

struct ReplicatorImpl<'ex, S: 'static + Store> {
    executor: Arc<Executor<'ex>>,
    instance_id: [u8; 16],
    loopback_check_code_secret: [u8; 48],
    domain_hash: [u8; 48],
    store: Arc<S>,
    config: Config,
    connections: Mutex<HashMap<ConnectionKey, Connection>>,
    connections_in_progress: Mutex<HashMap<SocketAddr, Task<()>>>,
    announced_objects_requested: Mutex<HashMap<[u8; IDENTITY_HASH_SIZE], u64>>,
}

pub struct Replicator<'ex, S: 'static + Store> {
    v4_listener_task: Option<Task<()>>,
    v6_listener_task: Option<Task<()>>,
    background_cleanup_task: Task<()>,
    _impl: Arc<ReplicatorImpl<'ex, S>>,
}

impl<'ex, S: 'static + Store> Replicator<'ex, S> {
    /// Create a new replicator to replicate the contents of the provided store.
    /// All async tasks, sockets, and connections will be dropped if the replicator is dropped.
    pub async fn start(executor: &Arc<Executor<'ex>>, store: Arc<S>, config: Config) -> Result<Replicator<'ex, S>, Box<dyn Error>> {
        let listener_v4 = socket2::Socket::new(socket2::Domain::IPV4, socket2::Type::STREAM, Some(socket2::Protocol::TCP)).and_then(|v4| {
            let _ = v4.set_reuse_address(true);
            let _ = v4.bind(&socket2::SockAddr::from(std::net::SocketAddrV4::new(std::net::Ipv4Addr::UNSPECIFIED, config.tcp_port)))?;
            let _ = v4.listen(64);
            Ok(v4)
        });
        let listener_v6 = socket2::Socket::new(socket2::Domain::IPV6, socket2::Type::STREAM, Some(socket2::Protocol::TCP)).and_then(|v6| {
            let _ = v6.set_only_v6(true);
            let _ = v6.set_reuse_address(true);
            let _ = v6.bind(&socket2::SockAddr::from(std::net::SocketAddrV6::new(std::net::Ipv6Addr::UNSPECIFIED, config.tcp_port, 0, 0)))?;
            let _ = v6.listen(64);
            Ok(v6)
        });
        if listener_v4.is_err() && listener_v6.is_err() {
            return Err(Box::new(listener_v4.unwrap_err()));
        }

        let r = Arc::new(ReplicatorImpl::<'ex, S> {
            executor: executor.clone(),
            instance_id: {
                let mut tmp = [0_u8; 16];
                random::fill_bytes_secure(&mut tmp);
                tmp
            },
            loopback_check_code_secret: {
                let mut tmp = [0_u8; 48];
                random::fill_bytes_secure(&mut tmp);
                tmp
            },
            domain_hash: SHA384::hash(config.domain.as_bytes()),
            config,
            store,
            connections: Mutex::new(HashMap::new()),
            connections_in_progress: Mutex::new(HashMap::new()),
            announced_objects_requested: Mutex::new(HashMap::new()),
        });

        Ok(Self {
            v4_listener_task: listener_v4.map_or(None, |listener_v4| {
                Some(executor.spawn(r.clone().tcp_listener_task(smol::net::TcpListener::try_from(std::net::TcpListener::from(listener_v4)).unwrap())))
            }),
            v6_listener_task: listener_v6.map_or(None, |listener_v6| {
                Some(executor.spawn(r.clone().tcp_listener_task(smol::net::TcpListener::try_from(std::net::TcpListener::from(listener_v6)).unwrap())))
            }),
            background_cleanup_task: executor.spawn(r.clone().background_cleanup_task()),
            _impl: r,
        })
    }
}

unsafe impl<'ex, S: 'static + Store> Send for Replicator<'ex, S> {}

unsafe impl<'ex, S: 'static + Store> Sync for Replicator<'ex, S> {}

impl<'ex, S: 'static + Store> ReplicatorImpl<'ex, S> {
    async fn background_cleanup_task(self: Arc<ReplicatorImpl<'ex, S>>) {
        let mut timer = smol::Timer::interval(Duration::from_secs(CONNECTION_TIMEOUT_SECONDS / 10));
        loop {
            timer.next().await;
            let now_mt = ms_monotonic();

            // Garbage collect the map used to track objects we've requested.
            self.announced_objects_requested.lock().await.retain(|_, ts| now_mt.saturating_sub(*ts) < (CONNECTION_TIMEOUT_SECONDS * 1000));

            let mut connections = self.connections.lock().await;

            // Close connections that haven't spoken in too long.
            connections.retain(|_, c| (now_mt.saturating_sub(c.last_receive.load(Ordering::Relaxed))) < (CONNECTION_TIMEOUT_SECONDS * 1000));
            let num_connections = connections.len();
            drop(connections); // release lock

            // Try to connect to more nodes if the count is below the target count.
            if num_connections < self.config.target_link_count {
                let new_link_seed = self.store.get_remote_endpoint();
                if new_link_seed.is_some() {
                    let new_link_seed = new_link_seed.unwrap();
                    let mut connections_in_progress = self.connections_in_progress.lock().await;
                    if !connections_in_progress.contains_key(&new_link_seed) {
                        let s2 = self.clone();
                        let _ = connections_in_progress.insert(new_link_seed.clone(), self.executor.spawn(async move {
                            let new_link = TcpStream::connect(&new_link_seed).await;
                            if new_link.is_ok() {
                                s2.handle_new_connection(new_link.unwrap(), new_link_seed, true).await;
                            } else {
                                let _task = s2.connections_in_progress.lock().await.remove(&new_link_seed);
                            }
                        }));
                    }
                }
            }
        }
    }

    async fn tcp_listener_task(self: Arc<ReplicatorImpl<'ex, S>>, listener: TcpListener) {
        loop {
            let stream = listener.accept().await;
            if stream.is_ok() {
                let (stream, remote_address) = stream.unwrap();
                let mut connections_in_progress = self.connections_in_progress.lock().await;
                if !connections_in_progress.contains_key(&remote_address) {
                    let s2 = self.clone();
                    let _ = connections_in_progress.insert(remote_address, self.executor.spawn(s2.handle_new_connection(stream, remote_address.clone(), false)));
                }
            }
        }
    }

    async fn handle_new_connection(self: Arc<ReplicatorImpl<'ex, S>>, mut stream: TcpStream, remote_address: SocketAddr, outgoing: bool) {
        let _ = stream.set_nodelay(true);

        let mut loopback_check_code_salt = [0_u8; 16];
        random::fill_bytes_secure(&mut loopback_check_code_salt);
        let hello = protocol::Hello {
            hello_size: size_of::<protocol::Hello>() as u8,
            protocol_version: protocol::PROTOCOL_VERSION,
            hash_algorithm: protocol::HASH_ALGORITHM_SHA384,
            flags: if outgoing { protocol::HELLO_FLAG_OUTGOING } else { 0 },
            clock: ms_since_epoch().to_le_bytes(),
            domain_hash: self.domain_hash.clone(),
            instance_id: self.instance_id.clone(),
            loopback_check_code_salt,
            loopback_check_code: (&SHA384::hmac(&self.loopback_check_code_secret, &loopback_check_code_salt)[0..16]).try_into().unwrap(),
        };
        let hello: [u8; size_of::<protocol::Hello>()] = unsafe { transmute(hello) };

        if stream.write_all(&hello).await.is_ok() {
            let mut hello_buf = [0_u8; size_of::<protocol::Hello>()];
            if stream.read_exact(&mut hello_buf).await.is_ok() {
                let hello: protocol::Hello = unsafe { transmute(hello_buf) };

                // Sanity check HELLO packet. In the future we may support different versions and sizes.
                if hello.hello_size == size_of::<protocol::Hello>() as u8 && hello.protocol_version == protocol::PROTOCOL_VERSION && hello.hash_algorithm == protocol::HASH_ALGORITHM_SHA384 {
                    if !SHA384::hmac(&self.loopback_check_code_secret, &hello.loopback_check_code_salt)[0..16].eq(&hello.loopback_check_code) {
                        let k = ConnectionKey {
                            instance_id: hello.instance_id.clone(),
                            ip: remote_address.ip(),
                        };
                        let mut connections = self.connections.lock().await;
                        let s2 = self.clone();
                        let _ = connections.entry(k).or_insert_with(move || {
                            let _ = stream.set_nodelay(false);
                            let last_receive = Arc::new(AtomicU64::new(ms_monotonic()));
                            Connection {
                                remote_address,
                                last_receive: last_receive.clone(),
                                task: s2.executor.spawn(s2.clone().connection_io_task(stream, hello.instance_id, last_receive)),
                            }
                        });
                    }
                }
            }
        }

        let _task = self.connections_in_progress.lock().await.remove(&remote_address);
    }

    async fn connection_io_task(self: Arc<ReplicatorImpl<'ex, S>>, stream: TcpStream, remote_instance_id: [u8; 16], last_receive: Arc<AtomicU64>) {
        let mut reader = BufReader::with_capacity(65536, stream.clone());
        let writer = Arc::new(Mutex::new(stream));

        //let writer2 = writer.clone();
        let _sync_search_init_task = self.executor.spawn(async move {
            //let writer = writer2;
            let mut periodic_timer = Timer::interval(Duration::from_secs(1));
            loop {
                let _ = periodic_timer.next().await;
            }
        });

        let mut get_buffer = Vec::new();
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
                protocol::MESSAGE_TYPE_NOP => {}

                protocol::MESSAGE_TYPE_HAVE_NEW_OBJECT => {
                    if reader.read_exact(&mut tmp[0..IDENTITY_HASH_SIZE]).await.is_err() {
                        break 'main_io_loop;
                    }
                    let identity_hash: [u8; 48] = (&tmp[0..IDENTITY_HASH_SIZE]).try_into().unwrap();
                    let mut announced_objects_requested = self.announced_objects_requested.lock().await;
                    if !announced_objects_requested.contains_key(&identity_hash) && !self.store.have(ms_since_epoch(), &identity_hash) {
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

                    let identity_hash: [u8; 48] = SHA384::hash(object);
                    match self.store.put(ms_since_epoch(), &identity_hash, object) {
                        StorePutResult::Invalid => {
                            break 'main_io_loop;
                        }
                        StorePutResult::Ok | StorePutResult::Duplicate => {
                            if self.announced_objects_requested.lock().await.remove(&identity_hash).is_some() {
                                // TODO: propagate rumor if we requested this object in response to a HAVE message.
                            }
                        }
                        _ => {
                            let _ = self.announced_objects_requested.lock().await.remove(&identity_hash);
                        }
                    }
                }

                protocol::MESSAGE_TYPE_GET_OBJECTS => {
                    // Get the reference time for this query.
                    let reference_time = varint::async_read(&mut reader).await;
                    if reference_time.is_err() {
                        break 'main_io_loop;
                    }
                    let reference_time = reference_time.unwrap();

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
                        if self.store.get(reference_time, &identity_hash, &mut get_buffer) {
                            let mut w = writer.lock().await;
                            if varint::async_write(&mut *w, get_buffer.len() as u64).await.is_err() {
                                break 'main_io_loop;
                            }
                            if w.write_all(get_buffer.as_slice()).await.is_err() {
                                break 'main_io_loop;
                            }
                        }
                    }
                }

                _ => {
                    break 'main_io_loop;
                }
            }
        }
    }
}

unsafe impl<'ex, S: 'static + Store> Send for ReplicatorImpl<'ex, S> {}

unsafe impl<'ex, S: 'static + Store> Sync for ReplicatorImpl<'ex, S> {}
 */
