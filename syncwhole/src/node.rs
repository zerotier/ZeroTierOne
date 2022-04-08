/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2022 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::collections::{HashMap, HashSet};
use std::io::IoSlice;
use std::mem::MaybeUninit;
use std::net::{Ipv4Addr, Ipv6Addr, SocketAddr, SocketAddrV4, SocketAddrV6};
use std::ops::Add;
use std::sync::atomic::{AtomicBool, AtomicI64, AtomicU64, Ordering};
use std::sync::Arc;

use serde::{Deserialize, Serialize};
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::tcp::{OwnedReadHalf, OwnedWriteHalf};
use tokio::net::{TcpListener, TcpSocket, TcpStream};
use tokio::sync::Mutex;
use tokio::task::JoinHandle;
use tokio::time::{Duration, Instant};

use crate::datastore::*;
use crate::host::Host;
use crate::iblt::IBLT;
use crate::protocol::*;
use crate::utils::*;
use crate::varint;

/// Period for running main housekeeping pass.
const HOUSEKEEPING_PERIOD: i64 = SYNC_STATUS_PERIOD;

/// Inactivity timeout for connections in milliseconds.
const CONNECTION_TIMEOUT: i64 = SYNC_STATUS_PERIOD * 4;

/// Announce when we get records from peers if sync status estimate is more than this threshold.
/// This is used to stop us from spamming with HaveRecords while catching up.
const ANNOUNCE_IF_SYNCED_MORE_THAN: f64 = 0.95;

/// Information about a remote node to which we are connected.
#[derive(Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct RemoteNodeInfo {
    /// Optional name advertised by remote node (arbitrary).
    pub name: String,

    /// Optional contact information advertised by remote node (arbitrary).
    pub contact: String,

    /// Actual remote endpoint address.
    pub remote_address: SocketAddr,

    /// Explicitly advertised remote addresses supplied by remote node (not necessarily verified).
    pub explicit_addresses: Vec<SocketAddr>,

    /// Time TCP connection was established (ms since epoch).
    pub connect_time: i64,

    /// Time TCP connection was estaablished (ms, monotonic).
    pub connect_instant: i64,

    /// True if this is an inbound TCP connection.
    pub inbound: bool,

    /// True if this connection has exchanged init messages successfully.
    pub initialized: bool,
}

/// An instance of the syncwhole data set synchronization engine.
///
/// This holds a number of async tasks that are terminated or aborted if this object
/// is dropped. In other words this implements structured concurrency.
pub struct Node<D: DataStore + 'static, H: Host + 'static> {
    internal: Arc<NodeInternal<D, H>>,
    housekeeping_task: JoinHandle<()>,
    announce_task: JoinHandle<()>,
    listener_task: JoinHandle<()>,
}

impl<D: DataStore + 'static, H: Host + 'static> Node<D, H> {
    pub async fn new(db: Arc<D>, host: Arc<H>, bind_address: SocketAddr) -> std::io::Result<Self> {
        let listener = if bind_address.is_ipv4() { TcpSocket::new_v4() } else { TcpSocket::new_v6() }?;
        configure_tcp_socket(&listener)?;
        listener.bind(bind_address.clone())?;
        let listener = listener.listen(1024)?;

        let internal = Arc::new(NodeInternal::<D, H> {
            anti_loopback_secret: {
                let mut tmp = [0_u8; 64];
                host.get_secure_random(&mut tmp);
                tmp
            },
            datastore: db.clone(),
            host: host.clone(),
            connections: Mutex::new(HashMap::with_capacity(64)),
            announce_queue: Mutex::new(HashMap::with_capacity(256)),
            bind_address,
            starting_instant: Instant::now(),
            sync_completeness_estimate: AtomicU64::new((0.0_f64).to_bits()),
        });

        Ok(Self {
            internal: internal.clone(),
            housekeeping_task: tokio::spawn(internal.clone().housekeeping_task_main()),
            announce_task: tokio::spawn(internal.clone().announce_task_main()),
            listener_task: tokio::spawn(internal.listener_task_main(listener)),
        })
    }

    #[inline(always)]
    pub fn datastore(&self) -> &Arc<D> {
        &self.internal.datastore
    }

    #[inline(always)]
    pub fn host(&self) -> &Arc<H> {
        &self.internal.host
    }

