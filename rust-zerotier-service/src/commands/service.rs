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

use std::cell::Cell;
use std::collections::BTreeMap;
use std::net::IpAddr;
use std::rc::Rc;
use std::str::FromStr;
use std::sync::{Arc, Mutex};
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Duration;

use futures::stream::{self, StreamExt};
use warp::{Filter, Rejection, Reply};
use warp::http::{HeaderMap, Method, StatusCode};
use warp::hyper::body::Bytes;

use zerotier_core::{Buffer, Address, IpScope, Node, NodeEventHandler, NetworkId, VirtualNetworkConfigOperation, VirtualNetworkConfig, StateObjectType, MAC, Event, InetAddress, InetAddressFamily, Identity};

use crate::fastudpsocket::*;
use crate::getifaddrs;
use crate::localconfig::*;
use crate::log::Log;
use crate::network::Network;
use crate::store::Store;

// Check local addresses and bindings every (this) milliseconds.
const BINDING_CHECK_INTERVAL: i64 = 5000;

struct Service {
    local_config: Mutex<Arc<LocalConfig>>,
    run: Arc<AtomicBool>,
    store: Arc<Store>,
}

impl Clone for Service {
    fn clone(&self) -> Self {
        Service {
            local_config: Mutex::new(self.local_config.lock().unwrap().clone()),
            run: self.run.clone(),
            store: self.store.clone(),
        }
    }
}

impl NodeEventHandler<Network> for Service {
    fn virtual_network_config(&self, network_id: NetworkId, network_obj: &Arc<Network>, config_op: VirtualNetworkConfigOperation, config: Option<&VirtualNetworkConfig>) {
    }

    #[inline(always)]
    fn virtual_network_frame(&self, network_id: NetworkId, network_obj: &Arc<Network>, source_mac: MAC, dest_mac: MAC, ethertype: u16, vlan_id: u16, data: &[u8]) {
    }

