// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::collections::{HashMap, HashSet};
use std::error::Error;
use std::sync::Arc;

use async_trait::async_trait;

use zerotier_crypto::random;
use zerotier_network_hypervisor::vl1::{Endpoint, Event, HostSystem, Identity, InnerProtocol, Node, PathFilter, Storage};
use zerotier_utils::{ms_monotonic, ms_since_epoch};

use crate::sys::udp::BoundUdpPort;

use tokio::task::JoinHandle;
use tokio::time::Duration;

/// VL1 service that connects to the physical network and hosts an inner protocol like ZeroTier VL2.
///
/// This is the "outward facing" half of a full ZeroTier stack on a normal system. It binds sockets,
/// talks to the physical network, manages the vl1 node, and presents a templated interface for
/// whatever inner protocol implementation is using it. This would typically be VL2 but could be
/// a test harness or just the controller for a controller that runs stand-alone.
pub struct VL1Service<StorageImpl: Storage, PathFilterImpl: PathFilter<Self>, InnerProtocolImpl: InnerProtocol> {
    daemons: parking_lot::Mutex<Vec<JoinHandle<()>>>,
    udp_sockets_by_port: tokio::sync::RwLock<HashMap<u16, BoundUdpPort>>,
    storage: Arc<StorageImpl>,
    inner: Arc<InnerProtocolImpl>,
    path_filter: Arc<PathFilterImpl>,
    node_container: Option<Node<Self>>,
}

impl<StorageImpl: Storage, PathFilterImpl: PathFilter<Self>, InnerProtocolImpl: InnerProtocol>
    VL1Service<StorageImpl, PathFilterImpl, InnerProtocolImpl>
{
    pub async fn new(
        storage: Arc<StorageImpl>,
        inner: Arc<InnerProtocolImpl>,
        path_filter: Arc<PathFilterImpl>,
    ) -> Result<Arc<Self>, Box<dyn Error>> {
        let mut service = VL1Service {
            daemons: parking_lot::Mutex::new(Vec::with_capacity(2)),
            udp_sockets_by_port: tokio::sync::RwLock::new(HashMap::with_capacity(8)),
            storage,
            inner,
            path_filter,
            node_container: None,
        };

        service
            .node_container
            .replace(Node::new(&service, &*service.storage, true, false).await?);

        let service = Arc::new(service);

        let mut daemons = service.daemons.lock();
        daemons.push(tokio::spawn(service.clone().udp_bind_daemon()));
        daemons.push(tokio::spawn(service.clone().node_background_task_daemon()));
        drop(daemons);

        Ok(service)
    }

    #[inline(always)]
    pub fn node(&self) -> &Node<Self> {
        debug_assert!(self.node_container.is_some());
        unsafe { self.node_container.as_ref().unwrap_unchecked() }
    }

    async fn udp_bind_daemon(self: Arc<Self>) {}

    async fn node_background_task_daemon(self: Arc<Self>) {}
}

#[async_trait]
impl<StorageImpl: Storage, PathFilterImpl: PathFilter<Self>, InnerProtocolImpl: InnerProtocol> HostSystem
    for VL1Service<StorageImpl, PathFilterImpl, InnerProtocolImpl>
{
    type LocalSocket = crate::LocalSocket;
    type LocalInterface = crate::LocalInterface;

    fn event(&self, event: Event) {
        println!("{}", event.to_string());
        match event {
            _ => {}
        }
    }

    async fn user_message(&self, _source: &Identity, _message_type: u64, _message: &[u8]) {}

    #[inline(always)]
    fn local_socket_is_valid(&self, socket: &Self::LocalSocket) -> bool {
        socket.is_valid()
    }

    async fn wire_send(
        &self,
        endpoint: &Endpoint,
        local_socket: Option<&Self::LocalSocket>,
        local_interface: Option<&Self::LocalInterface>,
        data: &[u8],
        packet_ttl: u8,
    ) -> bool {
        match endpoint {
            Endpoint::IpUdp(address) => {
                // This is the fast path -- the socket is known to the core so just send it.
                if let Some(s) = local_socket {
                    if let Some(s) = s.0.upgrade() {
                        return s.send_sync_nonblock(address, data, packet_ttl);
                    } else {
                        return false;
                    }
                }

                let udp_sockets_by_port = self.udp_sockets_by_port.read().await;
                if !udp_sockets_by_port.is_empty() {
                    if let Some(specific_interface) = local_interface {
                        // Send from a specific interface if that interface is specified.
                        for (_, p) in udp_sockets_by_port.iter() {
                            if !p.sockets.is_empty() {
                                let mut i = (random::next_u32_secure() as usize) % p.sockets.len();
                                for _ in 0..p.sockets.len() {
                                    let s = p.sockets.get(i).unwrap();
                                    if s.interface.eq(specific_interface) {
                                        if s.send_sync_nonblock(address, data, packet_ttl) {
                                            return true;
                                        }
                                    }
                                    i = (i + 1) % p.sockets.len();
                                }
                            }
                        }
                    } else {
                        // Otherwise send from one socket on every interface.
                        let mut sent_on_interfaces = HashSet::with_capacity(4);
                        for p in udp_sockets_by_port.values() {
                            if !p.sockets.is_empty() {
                                let mut i = (random::next_u32_secure() as usize) % p.sockets.len();
                                for _ in 0..p.sockets.len() {
                                    let s = p.sockets.get(i).unwrap();
                                    if !sent_on_interfaces.contains(&s.interface) {
                                        if s.send_sync_nonblock(address, data, packet_ttl) {
                                            sent_on_interfaces.insert(s.interface.clone());
                                        }
                                    }
                                    i = (i + 1) % p.sockets.len();
                                }
                            }
                        }
                        return !sent_on_interfaces.is_empty();
                    }
                }

                return false;
            }
            _ => {}
        }
        return false;
    }

    #[inline(always)]
    fn time_ticks(&self) -> i64 {
        ms_monotonic()
    }

    #[inline(always)]
    fn time_clock(&self) -> i64 {
        ms_since_epoch()
    }
}

impl<StorageImpl: Storage, PathFilterImpl: PathFilter<Self>, InnerProtocolImpl: InnerProtocol> Drop
    for VL1Service<StorageImpl, PathFilterImpl, InnerProtocolImpl>
{
    fn drop(&mut self) {
        for d in self.daemons.lock().drain(..) {
            d.abort();
        }

        // Drop all bound sockets since these can hold circular Arc<> references to 'internal'.
        // This shouldn't have to loop much if at all to acquire the lock, but it might if something
        // is still completing somewhere in an aborting task.
        loop {
            if let Ok(mut udp_sockets) = self.udp_sockets_by_port.try_write() {
                udp_sockets.clear();
                break;
            }
            std::thread::sleep(Duration::from_millis(2));
        }
    }
}