    /// Broadcast a new record to the world.
    ///
    /// This should be called when new records are added to the synchronized data store
    /// that are created locally. If this isn't called it may take a while for normal
    /// sync to pick up and propagate the record.
    pub async fn broadcast_new_record(&self, key: &[u8], value: &[u8]) {}

    /// Attempt to connect to an explicitly specified TCP endpoint.
    pub async fn connect(&self, endpoint: &SocketAddr) -> std::io::Result<bool> {
        self.internal.clone().connect(endpoint, Instant::now().add(Duration::from_millis(CONNECTION_TIMEOUT as u64))).await
    }

    /// Get open peer to peer connections.
    pub async fn list_connections(&self) -> Vec<RemoteNodeInfo> {
        let connections = self.internal.connections.lock().await;
        let mut cl: Vec<RemoteNodeInfo> = Vec::with_capacity(connections.len());
        for (_, c) in connections.iter() {
            cl.push(c.info.lock().unwrap().clone());
        }
        cl
    }

    /// Get the number of open peer to peer connections.
    pub async fn connection_count(&self) -> usize {
        self.internal.connections.lock().await.len()
    }

    /// Get a value from 0.0 to 1.0 estimating how synchronized we are with the network.
    ///
    /// This is an inexact estimate since it's based on record counts and it's possible for
    /// two nodes to have the same count but disjoint sets. It tends to be fairly good in
    /// practice though unless you have been disconnected for a very long time.
    pub async fn sync_completeness_estimate(&self) -> f64 {
        f64::from_bits(self.internal.sync_completeness_estimate.load(Ordering::Relaxed))
    }
}

impl<D: DataStore + 'static, H: Host + 'static> Drop for Node<D, H> {
    fn drop(&mut self) {
        self.housekeeping_task.abort();
        self.announce_task.abort();
        self.listener_task.abort();
    }
}

/********************************************************************************************************************/

fn configure_tcp_socket(socket: &TcpSocket) -> std::io::Result<()> {
    let _ = socket.set_linger(None);
    if socket.set_reuseport(true).is_ok() {
        Ok(())
    } else {
        socket.set_reuseaddr(true)
    }
}

fn decode_msgpack<'a, T: Deserialize<'a>>(b: &'a [u8]) -> std::io::Result<T> {
    rmp_serde::from_slice(b).map_err(|e| std::io::Error::new(std::io::ErrorKind::InvalidData, format!("invalid msgpack object: {}", e.to_string())))
}

pub struct NodeInternal<D: DataStore + 'static, H: Host + 'static> {
    // Secret used to perform HMAC to detect and drop loopback connections to self.
    anti_loopback_secret: [u8; 64],

    // Outside code implementations of DataStore and Host traits.
    datastore: Arc<D>,
    host: Arc<H>,

    // Connections and their task join handles, by remote endpoint address.
    connections: Mutex<HashMap<SocketAddr, Arc<Connection>>>,

    // Records received since last announce and the endpoints that we know already have them.
    announce_queue: Mutex<HashMap<[u8; ANNOUNCE_KEY_LEN], Vec<SocketAddr>>>,

    // Local address to which this node is bound
    bind_address: SocketAddr,

    // Instant this node started.
    starting_instant: Instant,

    // Latest estimate of sync completeness.
    sync_completeness_estimate: AtomicU64,
}

impl<D: DataStore + 'static, H: Host + 'static> NodeInternal<D, H> {
    fn ms_monotonic(&self) -> i64 {
        Instant::now().duration_since(self.starting_instant).as_millis() as i64
    }

