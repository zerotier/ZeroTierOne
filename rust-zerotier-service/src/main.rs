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

mod cli;
mod fastudpsocket;
mod localconfig;
mod physicallink;
mod log;
mod store;
mod network;
mod vnic;

#[allow(non_snake_case,non_upper_case_globals,non_camel_case_types,dead_code,improper_ctypes)]
mod osdep;

use std::cell::Cell;
use std::collections::BTreeMap;
use std::net::IpAddr;
use std::rc::Rc;
use std::str::FromStr;
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Duration;

use futures::stream::{self, StreamExt};

use warp::Filter;
use warp::hyper::{HeaderMap, Method};
use warp::hyper::body::Bytes;

use zerotier_core::*;

use crate::fastudpsocket::*;
use crate::localconfig::*;
use crate::log::Log;
use crate::physicallink::PhysicalLink;
use crate::network::Network;

pub struct ServiceEventHandler {}

impl NodeEventHandler<Network> for ServiceEventHandler {
    fn virtual_network_config(&self, network_id: NetworkId, network_obj: &Arc<Network>, config_op: VirtualNetworkConfigOperation, config: Option<&VirtualNetworkConfig>) {
    }

    #[inline(always)]
    fn virtual_network_frame(&self, network_id: NetworkId, network_obj: &Arc<Network>, source_mac: MAC, dest_mac: MAC, ethertype: u16, vlan_id: u16, data: &[u8]) {
    }

    fn event(&self, event: Event, event_data: &[u8]) {
    }

    fn state_put(&self, obj_type: StateObjectType, obj_id: &[u64], obj_data: &[u8]) {
    }

    fn state_get(&self, obj_type: StateObjectType, obj_id: &[u64]) -> Option<Box<[u8]>> {
        None
    }

    #[inline(always)]
    fn wire_packet_send(&self, local_socket: i64, sock_addr: &InetAddress, data: &[u8], packet_ttl: u32) -> i32 {
        0
    }

    #[inline(always)]
    fn path_check(&self, address: Address, id: &Identity, local_socket: i64, sock_addr: &InetAddress) -> bool {
        true
    }

    fn path_lookup(&self, address: Address, id: &Identity, desired_family: InetAddressFamily) -> Option<InetAddress> {
        None
    }
}

