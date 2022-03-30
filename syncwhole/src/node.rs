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

/// Inactivity timeout for connections in milliseconds.
const CONNECTION_TIMEOUT: i64 = SYNC_STATUS_PERIOD * 4;

/// How often to run the housekeeping task's loop in milliseconds.
const HOUSEKEEPING_INTERVAL: i64 = SYNC_STATUS_PERIOD;

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

/// An instance of the syncwhole data set synchronization engine.
///
/// This holds a number of async tasks that are terminated or aborted if this object
/// is dropped. In other words this implements structured concurrency.
pub struct Node<D: DataStore + 'static, H: Host + 'static> {
    internal: Arc<NodeInternal<D, H>>,
    housekeeping_task: JoinHandle<()>,
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
            bind_address,
            starting_instant: Instant::now(),
        });

        Ok(Self { internal: internal.clone(), housekeeping_task: tokio::spawn(internal.clone().housekeeping_task_main()), listener_task: tokio::spawn(internal.listener_task_main(listener)) })
    }

    #[inline(always)]
    pub fn datastore(&self) -> &Arc<D> {
        &self.internal.datastore
    }

    #[inline(always)]
    pub fn host(&self) -> &Arc<H> {
        &self.internal.host
    }

    pub async fn connect(&self, endpoint: &SocketAddr) -> std::io::Result<bool> {
        self.internal.clone().connect(endpoint, Instant::now().add(Duration::from_millis(CONNECTION_TIMEOUT as u64))).await
    }

    pub async fn list_connections(&self) -> Vec<RemoteNodeInfo> {
        let connections = self.internal.connections.lock().await;
        let mut cl: Vec<RemoteNodeInfo> = Vec::with_capacity(connections.len());
        for (_, c) in connections.iter() {
            cl.push(c.info.lock().await.clone());
        }
        cl
    }

    pub async fn connection_count(&self) -> usize {
        self.internal.connections.lock().await.len()
    }
}

impl<D: DataStore + 'static, H: Host + 'static> Drop for Node<D, H> {
    fn drop(&mut self) {
        self.housekeeping_task.abort();
        self.listener_task.abort();
    }
}

pub struct NodeInternal<D: DataStore + 'static, H: Host + 'static> {
    // Secret used to perform HMAC to detect and drop loopback connections to self.
    anti_loopback_secret: [u8; 64],

    // Outside code implementations of DataStore and Host traits.
    datastore: Arc<D>,
    host: Arc<H>,

    // Connections and their task join handles, by remote endpoint address.
    connections: Mutex<HashMap<SocketAddr, Arc<Connection>>>,

    // Local address to which this node is bound
    bind_address: SocketAddr,

    // Instant this node started.
    starting_instant: Instant,
}

impl<D: DataStore + 'static, H: Host + 'static> NodeInternal<D, H> {
    fn ms_monotonic(&self) -> i64 {
        Instant::now().duration_since(self.starting_instant).as_millis() as i64
    }