    async fn housekeeping_task_main(self: Arc<Self>) {
        let mut tasks: Vec<JoinHandle<()>> = Vec::new();
        let mut counts: Vec<u64> = Vec::new();
        let mut connected_to_addresses: HashSet<SocketAddr> = HashSet::new();
        let mut sleep_until = Instant::now().add(Duration::from_millis(500));
        loop {
            tokio::time::sleep_until(sleep_until).await;
            sleep_until = sleep_until.add(Duration::from_millis(HOUSEKEEPING_PERIOD as u64));

            tasks.clear();
            counts.clear();
            connected_to_addresses.clear();
            let now = self.ms_monotonic();

            // Drop dead connections, send SyncStatus, and populate counts for computing sync status.
            let sync_status = Arc::new(
                rmp_serde::encode::to_vec_named(&msg::SyncStatus {
                    record_count: self.datastore.total_count().await,
                    clock: self.datastore.clock() as u64,
                })
                .unwrap(),
            );
            self.connections.lock().await.retain(|sa, c| {
                if !c.closed.load(Ordering::Relaxed) {
                    let cc = c.clone();
                    if (now - c.last_receive_time.load(Ordering::Relaxed)) < CONNECTION_TIMEOUT {
                        connected_to_addresses.insert(sa.clone());
                        if c.info.lock().unwrap().initialized {
                            counts.push(c.last_sync_status_record_count.load(Ordering::Relaxed));
                            let ss2 = sync_status.clone();
                            tasks.push(tokio::spawn(async move {
                                let _ = tokio::time::timeout_at(sleep_until, cc.send_msg(MessageType::SyncStatus, ss2.as_slice(), now)).await;
                            }));
                        }
                        true // keep connection
                    } else {
                        let _ = c.read_task.lock().unwrap().take().map(|j| j.abort());
                        let host = self.host.clone();
                        tasks.push(tokio::spawn(async move {
                            host.on_connection_closed(&*cc.info.lock().unwrap(), "timeout".to_string());
                        }));
                        false // discard connection
                    }
                } else {
                    let host = self.host.clone();
                    let cc = c.clone();
                    let j = c.read_task.lock().unwrap().take();
                    tasks.push(tokio::spawn(async move {
                        if j.is_some() {
                            let e = j.unwrap().await;
                            if e.is_ok() {
                                let e = e.unwrap();
                                host.on_connection_closed(&*cc.info.lock().unwrap(), e.map_or_else(|e| e.to_string(), |_| "unknown error".to_string()));
                            } else {
                                host.on_connection_closed(&*cc.info.lock().unwrap(), "remote host closed connection".to_string());
                            }
                        } else {
                            host.on_connection_closed(&*cc.info.lock().unwrap(), "remote host closed connection".to_string());
                        }
                    }));
                    false // discard connection
                }
            });

            let sync_completness_estimate = if !counts.is_empty() {
                counts.sort_unstable();
                let twothirds = if counts.len() > 3 { *counts.get((counts.len() / 3) * 2).unwrap() } else { *counts.last().unwrap() };
                if twothirds > 0 {
                    ((self.datastore.total_count().await as f64) / (twothirds as f64)).min(1.0)
                } else {
                    1.0
                }
            } else {
                1.0
            };
            self.sync_completeness_estimate.store(sync_completness_estimate.to_bits(), Ordering::Relaxed);

            let config = self.host.node_config();

            // Always try to connect to anchor peers.
            for sa in config.anchors.iter() {
                if !connected_to_addresses.contains(sa) {
                    let sa = sa.clone();
                    let self2 = self.clone();
                    tasks.push(tokio::spawn(async move {
                        let _ = self2.connect(&sa, sleep_until).await;
                    }));
                    connected_to_addresses.insert(sa.clone());
                }
            }

            // Try to connect to more peers until desired connection count is reached.
            let desired_connection_count = config.desired_connection_count.min(config.max_connection_count);
            for sa in config.seeds.iter() {
                if connected_to_addresses.len() >= desired_connection_count {
                    break;
                }
                if !connected_to_addresses.contains(sa) {
                    connected_to_addresses.insert(sa.clone());
                    let self2 = self.clone();
                    let sa = sa.clone();
                    tasks.push(tokio::spawn(async move {
                        let _ = self2.connect(&sa, sleep_until).await;
                    }));
                }
            }

            // Wait for this iteration's batched background tasks to complete.
            loop {
                let s = tasks.pop();
                if s.is_some() {
                    let _ = s.unwrap().await;
                } else {
                    break;
                }
            }
        }
    }

