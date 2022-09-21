// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::collections::{HashMap, HashSet};
use std::error::Error;
use std::sync::Arc;

use zerotier_crypto::random;
use zerotier_network_hypervisor::protocol::{PacketBufferFactory, PacketBufferPool};
use zerotier_network_hypervisor::vl1::*;
use zerotier_utils::{ms_monotonic, ms_since_epoch};

use crate::constants::UNASSIGNED_PRIVILEGED_PORTS;
use crate::settings::VL1Settings;
use crate::sys::udp::{udp_test_bind, BoundUdpPort};
use crate::LocalSocket;

use tokio::task::JoinHandle;
use tokio::time::Duration;

/// VL1 service that connects to the physical network and hosts an inner protocol like ZeroTier VL2.
///
/// This is the "outward facing" half of a full ZeroTier stack on a normal system. It binds sockets,
/// talks to the physical network, manages the vl1 node, and presents a templated interface for
/// whatever inner protocol implementation is using it. This would typically be VL2 but could be
/// a test harness or just the controller for a controller that runs stand-alone.
pub struct VL1Service<
    NodeStorageImpl: NodeStorage + 'static,
    PathFilterImpl: PathFilter + 'static,
    InnerProtocolImpl: InnerProtocol + 'static,
> {
    state: parking_lot::RwLock<VL1ServiceMutableState>,
    storage: Arc<NodeStorageImpl>,
    inner: Arc<InnerProtocolImpl>,
    path_filter: Arc<PathFilterImpl>,
    buffer_pool: PacketBufferPool,
    node_container: Option<Node<Self>>,
}

struct VL1ServiceMutableState {
    daemons: Vec<JoinHandle<()>>,
    udp_sockets: HashMap<u16, parking_lot::RwLock<BoundUdpPort>>,
    settings: VL1Settings,
}

impl<NodeStorageImpl: NodeStorage + 'static, PathFilterImpl: PathFilter + 'static, InnerProtocolImpl: InnerProtocol + 'static>
    VL1Service<NodeStorageImpl, PathFilterImpl, InnerProtocolImpl>
{
    pub async fn new(
        storage: Arc<NodeStorageImpl>,
        inner: Arc<InnerProtocolImpl>,
        path_filter: Arc<PathFilterImpl>,
        settings: VL1Settings,
    ) -> Result<Arc<Self>, Box<dyn Error>> {
        let mut service = VL1Service {
            state: parking_lot::RwLock::new(VL1ServiceMutableState {
                daemons: Vec::with_capacity(2),
                udp_sockets: HashMap::with_capacity(8),
                settings,
            }),
            storage,
            inner,
            path_filter,
            buffer_pool: PacketBufferPool::new(
                std::thread::available_parallelism().map_or(2, |c| c.get() + 2),
                PacketBufferFactory::new(),
            ),
            node_container: None,
        };
        service.node_container.replace(Node::new(&service, &*service.storage, true, false)?);
        let service = Arc::new(service);

        let mut daemons = Vec::new();
        daemons.push(tokio::spawn(service.clone().udp_bind_daemon()));
        daemons.push(tokio::spawn(service.clone().node_background_task_daemon()));
        service.state.write().daemons = daemons;

        Ok(service)
    }

    #[inline(always)]
    pub fn node(&self) -> &Node<Self> {
        debug_assert!(self.node_container.is_some());
        unsafe { self.node_container.as_ref().unwrap_unchecked() }
    }

    pub fn bound_udp_ports(&self) -> Vec<u16> {
        self.state.read().udp_sockets.keys().cloned().collect()
    }

    async fn udp_bind_daemon(self: Arc<Self>) {
        loop {
            {
                let state = self.state.read();
                let mut need_fixed_ports: HashSet<u16> = HashSet::from_iter(state.settings.fixed_ports.iter().cloned());
                let mut have_random_port_count = 0;
                for (p, _) in state.udp_sockets.iter() {
                    need_fixed_ports.remove(p);
                    have_random_port_count += (!state.settings.fixed_ports.contains(p)) as usize;
                }
                let desired_random_port_count = state.settings.random_port_count;

                let state = if !need_fixed_ports.is_empty() || have_random_port_count != desired_random_port_count {
                    drop(state);
                    let mut state = self.state.write();

                    for p in need_fixed_ports.iter() {
                        state.udp_sockets.insert(*p, parking_lot::RwLock::new(BoundUdpPort::new(*p)));
                    }

                    while have_random_port_count > desired_random_port_count {
                        let mut most_stale_binding_liveness = (usize::MAX, i64::MAX);
                        let mut most_stale_binding_port = 0;
                        for (p, s) in state.udp_sockets.iter() {
                            if !state.settings.fixed_ports.contains(p) {
                                let (total_smart_ptr_handles, most_recent_receive) = s.read().liveness();
                                if total_smart_ptr_handles < most_stale_binding_liveness.0
                                    || (total_smart_ptr_handles == most_stale_binding_liveness.0
                                        && most_recent_receive <= most_stale_binding_liveness.1)
                                {
                                    most_stale_binding_liveness.0 = total_smart_ptr_handles;
                                    most_stale_binding_liveness.1 = most_recent_receive;
                                    most_stale_binding_port = *p;
                                }
                            }
                        }
                        if most_stale_binding_port != 0 {
                            have_random_port_count -= state.udp_sockets.remove(&most_stale_binding_port).is_some() as usize;
                        } else {
                            break;
                        }
                    }

                    'outer_add_port_loop: while have_random_port_count < desired_random_port_count {
                        let rn = random::xorshift64_random() as usize;
                        for i in 0..UNASSIGNED_PRIVILEGED_PORTS.len() {
                            let p = UNASSIGNED_PRIVILEGED_PORTS[rn.wrapping_add(i) % UNASSIGNED_PRIVILEGED_PORTS.len()];
                            if !state.udp_sockets.contains_key(&p) && udp_test_bind(p) {
                                let _ = state.udp_sockets.insert(p, parking_lot::RwLock::new(BoundUdpPort::new(p)));
                                continue 'outer_add_port_loop;
                            }
                        }

                        let p = 50000 + ((random::xorshift64_random() as u16) % 15535);
                        if !state.udp_sockets.contains_key(&p) && udp_test_bind(p) {
                            have_random_port_count += state
                                .udp_sockets
                                .insert(p, parking_lot::RwLock::new(BoundUdpPort::new(p)))
                                .is_none() as usize;
                        }
                    }

                    drop(state);
                    self.state.read()
                } else {
                    state
                };

                let num_cores = std::thread::available_parallelism().map_or(1, |c| c.get());
                for (_, binding) in state.udp_sockets.iter() {
                    let mut binding = binding.write();
                    let (_, mut new_sockets) =
                        binding.update_bindings(&state.settings.interface_prefix_blacklist, &state.settings.cidr_blacklist);
                    for s in new_sockets.drain(..) {
                        // Start one async task per system core. This is technically not necessary because tokio
                        // schedules and multiplexes, but this enables tokio to grab and schedule packets
                        // concurrently for up to the number of cores available for any given socket and is
                        // probably faster than other patterns that involve iterating through sockets and creating
                        // arrays of futures or using channels.
                        let mut socket_tasks = Vec::with_capacity(num_cores);
                        for _ in 0..num_cores {
                            let self_copy = self.clone();
                            let s_copy = s.clone();
                            let local_socket = LocalSocket::new(&s);
                            socket_tasks.push(tokio::spawn(async move {
                                loop {
                                    let mut buf = self_copy.buffer_pool.get();
                                    let now = ms_monotonic();
                                    if let Ok((bytes, from_sockaddr)) = s_copy.receive(unsafe { buf.entire_buffer_mut() }, now).await {
                                        unsafe { buf.set_size_unchecked(bytes) };
                                        self_copy.node().handle_incoming_physical_packet(
                                            &*self_copy,
                                            &*self_copy.inner,
                                            &Endpoint::IpUdp(InetAddress::from(from_sockaddr)),
                                            &local_socket,
                                            &s_copy.interface,
                                            buf,
                                        );
                                    }
                                }
                            }));
                        }
                        debug_assert!(s.associated_tasks.lock().is_empty());
                        *s.associated_tasks.lock() = socket_tasks;
                    }
                }
            }

            tokio::time::sleep(Duration::from_secs(10)).await;
        }
    }

    async fn node_background_task_daemon(self: Arc<Self>) {
        tokio::time::sleep(Duration::from_secs(1)).await;
        loop {
            tokio::time::sleep(self.node().do_background_tasks(self.as_ref())).await;
        }
    }
}