    /// Loop that constantly runs in the background to do cleanup and service things.
    async fn housekeeping_task_main(self: Arc<Self>) {
        let mut tasks: Vec<JoinHandle<()>> = Vec::new();
        let mut connected_to_addresses: HashSet<SocketAddr> = HashSet::new();
        let mut sleep_until = Instant::now().add(Duration::from_millis(500));
        loop {
            tokio::time::sleep_until(sleep_until).await;
            sleep_until = sleep_until.add(Duration::from_millis(HOUSEKEEPING_INTERVAL as u64));

            tasks.clear();
            connected_to_addresses.clear();
            let now = self.ms_monotonic();

            self.connections.lock().await.retain(|sa, c| {
                if !c.closed.load(Ordering::Relaxed) {
                    if (now - c.last_receive_time.load(Ordering::Relaxed)) < CONNECTION_TIMEOUT {
                        connected_to_addresses.insert(sa.clone());
                        true // keep connection
                    } else {
                        let _ = c.read_task.lock().unwrap().take().map(|j| j.abort());
                        let host = self.host.clone();
                        let cc = c.clone();
                        tasks.push(tokio::spawn(async move {
                            host.on_connection_closed(&*cc.info.lock().await, "timeout".to_string());
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
                                host.on_connection_closed(&*cc.info.lock().await, e.map_or_else(|e| e.to_string(), |_| "unknown error".to_string()));
                            } else {
                                host.on_connection_closed(&*cc.info.lock().await, "remote host closed connection".to_string());
                            }
                        } else {
                            host.on_connection_closed(&*cc.info.lock().await, "remote host closed connection".to_string());
                        }
                    }));
                    false // discard connection
                }
            });

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

    /// Incoming TCP acceptor task.
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

    /// Initiate an outgoing connection with a deadline based timeout.
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

    /// Sets up and spawns the task for a new TCP connection whether inbound or outbound.
    async fn connection_start(self: &Arc<Self>, address: SocketAddr, stream: TcpStream, inbound: bool) -> bool {
        let mut ok = false;
        let _ = self.connections.lock().await.entry(address.clone()).or_insert_with(|| {
            ok = true;
            //let _ = stream.set_nodelay(true);
            let (reader, writer) = stream.into_split();
            let now = self.ms_monotonic();
            let connection = Arc::new(Connection {
                writer: Mutex::new(writer),
                last_send_time: AtomicI64::new(now),
                last_receive_time: AtomicI64::new(now),
                bytes_sent: AtomicU64::new(0),
                bytes_received: AtomicU64::new(0),
                info: Mutex::new(RemoteNodeInfo { name: String::new(), contact: String::new(), remote_address: address.clone(), explicit_addresses: Vec::new(), connect_time: ms_since_epoch(), connect_instant: now, inbound, initialized: false }),
                read_task: std::sync::Mutex::new(None),
                closed: AtomicBool::new(false),
            });
            let self2 = self.clone();
            let c2 = connection.clone();
            connection.read_task.lock().unwrap().replace(tokio::spawn(async move {
                let result = self2.connection_io_task_main(&c2, reader).await;
                c2.closed.store(true, Ordering::Relaxed);
                result
            }));
            connection
        });
        ok
    }

    /// Main I/O task launched for each connection.
    ///
    /// This handles reading from the connection and reacting to what it sends. Killing this
    /// task is done when the connection is closed.
    async fn connection_io_task_main(self: Arc<Self>, connection: &Arc<Connection>, mut reader: OwnedReadHalf) -> std::io::Result<()> {
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
            let message = &read_buffer.as_slice()[header_size..total_size];

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
                        let mut info = connection.info.lock().await;
                        info.name = msg.node_name.to_string();
                        info.contact = msg.node_contact.to_string();
                        let _ = msg.explicit_ipv4.map(|pv4| {
                            info.explicit_addresses.push(SocketAddr::V4(SocketAddrV4::new(Ipv4Addr::from(pv4.ip), pv4.port)));
                        });
                        let _ = msg.explicit_ipv6.map(|pv6| {
                            info.explicit_addresses.push(SocketAddr::V6(SocketAddrV6::new(Ipv6Addr::from(pv6.ip), pv6.port, 0, 0)));
                        });

                        let auth_challenge_response = self.host.authenticate(&info, msg.auth_challenge);
                        if auth_challenge_response.is_none() {
                            return Err(std::io::Error::new(std::io::ErrorKind::Other, "authenticate() returned None, connection dropped"));
                        }

                        (H::hmac_sha512(&self.anti_loopback_secret, msg.anti_loopback_challenge), H::hmac_sha512(&H::sha512(&[self.datastore.domain().as_bytes()]), msg.domain_challenge), auth_challenge_response.unwrap())
                    };

                    connection.send_obj(&mut write_buffer, MessageType::InitResponse, &msg::InitResponse { anti_loopback_response: &anti_loopback_response, domain_response: &domain_challenge_response, auth_response: &auth_challenge_response }, now).await?;
                }

                MessageType::InitResponse => {
                    let msg: msg::InitResponse = decode_msgpack(message)?;

                    let mut info = connection.info.lock().await;
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

                    info.initialized = true;
                    initialized = true;

                    let info = info.clone();
                    self.host.on_connect(&info);
                }

                // Handle messages other than INIT and INIT_RESPONSE after checking 'initialized' flag.
                _ => {
                    if !initialized {
                        return Err(std::io::Error::new(std::io::ErrorKind::Other, "init exchange must be completed before other messages are sent"));
                    }

                    match message_type {
                        _ => {}

                        MessageType::HaveRecords => {
                            let msg: msg::HaveRecords = decode_msgpack(message)?;
                        }

                        MessageType::GetRecords => {
                            let msg: msg::GetRecords = decode_msgpack(message)?;
                        }

                        MessageType::Record => {
                            let key = H::sha512(&[message]);
                            match self.datastore.store(&key, message) {
                                StoreResult::Ok => {
                                    // TODO: probably should not announce if way out of sync
                                    let connections = self.connections.lock().await;
                                    let mut announce_to: Vec<Arc<Connection>> = Vec::with_capacity(connections.len());
                                    for (_, c) in connections.iter() {
                                        if !Arc::ptr_eq(&connection, c) {
                                            announce_to.push(c.clone());
                                        }
                                    }
                                    drop(connections); // release lock

                                    background_tasks.spawn(async move {
                                        for c in announce_to.iter() {
                                            let _ = c.send_msg(MessageType::HaveRecord, &key[0..ANNOUNCE_KEY_LEN], now).await;
                                        }
                                    });
                                }
                                StoreResult::Rejected => {
                                    return Err(std::io::Error::new(std::io::ErrorKind::Other, format!("record rejected by data store: {}", to_hex_string(&key))));
                                }
                                _ => {}
                            }
                        }

                        MessageType::SyncStatus => {
                            let msg: msg::SyncStatus = decode_msgpack(message)?;
                        }

                        MessageType::SyncRequest => {
                            let msg: msg::SyncRequest = decode_msgpack(message)?;
                        }

                        MessageType::SyncResponse => {
                            let msg: msg::SyncResponse = decode_msgpack(message)?;
                        }
                    }
                }
            }

            read_buffer.copy_within(total_size..buffer_fill, 0);
            buffer_fill -= total_size;

            connection.bytes_received.fetch_add(total_size as u64, Ordering::Relaxed);
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
    bytes_sent: AtomicU64,
    bytes_received: AtomicU64,
    info: Mutex<RemoteNodeInfo>,
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
            self.bytes_sent.fetch_add((header_size + data.len()) as u64, Ordering::Relaxed);
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