    async fn announce_task_main(self: Arc<Self>) {
        let mut sleep_until = Instant::now().add(Duration::from_millis(ANNOUNCE_PERIOD as u64));
        let mut to_announce: Vec<([u8; ANNOUNCE_KEY_LEN], Vec<SocketAddr>)> = Vec::with_capacity(256);
        let background_tasks = AsyncTaskReaper::new();
        let announce_timeout = Duration::from_millis(CONNECTION_TIMEOUT as u64);
        loop {
            tokio::time::sleep_until(sleep_until).await;
            sleep_until = sleep_until.add(Duration::from_millis(ANNOUNCE_PERIOD as u64));

            for (key, already_has) in self.announce_queue.lock().await.drain() {
                to_announce.push((key, already_has));
            }

            let now = self.ms_monotonic();
            for c in self.connections.lock().await.iter() {
                let mut have_records: Vec<u8> = Vec::with_capacity((to_announce.len() * ANNOUNCE_KEY_LEN) + 4);
                have_records.push(ANNOUNCE_KEY_LEN as u8);
                for (key, already_has) in to_announce.iter() {
                    if !already_has.contains(c.0) {
                        let _ = std::io::Write::write_all(&mut have_records, key);
                    }
                }
                if have_records.len() > 1 {
                    let c2 = c.1.clone();
                    background_tasks.spawn(async move {
                        // If the connection dies this will either fail or time out in 1s. Usually these execute instantly due to
                        // write buffering but a short timeout prevents them from building up too much.
                        let _ = tokio::time::timeout(announce_timeout, c2.send_msg(MessageType::HaveRecords, have_records.as_slice(), now));
                    })
                }
            }

            to_announce.clear();
        }
    }

    async fn listener_task_main(self: Arc<Self>, listener: TcpListener) {
        loop {
            let socket = listener.accept().await;
            if socket.is_ok() {
                let (stream, address) = socket.unwrap();
                if self.host.allow(&address) {
                    let config = self.host.node_config();
                    if self.connections.lock().await.len() < config.max_connection_count || config.anchors.contains(&address) {
                        Self::connection_start(&self, address, stream, true).await;
                    }
                }
            }
        }
    }

    async fn connect(self: Arc<Self>, address: &SocketAddr, deadline: Instant) -> std::io::Result<bool> {
        self.host.on_connect_attempt(address);
        let stream = if address.is_ipv4() { TcpSocket::new_v4() } else { TcpSocket::new_v6() }?;
        configure_tcp_socket(&stream)?;
        stream.bind(self.bind_address.clone())?;
        let stream = tokio::time::timeout_at(deadline, stream.connect(address.clone())).await;
        if stream.is_ok() {
            Ok(self.connection_start(address.clone(), stream.unwrap()?, false).await)
        } else {
            Err(std::io::Error::new(std::io::ErrorKind::TimedOut, "connect timed out"))
        }
    }

    async fn connection_start(self: &Arc<Self>, address: SocketAddr, stream: TcpStream, inbound: bool) -> bool {
        let mut ok = false;
        let _ = self.connections.lock().await.entry(address.clone()).or_insert_with(|| {
            ok = true;
            let _ = stream.set_nodelay(false);
            let (reader, writer) = stream.into_split();
            let now = self.ms_monotonic();
            let connection = Arc::new(Connection {
                writer: Mutex::new(writer),
                last_send_time: AtomicI64::new(now),
                last_receive_time: AtomicI64::new(now),
                last_sync_status_record_count: AtomicU64::new(0),
                info: std::sync::Mutex::new(RemoteNodeInfo {
                    name: String::new(),
                    contact: String::new(),
                    remote_address: address.clone(),
                    explicit_addresses: Vec::new(),
                    connect_time: ms_since_epoch(),
                    connect_instant: now,
                    inbound,
                    initialized: false,
                }),
                read_task: std::sync::Mutex::new(None),
                closed: AtomicBool::new(false),
            });
            let self2 = self.clone();
            let c2 = connection.clone();
            connection.read_task.lock().unwrap().replace(tokio::spawn(async move {
                let result = self2.connection_io_task_main(&c2, address, reader).await;
                c2.closed.store(true, Ordering::Relaxed);
                result
            }));
            connection
        });
        ok
    }

