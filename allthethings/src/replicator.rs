use std::collections::HashMap;
use std::convert::TryInto;
use std::future::Future;
use std::hash::{Hash, Hasher};
use std::io::Read;
use std::mem::{size_of, transmute};
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, AtomicU64, AtomicUsize, Ordering};
use std::time::Duration;

use getrandom::getrandom;
use sha2::{Digest, Sha256};
use sha2::digest::{FixedOutput, Reset, Update};
use smol::{Executor, Task, Timer};
use smol::future;
use smol::io::{AsyncBufReadExt, AsyncReadExt, AsyncWriteExt, BufReader};
use smol::lock::Mutex;
use smol::net::{IpAddr, Ipv4Addr, Ipv6Addr, SocketAddrV4, SocketAddrV6, TcpListener, TcpStream, SocketAddr};
use smol::stream::StreamExt;

use crate::{ms_monotonic, ms_since_epoch, protocol};
use crate::store::Store;
use crate::varint;

const CONNECTION_TIMEOUT_SECONDS: u64 = 30;

pub struct Config {
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
        state.write(&self.instance_id);
        self.ip.hash(state);
    }
}

struct ReplicatorImpl<S: Store> {
    instance_id: [u8; 16],
    loopback_check_code_secret: [u8; 16],
    domain_hash: [u8; 32],
    store: Arc<S>,
    config: Config,
    connections: Mutex<HashMap<ConnectionKey, (SocketAddr, Task<()>)>>,
}

pub struct Replicator<S: Store> {
    state: Arc<ReplicatorImpl<S>>,
    v4_listener_task: Option<Task<()>>,
    v6_listener_task: Option<Task<()>>,
    service_task: Task<()>,
}

impl<S: Store> Replicator<S> {
    /// Create a new replicator to replicate the contents of the provided store.
    /// All async tasks, sockets, and connections will be dropped if the replicator is dropped. Use
    /// the shutdown() method for a graceful shutdown.
    pub async fn start(executor: Arc<Executor>, store: Arc<S>, config: Config) -> Result<Self, Box<dyn Err>> {
        let listener_v4 = TcpListener::bind(SocketAddrV4::new(Ipv4Addr::UNSPECIFIED, config.tcp_port)).await;
        let listener_v6 = TcpListener::bind(SocketAddrV6::new(Ipv6Addr::UNSPECIFIED, config.tcp_port, 0, 0)).await;
        if listener_v4.is_err() && listener_v6.is_err() {
            return Err(Box::new(listener_v4.unwrap_err()));
        }

        let r = Arc::new(ReplicatorImpl::<S> {
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
                let mut h = Sha256::new();
                h.update(config.domain.as_bytes());
                h.finalize_fixed().into()
            },
            config,
            store,
            connections: Mutex::new(HashMap::new()),
        });

        let (e0, e1) = (executor.clone(), executor.clone());
        Ok(Self {
            state: r,
            v4_listener_task: listener_v4.map_or(None, |listener_v4| Some(executor.spawn(r.listener_task_main(listener_v4, e0)))),
            v6_listener_task: listener_v6.map_or(None, |listener_v6| Some(executor.spawn(r.listener_task_main(listener_v6, e1)))),
            service_task: executor.spawn(r.service_main(executor.clone())),
        })
    }

    pub async fn shutdown(self) {
        // Get a joined future including our service task and one or both listeners. There is always
        // at least one listener. If there are no listeners this is a bug and will panic.
        let main_tasks = self.v4_listener_task.map_or_else(|| {
            future::zip(self.service_task.cancel(), self.v6_listener_task.unwrap().cancel())
        }, |v4| {
            self.v6_listener_task.map_or_else(|| {
                future::zip(self.service_task.cancel(), v4.cancel())
            }, |v6| {
                future::zip(self.service_task.cancel(), future::zip(v4.cancel(), v6.cancel()))
            })
        });

        // Just dropping all connections is fine.
        self.state.connections.lock().await.clear();

        // Then gracefully wait for the main tasks to finish.
        let _ = main_tasks.await;
    }
}

