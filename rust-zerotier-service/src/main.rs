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

mod fastudpsocket;
mod localconfig;
mod physicallink;
mod log;
mod store;
mod network;

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

use zerotier_core::*;

use crate::fastudpsocket::*;
use crate::localconfig::*;
use crate::log::Log;
use crate::physicallink::PhysicalLink;
use crate::network::Network;
use futures::TryFutureExt;

pub struct ServiceEventHandler {}

impl FastUDPSocketPacketHandler for ServiceEventHandler {
    #[inline(always)]
    fn incoming_udp_packet(&self, raw_socket: &FastUDPRawOsSocket, from_adddress: &InetAddress, data: Buffer) {
    }
}

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

    fn path_check(&self, address: Address, id: &Identity, local_socket: i64, sock_addr: &InetAddress) -> bool {
        true
    }

    fn path_lookup(&self, address: Address, id: &Identity, desired_family: InetAddressFamily) -> Option<InetAddress> {
        None
    }
}

fn main() {
    tokio::runtime::Builder::new_multi_thread().thread_stack_size(zerotier_core::RECOMMENDED_THREAD_STACK_SIZE).build().unwrap().block_on(async {
        let inaddr_v6_any = IpAddr::from_str("::0").unwrap();

        let mut local_config: Box<LocalConfig> = Box::new(LocalConfig::default());
        let mut udp_sockets: BTreeMap<InetAddress, FastUDPSocket<ServiceEventHandler>> = BTreeMap::new();
        let handler: Arc<ServiceEventHandler> = Arc::new(ServiceEventHandler{});
        let run: AtomicBool = AtomicBool::new(true);
        let (mut interrupt_tx, mut interrupt_rx) = futures::channel::mpsc::channel::<u8>(2);

        //
        // The inner loop periodically updates UDP socket bindings and does other housekeeping, but
        // otherwise does nothing. If it detects that the primary port has changed, it breaks and
        // causes the outer loop to run which reboots the HTTP server. If the 'run' flag is set
        // to false this causes a break of both loops which terminates the service.
        //

        loop {
            let mut warp_server_port = local_config.settings.primary_port;

            let root = warp::path::end().map(|| { warp::reply::with_status("not found", warp::hyper::StatusCode::NOT_FOUND) });
            let status = warp::path("status").map(|| { "status" });
            let network = warp::path!("network" / String).map(|nwid_str| { "network" });
            let peer = warp::path!("peer" / String).map(|peer_str| { "peer" });

            let (shutdown_tx, shutdown_rx) = futures::channel::oneshot::channel();
            let (_, warp_server) = warp::serve(warp::any().and(
                root
                    .or(status)
                    .or(network)
                    .or(peer)
            )).bind_with_graceful_shutdown((inaddr_v6_any, warp_server_port), async {
                let _ = shutdown_rx.await;
            });
            let warp_server = tokio::spawn(warp_server);

            let mut loop_delay = 10;
            loop {
                let _ = tokio::time::timeout(Duration::from_secs(loop_delay), interrupt_rx.next());

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
                        let s = FastUDPSocket::new(addr.1.device.as_str(), addr.0, &handler);
                        if s.is_ok() {
                            udp_sockets.insert(addr.0.clone(), s.unwrap());
                        }
                    }
                }

                if local_config.settings.primary_port != warp_server_port || !run.load(Ordering::Relaxed) {
                    let _ = shutdown_tx.send(());
                    let _ = warp_server.await;
                    break;
                }
            }

            tokio::time::sleep(Duration::from_millis(250)).await;
            if !run.load(Ordering::Relaxed) {
                break;
            }
        }
    });
}