    async fn connection_io_task_main(self: Arc<Self>, connection: &Arc<Connection>, remote_address: SocketAddr, mut reader: OwnedReadHalf) -> std::io::Result<()> {
        const BUF_CHUNK_SIZE: usize = 4096;
        const READ_BUF_INITIAL_SIZE: usize = 65536; // should be a multiple of BUF_CHUNK_SIZE

        let background_tasks = AsyncTaskReaper::new();
        let mut write_buffer: Vec<u8> = Vec::with_capacity(BUF_CHUNK_SIZE);
        let mut read_buffer: Vec<u8> = Vec::new();
        read_buffer.resize(READ_BUF_INITIAL_SIZE, 0);

        let config = self.host.node_config();
        let mut anti_loopback_challenge_sent = [0_u8; 64];
        let mut domain_challenge_sent = [0_u8; 64];
        let mut auth_challenge_sent = [0_u8; 64];
        self.host.get_secure_random(&mut anti_loopback_challenge_sent);
        self.host.get_secure_random(&mut domain_challenge_sent);
        self.host.get_secure_random(&mut auth_challenge_sent);
        connection
            .send_obj(
                &mut write_buffer,
                MessageType::Init,
                &msg::Init {
                    anti_loopback_challenge: &anti_loopback_challenge_sent,
                    domain_challenge: &domain_challenge_sent,
                    auth_challenge: &auth_challenge_sent,
                    node_name: config.name.as_str(),
                    node_contact: config.contact.as_str(),
                    locally_bound_port: self.bind_address.port(),
                    explicit_ipv4: None,
                    explicit_ipv6: None,
                },
                self.ms_monotonic(),
            )
            .await?;
        drop(config);

        let max_message_size = ((D::MAX_VALUE_SIZE * 8) + (D::KEY_SIZE * 1024) + 65536) as u64; // sanity limit
        let mut initialized = false;
        let mut init_received = false;
        let mut buffer_fill = 0_usize;
        loop {
            let message_type: MessageType;
            let message_size: usize;
            let header_size: usize;
            let total_size: usize;
            loop {
                buffer_fill += reader.read(&mut read_buffer.as_mut_slice()[buffer_fill..]).await?;
                if buffer_fill >= 2 {
                    // type and at least one byte of varint
                    let ms = varint::decode(&read_buffer.as_slice()[1..]);
                    if ms.1 > 0 {
                        // varint is all there and parsed correctly
                        if ms.0 > max_message_size {
                            return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "message too large"));
                        }

                        message_type = MessageType::from(*read_buffer.get(0).unwrap());
                        message_size = ms.0 as usize;
                        header_size = 1 + ms.1;
                        total_size = header_size + message_size;

                        if read_buffer.len() < total_size {
                            read_buffer.resize(((total_size / BUF_CHUNK_SIZE) + 1) * BUF_CHUNK_SIZE, 0);
                        }
                        while buffer_fill < total_size {
                            buffer_fill += reader.read(&mut read_buffer.as_mut_slice()[buffer_fill..]).await?;
                        }

                        break;
                    }
                }
            }
            let mut message = &read_buffer.as_slice()[header_size..total_size];

            let now = self.ms_monotonic();
            connection.last_receive_time.store(now, Ordering::Relaxed);