impl<NodeStorageImpl: NodeStorage, PathFilterImpl: PathFilter, InnerProtocolImpl: InnerProtocol> HostSystem
    for VL1Service<NodeStorageImpl, PathFilterImpl, InnerProtocolImpl>
{
    type LocalSocket = crate::LocalSocket;
    type LocalInterface = crate::LocalInterface;

    fn event(&self, event: Event) {
        println!("{}", event.to_string());
        match event {
            _ => {}
        }
    }

    #[inline(always)]
    fn local_socket_is_valid(&self, socket: &Self::LocalSocket) -> bool {
        socket.is_valid()
    }

    fn wire_send(
        &self,
        endpoint: &Endpoint,
        local_socket: Option<&Self::LocalSocket>,
        local_interface: Option<&Self::LocalInterface>,
        data: &[u8],
        packet_ttl: u8,
    ) {
        match endpoint {
            Endpoint::IpUdp(address) => {
                // This is the fast path -- the socket is known to the core so just send it.
                if let Some(s) = local_socket {
                    if let Some(s) = s.0.upgrade() {
                        s.send_sync_nonblock(address, data, packet_ttl);
                    } else {
                        return;
                    }
                }

                let state = self.state.read();
                if !state.udp_sockets.is_empty() {
                    if let Some(specific_interface) = local_interface {
                        // Send from a specific interface if that interface is specified.
                        'socket_search: for (_, p) in state.udp_sockets.iter() {
                            let p = p.read();
                            if !p.sockets.is_empty() {
                                let mut i = (random::next_u32_secure() as usize) % p.sockets.len();
                                for _ in 0..p.sockets.len() {
                                    let s = p.sockets.get(i).unwrap();
                                    if s.interface.eq(specific_interface) {
                                        if s.send_sync_nonblock(address, data, packet_ttl) {
                                            break 'socket_search;
                                        }
                                    }
                                    i = (i + 1) % p.sockets.len();
                                }
                            }
                        }
                    } else {
                        // Otherwise send from one socket on every interface.
                        let mut sent_on_interfaces = HashSet::with_capacity(4);
                        for p in state.udp_sockets.values() {
                            let p = p.read();
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
                    }
                }
            }
            _ => {}
        }
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

impl<NodeStorageImpl: NodeStorage, PathFilterImpl: PathFilter, InnerProtocolImpl: InnerProtocol> Drop
    for VL1Service<NodeStorageImpl, PathFilterImpl, InnerProtocolImpl>
{
    fn drop(&mut self) {
        let mut state = self.state.write();
        for d in state.daemons.drain(..) {
            d.abort();
        }
        state.udp_sockets.clear();
    }
}
