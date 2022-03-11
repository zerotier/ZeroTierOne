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

use serde::{Deserialize, Serialize};
use tokio::io::{AsyncReadExt, AsyncWriteExt, BufReader};
use tokio::net::{TcpListener, TcpSocket, TcpStream};
use tokio::net::tcp::{OwnedReadHalf, OwnedWriteHalf};
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
            bind_address,
        });

        Ok(Self {
            internal: internal.clone(),
            housekeeping_task: tokio::spawn(internal.clone().housekeeping_task_main()),
            listener_task: tokio::spawn(internal.listener_task_main(listener)),
        })
    }

    pub fn datastore(&self) -> &Arc<D> { &self.internal.datastore }

    pub fn host(&self) -> &Arc<H> { &self.internal.host }

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
    // Secret used to perform HMAC to detect and drop loopback connections to self.
    anti_loopback_secret: [u8; 64],

    // Outside code implementations of DataStore and Host traits.
    datastore: Arc<D>,
    host: Arc<H>,

    // Connections and their task join handles, by remote endpoint address.
    connections: Mutex<HashMap<SocketAddr, (Arc<Connection>, Option<JoinHandle<std::io::Result<()>>>)>>,

    // Local address to which this node is bound
    bind_address: SocketAddr,
}

impl<D: DataStore + 'static, H: Host + 'static> NodeInternal<D, H> {
    /// Loop that constantly runs in the background to do cleanup and service things.
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
                    total_record_count: self.datastore.total_count(),
                    reference_time: self.datastore.clock()
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
                                    // This almost always completes instantly due to queues, but add a timeout in case connection
                                    // is stalled. In this case the result is a closed connection.
                                    if !tokio::time::timeout_at(sleep_until, cc.send_obj(MESSAGE_TYPE_STATUS, &status, now)).await.map_or(false, |r| r.is_ok()) {
                                        let _ = self2.connections.lock().await.remove(&sa).map(|c| c.1.map(|j| j.abort()));
                                        self2.host.on_connection_closed(&*cc.info.lock().await, "write overflow (timeout)".to_string());
                                    }
                                }
                            }));
                        });
                        true // keep connection
                    } else {
                        let _ = c.1.take().map(|j| j.abort());
                        let host = self.host.clone();
                        let cc = cc.clone();
                        tasks.push(tokio::spawn(async move {
                            host.on_connection_closed(&*cc.info.lock().await, "timeout".to_string());
                        }));
                        false // discard connection
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
                    false // discard connection
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
            let desired_connection_count = self.host.desired_connection_count().min(self.host.max_connection_count());
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

    /// Incoming TCP acceptor task.
    async fn listener_task_main(self: Arc<Self>, listener: TcpListener) {
        loop {
            let socket = listener.accept().await;
            if socket.is_ok() {
                let (stream, address) = socket.unwrap();
                if self.host.allow(&address) {
                    if self.connections.lock().await.len() < self.host.max_connection_count() || self.host.fixed_peers().contains(&address) {
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
                    connect_time: ms_since_epoch(),
                    connect_instant: ms_monotonic(),
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

    /// Main I/O task launched for each connection.
    ///
    /// This handles reading from the connection and reacting to what it sends. Killing this
    /// task is done when the connection is closed.
    async fn connection_io_task_main(self: Arc<Self>, connection: &Arc<Connection>, mut reader: BufReader<OwnedReadHalf>) -> std::io::Result<()> {
        let mut anti_loopback_challenge_sent = [0_u8; 64];
        let mut domain_challenge_sent = [0_u8; 64];
        let mut auth_challenge_sent = [0_u8; 64];
        self.host.get_secure_random(&mut anti_loopback_challenge_sent);
        self.host.get_secure_random(&mut domain_challenge_sent);
        self.host.get_secure_random(&mut auth_challenge_sent);
        connection.send_obj(MESSAGE_TYPE_INIT, &msg::Init {
            anti_loopback_challenge: &anti_loopback_challenge_sent,
            domain_challenge: &domain_challenge_sent,
            auth_challenge: &auth_challenge_sent,
            node_name: self.host.name().map(|n| n.to_string()),
            node_contact: self.host.contact().map(|c| c.to_string()),
            locally_bound_port: self.bind_address.port(),
            explicit_ipv4: None,
            explicit_ipv6: None
        }, ms_monotonic()).await?;

        let mut initialized = false;
        let background_tasks = AsyncTaskReaper::new();
        let mut init_received = false;
        let mut buf: Vec<u8> = Vec::new();
        buf.resize(4096, 0);
        loop {
            let message_type = reader.read_u8().await?;
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
                        return Err(std::io::Error::new(std::io::ErrorKind::Other, "duplicate init"));
                    }

                    let msg: msg::Init = connection.read_obj(&mut reader, &mut buf, message_size as usize).await?;
                    let (anti_loopback_response, domain_challenge_response, auth_challenge_response) = {
                        let mut info = connection.info.lock().await;
                        info.node_name = msg.node_name.clone();
                        info.node_contact = msg.node_contact.clone();
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
                        (
                            H::hmac_sha512(&self.anti_loopback_secret, msg.anti_loopback_challenge),
                            H::hmac_sha512(&H::sha512(&[self.datastore.domain().as_bytes()]), msg.domain_challenge),
                            auth_challenge_response.unwrap()
                        )
                    };

                    connection.send_obj(MESSAGE_TYPE_INIT_RESPONSE, &msg::InitResponse {
                        anti_loopback_response: &anti_loopback_response,
                        domain_response: &domain_challenge_response,
                        auth_response: &auth_challenge_response
                    }, now).await?;

                    init_received = true;
                },

                MESSAGE_TYPE_INIT_RESPONSE => {
                    let msg: msg::InitResponse = connection.read_obj(&mut reader, &mut buf, message_size as usize).await?;

                    let mut info = connection.info.lock().await;
                    if info.initialized {
                        return Err(std::io::Error::new(std::io::ErrorKind::Other, "duplicate init response"));
                    }
                    info.initialized = true;
                    let info = info.clone();

                    if msg.anti_loopback_response.eq(&H::hmac_sha512(&self.anti_loopback_secret, &anti_loopback_challenge_sent)) {
                        return Err(std::io::Error::new(std::io::ErrorKind::Other, "rejected connection to self"));
                    }
                    if msg.domain_response.eq(&H::hmac_sha512(&H::sha512(&[self.datastore.domain().as_bytes()]), &domain_challenge_sent)) {
                        return Err(std::io::Error::new(std::io::ErrorKind::Other, "domain mismatch"));
                    }
                    if !self.host.authenticate(&info, &auth_challenge_sent).map_or(false, |cr| msg.auth_response.eq(&cr)) {
                        return Err(std::io::Error::new(std::io::ErrorKind::Other, "challenge/response authentication failed"));
                    }

                    self.host.on_connect(&info);
                    initialized = true;
                },

                _ => {
                    if !initialized {
                        return Err(std::io::Error::new(std::io::ErrorKind::Other, "init exchange must be completed before other messages are sent"));
                    }

                    match message_type {

                        MESSAGE_TYPE_STATUS => {
                            let msg: msg::Status = connection.read_obj(&mut reader, &mut buf, message_size as usize).await?;
                            self.connection_request_summary(connection, msg.total_record_count, now, msg.reference_time).await?;
                        },

                        MESSAGE_TYPE_GET_SUMMARY => {
                            //let msg: msg::GetSummary = connection.read_obj(&mut reader, &mut buf, message_size as usize).await?;
                        },

                        MESSAGE_TYPE_SUMMARY => {
                            let mut remaining = message_size as isize;

                            // Read summary header.
                            let summary_header_size = varint::read_async(&mut reader).await?;
                            remaining -= summary_header_size.1 as isize;
                            let summary_header_size = summary_header_size.0;
                            if (summary_header_size as i64) > (remaining as i64) {
                                return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid summary header"));
                            }
                            let summary_header: msg::SummaryHeader = connection.read_obj(&mut reader, &mut buf, summary_header_size as usize).await?;
                            remaining -= summary_header_size as isize;

                            // Read and evaluate summary that we were sent.
                            match summary_header.summary_type {
                                SUMMARY_TYPE_KEYS => {
                                    self.connection_receive_and_process_remote_hash_list(
                                        connection,
                                        remaining,
                                        &mut reader,
                                        now,
                                        summary_header.reference_time,
                                        &summary_header.prefix[0..summary_header.prefix.len().min((summary_header.prefix_bits / 8) as usize)]).await?;
                                },
                                SUMMARY_TYPE_IBLT => {
                                    //let summary = IBLT::new_from_reader(&mut reader, remaining as usize).await?;
                                },
                                _ => {} // ignore unknown summary types
                            }

                            // Request another summary if needed, keeping a ping-pong game going in a tight loop until synced.
                            self.connection_request_summary(connection, summary_header.total_record_count, now, summary_header.reference_time).await?;
                        },

                        MESSAGE_TYPE_HAVE_RECORDS => {
                            let mut remaining = message_size as isize;
                            let reference_time = varint::read_async(&mut reader).await?;
                            remaining -= reference_time.1 as isize;
                            if remaining <= 0 {
                                return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid message"));
                            }
                            self.connection_receive_and_process_remote_hash_list(connection, remaining, &mut reader, now, reference_time.0 as i64, &[]).await?
                        },

                        MESSAGE_TYPE_GET_RECORDS => {
                        },

                        MESSAGE_TYPE_RECORD => {
                            let value = connection.read_msg(&mut reader, &mut buf, message_size as usize).await?;
                            if value.len() > D::MAX_VALUE_SIZE {
                                return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "value larger than MAX_VALUE_SIZE"));
                            }
                            let key = H::sha512(&[value]);
                            match self.datastore.store(&key, value) {
                                StoreResult::Ok(reference_time) => {
                                    let mut have_records_msg = [0_u8; 2 + 10 + ANNOUNCE_HASH_BYTES];
                                    let mut msg_len = varint::encode(&mut have_records_msg, reference_time as u64);
                                    have_records_msg[msg_len] = ANNOUNCE_HASH_BYTES as u8;
                                    msg_len += 1;
                                    have_records_msg[msg_len..(msg_len + ANNOUNCE_HASH_BYTES)].copy_from_slice(&key[..ANNOUNCE_HASH_BYTES]);
                                    msg_len += ANNOUNCE_HASH_BYTES;

                                    let self2 = self.clone();
                                    let connection2 = connection.clone();
                                    background_tasks.spawn(async move {
                                        let connections = self2.connections.lock().await;
                                        let mut recipients = Vec::with_capacity(connections.len());
                                        for (_, c) in connections.iter() {
                                            if !Arc::ptr_eq(&(c.0), &connection2) {
                                                recipients.push(c.0.clone());
                                            }
                                        }
                                        drop(connections); // release lock

                                        for c in recipients.iter() {
                                            // This typically completes instantly due to buffering, as this message is small.
                                            // Add a small timeout in the case that some connections are stalled. Misses will
                                            // not impact the overall network much.
                                            let _ = tokio::time::timeout(Duration::from_millis(250), c.send_msg(MESSAGE_TYPE_HAVE_RECORDS, &have_records_msg[..msg_len], now)).await;
                                        }
                                    });
                                },
                                StoreResult::Rejected => {
                                    return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid datum received"));
                                },
                                _ => {} // duplicate or ignored values are just... ignored
                            }
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
                }
            }

            connection.bytes_received.fetch_add((header_size as u64) + message_size, Ordering::Relaxed);
        }
    }

    /// Request a summary if needed, or do nothing if not.
    ///
    /// This is where all the logic lives that determines whether to request summaries, choosing a
    /// prefix, etc. It's called when the remote node tells us its total record count with an
    /// associated reference time, which happens in status announcements and in summaries.
    async fn connection_request_summary(&self, connection: &Arc<Connection>, total_record_count: u64, now: i64, reference_time: i64) -> std::io::Result<()> {
        let my_total_record_count = self.datastore.total_count();
        if my_total_record_count < total_record_count {
            // Figure out how many bits need to be in a randomly chosen prefix to choose a slice of
            // the data set such that the set difference should be around 4096 records. This assumes
            // random distribution, which should be mostly maintained by probing prefixes at random.
            let prefix_bits = ((total_record_count - my_total_record_count) as f64) / 4096.0;
            let prefix_bits = if prefix_bits > 1.0 {
                (prefix_bits.log2().ceil() as usize).min(64)
            } else {
                0 as usize
            };
            let prefix_bytes = (prefix_bits / 8) + (((prefix_bits % 8) != 0) as usize);

            // Generate a random prefix of this many bits (to the nearest byte).
            let mut prefix = [0_u8; 64];
            self.host.get_secure_random(&mut prefix[..prefix_bytes]);

            // Request a set summary for this prefix, providing our own count for this prefix so
            // the remote can decide whether to send something like an IBLT or just hashes.
            let (local_range_start, local_range_end) = range_from_prefix(&prefix, prefix_bits);
            connection.send_obj(MESSAGE_TYPE_GET_SUMMARY, &msg::GetSummary {
                reference_time,
                prefix: &prefix[..prefix_bytes],
                prefix_bits: prefix_bits as u8,
                record_count: self.datastore.count(reference_time, &local_range_start, &local_range_end)
            }, now).await
        } else {
            Ok(())
        }
    }

    /// Read a stream of record hashes (or hash prefixes) from a connection and request records we don't have.
    async fn connection_receive_and_process_remote_hash_list(&self, connection: &Arc<Connection>, mut remaining: isize, reader: &mut BufReader<OwnedReadHalf>, now: i64, reference_time: i64, common_prefix: &[u8]) -> std::io::Result<()> {
        if remaining > 0 {
            // Hash list is prefaced by the number of bytes in each hash, since whole 64 byte hashes do not have to be sent.
            let prefix_entry_size = reader.read_u8().await? as usize;
            let total_prefix_size = common_prefix.len() + prefix_entry_size;

            if prefix_entry_size > 0 && total_prefix_size <= 64 {
                remaining -= 1;
                if remaining >= (prefix_entry_size as isize) {
                    let mut get_records_msg: Vec<u8> = Vec::with_capacity(((remaining as usize) / prefix_entry_size) * total_prefix_size);
                    varint::write(&mut get_records_msg, reference_time as u64)?;
                    get_records_msg.push(total_prefix_size as u8);

                    let mut key_prefix_buf = [0_u8; 64];
                    key_prefix_buf[..common_prefix.len()].copy_from_slice(common_prefix);

                    while remaining >= (prefix_entry_size as isize) {
                        remaining -= prefix_entry_size as isize;
                        reader.read_exact(&mut key_prefix_buf[common_prefix.len()..total_prefix_size]).await?;

                        if if total_prefix_size < 64 {
                            let (s, e) = range_from_prefix(&key_prefix_buf[..total_prefix_size], total_prefix_size * 8);
                            self.datastore.count(reference_time, &s, &e) == 0
                        } else {
                            !self.datastore.contains(reference_time, &key_prefix_buf)
                        } {
                            let _ = get_records_msg.write_all(&key_prefix_buf[..total_prefix_size]);
                        }
                    }

                    if remaining == 0 {
                        return connection.send_msg(MESSAGE_TYPE_GET_RECORDS, get_records_msg.as_slice(), now).await;
                    }
                }
            }
        }
        return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid hash list"));
    }
}

impl<D: DataStore + 'static, H: Host + 'static> Drop for NodeInternal<D, H> {
    fn drop(&mut self) {
        let _ = tokio::runtime::Handle::try_current().map_or_else(|_| {
            for (_, c) in self.connections.blocking_lock().drain() {
                c.1.map(|c| c.abort());
            }
        }, |h| {
            let _ = h.block_on(async {
                for (_, c) in self.connections.lock().await.drain() {
                    c.1.map(|c| c.abort());
                }
            });
        });
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
    async fn send_msg(&self, message_type: u8, data: &[u8], now: i64) -> std::io::Result<()> {
        let mut type_and_size = [0_u8; 16];
        type_and_size[0] = message_type;
        let tslen = 1 + varint::encode(&mut type_and_size[1..], data.len() as u64) as usize;
        let total_size = tslen + data.len();
        if self.writer.lock().await.write_vectored(&[IoSlice::new(&type_and_size[..tslen]), IoSlice::new(data)]).await? == total_size {
            self.last_send_time.store(now, Ordering::Relaxed);
            self.bytes_sent.fetch_add(total_size as u64, Ordering::Relaxed);
            Ok(())
        } else {
            Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "write error"))
        }
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