            match message_type {
                MessageType::Nop => {}

                MessageType::Init => {
                    if init_received {
                        return Err(std::io::Error::new(std::io::ErrorKind::Other, "duplicate init"));
                    }
                    init_received = true;

                    let msg: msg::Init = decode_msgpack(message)?;
                    let (anti_loopback_response, domain_challenge_response, auth_challenge_response) = {
                        let mut info = connection.info.lock().unwrap();
                        info.name = msg.node_name.to_string();
                        info.contact = msg.node_contact.to_string();
                        let _ = msg.explicit_ipv4.map(|pv4| {
                            info.explicit_addresses.push(SocketAddr::V4(SocketAddrV4::new(Ipv4Addr::from(pv4.ip), pv4.port)));
                        });
                        let _ = msg.explicit_ipv6.map(|pv6| {
                            info.explicit_addresses.push(SocketAddr::V6(SocketAddrV6::new(Ipv6Addr::from(pv6.ip), pv6.port, 0, 0)));
                        });
                        let info = info.clone();

                        let auth_challenge_response = self.host.authenticate(&info, msg.auth_challenge);
                        if auth_challenge_response.is_none() {
                            return Err(std::io::Error::new(std::io::ErrorKind::Other, "authenticate() returned None, connection dropped"));
                        }
                        let auth_challenge_response = auth_challenge_response.unwrap();

                        (H::hmac_sha512(&self.anti_loopback_secret, msg.anti_loopback_challenge), H::hmac_sha512(&H::sha512(&[self.datastore.domain().as_bytes()]), msg.domain_challenge), auth_challenge_response)
                    };

                    connection
                        .send_obj(
                            &mut write_buffer,
                            MessageType::InitResponse,
                            &msg::InitResponse {
                                anti_loopback_response: &anti_loopback_response,
                                domain_response: &domain_challenge_response,
                                auth_response: &auth_challenge_response,
                            },
                            now,
                        )
                        .await?;
                }

                MessageType::InitResponse => {
                    let msg: msg::InitResponse = decode_msgpack(message)?;
                    let mut info = connection.info.lock().unwrap();

                    if info.initialized {
                        return Err(std::io::Error::new(std::io::ErrorKind::Other, "duplicate init response"));
                    }

                    if msg.anti_loopback_response.eq(&H::hmac_sha512(&self.anti_loopback_secret, &anti_loopback_challenge_sent)) {
                        return Err(std::io::Error::new(std::io::ErrorKind::Other, "rejected connection to self"));
                    }
                    if !msg.domain_response.eq(&H::hmac_sha512(&H::sha512(&[self.datastore.domain().as_bytes()]), &domain_challenge_sent)) {
                        return Err(std::io::Error::new(std::io::ErrorKind::Other, "domain mismatch"));
                    }
                    if !self.host.authenticate(&info, &auth_challenge_sent).map_or(false, |cr| msg.auth_response.eq(&cr)) {
                        return Err(std::io::Error::new(std::io::ErrorKind::Other, "challenge/response authentication failed"));
                    }

                    initialized = true;

                    info.initialized = true;

                    let info = info.clone(); // also releases lock since info is replaced/destroyed
                    self.host.on_connect(&info);
                }

                // Handle messages other than INIT and INIT_RESPONSE after checking 'initialized' flag.
                _ => {
                    if !initialized {
                        return Err(std::io::Error::new(std::io::ErrorKind::Other, "init exchange must be completed before other messages are sent"));
                    }

                    match message_type {
                        MessageType::HaveRecords => {
                            if message.len() > 1 {
                                let clock = self.datastore.clock();
                                let mut announce_queue_key = [0_u8; ANNOUNCE_KEY_LEN];
                                let mut start = [0_u8; KEY_SIZE];
                                let mut end = [0xff_u8; KEY_SIZE];
                                let key_prefix_len = message[0] as usize;
                                message = &message[1..];
                                if key_prefix_len > 0 && key_prefix_len <= KEY_SIZE {
                                    write_buffer.clear();
                                    write_buffer.push(key_prefix_len as u8);
                                    while message.len() >= key_prefix_len {
                                        let key_prefix = &message[..key_prefix_len];

                                        if key_prefix_len >= ANNOUNCE_KEY_LEN {
                                            // If the key prefix is appropriately sized, look up and add this remote endpoint
                                            // to the list of endpoints that already have this record if it's in the announce
                                            // queue. We don't add a new entry to the announce queue if one doesn't already
                                            // exist because we did not just receive the actual record. This just avoids announcing
                                            // to peers that just told us they have it.
                                            announce_queue_key.copy_from_slice(&key_prefix[..ANNOUNCE_KEY_LEN]);
                                            self.announce_queue.lock().await.get_mut(&announce_queue_key).map(|already_has| {
                                                if !already_has.contains(&remote_address) {
                                                    already_has.push(remote_address.clone());
                                                }
                                            });
                                        }

                                        if if key_prefix_len < KEY_SIZE {
                                            (&mut start[..key_prefix_len]).copy_from_slice(key_prefix);
                                            (&mut end[..key_prefix_len]).copy_from_slice(key_prefix);
                                            self.datastore.count(clock, &start, &end).await == 0
                                        } else {
                                            !self.datastore.contains(clock, key_prefix).await
                                        } {
                                            let _ = std::io::Write::write_all(&mut write_buffer, key_prefix);
                                        }

                                        message = &message[key_prefix_len..];
                                    }
                                    if write_buffer.len() > 1 {
                                        let _ = connection.send_msg(MessageType::GetRecords, write_buffer.as_slice(), now).await?;
                                    }
                                }
                            }
                        }

                        MessageType::GetRecords => {
                            if message.len() > 1 {
                                let mut start = [0_u8; KEY_SIZE];
                                let mut end = [0xff_u8; KEY_SIZE];
                                let key_prefix_len = message[0] as usize;
                                message = &message[1..];
                                if key_prefix_len > 0 && key_prefix_len <= KEY_SIZE {
                                    while message.len() >= key_prefix_len {
                                        let key_prefix = &message[..key_prefix_len];

                                        if key_prefix_len < KEY_SIZE {
                                            (&mut start[..key_prefix_len]).copy_from_slice(key_prefix);
                                            (&mut end[..key_prefix_len]).copy_from_slice(key_prefix);
                                            self.datastore
                                                .for_each(0, &start, &end, |_, v| {
                                                    let v2 = v.clone();
                                                    let c2 = connection.clone();
                                                    background_tasks.spawn(async move {
                                                        let _ = c2.send_msg(MessageType::Record, v2.as_ref(), now).await;
                                                    });
                                                    true
                                                })
                                                .await;
                                        } else {
                                            let record = self.datastore.load(0, key_prefix).await;
                                            if record.is_some() {
                                                let record = record.unwrap();
                                                let v: &[u8] = record.as_ref();
                                                let _ = connection.send_msg(MessageType::Record, v, now).await?;
                                            }
                                        }

                                        message = &message[key_prefix_len..];
                                    }
                                }
                            }
                        }

                        MessageType::Record => {
                            let key = H::sha512(&[message]);
                            match self.datastore.store(&key, message).await {
                                StoreResult::Ok => {
                                    if f64::from_bits(self.sync_completeness_estimate.load(Ordering::Relaxed)) >= ANNOUNCE_IF_SYNCED_MORE_THAN {
                                        let announce_key: [u8; ANNOUNCE_KEY_LEN] = (&key[..ANNOUNCE_KEY_LEN]).try_into().unwrap();
                                        let mut q = self.announce_queue.lock().await;
                                        let ql = q.entry(announce_key).or_insert_with(|| Vec::with_capacity(2));
                                        if !ql.contains(&remote_address) {
                                            ql.push(remote_address.clone());
                                        }
                                    }
                                }
                                StoreResult::Rejected => {
                                    return Err(std::io::Error::new(std::io::ErrorKind::Other, format!("record rejected by data store: {}", to_hex_string(&key))));
                                }
                                _ => {}
                            }
                        }

                        MessageType::SyncStatus => {
                            let msg: msg::SyncStatus = decode_msgpack(message)?;
                            connection.last_sync_status_record_count.store(msg.record_count, Ordering::Relaxed);
                        }

                        MessageType::SyncRequest => {
                            let msg: msg::SyncRequest = decode_msgpack(message)?;
                        }

                        MessageType::SyncResponse => {
                            let msg: msg::SyncResponse = decode_msgpack(message)?;
                        }

                        _ => {}
                    }
                }
            }

