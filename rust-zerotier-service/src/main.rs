mod fastudpsocket;
mod localconfig;
mod physicallink;
mod log;
mod store;

use std::any::Any;
use std::cell::Cell;
use std::collections::BTreeMap;
use std::net::IpAddr;
use std::rc::Rc;
use std::str::FromStr;
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Duration;

use warp::Filter;

use zerotier_core::{Address, Buffer, Event, Identity, InetAddress, InetAddressFamily, MAC, NetworkId, Node, NodeEventHandler, StateObjectType, VirtualNetworkConfig, VirtualNetworkConfigOperation};

use crate::fastudpsocket::*;
use crate::localconfig::*;
use crate::physicallink::PhysicalLink;
use crate::log::Log;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

pub struct ServiceEventHandler {}

impl FastUDPSocketPacketHandler for ServiceEventHandler {
    fn incoming_udp_packet(&self, raw_socket: &FastUDPRawOsSocket, from_adddress: &InetAddress, data: Buffer) {}
}

impl NodeEventHandler for ServiceEventHandler {
    fn virtual_network_config(&self, network_id: NetworkId, network_obj: &Arc<dyn Any>, config_op: VirtualNetworkConfigOperation, config: Option<&VirtualNetworkConfig>) {
    }

    fn virtual_network_frame(&self, network_id: NetworkId, network_obj: &Arc<dyn Any>, source_mac: MAC, dest_mac: MAC, ethertype: u16, vlan_id: u16, data: &[u8]) {
    }

    fn event(&self, event: Event, event_data: &[u8]) {
    }

    fn state_put(&self, obj_type: StateObjectType, obj_id: &[u64], obj_data: &[u8]) {
    }

    fn state_get(&self, obj_type: StateObjectType, obj_id: &[u64]) -> Option<Box<[u8]>> {
        None
    }

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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

fn main() {
    tokio::runtime::Builder::new_multi_thread().thread_stack_size(zerotier_core::RECOMMENDED_THREAD_STACK_SIZE).build().unwrap().block_on(async {
        let inaddr_v6_any = IpAddr::from_str("::0").unwrap();

        let mut local_config: Box<LocalConfig> = Box::new(LocalConfig::default());
        let mut udp_sockets: BTreeMap<InetAddress, FastUDPSocket<ServiceEventHandler>> = BTreeMap::new();
        let handler: Arc<ServiceEventHandler> = Arc::new(ServiceEventHandler{});
        let run: AtomicBool = AtomicBool::new(true);

        loop {
            let mut warp_server_port = local_config.settings.primary_port;
            loop {
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

                tokio::time::sleep(Duration::from_secs(10)).await;

                // Diff system addresses against currently bound UDP sockets and update as needed.
                // Also check interface prefix blacklists.
                let mut system_addrs: BTreeMap<InetAddress, Rc<PhysicalLink>> = BTreeMap::new();
                PhysicalLink::map(|link: PhysicalLink| {
                    if !local_config.settings.is_interface_blacklisted(link.device.as_str()) {
                        // Add two entries to system_addrs: one for primary port, and one for secondary if enabled.
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

                // Breaking the inner loop causes the HTTP server to recycle, or may exit entirely if run is false.
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
