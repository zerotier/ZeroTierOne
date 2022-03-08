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
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, AtomicI64, AtomicU64, Ordering};
use std::time::SystemTime;

use serde::{Deserialize, Serialize};
use tokio::io::{AsyncReadExt, AsyncWriteExt, BufReader};
use tokio::net::{TcpListener, TcpSocket, TcpStream};
use tokio::net::tcp::{OwnedReadHalf, OwnedWriteHalf};
use tokio::sync::Mutex;
use tokio::task::JoinHandle;
use tokio::time::{Instant, Duration};

use crate::datastore::DataStore;
use crate::host::Host;
use crate::ms_monotonic;
use crate::protocol::*;
use crate::varint;

/// Inactivity timeout for connections in milliseconds.
const CONNECTION_TIMEOUT: i64 = 120000;

/// How often to send STATUS messages in milliseconds.
const STATUS_INTERVAL: i64 = 10000;

/// How often to run the housekeeping task's loop in milliseconds.
const HOUSEKEEPING_INTERVAL: i64 = STATUS_INTERVAL / 2;

/// Size of read buffer, which is used to reduce the number of syscalls.
const READ_BUFFER_SIZE: usize = 16384;

/// Information about a remote node to which we are connected.
#[derive(Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct RemoteNodeInfo {
    /// Optional name advertised by remote node (arbitrary).
    pub node_name: Option<String>,

    /// Optional contact information advertised by remote node (arbitrary).
    pub node_contact: Option<String>,

    /// Actual remote endpoint address.
    pub remote_address: SocketAddr,

    /// Explicitly advertised remote addresses supplied by remote node (not necessarily verified).
    pub explicit_addresses: Vec<SocketAddr>,

    /// Time TCP connection was established.
    pub connect_time: SystemTime,

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