            read_buffer.copy_within(total_size..buffer_fill, 0);
            buffer_fill -= total_size;
        }
    }
}

impl<D: DataStore + 'static, H: Host + 'static> Drop for NodeInternal<D, H> {
    fn drop(&mut self) {
        let _ = tokio::runtime::Handle::try_current().map_or_else(
            |_| {
                for (_, c) in self.connections.blocking_lock().drain() {
                    c.read_task.lock().unwrap().as_mut().map(|c| c.abort());
                }
            },
            |h| {
                let _ = h.block_on(async {
                    for (_, c) in self.connections.lock().await.drain() {
                        c.read_task.lock().unwrap().as_mut().map(|c| c.abort());
                    }
                });
            },
        );
    }
}

struct Connection {
    writer: Mutex<OwnedWriteHalf>,
    last_send_time: AtomicI64,
    last_receive_time: AtomicI64,
    last_sync_status_record_count: AtomicU64,
    info: std::sync::Mutex<RemoteNodeInfo>,
    read_task: std::sync::Mutex<Option<JoinHandle<std::io::Result<()>>>>,
    closed: AtomicBool,
}

impl Connection {
    async fn send_msg(&self, message_type: MessageType, data: &[u8], now: i64) -> std::io::Result<()> {
        let mut header: [u8; 16] = unsafe { MaybeUninit::uninit().assume_init() };
        header[0] = message_type as u8;
        let header_size = 1 + varint::encode(&mut header[1..], data.len() as u64);
        if self.writer.lock().await.write_vectored(&[IoSlice::new(&header[0..header_size]), IoSlice::new(data)]).await? == (data.len() + header_size) {
            self.last_send_time.store(now, Ordering::Relaxed);
            Ok(())
        } else {
            Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "write error"))
        }
    }

    async fn send_obj<O: Serialize>(&self, write_buf: &mut Vec<u8>, message_type: MessageType, obj: &O, now: i64) -> std::io::Result<()> {
        write_buf.clear();
        if rmp_serde::encode::write_named(write_buf, obj).is_ok() {
            self.send_msg(message_type, write_buf.as_slice(), now).await
        } else {
            Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "serialize failure (internal error)"))
        }
    }
}