    fn event(&self, event: Event, event_data: &[u8]) {
        match event {
            Event::Up => {},
            Event::Down => {},
            Event::Online => {},
            Event::Offline => {},
            Event::Trace => {},
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

    fn path_check(&self, address: Address, id: &Identity, local_socket: i64, sock_addr: &InetAddress) -> bool {
        true
    }

    fn path_lookup(&self, address: Address, id: &Identity, desired_family: InetAddressFamily) -> Option<InetAddress> {
        let lc = self.get_local_config();
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
        Box::new(warp::http::StatusCode::BAD_REQUEST)
    }

    #[inline(always)]
    fn web_api_network(&self, network_str: String, method: Method, headers: HeaderMap, post_data: Bytes) -> Box<dyn Reply> {
        Box::new(warp::http::StatusCode::BAD_REQUEST)
    }

    #[inline(always)]
    fn web_api_peer(&self, peer_str: String, method: Method, headers: HeaderMap, post_data: Bytes) -> Box<dyn Reply> {
        Box::new(warp::http::StatusCode::BAD_REQUEST)
    }

    #[inline(always)]
    fn get_local_config(&self) -> Arc<LocalConfig> {
        self.local_config.lock().unwrap().clone()
    }

    #[inline(always)]
    fn set_local_config(&self, new_lc: &Arc<LocalConfig>) {
        let mut lc = self.local_config.lock().unwrap();
        *lc = new_lc.clone();
    }
}

pub(crate) fn run(store: &Arc<Store>) -> i32 {
    let mut process_exit_value: i32 = 0;

    let tokio_rt = tokio::runtime::Builder::new_current_thread().build().unwrap();
    tokio_rt.block_on(async {
        let mut udp_sockets: BTreeMap<InetAddress, FastUDPSocket> = BTreeMap::new();
        let (mut interrupt_tx, mut interrupt_rx) = futures::channel::mpsc::channel::<u8>(2);

        let service = Service {
            local_config: Mutex::new(Arc::new(LocalConfig::default())),
            run: Arc::new(AtomicBool::new(true)),
            store: store.clone(),
        };

        let node = Node::new(service.clone());
        if node.is_err() {
            // TODO: log and handle error
            return;
        }
        let node = Arc::new(node.ok().unwrap());

        let mut primary_port_bind_failure = false;
        let mut last_checked_bindings: i64 = 0;
        let mut loop_delay = zerotier_core::NODE_BACKGROUND_TASKS_MAX_INTERVAL;
        loop {
            let mut current_local_config = service.get_local_config();

            let (mut shutdown_tx, mut shutdown_rx) = futures::channel::oneshot::channel();
            let warp_server;
            {
                let s0 = service.clone();
                let s1 = service.clone();
                let s2 = service.clone();
                warp_server = warp::serve(warp::any().and(warp::path::end().map(|| { warp::reply::with_status("404", warp::hyper::StatusCode::NOT_FOUND) })
                    .or(warp::path("status").and(warp::method()).and(warp::header::headers_cloned()).and(warp::body::bytes())
                        .map(move |method: Method, headers: HeaderMap, post_data: Bytes| { s0.web_api_status(method, headers, post_data) }))
                    .or(warp::path!("network" / String).and(warp::method()).and(warp::header::headers_cloned()).and(warp::body::bytes())
                        .map(move |network_str: String, method: Method, headers: HeaderMap, post_data: Bytes| { s1.web_api_network(network_str, method, headers, post_data) }))
                    .or(warp::path!("peer" / String).and(warp::method()).and(warp::header::headers_cloned()).and(warp::body::bytes())
                        .map(move |peer_str: String, method: Method, headers: HeaderMap, post_data: Bytes| { s2.web_api_peer(peer_str, method, headers, post_data) }))
                )).try_bind_with_graceful_shutdown((IpAddr::from([127_u8, 0_u8, 0_u8, 1_u8]), current_local_config.settings.primary_port), async { let _ = shutdown_rx.await; });
            }
            if warp_server.is_err() {
                primary_port_bind_failure = true;
                break;
            }
            let warp_server = tokio_rt.spawn(warp_server.unwrap().1);

            loop {
                tokio::select! {
                    _ = tokio::time::sleep(Duration::from_millis(loop_delay)) => {},
                    _ = interrupt_rx.next() => {},
                    _ = tokio::signal::ctrl_c() => {
                        // TODO: log CTRL+C received
                        service.run.store(false, Ordering::Relaxed);
                        let _ = shutdown_tx.send(());
                        break;
                    }
                }

                loop_delay = node.process_background_tasks();

                let now = zerotier_core::now();
                if (now - last_checked_bindings) >= BINDING_CHECK_INTERVAL {
                    last_checked_bindings = now;

                    let mut system_addrs: BTreeMap<InetAddress, String> = BTreeMap::new();
                    getifaddrs::for_each_address(|addr: &InetAddress, dev: &str| {
                        match addr.ip_scope() {
                            IpScope::Global | IpScope::Private | IpScope::PseudoPrivate | IpScope::Shared => {
                                if !current_local_config.settings.is_interface_blacklisted(dev) {
                                    let mut a = addr.clone();
                                    a.set_port(current_local_config.settings.primary_port);
                                    system_addrs.insert(a, String::from(dev));
                                    if current_local_config.settings.secondary_port.is_some() {
                                        let mut a = addr.clone();
                                        a.set_port(current_local_config.settings.secondary_port.unwrap());
                                        system_addrs.insert(a, String::from(dev));
                                    }
                                }
                            },
                            _ => {}
                        }
                    });

                    let mut udp_sockets_to_close: Vec<InetAddress> = Vec::new();
                    for sock in udp_sockets.iter() {
                        if !system_addrs.contains_key(sock.0) {
                            udp_sockets_to_close.push(sock.0.clone());
                        }
                    }
                    for k in udp_sockets_to_close.iter() {
                        udp_sockets.remove(k);
                    }

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

                    primary_port_bind_failure = true;
                    for s in udp_sockets.iter() {
                        if s.0.port() == current_local_config.settings.primary_port {
                            primary_port_bind_failure = false;
                            break;
                        }
                    }
                    if primary_port_bind_failure {
                        break;
                    }
                }

                let next_local_config = service.get_local_config();
                if !service.run.load(Ordering::Relaxed) || current_local_config.settings.primary_port != next_local_config.settings.primary_port {
                    let _ = shutdown_tx.send(());
                    break;
                }
                current_local_config = next_local_config;
            }

            let _ = warp_server.await;

            if !service.run.load(Ordering::Relaxed) {
                break;
            }
            tokio::time::sleep(Duration::from_millis(250)).await;
            if !service.run.load(Ordering::Relaxed) {
                break;
            }

            if primary_port_bind_failure {
                let local_config = service.get_local_config();
                if local_config.settings.auto_port_search {
                    // TODO: port hunting if enabled
                }
            }
        }
    });

    process_exit_value
}
