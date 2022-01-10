/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::collections::{HashMap, HashSet};
use std::error::Error;
use std::sync::{Arc, Weak};
use std::time::Duration;

use smol::{Executor, Task, Timer};
use smol::lock::Mutex;
use smol::net::SocketAddr;

use zerotier_core_crypto::p521::P521KeyPair;

use crate::{Config, Store};
use crate::link::Link;

struct NodeIntl<'e, S: Store + 'static> {
    config: &'e Config,
    secret: &'e P521KeyPair,
    store: &'e S,
    executor: &'e Executor<'e>,
    connections: Mutex<HashMap<SocketAddr, (Weak<Link<'e, S>>, Task<()>)>>
}

pub struct Node<'e, S: Store + 'static> {
    daemon_tasks: Vec<Task<()>>,
    intl: Weak<NodeIntl<'e, S>>
}

impl<'e, S: Store + 'static> Node<'e, S> {
    pub fn new(config: &'e Config, secret: &'e P521KeyPair, store: &'e S, executor: &'e Executor<'e>) -> Result<Self, Box<dyn Error>> {
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

        let ni = Arc::new(NodeIntl {
            config,
            secret,
            store,
            executor,
            connections: Mutex::new(HashMap::with_capacity(64)),
        });

        let mut n = Self {
            daemon_tasks: Vec::with_capacity(3),
            intl: Arc::downgrade(&ni)
        };

        if listener_v4.is_ok() {
            let listener_v4 = listener_v4.unwrap();
            let ni2 = ni.clone();
            n.daemon_tasks.push(executor.spawn(async move { ni2.tcp_listener_main(smol::net::TcpListener::try_from(std::net::TcpListener::from(listener_v4)).unwrap()).await }));
        }
        if listener_v6.is_ok() {
            let listener_v6 = listener_v6.unwrap();
            let ni2 = ni.clone();
            n.daemon_tasks.push(executor.spawn(async move { ni2.tcp_listener_main(smol::net::TcpListener::try_from(std::net::TcpListener::from(listener_v6)).unwrap()).await }));
        }

        let ni2 = ni.clone();
        n.daemon_tasks.push(executor.spawn(async move { ni2.background_task_main().await }));

        Ok(n)
    }
}

impl<'e, S: Store + 'static> NodeIntl<'e, S> {
    async fn background_task_main(&self) {
        let io_timeout_ms = self.config.io_timeout * 1000;
        let delay = Duration::from_secs(10);
        loop {
            Timer::after(delay).await;

            let mut connections = self.connections.lock().await;
            let (done_sender, done_receiver) = smol::channel::bounded::<()>(16);
            let done_sender = Arc::new(done_sender);

            let to_erase: Arc<Mutex<Vec<SocketAddr>>> = Arc::new(Mutex::new(Vec::new()));
            let mut tasks: Vec<Task<()>> = Vec::with_capacity(connections.len());

            // Search for connections that are dead, have timed out during negotiation, or
            // that are duplicates of another connection to the same remote node.
            let have_node_ids: Arc<Mutex<HashSet<[u8; 48]>>> = Arc::new(Mutex::new(HashSet::with_capacity(connections.len())));
            let now_monotonic = self.store.monotonic_clock();
            for c in connections.iter() {
                let l = c.1.0.upgrade();
                if l.is_some() {
                    let l = l.unwrap();
                    let remote_node_id = l.remote_node_id();
                    if remote_node_id.is_some() {
                        let remote_node_id = remote_node_id.unwrap();
                        if !have_node_ids.lock().await.contains(&remote_node_id) {
                            let a = c.0.clone();
                            let hn = have_node_ids.clone();
                            let te = to_erase.clone();
                            let ds = done_sender.clone();
                            tasks.push(self.executor.spawn(async move {
                                if l.do_periodic_tasks(now_monotonic).await.is_ok() {
                                    if !hn.lock().await.insert(remote_node_id) {
                                        // This is a redudant link to the same remote node.
                                        te.lock().await.push(a);
                                    }
                                } else {
                                    // A fatal error occurred while servicing the connection.
                                    te.lock().await.push(a);
                                }
                                let _ = ds.send(()).await;
                            }));
                        } else {
                            // This is a redudant link to the same remote node.
                            to_erase.lock().await.push(c.0.clone());
                        }
                    } else if (now_monotonic - l.connect_time) > io_timeout_ms {
                        // Link negotiation timed out if we aren't connected yet.
                        to_erase.lock().await.push(c.0.clone());
                    }
                } else {
                    // Connection is closed and has released its internally held Arc<>.
                    to_erase.lock().await.push(c.0.clone());
                }
            }

            // Wait for a message on the channel from each task indicating that it is complete.
            for _ in 0..tasks.len() {
                let _ = done_receiver.recv().await;
            }

            // Close and erase all connections slated for cleanup.
            for e in to_erase.lock().await.iter() {
                let _ = connections.remove(e);
            }
        }
    }

    async fn tcp_listener_main(&self, listener: smol::net::TcpListener) {
        loop {
            let c = listener.accept().await;
            if c.is_ok() {
                let (connection, remote_address) = c.unwrap();
                let l = Arc::new(Link::<'e, S>::new(connection, self.secret, self.config, self.store));
                self.connections.lock().await.insert(remote_address.clone(), (Arc::downgrade(&l), self.executor.spawn(async move {
                    let _ = l.io_main().await;
                    // Arc<Link> is now released, causing Weak<Link> to go null and then causing this
                    // entry to be removed from the connection map on the next background task sweep.
                })));
            } else {
                break;
            }
        }
    }
}