impl<S: Store> ReplicatorImpl<S> {
    async fn service_main(&self, executor: Arc<Executor>) {
        let mut timer = smol::Timer::interval(Duration::from_secs(1));
        let mut to_close: Vec<ConnectionKey> = Vec::new();
        loop {
            timer.next().await;

            let mut connections = self.connections.lock().await;

            let now_mt = ms_monotonic();
            for cc in connections.iter_mut() {
                let c = &(*cc.1).0;
                if c.closed.load(Ordering::Relaxed) || (now_mt - c.last_receive_time.load(Ordering::Relaxed)) > CONNECTION_TIMEOUT_SECONDS {
                    to_close.push(cc.0.clone());
                }
            }

            for tc in to_close.iter() {
                let _ = connections.remove(tc);
            }
            to_close.clear();

            drop(connections);
        }
    }

    async fn listener_task_main(&self, listener: TcpListener, executor: Arc<Executor>) {
        loop {
            let stream = listener.accept().await;
            if stream.is_ok() {
                let (mut stream, remote_address) = stream.unwrap();
                self.handle_new_connection(stream, remote_address, false, executor.clone()).await;
            }
        }
    }

    async fn handle_new_connection(&self, mut stream: TcpStream, remote_address: SocketAddr, outgoing: bool, executor: Arc<Executor>) {
        stream.set_nodelay(true);

        let mut loopback_check_code_salt = [0_u8; 8];
        getrandom(&mut tmp).expect("getrandom failed");

        let mut h = Sha256::new();
        h.update(&loopback_check_code_salt);
        h.update(&self.loopback_check_code_secret);
        let loopback_check_code: [u8; 32] = h.finalize_fixed().into();

        let hello = protocol::Hello {
            hello_size: size_of::<protocol::Hello>() as u8,
            protocol_version: protocol::PROTOCOL_VERSION,
            flags: [0_u8; 4],
            clock: ms_since_epoch().to_le_bytes(),
            data_set_size: self.store.total_size().await.to_le_bytes(),
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
                    let mut h = Sha256::new();
                    h.update(&hello.loopback_check_code_salt);
                    h.update(&self.loopback_check_code_secret);
                    let loopback_if_equal: [u8; 32] = h.finalize_fixed().into();

                    if !loopback_if_equal[0..16].eq(&hello.loopback_check_code) {
                        let k = ConnectionKey {
                            instance_id: hello.instance_id.clone(),
                            ip: remote_address.ip()
                        };
                        let mut connections = self.connections.lock().await;
                        let _ = connections.entry(k).or_insert_with(move || {
                            stream.set_nodelay(false);
                            (remote_address.clone(), executor.spawn(self.connection_io_task_main(stream, remote_address, false, executor.clone())))
                        });
                    }
                }
            }
        }
    }

    async fn connection_sync_init_task_main(&self, writer: Arc<Mutex<TcpStream>>) {
        let mut periodic_timer = Timer::interval(Duration::from_secs(1));
        loop {
            let _ = periodic_timer.next().await;
        }
    }

    async fn connection_io_task_main(&self, mut stream: TcpStream, remote_address: SocketAddr, outgoing: bool, executor: Arc<Executor>) {
        let mut reader = BufReader::with_capacity(S::MAX_OBJECT_SIZE * 2, stream.clone());
        let writer = Arc::new(Mutex::new(stream));

        let _sync_search_init_task = executor.spawn(self.connection_sync_init_task_main(writer.clone()));

        let mut tmp = [0_u8; 4096];
        'main_io_loop: loop {
            if reader.read_exact(&mut tmp[0..1]).await.is_err() {
                break 'main_io_loop;
            }
            let message_type = tmp[0];

            if message_type == protocol::MESSAGE_TYPE_NOP {
                continue 'main_io_loop;
            }

            let message_size = varint::async_read(&mut reader).await;
            if message_size.is_err() {
                break 'main_io_loop;
            }
            let mut message_size = message_size.unwrap();

            if message_size > S::MAX_OBJECT_SIZE as u64 {
                break 'main_io_loop;
            }

            match message_type {
                protocol::MESSAGE_TYPE_OBJECT => {
                },
                protocol::MESSAGE_TYPE_GET_OBJECTS => {
                },
                _ => {
                    // Skip the bodies of unrecognized message types.
                    while message_size >= tmp.len() as u64 {
                        if reader.read_exact(&tmp).await.is_err() {
                            break 'main_io_loop;
                        }
                        message_size -= tmp.len() as u64;
                    }
                    if message_size > 0 {
                        if reader.read_exact(&mut tmp[0..(message_size as usize)]).await.is_err() {
                            break 'main_io_loop;
                        }
                    }
                }
            }
        }
    }
}
