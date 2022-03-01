/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2022 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::collections::HashMap;
use std::net::{Ipv4Addr, Ipv6Addr, SocketAddr, SocketAddrV4, SocketAddrV6};
use std::sync::{Arc, Weak};
use std::sync::atomic::{AtomicI64, Ordering};
use std::time::{Duration, SystemTime};

use serde::{Deserialize, Serialize};
use tokio::io::{AsyncReadExt, AsyncWriteExt, BufReader, BufWriter};
use tokio::net::{TcpListener, TcpSocket, TcpStream};
use tokio::net::tcp::{OwnedReadHalf, OwnedWriteHalf};
use tokio::sync::Mutex;
use tokio::task::JoinHandle;

use crate::datastore::DataStore;
use crate::host::Host;
use crate::ms_monotonic;
use crate::protocol::*;
use crate::varint;

const CONNECTION_TIMEOUT: i64 = 60000;
const CONNECTION_KEEPALIVE_AFTER: i64 = 20000;
const IO_BUFFER_SIZE: usize = 65536;

#[derive(Clone, PartialEq, Eq)]
pub struct RemoteNodeInfo {
    pub node_name: Option<String>,
    pub node_contact: Option<String>,
    pub endpoint: SocketAddr,
    pub preferred_endpoints: Vec<SocketAddr>,
    pub connect_time: SystemTime,
    pub inbound: bool,
    pub initialized: bool,
}

pub struct Node<D: DataStore + 'static, H: Host + 'static> {
    internal: Arc<NodeInternal<D, H>>,
    housekeeping_task: JoinHandle<()>,
    listener_task: JoinHandle<()>
}

impl<D: DataStore + 'static, H: Host + 'static> Node<D, H> {
    pub async fn new(db: Arc<D>, host: Arc<H>, bind_address: SocketAddr) -> std::io::Result<Self> {
        let listener = if bind_address.is_ipv4() { TcpSocket::new_v4() } else { TcpSocket::new_v6() }?;
        if listener.set_reuseport(true).is_err() {
            listener.set_reuseaddr(true)?;
        }
        listener.bind(bind_address.clone())?;
        let listener = listener.listen(1024)?;

        let internal = Arc::new(NodeInternal::<D, H> {
            anti_loopback_secret: {
                let mut tmp = [0_u8; 16];
                host.get_secure_random(&mut tmp);
                tmp
            },
            db: db.clone(),
            host: host.clone(),
            bind_address,
            connections: Mutex::new(HashMap::with_capacity(64)),
        });

        Ok(Self {
            internal: internal.clone(),
            housekeeping_task: tokio::spawn(internal.clone().housekeeping_task_main()),
            listener_task: tokio::spawn(internal.listener_task_main(listener)),
        })
    }

    #[inline(always)]
    pub async fn connect(&self, endpoint: &SocketAddr) -> std::io::Result<bool> {
        self.internal.connect(endpoint).await
    }

    pub fn list_connections(&self) -> Vec<RemoteNodeInfo> {
        let mut connections = self.internal.connections.blocking_lock();
        let mut cl: Vec<RemoteNodeInfo> = Vec::with_capacity(connections.len());
        connections.retain(|_, c| {
            c.0.upgrade().map_or(false, |c| {
                cl.push(c.info.lock().unwrap().clone());
                true
            })
        });
        cl
    }
}

impl<D: DataStore + 'static, H: Host + 'static> Drop for Node<D, H> {
    fn drop(&mut self) {
        self.housekeeping_task.abort();
        self.listener_task.abort();
    }
}

pub struct NodeInternal<D: DataStore + 'static, H: Host + 'static> {
    anti_loopback_secret: [u8; 16],
    db: Arc<D>,
    host: Arc<H>,
    bind_address: SocketAddr,
    connections: Mutex<HashMap<SocketAddr, (Weak<Connection>, Option<JoinHandle<std::io::Result<()>>>)>>,
}

