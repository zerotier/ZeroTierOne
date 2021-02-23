/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

use std::collections::BTreeMap;
use std::net::{IpAddr, SocketAddr};
use std::str::FromStr;
use std::sync::{Arc, Mutex, Weak};
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Duration;

use futures::stream::StreamExt;
use warp::{Filter, Reply};
use warp::http::{HeaderMap, Method, StatusCode};
use warp::hyper::body::Bytes;

use zerotier_core::*;
use zerotier_core::trace::{TraceEvent, TraceEventLayer};

use crate::fastudpsocket::*;
use crate::getifaddrs;
use crate::localconfig::*;
use crate::log::Log;
use crate::network::Network;
use crate::store::Store;
use crate::utils::ms_since_epoch;

const CONFIG_CHECK_INTERVAL: i64 = 5000;

/// Core ZeroTier service.
/// This object must be clonable across threads, so all its innards are in
/// Arc containers. It's probably faster to clone all these Arcs when new
/// threads are created (a rare event) so we only have to dereference each
/// Arc once for common events like packet receipt.
#[derive(Clone)]
struct Service {
    auth_token: Arc<String>,
    log: Arc<Log>,
    _local_config: Arc<Mutex<Arc<LocalConfig>>>, // Arc -> shared Mutex container so it can be changed globally
    run: Arc<AtomicBool>,
    online: Arc<AtomicBool>,
    store: Arc<Store>,
    node: Weak<Node<Service, Network>>, // weak since Node itself may hold a reference to this
}

impl NodeEventHandler<Network> for Service {
    #[inline(always)]
    fn virtual_network_config(&self, network_id: NetworkId, network_obj: &Arc<Network>, config_op: VirtualNetworkConfigOperation, config: Option<&VirtualNetworkConfig>) {}

    #[inline(always)]
    fn virtual_network_frame(&self, network_id: NetworkId, network_obj: &Arc<Network>, source_mac: MAC, dest_mac: MAC, ethertype: u16, vlan_id: u16, data: &[u8]) {}