/// An instance of the syncwhole data set synchronization engine.
///
/// This holds a number of async tasks that are terminated or aborted if this object
/// is dropped. In other words this implements structured concurrency.
pub struct Node<D: DataStore + 'static, H: Host + 'static> {
    internal: Arc<NodeInternal<D, H>>,
    housekeeping_task: JoinHandle<()>,
    listener_task: JoinHandle<()>
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
            bind_address
        });

        Ok(Self {
            internal: internal.clone(),
            housekeeping_task: tokio::spawn(internal.clone().housekeeping_task_main()),
            listener_task: tokio::spawn(internal.listener_task_main(listener)),
        })
    }

    pub fn datastore(&self) -> &Arc<D> { &self.internal.datastore }

    pub fn host(&self) -> &Arc<H> { &self.internal.host }

    #[inline(always)]
    pub async fn connect(&self, endpoint: &SocketAddr) -> std::io::Result<bool> {
        self.internal.clone().connect(endpoint, Instant::now().add(Duration::from_millis(CONNECTION_TIMEOUT as u64))).await
    }

    pub async fn list_connections(&self) -> Vec<RemoteNodeInfo> {
        let connections = self.internal.connections.lock().await;
        let mut cl: Vec<RemoteNodeInfo> = Vec::with_capacity(connections.len());
        for (_, c) in connections.iter() {
            cl.push(c.0.info.blocking_lock().clone());
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
    anti_loopback_secret: [u8; 64],
    datastore: Arc<D>,
    host: Arc<H>,
    connections: Mutex<HashMap<SocketAddr, (Arc<Connection>, Option<JoinHandle<std::io::Result<()>>>)>>,
    bind_address: SocketAddr,
}

impl<D: DataStore + 'static, H: Host + 'static> NodeInternal<D, H> {
    async fn housekeeping_task_main(self: Arc<Self>) {
        let mut last_status_sent = ms_monotonic();
        let mut tasks: Vec<JoinHandle<()>> = Vec::new();
        let mut connected_to_addresses: HashSet<SocketAddr> = HashSet::new();
        let mut sleep_until = Instant::now().add(Duration::from_millis(500));
        loop {
            tokio::time::sleep_until(sleep_until).await;
            sleep_until = sleep_until.add(Duration::from_millis(HOUSEKEEPING_INTERVAL as u64));

            tasks.clear();
            connected_to_addresses.clear();
            let now = ms_monotonic();

            // Check connection timeouts, send status updates, and garbage collect from the connections map.
            // Status message outputs are backgrounded since these can block of TCP buffers are nearly full.
            // A timeout based on the service loop interval is used. Usually these sends will finish instantly
            // but if they take too long this typically means the link is dead. We wait for all tasks at the
            // end of the service loop. The on_connection_closed() method in 'host' is called in sub-tasks to
            // prevent the possibility of deadlocks on self.connections.lock() if the Host implementation calls
            // something that tries to lock it.
            let status = if (now - last_status_sent) >= STATUS_INTERVAL {
                last_status_sent = now;
                Some(msg::Status {
                    record_count: self.datastore.total_count(),
                    clock: self.datastore.clock() as u64
                })
            } else {
                None
            };
            self.connections.lock().await.retain(|sa, c| {
                let cc = &(c.0);
                if !cc.closed.load(Ordering::Relaxed) {
                    if (now - cc.last_receive_time.load(Ordering::Relaxed)) < CONNECTION_TIMEOUT {
                        connected_to_addresses.insert(sa.clone());
                        let _ = status.as_ref().map(|status| {
                            let status = status.clone();
                            let self2 = self.clone();
                            let cc = cc.clone();
                            let sa = sa.clone();
                            tasks.push(tokio::spawn(async move {
                                if cc.info.lock().await.initialized {
                                    if !tokio::time::timeout_at(sleep_until, cc.send_obj(MESSAGE_TYPE_STATUS, &status, now)).await.map_or(false, |r| r.is_ok()) {
                                        let _ = self2.connections.lock().await.remove(&sa).map(|c| c.1.map(|j| j.abort()));
                                        self2.host.on_connection_closed(&*cc.info.lock().await, "write overflow (timeout)".to_string());
                                    }
                                }
                            }));
                        });
                        true
                    } else {
                        let _ = c.1.take().map(|j| j.abort());
                        let host = self.host.clone();
                        let cc = cc.clone();
                        tasks.push(tokio::spawn(async move {
                            host.on_connection_closed(&*cc.info.lock().await, "timeout".to_string());
                        }));
                        false
                    }
                } else {
                    let host = self.host.clone();
                    let cc = cc.clone();
                    let j = c.1.take();
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
                    false
                }
            });

            // Always try to connect to fixed peers.
            let fixed_peers = self.host.fixed_peers();
            for sa in fixed_peers.iter() {
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
            let desired_connection_count = self.host.desired_connection_count();
            while connected_to_addresses.len() < desired_connection_count {
                let sa = self.host.another_peer(&connected_to_addresses);
                if sa.is_some() {
                    let sa = sa.unwrap();
                    let self2 = self.clone();
                    tasks.push(tokio::spawn(async move {
                        let _ = self2.connect(&sa, sleep_until).await;
                    }));
                    connected_to_addresses.insert(sa.clone());
                } else {
                    break;
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

    async fn listener_task_main(self: Arc<Self>, listener: TcpListener) {
        loop {
            let socket = listener.accept().await;
            if socket.is_ok() {
                let (stream, endpoint) = socket.unwrap();
                let num_conn = self.connections.lock().await.len();
                if num_conn < self.host.max_connection_count() || self.host.fixed_peers().contains(&endpoint) {
                    Self::connection_start(&self, endpoint, stream, true).await;
                }
            }
        }
    }

    #[inline(always)]
    async fn connection_io_task_main(self: Arc<Self>, connection: &Arc<Connection>, mut reader: BufReader<OwnedReadHalf>) -> std::io::Result<()> {
        let mut buf: Vec<u8> = Vec::new();
        buf.resize(4096, 0);

        let mut anti_loopback_challenge_sent = [0_u8; 64];
        let mut challenge_sent = [0_u8; 64];
        self.host.get_secure_random(&mut anti_loopback_challenge_sent);
        self.host.get_secure_random(&mut challenge_sent);
        connection.send_obj(MESSAGE_TYPE_INIT, &msg::Init {
            anti_loopback_challenge: &anti_loopback_challenge_sent,
            challenge: &challenge_sent,
            domain: self.datastore.domain().to_string(),
            key_size: D::KEY_SIZE as u16,
            max_value_size: D::MAX_VALUE_SIZE as u64,
            node_name: self.host.name().map(|n| n.to_string()),
            node_contact: self.host.contact().map(|c| c.to_string()),
            locally_bound_port: self.bind_address.port(),
            explicit_ipv4: None,
            explicit_ipv6: None
        }, ms_monotonic()).await?;

        let mut init_received = false;
        loop {
            reader.read_exact(&mut buf.as_mut_slice()[0..1]).await?;
            let message_type = unsafe { *buf.get_unchecked(0) };
            let message_size = varint::read_async(&mut reader).await?;
            let header_size = 1 + message_size.1;
            let message_size = message_size.0;
            if message_size > (D::MAX_VALUE_SIZE + ((D::KEY_SIZE + 10) * 256) + 65536) as u64 {
                return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "message too large"));
            }

            let now = ms_monotonic();
            connection.last_receive_time.store(now, Ordering::Relaxed);

            match message_type {

                MESSAGE_TYPE_INIT => {
                    if init_received {
                        return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "duplicate init"));
                    }

                    let msg: msg::Init = connection.read_obj(&mut reader, &mut buf, message_size as usize).await?;

                    if !msg.domain.as_str().eq(self.datastore.domain()) {
                        return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, format!("data set domain mismatch: '{}' != '{}'", msg.domain, self.datastore.domain())));
                    }
                    if msg.key_size != D::KEY_SIZE as u16 || msg.max_value_size > D::MAX_VALUE_SIZE as u64 {
                        return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "data set key/value sizing mismatch"));
                    }

                    let (anti_loopback_response, challenge_response) = {
                        let mut info = connection.info.lock().await;
                        info.node_name = msg.node_name.clone();
                        info.node_contact = msg.node_contact.clone();
                        let _ = msg.explicit_ipv4.map(|pv4| {
                            info.explicit_addresses.push(SocketAddr::V4(SocketAddrV4::new(Ipv4Addr::from(pv4.ip), pv4.port)));
                        });
                        let _ = msg.explicit_ipv6.map(|pv6| {
                            info.explicit_addresses.push(SocketAddr::V6(SocketAddrV6::new(Ipv6Addr::from(pv6.ip), pv6.port, 0, 0)));
                        });

                        let challenge_response = self.host.authenticate(&info, msg.challenge);
                        if challenge_response.is_none() {
                            return Err(std::io::Error::new(std::io::ErrorKind::Other, "authenticate() returned None, connection dropped"));
                        }
                        (H::hmac_sha512(&self.anti_loopback_secret, msg.anti_loopback_challenge), challenge_response.unwrap())
                    };

                    connection.send_obj(MESSAGE_TYPE_INIT_RESPONSE, &msg::InitResponse {
                        anti_loopback_response: &anti_loopback_response,
                        challenge_response: &challenge_response
                    }, now).await?;

                    init_received = true;
                },

                MESSAGE_TYPE_INIT_RESPONSE => {
                    let msg: msg::InitResponse = connection.read_obj(&mut reader, &mut buf, message_size as usize).await?;

                    let mut info = connection.info.lock().await;
                    if info.initialized {
                        return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "duplicate init response"));
                    }
                    info.initialized = true;
                    let info = info.clone();

                    if msg.anti_loopback_response.eq(&H::hmac_sha512(&self.anti_loopback_secret, &anti_loopback_challenge_sent)) {
                        return Err(std::io::Error::new(std::io::ErrorKind::Other, "rejected connection to self"));
                    }
                    if !self.host.authenticate(&info, &challenge_sent).map_or(false, |cr| msg.challenge_response.eq(&cr)) {
                        return Err(std::io::Error::new(std::io::ErrorKind::Other, "challenge/response authentication failed"));
                    }

                    self.host.on_connect(&info);
                },

                _ => {
                    // Skip messages that aren't recognized or don't need to be parsed.
                    let mut remaining = message_size as usize;
                    while remaining > 0 {
                        let s = remaining.min(buf.len());
                        reader.read_exact(&mut buf.as_mut_slice()[0..s]).await?;
                        remaining -= s;
                    }
                }

            }

            connection.bytes_received.fetch_add((header_size as u64) + message_size, Ordering::Relaxed);
        }
    }

    async fn connection_start(self: &Arc<Self>, address: SocketAddr, stream: TcpStream, inbound: bool) -> bool {
        let mut ok = false;
        let _ = self.connections.lock().await.entry(address.clone()).or_insert_with(|| {
            ok = true;
            let _ = stream.set_nodelay(true);
            let (reader, writer) = stream.into_split();
            let now = ms_monotonic();
            let connection = Arc::new(Connection {
                writer: Mutex::new(writer),
                last_send_time: AtomicI64::new(now),
                last_receive_time: AtomicI64::new(now),
                bytes_sent: AtomicU64::new(0),
                bytes_received: AtomicU64::new(0),
                info: Mutex::new(RemoteNodeInfo {
                    node_name: None,
                    node_contact: None,
                    remote_address: address.clone(),
                    explicit_addresses: Vec::new(),
                    connect_time: SystemTime::now(),
                    inbound,
                    initialized: false
                }),
                closed: AtomicBool::new(false)
            });
            let self2 = self.clone();
            (connection.clone(), Some(tokio::spawn(async move {
                let result = self2.connection_io_task_main(&connection, BufReader::with_capacity(READ_BUFFER_SIZE, reader)).await;
                connection.closed.store(true, Ordering::Relaxed);
                result
            })))
        });
        ok
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
}