impl<D: DataStore + 'static, H: Host + 'static> NodeInternal<D, H> {
    async fn housekeeping_task_main(self: Arc<Self>) {
        loop {
            tokio::time::sleep(Duration::from_millis((CONNECTION_KEEPALIVE_AFTER / 2) as u64)).await;

            let mut to_ping: Vec<Arc<Connection>> = Vec::new();
            let mut dead: Vec<(SocketAddr, Option<JoinHandle<std::io::Result<()>>>)> = Vec::new();

            let mut connections = self.connections.lock().await;
            let now = ms_monotonic();
            connections.retain(|sa, c| {
                let cc = c.0.upgrade();
                if cc.is_some() {
                    let cc = cc.unwrap();
                    if (now - cc.last_receive_time.load(Ordering::Relaxed)) < CONNECTION_TIMEOUT {
                        if (now - cc.last_send_time.load(Ordering::Relaxed)) >= CONNECTION_KEEPALIVE_AFTER {
                            to_ping.push(cc);
                        }
                        true
                    } else {
                        c.1.take().map(|j| j.abort());
                        false
                    }
                } else {
                    let _ = c.1.take().map(|j| dead.push((sa.clone(), Some(j))));
                    false
                }
            });
            drop(connections); // release lock

            for d in dead.iter_mut() {
                d.1.take().unwrap().await.map_or_else(|e| {
                    self.host.on_connection_closed(&d.0, Some(Box::new(std::io::Error::new(std::io::ErrorKind::Other, "timed out"))));
                }, |r| {
                    if r.is_ok() {
                        self.host.on_connection_closed(&d.0, None);
                    } else {
                        self.host.on_connection_closed(&d.0, Some(Box::new(r.unwrap_err())));
                    }
                });
            }

            for c in to_ping.iter() {
                let _ = c.send(&[MESSAGE_TYPE_NOP, 0], now).await;
            }
        }
    }

    async fn listener_task_main(self: Arc<Self>, listener: TcpListener) {
        loop {
            let socket = listener.accept().await;
            if self.connections.lock().await.len() < self.host.max_endpoints() && socket.is_ok() {
                let (stream, endpoint) = socket.unwrap();
                Self::connection_start(&self, endpoint, stream, true).await;
            }
        }
    }

    async fn connection_io_task_main(self: Arc<Self>, connection: Arc<Connection>, reader: OwnedReadHalf) -> std::io::Result<()> {
        let mut challenge = [0_u8; 16];
        self.host.get_secure_random(&mut challenge);
        connection.send_obj(MESSAGE_TYPE_INIT, &msg::Init {
            anti_loopback_challenge: &challenge,
            domain: String::new(), // TODO
            key_size: D::KEY_SIZE as u16,
            max_value_size: D::MAX_VALUE_SIZE as u64,
            node_name: None,
            node_contact: None,
            preferred_ipv4: None,
            preferred_ipv6: None
        }, ms_monotonic()).await?;

        let mut init_received = false;
        let mut initialized = false;
        let mut reader = BufReader::with_capacity(IO_BUFFER_SIZE, reader);
        let mut buf: Vec<u8> = Vec::new();
        buf.resize(4096, 0);
        loop {
            reader.read_exact(&mut buf.as_mut_slice()[0..1]).await?;
            let message_type = unsafe { *buf.get_unchecked(0) };
            let message_size = varint::read_async(&mut reader).await?;
            if message_size > (D::MAX_VALUE_SIZE + ((D::KEY_SIZE + 10) * 256) + 65536) as u64 {
                return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "message too large"));
            }
            let now = ms_monotonic();

            match message_type {
                MESSAGE_TYPE_INIT => {
                    if init_received {
                        return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "duplicate init"));
                    }

                    let msg: msg::Init = connection.read_obj(&mut reader, &mut buf, message_size as usize, now).await?;

