/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

use std::collections::BTreeMap;
use std::net::IpAddr;
use std::str::FromStr;
use std::sync::{Arc, Mutex, Weak};
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Duration;

use futures::stream::StreamExt;
use warp::{Filter, Reply};
use warp::http::{HeaderMap, Method, StatusCode};
use warp::hyper::body::Bytes;

use zerotier_core::{Buffer, Address, IpScope, Node, NodeEventHandler, NetworkId, VirtualNetworkConfigOperation, VirtualNetworkConfig, StateObjectType, MAC, Event, InetAddress, InetAddressFamily, Identity};

use crate::fastudpsocket::*;
use crate::getifaddrs;
use crate::localconfig::*;
use crate::log::Log;
use crate::network::Network;
use crate::store::Store;

const CONFIG_CHECK_INTERVAL: i64 = 5000;

#[derive(Clone)]
struct Service {
    auth_token: Arc<String>,
    log: Arc<Log>,
    _local_config: Arc<Mutex<Arc<LocalConfig>>>,
    run: Arc<AtomicBool>,
    store: Arc<Store>,
    node: Weak<Node<Service, Network>>, // weak since Node can hold a reference to this
}

impl NodeEventHandler<Network> for Service {
    fn virtual_network_config(&self, network_id: NetworkId, network_obj: &Arc<Network>, config_op: VirtualNetworkConfigOperation, config: Option<&VirtualNetworkConfig>) {}

    #[inline(always)]
    fn virtual_network_frame(&self, network_id: NetworkId, network_obj: &Arc<Network>, source_mac: MAC, dest_mac: MAC, ethertype: u16, vlan_id: u16, data: &[u8]) {}

    fn event(&self, event: Event, event_data: &[u8]) {
        match event {
            Event::Up => {}
            Event::Down => {}
            Event::Online => {}
            Event::Offline => {}
            Event::Trace => {}
            Event::UserMessage => {}
        }
    }

    #[inline(always)]
    fn state_put(&self, obj_type: StateObjectType, obj_id: &[u64], obj_data: &[u8]) -> std::io::Result<()> {
        self.store.store_object(&obj_type, obj_id, obj_data)
    }

    #[inline(always)]
    fn state_get(&self, obj_type: StateObjectType, obj_id: &[u64]) -> std::io::Result<Vec<u8>> {
        self.store.load_object(&obj_type, obj_id)
    }

    #[inline(always)]
    fn wire_packet_send(&self, local_socket: i64, sock_addr: &InetAddress, data: &[u8], packet_ttl: u32) -> i32 {
        0
    }

    fn path_check(&self, address: Address, id: &Identity, local_socket: i64, sock_addr: &InetAddress) -> bool {
        true
    }

    fn path_lookup(&self, address: Address, id: &Identity, desired_family: InetAddressFamily) -> Option<InetAddress> {
        let lc = self.local_config();
        let vc = lc.virtual_.get(&address);
        vc.map_or(None, |c: &LocalConfigVirtualConfig| {
            if c.try_.is_empty() {
                None
            } else {
                let t = c.try_.get((zerotier_core::random() as usize) % c.try_.len());
                t.map_or(None, |v: &InetAddress| {
                    Some(v.clone())
                })
            }
        })
    }
}

impl Service {
    #[inline(always)]
    fn web_api_status(&self, method: Method, headers: HeaderMap, post_data: Bytes) -> Box<dyn Reply> {
        Box::new(StatusCode::BAD_REQUEST)
    }

    #[inline(always)]
    fn web_api_network(&self, network_str: String, method: Method, headers: HeaderMap, post_data: Bytes) -> Box<dyn Reply> {
        Box::new(StatusCode::BAD_REQUEST)
    }

    #[inline(always)]
    fn web_api_peer(&self, peer_str: String, method: Method, headers: HeaderMap, post_data: Bytes) -> Box<dyn Reply> {
        Box::new(StatusCode::BAD_REQUEST)
    }

    #[inline(always)]
    fn local_config(&self) -> Arc<LocalConfig> {
        self._local_config.lock().unwrap().clone()
    }

    #[inline(always)]
    fn set_local_config(&self, new_lc: LocalConfig) {
        *(self._local_config.lock().unwrap()) = Arc::new(new_lc);
    }
}