impl<D: DataStore + 'static, H: Host + 'static> Drop for NodeInternal<D, H> {
    fn drop(&mut self) {
        for (_, c) in self.connections.blocking_lock().drain() {
            c.1.map(|c| c.abort());
        }
    }
}

struct Connection {
    writer: Mutex<OwnedWriteHalf>,
    last_send_time: AtomicI64,
    last_receive_time: AtomicI64,
    bytes_sent: AtomicU64,
    bytes_received: AtomicU64,
    info: Mutex<RemoteNodeInfo>,
    closed: AtomicBool,
}

impl Connection {
    async fn send(&self, data: &[u8], now: i64) -> std::io::Result<()> {
        self.writer.lock().await.write_all(data).await?;
        self.last_send_time.store(now, Ordering::Relaxed);
        self.bytes_sent.fetch_add(data.len() as u64, Ordering::Relaxed);
        Ok(())
    }

    async fn send_obj<O: Serialize>(&self, message_type: u8, obj: &O, now: i64) -> std::io::Result<()> {
        let data = rmp_serde::encode::to_vec_named(&obj);
        if data.is_ok() {
            let data = data.unwrap();
            let mut header: [u8; 16] = unsafe { MaybeUninit::uninit().assume_init() };
            header[0] = message_type;
            let header_size = 1 + varint::encode(&mut header[1..], data.len() as u64);
            if self.writer.lock().await.write_vectored(&[IoSlice::new(&header[0..header_size]), IoSlice::new(data.as_slice())]).await? == (data.len() + header_size) {
                self.last_send_time.store(now, Ordering::Relaxed);
                self.bytes_sent.fetch_add((header_size + data.len()) as u64, Ordering::Relaxed);
                Ok(())
            } else {
                Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "write error"))
            }
        } else {
            Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "serialize failure (internal error)"))
        }
    }

    async fn read_msg<'a, R: AsyncReadExt + Unpin>(&self, reader: &mut R, buf: &'a mut Vec<u8>, message_size: usize) -> std::io::Result<&'a [u8]> {
        if message_size > buf.len() {
            buf.resize(((message_size / 4096) + 1) * 4096, 0);
        }
        let b = &mut buf.as_mut_slice()[0..message_size];
        reader.read_exact(b).await?;
        Ok(b)
    }

    async fn read_obj<'a, R: AsyncReadExt + Unpin, O: Deserialize<'a>>(&self, reader: &mut R, buf: &'a mut Vec<u8>, message_size: usize) -> std::io::Result<O> {
        rmp_serde::from_read_ref(self.read_msg(reader, buf, message_size).await?).map_err(|e| std::io::Error::new(std::io::ErrorKind::InvalidData, format!("invalid msgpack: {}", e.to_string())))
    }
}