    #[inline(always)]
    fn event(&self, event: Event, event_data: &[u8]) {
        match event {
            Event::Up => {
                let _ = self.node.upgrade().map(|n: Arc<Node<Service, Network>>| {
                    d!(self.log, "node {} started up in data store '{}'", n.address().to_string(), self.store.base_path.to_str().unwrap());
                });
            },
            Event::Down => {
                d!(self.log, "node shutting down.");
                self.run.store(false, Ordering::Relaxed);
            },
            Event::Online => {
                d!(self.log, "node is online.");
                self.online.store(true, Ordering::Relaxed);
            },
            Event::Offline => {
                d!(self.log, "node is offline.");
                self.online.store(true, Ordering::Relaxed);
            },
            Event::Trace => {
                if !event_data.is_empty() {
                    let _ = Dictionary::new_from_bytes(event_data).map(|tm| {
                        let tm = TraceEvent::parse_message(&tm);
                        let _ = tm.map(|tm: TraceEvent| {
                            let local_config = self.local_config();
                            if match tm.layer() {
                                TraceEventLayer::VL1 => local_config.settings.log.vl1,
                                TraceEventLayer::VL2 => local_config.settings.log.vl2,
                                TraceEventLayer::VL2Filter => local_config.settings.log.vl2_trace_rules,
                                TraceEventLayer::VL2Multicast => local_config.settings.log.vl2_trace_multicast,
                                TraceEventLayer::Other => true,
                            } {
                                self.log.log(tm.to_string());
                            }
                        });
                    });
                }
            },
            Event::UserMessage => {},
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

    #[inline(always)]
    fn path_check(&self, _: Address, _: &Identity, _: i64, _: &InetAddress) -> bool {
        true
    }

    #[inline(always)]
    fn path_lookup(&self, address: Address, id: &Identity, desired_family: InetAddressFamily) -> Option<InetAddress> {
        let lc = self.local_config();
        let vc = lc.virtual_.get(&address);
        vc.map_or(None, |c: &LocalConfigVirtualConfig| {
            if c.try_.is_empty() {
                None
            } else {
                let t = c.try_.get((zerotier_core::random() as usize) % c.try_.len());
                t.map_or(None, |v: &InetAddress| {
                    d!(self.log, "path lookup for {} returned {}", address.to_string(), v.to_string());
                    Some(v.clone())
                })
            }
        })
    }
}

impl Service {
    #[inline(always)]
    fn web_api_status(&self, remote: Option<SocketAddr>, method: Method, headers: HeaderMap, post_data: Bytes) -> Box<dyn Reply> {
        Box::new(StatusCode::BAD_REQUEST)
    }

    #[inline(always)]
    fn web_api_network(&self, network_str: String, remote: Option<SocketAddr>, method: Method, headers: HeaderMap, post_data: Bytes) -> Box<dyn Reply> {
        Box::new(StatusCode::BAD_REQUEST)
    }

    #[inline(always)]
    fn web_api_peer(&self, peer_str: String, remote: Option<SocketAddr>, method: Method, headers: HeaderMap, post_data: Bytes) -> Box<dyn Reply> {
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

    let local_config = Arc::new(store.read_local_conf(false).unwrap_or_else(|_| { LocalConfig::default() }));

    let log = Arc::new(Log::new(
        if local_config.settings.log.path.as_ref().is_some() {
            local_config.settings.log.path.as_ref().unwrap().as_str()
        } else {
            store.default_log_path.to_str().unwrap()
        },
        local_config.settings.log.max_size,
        local_config.settings.log.stderr,
        local_config.settings.log.debug,
        "",
    ));

    // Generate authtoken.secret from secure random bytes if not already set.
    let auth_token = auth_token.unwrap_or_else(|| -> String {
        d!(log, "authtoken.secret not found, generating new...");
        let mut rb = [0_u8; 64];
        unsafe { crate::osdep::getSecureRandom(rb.as_mut_ptr().cast(), 64) };
        let mut generated_auth_token = String::new();
        generated_auth_token.reserve(rb.len());
        for b in rb.iter() {
            if *b > 127_u8 {
                generated_auth_token.push((65 + (*b % 26)) as char); // A..Z
            } else {
                generated_auth_token.push((97 + (*b % 26)) as char); // a..z
            }
        }
        if store.write_authtoken_secret(generated_auth_token.as_str()).is_err() {
            generated_auth_token.clear();
        }
        generated_auth_token
    });
    if auth_token.is_empty() {
        log.fatal(format!("unable to write authtoken.secret to '{}'", store.base_path.to_str().unwrap()));
        return 1;
    }
    let auth_token = Arc::new(auth_token);

    // From this point on we're in tokio / async.
    let tokio_rt = tokio::runtime::Builder::new_current_thread().build().unwrap();
    tokio_rt.block_on(async {
        let mut udp_sockets: BTreeMap<InetAddress, FastUDPSocket> = BTreeMap::new();
        let (mut interrupt_tx, mut interrupt_rx) = futures::channel::mpsc::channel::<()>(1);

        let mut service = Service {
            auth_token: auth_token.clone(),
            log: log.clone(),
            _local_config: Arc::new(Mutex::new(local_config)),
            run: Arc::new(AtomicBool::new(true)),
            online: Arc::new(AtomicBool::new(false)),
            store: store.clone(),
            node: Weak::new(),
        };

        let node = Node::new(service.clone(), ms_since_epoch());
        if node.is_err() {
            log.fatal(format!("error initializing node: {}", node.err().unwrap().to_str()));
            process_exit_value = 1;
            return;
        }
        let node = Arc::new(node.ok().unwrap());

        service.node = Arc::downgrade(&node);
        let service = service; // make immutable after setting node

        let mut loop_delay = zerotier_core::NODE_BACKGROUND_TASKS_MAX_INTERVAL;
        loop {
            let mut local_config = service.local_config();

            d!(log, "starting local HTTP API server on 127.0.0.1 port {}", local_config.settings.primary_port);
            let (mut shutdown_tx, mut shutdown_rx) = futures::channel::oneshot::channel();
            let warp_server;
            {
                let s0 = service.clone();
                let s1 = service.clone();
                let s2 = service.clone();
                warp_server = warp::serve(warp::any()
                    .and(warp::path::end().map(|| { warp::reply::with_status("404", StatusCode::NOT_FOUND) })
                        .or(warp::path("status")
                            .and(warp::addr::remote())
                            .and(warp::method())
                            .and(warp::header::headers_cloned())
                            .and(warp::body::content_length_limit(1048576))
                            .and(warp::body::bytes())
                            .map(move |remote: Option<SocketAddr>, method: Method, headers: HeaderMap, post_data: Bytes| { s0.web_api_status(remote, method, headers, post_data) }))
                        .or(warp::path!("network" / String)
                            .and(warp::addr::remote())
                            .and(warp::method())
                            .and(warp::header::headers_cloned())
                            .and(warp::body::content_length_limit(1048576))
                            .and(warp::body::bytes())
                            .map(move |network_str: String, remote: Option<SocketAddr>, method: Method, headers: HeaderMap, post_data: Bytes| { s1.web_api_network(network_str, remote, method, headers, post_data) }))
                        .or(warp::path!("peer" / String)
                            .and(warp::addr::remote())
                            .and(warp::method())
                            .and(warp::header::headers_cloned())
                            .and(warp::body::content_length_limit(1048576))
                            .and(warp::body::bytes())
                            .map(move |peer_str: String, remote: Option<SocketAddr>, method: Method, headers: HeaderMap, post_data: Bytes| { s2.web_api_peer(peer_str, remote, method, headers, post_data) }))
                    )
                ).try_bind_with_graceful_shutdown((IpAddr::from([127_u8, 0_u8, 0_u8, 1_u8]), local_config.settings.primary_port), async { let _ = shutdown_rx.await; });
            }
            if warp_server.is_err() {
                l!(log, "ERROR: local API http server failed to bind to port {} or failed to start: {}, restarting inner loop...", local_config.settings.primary_port, warp_server.err().unwrap().to_string());
                break;
            }
            let warp_server = tokio_rt.spawn(warp_server.unwrap().1);

            // Write zerotier.port which is used by the CLI to know how to reach the HTTP API.
            store.write_port(local_config.settings.primary_port);

            // The inner loop runs the web server in the "background" (async) while periodically
            // scanning for significant configuration changes. Some major changes may require
            // the inner loop to exit and be restarted.
            let mut last_checked_config: i64 = 0;
            d!(log, "local HTTP API server running, inner loop starting.");
            loop {
                let loop_start = ms_since_epoch();
                let mut now: i64 = 0;

                // Wait for (1) loop delay elapsed, (2) a signal to interrupt delay now, or
                // (3) an external signal to exit.
                tokio::select! {
                    _ = tokio::time::sleep(Duration::from_millis(loop_delay)) => {
                        now = ms_since_epoch();
                        let actual_delay = now - loop_start;
                        if actual_delay > ((loop_delay as i64) * 4_i64) {
                            l!(log, "likely sleep/wake detected due to excess delay, reestablishing links...");
                            // TODO: handle likely sleep/wake or other system interruption
                        }
                    },
                    _ = interrupt_rx.next() => {
                        d!(log, "inner loop delay interrupted!");
                        now = ms_since_epoch();
                    },
                    _ = tokio::signal::ctrl_c() => {
                        l!(log, "exit signal received, shutting down...");
                        service.run.store(false, Ordering::Relaxed);
                        break;
                    }
                }

                // Check every CONFIG_CHECK_INTERVAL for changes to either the system configuration
                // or the node's local configuration and take actions as needed.
                if (now - last_checked_config) >= CONFIG_CHECK_INTERVAL {
                    last_checked_config = now;

                    // Check for changes to local.conf.
                    let new_config = store.read_local_conf(true);
                    if new_config.is_ok() {
                        d!(log, "local.conf changed on disk, reloading.");
                        service.set_local_config(new_config.unwrap());
                    }

                    // Check for and handle configuration changes, some of which require inner loop restart.
                    let next_local_config = service.local_config();
                    if local_config.settings.primary_port != next_local_config.settings.primary_port {
                        break;
                    }
                    if local_config.settings.log.max_size != next_local_config.settings.log.max_size {
                        log.set_max_size(next_local_config.settings.log.max_size);
                    }
                    if local_config.settings.log.stderr != next_local_config.settings.log.stderr {
                        log.set_log_to_stderr(next_local_config.settings.log.stderr);
                    }
                    if local_config.settings.log.debug != next_local_config.settings.log.debug {
                        log.set_debug(next_local_config.settings.log.debug);
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
                        d!(log, "unbinding UDP socket at {} (no longer appears to be present or port has changed)", k.to_string());
                        udp_sockets.remove(k);
                    }

                    // Create sockets for unbound addresses.
                    for addr in system_addrs.iter() {
                        if !udp_sockets.contains_key(addr.0) {
                            let _ = FastUDPSocket::new(addr.1.as_str(), addr.0, move |raw_socket: &FastUDPRawOsSocket, from_address: &InetAddress, data: Buffer| {
                                // TODO: incoming packet handler
                            }).map_or_else(|e| {
                                d!(log, "error binding UDP socket to {}: {}", addr.0.to_string(), e.to_string());
                            }, |s| {
                                d!(log, "bound UDP socket at {}", addr.0.to_string());
                                udp_sockets.insert(addr.0.clone(), s.unwrap());
                            });
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
                            // TODO: port hunting
                        } else {
                            l!(log, "WARNING: failed to bind to any address at primary port {} (will try again)", local_config.settings.primary_port);
                        }
                    }
                    if secondary_port_bind_failure {
                        if local_config.settings.auto_port_search {
                            // TODO: port hunting
                        } else {
                            l!(log, "WARNING: failed to bind to any address at secondary port {} (will try again)", local_config.settings.secondary_port.unwrap_or(0));
                        }
                    }
                }

                if !service.run.load(Ordering::Relaxed) {
                    break;
                }

                // Run background task handler in ZeroTier core.
                loop_delay = node.process_background_tasks(now);
            }

            d!(log, "inner loop exited, shutting down local API HTTP server...");

            // Gracefully shut down the local web server.
            let _ = shutdown_tx.send(());
            let _ = warp_server.await;

            // Sleep for a brief period of time to prevent thrashing if some invalid
            // state is hit that causes the inner loop to keep breaking.
            if !service.run.load(Ordering::Relaxed) {
                d!(log, "exiting.");
                break;
            }
            let _ = tokio::time::sleep(Duration::from_secs(1)).await;
            if !service.run.load(Ordering::Relaxed) {
                d!(log, "exiting.");
                break;
            }
        }
    });

    process_exit_value
}