fn main() {
    let cli_args = Some(cli::parse_cli_args());

    let inaddr_v6_any = IpAddr::from_str("::0").unwrap();
    let mut process_exit_value: i32 = 0;

    // Current active local configuration for this node.
    let mut local_config: Box<LocalConfig> = Box::new(LocalConfig::default());

    // Handler for incoming packets from FastUDPSocket and incoming events from Node.
    let handler: Arc<ServiceEventHandler> = Arc::new(ServiceEventHandler{});

    // From this point on we are in Tokio async land...
    let tokio_rt = tokio::runtime::Builder::new_multi_thread().thread_stack_size(zerotier_core::RECOMMENDED_THREAD_STACK_SIZE).build().unwrap();
    tokio_rt.block_on(async {
        // Keeps track of FastUDPSocket instances by bound address.
        let mut udp_sockets: BTreeMap<InetAddress, FastUDPSocket> = BTreeMap::new();

        // Send something to interrupt_tx to interrupt the inner loop and force it to
        // detect a change or exit if run has been set to false.
        let (mut interrupt_tx, mut interrupt_rx) = futures::channel::mpsc::channel::<u8>(2);

        // Setting this to false terminates the service. It's atomic since this is multithreaded.
        let run = AtomicBool::new(true);

        loop {
            let mut warp_server_port = local_config.settings.primary_port;

            let root = warp::path::end().map(|| {
                warp::reply::with_status("404", warp::hyper::StatusCode::NOT_FOUND)
            });

            let status = warp::path("status")
                .and(warp::method())
                .and(warp::header::headers_cloned())
                .and(warp::body::bytes())
                .map(|method: Method, headers: HeaderMap, post_data: Bytes| {
                "status"
            });
            let network = warp::path!("network" / String)
                .and(warp::method())
                .and(warp::header::headers_cloned())
                .and(warp::body::bytes())
                .map(|nwid_str: String, method: Method, headers: HeaderMap, post_data: Bytes| {
                "network"
            });
            let peer = warp::path!("peer" / String)
                .and(warp::method())
                .and(warp::header::headers_cloned())
                .and(warp::body::bytes())
                .map(|peer_str: String, method: Method, headers: HeaderMap, post_data: Bytes| {
                "peer"
            });

            let (mut shutdown_tx, mut shutdown_rx) = futures::channel::oneshot::channel();
            let warp_server = warp::serve(warp::any().and(
                root
                    .or(status)
                    .or(network)
                    .or(peer)
            )).try_bind_with_graceful_shutdown((inaddr_v6_any, warp_server_port), async { let _ = shutdown_rx.await; });
            if warp_server.is_err() {
                // TODO: log unable to bind to primary port
                run.store(false, Ordering::Relaxed);
            }
            let warp_server = tokio_rt.spawn(warp_server.unwrap().1);

            let mut loop_delay = 10;
            loop {
                tokio::select! {
                    _ = tokio::time::sleep(Duration::from_secs(loop_delay)) => {},
                    _ = interrupt_rx.next() => {},
                    _ = tokio::signal::ctrl_c() => {
                        // TODO: log CTRL+C received
                        run.store(false, Ordering::Relaxed);
                        let _ = shutdown_tx.send(());
                        break;
                    }
                }

                // Enumerate physical addresses on the system, creating a map with an entry for
                // the primary_port and another for the secondary_port if bound.
                let mut system_addrs: BTreeMap<InetAddress, Rc<PhysicalLink>> = BTreeMap::new();
                PhysicalLink::map(|link: PhysicalLink| {
                    if !local_config.settings.is_interface_blacklisted(link.device.as_str()) {
                        let l = Rc::new(link);
                        let mut a = l.address.clone();
                        a.set_port(local_config.settings.primary_port);
                        system_addrs.insert(a, l.clone());
                        if local_config.settings.secondary_port.is_some() {
                            let mut a = l.address.clone();
                            a.set_port(local_config.settings.secondary_port.unwrap());
                            system_addrs.insert(a, l.clone());
                        }
                    }
                });

                // Close UDP bindings that no longer apply.
                let mut udp_sockets_to_close: Vec<InetAddress> = Vec::new();
                for sock in udp_sockets.iter() {
                    if !system_addrs.contains_key(sock.0) {
                        udp_sockets_to_close.push(sock.0.clone());
                    }
                }
                for k in udp_sockets_to_close.iter() {
                    udp_sockets.remove(k);
                }

                // Bind addresses that are not already bound.
                for addr in system_addrs.iter() {
                    if !udp_sockets.contains_key(addr.0) {
                        let s = FastUDPSocket::new(addr.1.device.as_str(), addr.0, |raw_socket: &FastUDPRawOsSocket, from_address: &InetAddress, data: Buffer| {
                            // TODO
                        });
                        if s.is_ok() {
                            udp_sockets.insert(addr.0.clone(), s.unwrap());
                        } else if addr.0.port() == local_config.settings.primary_port {
                            run.store(false, Ordering::Relaxed);
                            // TODO: log failure to bind to primary port (UDP)
                            break;
                        }
                    }
                }

                if local_config.settings.primary_port != warp_server_port || !run.load(Ordering::Relaxed) {
                    let _ = shutdown_tx.send(());
                    break;
                }
            }

            let _ = warp_server.await;

            if !run.load(Ordering::Relaxed) {
                break;
            }
            tokio::time::sleep(Duration::from_millis(250)).await;
            if !run.load(Ordering::Relaxed) {
                break;
            }
        }
    });

    std::process::exit(process_exit_value);
}