pub(crate) fn run(store: &Arc<Store>, auth_token: Option<String>) -> i32 {
    let mut process_exit_value: i32 = 0;

    let init_local_config = Arc::new(store.read_local_conf(false).unwrap_or(LocalConfig::default()));

    // Open log in store.
    let log = Arc::new(Log::new(
        if init_local_config.settings.log_path.as_ref().is_some() { init_local_config.settings.log_path.as_ref().unwrap().as_str() } else { store.default_log_path.to_str().unwrap() },
        init_local_config.settings.log_size_max,
        "",
    ));

    // Generate authtoken.secret from secure random bytes if not already set.
    let auth_token = auth_token.unwrap_or_else(|| {
        let mut rb = [0_u8; 64];
        unsafe {
            crate::osdep::getSecureRandom(rb.as_mut_ptr().cast(), 64);
        }
        let mut t = String::new();
        t.reserve(64);
        for b in rb.iter() {
            if *b > 127_u8 {
                t.push((65 + (*b % 26)) as char); // A..Z
            } else {
                t.push((97 + (*b % 26)) as char); // a..z
            }
        }
        if store.write_authtoken_secret(t.as_str()).is_err() {
            t.clear();
        }
        t
    });
    if auth_token.is_empty() {
        l!(log, "FATAL: unable to write authtoken.secret to '{}'", store.base_path.to_str().unwrap());
        return 1;
    }
    let auth_token = Arc::new(auth_token);

    let tokio_rt = tokio::runtime::Builder::new_current_thread().build().unwrap();
    tokio_rt.block_on(async {
        let mut udp_sockets: BTreeMap<InetAddress, FastUDPSocket> = BTreeMap::new();
        let (mut interrupt_tx, mut interrupt_rx) = futures::channel::mpsc::channel::<()>(1);

        // Create clonable implementation of NodeEventHandler and local web API endpoints.
        let mut service = Service {
            auth_token: auth_token.clone(),
            log: log.clone(),
            _local_config: Arc::new(Mutex::new(init_local_config)),
            run: Arc::new(AtomicBool::new(true)),
            store: store.clone(),
            node: Weak::new(),
        };

        // Create instance of Node which will call Service on events.
        let node = Node::new(service.clone());
        if node.is_err() {
            process_exit_value = 1;
            l!(log, "FATAL: error initializing node: {}", node.err().unwrap().to_string());
            return;
        }
        let node = Arc::new(node.ok().unwrap());

        service.node = Arc::downgrade(&node);
        let service = service; // make immutable after setting node

        let mut last_checked_config: i64 = 0;
        let mut loop_delay = zerotier_core::NODE_BACKGROUND_TASKS_MAX_INTERVAL;
        loop {
            let mut local_config = service.local_config();

            let (mut shutdown_tx, mut shutdown_rx) = futures::channel::oneshot::channel();
            let warp_server;
            {
                let s0 = service.clone();
                let s1 = service.clone();
                let s2 = service.clone();
                warp_server = warp::serve(
                    warp::any().and(warp::path::end().map(|| {
                        warp::reply::with_status("404", StatusCode::NOT_FOUND)
                    })
                    .or(warp::path("status").and(warp::method()).and(warp::header::headers_cloned()).and(warp::body::bytes())
                        .map(move |method: Method, headers: HeaderMap, post_data: Bytes| {
                            s0.web_api_status(method, headers, post_data)
                        }))
                    .or(warp::path!("network" / String).and(warp::method()).and(warp::header::headers_cloned()).and(warp::body::bytes())
                        .map(move |network_str: String, method: Method, headers: HeaderMap, post_data: Bytes| {
                            s1.web_api_network(network_str, method, headers, post_data)
                        }))
                    .or(warp::path!("peer" / String).and(warp::method()).and(warp::header::headers_cloned()).and(warp::body::bytes())
                        .map(move |peer_str: String, method: Method, headers: HeaderMap, post_data: Bytes| {
                            s2.web_api_peer(peer_str, method, headers, post_data)
                        }))
                )).try_bind_with_graceful_shutdown(
                    (IpAddr::from([127_u8, 0_u8, 0_u8, 1_u8]), local_config.settings.primary_port),
                    async { let _ = shutdown_rx.await; },
                );
            }
            if warp_server.is_err() {
                l!(log, "ERROR: local API http server failed to bind to port {}: {}", local_config.settings.primary_port, warp_server.err().unwrap().to_string());
                break;
            }
            let warp_server = tokio_rt.spawn(warp_server.unwrap().1);

            loop {
                // Wait for (1) loop delay elapsed, (2) a signal to interrupt delay now, or
                // (3) an external signal to exit.
                tokio::select! {
                    _ = tokio::time::sleep(Duration::from_millis(loop_delay)) => {},
                    _ = interrupt_rx.next() => {},
                    _ = tokio::signal::ctrl_c() => {
                        l!(log, "exit signal received, shutting down...");
                        service.run.store(false, Ordering::Relaxed);
                        break;
                    }
                }

                // Check every CONFIG_CHECK_INTERVAL for changes to either the system configuration
                // or the node's local configuration and take actions as needed.
                let now = zerotier_core::now();
                if (now - last_checked_config) >= CONFIG_CHECK_INTERVAL {
                    last_checked_config = now;

                    // Check for changes to local.conf.
                    let new_config = store.read_local_conf(true);
                    if new_config.is_ok() {
                        service.set_local_config(new_config.unwrap());
                    }

                    // Check for configuration changes that require a reboot of the inner loop
                    // or other actions to be taken.
                    let next_local_config = service.local_config();
                    if local_config.settings.primary_port != next_local_config.settings.primary_port {
                        break;
                    }
                    local_config = next_local_config;

                    // Enumerate all useful addresses bound to interfaces on the system.
                    let mut system_addrs: BTreeMap<InetAddress, String> = BTreeMap::new();
                    getifaddrs::for_each_address(|addr: &InetAddress, dev: &str| {
                        match addr.ip_scope() {
                            IpScope::Global | IpScope::Private | IpScope::PseudoPrivate | IpScope::Shared => {
                                if !local_config.settings.is_interface_blacklisted(dev) {
                                    let mut a = addr.clone();
                                    a.set_port(local_config.settings.primary_port);
                                    system_addrs.insert(a, String::from(dev));
                                    if local_config.settings.secondary_port.is_some() {
                                        let mut a = addr.clone();
                                        a.set_port(local_config.settings.secondary_port.unwrap());
                                        system_addrs.insert(a, String::from(dev));
                                    }
                                }
                            }
                            _ => {}
                        }
                    });

                    // Drop bound sockets that are no longer valid or are now blacklisted.
                    let mut udp_sockets_to_close: Vec<InetAddress> = Vec::new();
                    for sock in udp_sockets.iter() {
                        if !system_addrs.contains_key(sock.0) {
                            udp_sockets_to_close.push(sock.0.clone());
                        }
                    }
                    for k in udp_sockets_to_close.iter() {
                        udp_sockets.remove(k);
                    }

                    // Create sockets for unbound addresses.
                    for addr in system_addrs.iter() {
                        if !udp_sockets.contains_key(addr.0) {
                            let s = FastUDPSocket::new(addr.1.as_str(), addr.0, |raw_socket: &FastUDPRawOsSocket, from_address: &InetAddress, data: Buffer| {
                                // TODO: incoming packet handler
                            });
                            if s.is_ok() {
                                udp_sockets.insert(addr.0.clone(), s.unwrap());
                            }
                        }
                    }

                    // Determine if primary and secondary port (if secondary enabled) failed to
                    // bind to any interface.
                    let mut primary_port_bind_failure = true;
                    let mut secondary_port_bind_failure = local_config.settings.secondary_port.is_some();
                    for s in udp_sockets.iter() {
                        if s.0.port() == local_config.settings.primary_port {
                            primary_port_bind_failure = false;
                            if !secondary_port_bind_failure {
                                break;
                            }
                        }
                        if s.0.port() == local_config.settings.secondary_port.unwrap() {
                            secondary_port_bind_failure = false;
                            if !primary_port_bind_failure {
                                break;
                            }
                        }
                    }
                    if primary_port_bind_failure {
                        if local_config.settings.auto_port_search {
                            // TODO: port hunting if enabled
                        } else {
                            l!(log, "primary port {} failed to bind, waiting and trying again...", local_config.settings.primary_port);
                            break;
                        }
                    }
                    if secondary_port_bind_failure {
                        l!(log, "secondary port {} failed to bind (non-fatal, will try again)", local_config.settings.secondary_port.unwrap_or(0));
                        // hunt for a secondary port.
                    }
                }

                // Check to make sure nothing outside this code turned off the run flag.
                if !service.run.load(Ordering::Relaxed) {
                    break;
                }

                // Run background task handler in ZeroTier core.
                loop_delay = node.process_background_tasks();
            }

            // Gracefully shut down the local web server.
            let _ = shutdown_tx.send(());
            let _ = warp_server.await;

            // Sleep for a brief period of time to prevent thrashing if some invalid
            // state is hit that causes the inner loop to keep breaking.
            if !service.run.load(Ordering::Relaxed) {
                break;
            }
            tokio::time::sleep(Duration::from_secs(1)).await;
            if !service.run.load(Ordering::Relaxed) {
                break;
            }
        }
    });

    process_exit_value
}