                    if !msg.domain.as_str().eq(self.db.domain()) {
                        return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "data set domain mismatch"));
                    }
                    if msg.key_size != D::KEY_SIZE as u16 || msg.max_value_size > D::MAX_VALUE_SIZE as u64 {
                        return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "data set key/value sizing mismatch"));
                    }

                    let mut antiloop = msg.anti_loopback_challenge.to_vec();
                    let _ = std::io::Write::write_all(&mut antiloop, &self.anti_loopback_secret);
                    let antiloop = H::sha512(antiloop.as_slice());
                    connection.send_obj(MESSAGE_TYPE_INIT_RESPONSE, &msg::InitResponse {
                        anti_loopback_response: &antiloop[0..16]
                    }, now).await?;

                    init_received = true;

                    let mut info = connection.info.lock().unwrap();
                    info.node_name = msg.node_name.clone();
                    info.node_contact = msg.node_contact.clone();
                    let _ = msg.preferred_ipv4.map(|pv4| {
                        info.preferred_endpoints.push(SocketAddr::V4(SocketAddrV4::new(Ipv4Addr::new(pv4.ip[0], pv4.ip[1], pv4.ip[2], pv4.ip[3]), pv4.port)));
                    });
                    let _ = msg.preferred_ipv6.map(|pv6| {
                        info.preferred_endpoints.push(SocketAddr::V6(SocketAddrV6::new(Ipv6Addr::from(pv6.ip), pv6.port, 0, 0)));
                    });
                },
                MESSAGE_TYPE_INIT_RESPONSE => {
                    if initialized {
                        return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "duplicate init response"));
                    }

                    let msg: msg::InitResponse = connection.read_obj(&mut reader, &mut buf, message_size as usize, now).await?;
                    let mut antiloop = challenge.to_vec();
                    let _ = std::io::Write::write_all(&mut antiloop, &self.anti_loopback_secret);
                    let antiloop = H::sha512(antiloop.as_slice());
                    if msg.anti_loopback_response.eq(&antiloop[0..16]) {
                        return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "rejected connection to self"));
                    }

                    initialized = true;
                    let mut info = connection.info.lock().unwrap();
                    info.initialized = true;
                    let info = info.clone();
                    self.host.on_connect(&info);
                },
                _ => {
                    // Skip messages that aren't recognized or don't need to be parsed like NOP.
                    let mut remaining = message_size as usize;
                    while remaining > 0 {
                        let s = remaining.min(buf.len());
                        reader.read_exact(&mut buf.as_mut_slice()[0..s]).await?;
                        remaining -= s;
                    }
                    connection.last_receive_time.store(ms_monotonic(), Ordering::Relaxed);
                }
            }
        }
    }

    async fn connection_start(self: &Arc<Self>, endpoint: SocketAddr, stream: TcpStream, inbound: bool) -> bool {
        let _ = stream.set_nodelay(true);
        let (reader, writer) = stream.into_split();

        let mut ok = false;
        let _ = self.connections.lock().await.entry(endpoint.clone()).or_insert_with(|| {
            ok = true;
            let now = ms_monotonic();
            let connection = Arc::new(Connection {
                writer: Mutex::new(BufWriter::with_capacity(IO_BUFFER_SIZE, writer)),
                last_send_time: AtomicI64::new(now),
                last_receive_time: AtomicI64::new(now),
                info: std::sync::Mutex::new(RemoteNodeInfo {
                    node_name: None,
                    node_contact: None,
                    endpoint: endpoint.clone(),
                    preferred_endpoints: Vec::new(),
                    connect_time: SystemTime::now(),
                    inbound,
                    initialized: false
                }),
            });
            (Arc::downgrade(&connection), Some(tokio::spawn(self.clone().connection_io_task_main(connection.clone(), reader))))
        });
        ok
    }

    async fn connect(self: &Arc<Self>, endpoint: &SocketAddr) -> std::io::Result<bool> {
        if !self.connections.lock().await.contains_key(endpoint) {
            let stream = if endpoint.is_ipv4() { TcpSocket::new_v4() } else { TcpSocket::new_v6() }?;
            if stream.set_reuseport(true).is_err() {
                stream.set_reuseaddr(true)?;
            }
            stream.bind(self.bind_address.clone())?;
            let stream = stream.connect(endpoint.clone()).await?;
            Ok(self.connection_start(endpoint.clone(), stream, false).await)
        } else {
            Ok(false)
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
    writer: Mutex<BufWriter<OwnedWriteHalf>>,
    last_send_time: AtomicI64,
    last_receive_time: AtomicI64,
    info: std::sync::Mutex<RemoteNodeInfo>,
}

impl Connection {
    async fn send(&self, data: &[u8], now: i64) -> std::io::Result<()> {
        let mut writer = self.writer.lock().await;
        writer.write_all(data).await?;
        writer.flush().await?;
        self.last_send_time.store(now, Ordering::Relaxed);
        Ok(())
    }

    async fn send_obj<O: Serialize>(&self, message_type: u8, obj: &O, now: i64) -> std::io::Result<()> {
        let data = rmp_serde::encode::to_vec_named(&obj);
        if data.is_ok() {
            let data = data.unwrap();
            let mut tmp = [0_u8; 16];
            tmp[0] = message_type;
            let len = 1 + varint::encode(&mut tmp[1..], data.len() as u64);
            let mut writer = self.writer.lock().await;
            writer.write_all(&tmp[0..len]).await?;
            writer.write_all(data.as_slice()).await?;
            writer.flush().await?;
            self.last_send_time.store(now, Ordering::Relaxed);
            Ok(())
        } else {
            Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "serialize failure"))
        }
    }

    async fn read_msg<'a>(&self, reader: &mut BufReader<OwnedReadHalf>, buf: &'a mut Vec<u8>, message_size: usize, now: i64) -> std::io::Result<&'a [u8]> {
        if message_size > buf.len() {
            buf.resize(((message_size / 4096) + 1) * 4096, 0);
        }
        let b = &mut buf.as_mut_slice()[0..message_size];
        reader.read_exact(b).await?;
        self.last_receive_time.store(now, Ordering::Relaxed);
        Ok(b)
    }

    async fn read_obj<'a, O: Deserialize<'a>>(&self, reader: &mut BufReader<OwnedReadHalf>, buf: &'a mut Vec<u8>, message_size: usize, now: i64) -> std::io::Result<O> {
        rmp_serde::from_slice(self.read_msg(reader, buf, message_size, now).await?).map_err(|e| std::io::Error::new(std::io::ErrorKind::InvalidData, e.to_string()))
    }
}
