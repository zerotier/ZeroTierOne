// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::collections::{HashMap, HashSet};
use std::error::Error;
use std::sync::Arc;
use std::sync::RwLock;
use std::thread::JoinHandle;
use std::time::Duration;

use zerotier_crypto::random;
use zerotier_network_hypervisor::protocol::{PacketBufferFactory, PacketBufferPool};
use zerotier_network_hypervisor::vl1::*;
use zerotier_utils::{ms_monotonic, ms_since_epoch};

use crate::constants::UNASSIGNED_PRIVILEGED_PORTS;
use crate::sys::udp::{udp_test_bind, BoundUdpPort, UdpPacketHandler};
use crate::vl1settings::VL1Settings;
use crate::LocalSocket;

/// Update UDP bindings every this many seconds.
const UPDATE_UDP_BINDINGS_EVERY_SECS: usize = 10;

/// VL1 service that connects to the physical network and hosts an inner protocol like ZeroTier VL2.
///
/// This is the "outward facing" half of a full ZeroTier stack on a normal system. It binds sockets,
/// talks to the physical network, manages the vl1 node, and presents a templated interface for
/// whatever inner protocol implementation is using it. This would typically be VL2 but could be
/// a test harness or just the controller for a controller that runs stand-alone.
pub struct VL1Service<
    NodeStorageImpl: NodeStorage + ?Sized + 'static,
    VL1AuthProviderImpl: VL1AuthProvider + ?Sized + 'static,
    InnerProtocolImpl: InnerProtocol + ?Sized + 'static,
> {
    state: RwLock<VL1ServiceMutableState>,
    storage: Arc<NodeStorageImpl>,
    vl1_auth_provider: Arc<VL1AuthProviderImpl>,
    inner: Arc<InnerProtocolImpl>,
    buffer_pool: Arc<PacketBufferPool>,
    node_container: Option<Node>, // never None, set in new()
}

struct VL1ServiceMutableState {
    daemons: Vec<JoinHandle<()>>,
    udp_sockets: HashMap<u16, RwLock<BoundUdpPort>>,
    settings: VL1Settings,
    running: bool,
}

impl<
        NodeStorageImpl: NodeStorage + ?Sized + 'static,
        VL1AuthProviderImpl: VL1AuthProvider + ?Sized + 'static,
        InnerProtocolImpl: InnerProtocol + ?Sized + 'static,
    > VL1Service<NodeStorageImpl, VL1AuthProviderImpl, InnerProtocolImpl>
{
    pub fn new(
        storage: Arc<NodeStorageImpl>,
        vl1_auth_provider: Arc<VL1AuthProviderImpl>,
        inner: Arc<InnerProtocolImpl>,
        settings: VL1Settings,
    ) -> Result<Arc<Self>, Box<dyn Error>> {
        let mut service = Self {
            state: RwLock::new(VL1ServiceMutableState {
                daemons: Vec::with_capacity(2),
                udp_sockets: HashMap::with_capacity(8),
                settings,
                running: true,
            }),
            storage,
            vl1_auth_provider,
            inner,
            buffer_pool: Arc::new(PacketBufferPool::new(
                std::thread::available_parallelism().map_or(2, |c| c.get() + 2),
                PacketBufferFactory::new(),
            )),
            node_container: None,
        };

        service.node_container.replace(Node::new(&service, true, false)?);
        let service = Arc::new(service);

        let mut daemons = Vec::new();
        let s = service.clone();
        daemons.push(std::thread::spawn(move || {
            s.background_task_daemon();
        }));
        service.state.write().unwrap().daemons = daemons;

        Ok(service)
    }

    #[inline(always)]
    pub fn node(&self) -> &Node {
        debug_assert!(self.node_container.is_some());
        unsafe { self.node_container.as_ref().unwrap_unchecked() }
    }

    pub fn bound_udp_ports(&self) -> Vec<u16> {
        self.state.read().unwrap().udp_sockets.keys().cloned().collect()
    }

    fn update_udp_bindings(self: &Arc<Self>) {
        let state = self.state.read().unwrap();
        let mut need_fixed_ports: HashSet<u16> = HashSet::from_iter(state.settings.fixed_ports.iter().cloned());
        let mut have_random_port_count = 0;
        for (p, _) in state.udp_sockets.iter() {
            need_fixed_ports.remove(p);
            have_random_port_count += (!state.settings.fixed_ports.contains(p)) as usize;
        }
        let desired_random_port_count = state.settings.random_port_count;

        let state = if !need_fixed_ports.is_empty() || have_random_port_count != desired_random_port_count {
            drop(state);
            let mut state = self.state.write().unwrap();

            for p in need_fixed_ports.iter() {
                state.udp_sockets.insert(*p, RwLock::new(BoundUdpPort::new(*p)));
            }

            while have_random_port_count > desired_random_port_count {
                let mut most_stale_binding_liveness = (usize::MAX, i64::MAX);
                let mut most_stale_binding_port = 0;
                for (p, s) in state.udp_sockets.iter() {
                    if !state.settings.fixed_ports.contains(p) {
                        let (total_smart_ptr_handles, most_recent_receive) = s.read().unwrap().liveness();
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
                        let _ = state.udp_sockets.insert(p, RwLock::new(BoundUdpPort::new(p)));
                        continue 'outer_add_port_loop;
                    }
                }

                let p = 50000 + ((random::xorshift64_random() as u16) % 15535);
                if !state.udp_sockets.contains_key(&p) && udp_test_bind(p) {
                    have_random_port_count += state.udp_sockets.insert(p, RwLock::new(BoundUdpPort::new(p))).is_none() as usize;
                }
            }

            drop(state);
            self.state.read().unwrap()
        } else {
            state
        };

        for (_, binding) in state.udp_sockets.iter() {
            let mut binding = binding.write().unwrap();
            let _ = binding.update_bindings(
                &state.settings.interface_prefix_blacklist,
                &state.settings.cidr_blacklist,
                &self.buffer_pool,
                self,
            );
            // TODO: if no bindings were successful do something with errors
        }
    }

    fn background_task_daemon(self: Arc<Self>) {
        std::thread::sleep(Duration::from_millis(500));
        let mut udp_binding_check_every: usize = 0;
        loop {
            if !self.state.read().unwrap().running {
                break;
            }
            if (udp_binding_check_every % UPDATE_UDP_BINDINGS_EVERY_SECS) == 0 {
                self.update_udp_bindings();
            }
            udp_binding_check_every = udp_binding_check_every.wrapping_add(1);
            std::thread::sleep(self.node().do_background_tasks(self.as_ref()));
        }
    }
}

impl<
        NodeStorageImpl: NodeStorage + ?Sized + 'static,
        VL1AuthProviderImpl: VL1AuthProvider + ?Sized + 'static,
        InnerProtocolImpl: InnerProtocol + ?Sized + 'static,
    > UdpPacketHandler for VL1Service<NodeStorageImpl, VL1AuthProviderImpl, InnerProtocolImpl>
{
    #[inline(always)]
    fn incoming_udp_packet(
        self: &Arc<Self>,
        time_ticks: i64,
        socket: &Arc<crate::sys::udp::BoundUdpSocket>,
        source_address: &InetAddress,
        packet: zerotier_network_hypervisor::protocol::PooledPacketBuffer,
    ) {
        self.node().handle_incoming_physical_packet(
            self.as_ref(),
            self.inner.as_ref(),
            &Endpoint::IpUdp(source_address.clone()),
            &LocalSocket::new(socket),
            &socket.interface,
            time_ticks,
            packet,
        );
    }
}

impl<
        NodeStorageImpl: NodeStorage + ?Sized + 'static,
        VL1AuthProviderImpl: VL1AuthProvider + ?Sized + 'static,
        InnerProtocolImpl: InnerProtocol + ?Sized + 'static,
    > HostSystem for VL1Service<NodeStorageImpl, VL1AuthProviderImpl, InnerProtocolImpl>
{
    type Storage = NodeStorageImpl;
    type LocalSocket = crate::LocalSocket;
    type LocalInterface = crate::LocalInterface;

    fn event(&self, event: Event) {
        println!("{}", event.to_string());
        match event {
            _ => {}
        }
    }

    #[inline]
    fn local_socket_is_valid(&self, socket: &Self::LocalSocket) -> bool {
        socket.is_valid()
    }

    #[inline(always)]
    fn storage(&self) -> &Self::Storage {
        self.storage.as_ref()
    }

    #[inline]
    fn get_buffer(&self) -> zerotier_network_hypervisor::protocol::PooledPacketBuffer {
        self.buffer_pool.get()
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
                        s.send(address, data, packet_ttl);
                    } else {
                        return;
                    }
                }

                let state = self.state.read().unwrap();
                if !state.udp_sockets.is_empty() {
                    if let Some(specific_interface) = local_interface {
                        // Send from a specific interface if that interface is specified.
                        'socket_search: for (_, p) in state.udp_sockets.iter() {
                            let p = p.read().unwrap();
                            if !p.sockets.is_empty() {
                                let mut i = (random::next_u32_secure() as usize) % p.sockets.len();
                                for _ in 0..p.sockets.len() {
                                    let s = p.sockets.get(i).unwrap();
                                    if s.interface.eq(specific_interface) {
                                        if s.send(address, data, packet_ttl) {
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
                            let p = p.read().unwrap();
                            if !p.sockets.is_empty() {
                                let mut i = (random::next_u32_secure() as usize) % p.sockets.len();
                                for _ in 0..p.sockets.len() {
                                    let s = p.sockets.get(i).unwrap();
                                    if !sent_on_interfaces.contains(&s.interface) {
                                        if s.send(address, data, packet_ttl) {
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

impl<
        NodeStorageImpl: NodeStorage + ?Sized + 'static,
        VL1AuthProviderImpl: VL1AuthProvider + ?Sized + 'static,
        InnerProtocolImpl: InnerProtocol + ?Sized + 'static,
    > NodeStorage for VL1Service<NodeStorageImpl, VL1AuthProviderImpl, InnerProtocolImpl>
{
    #[inline(always)]
    fn load_node_identity(&self) -> Option<Identity> {
        self.storage.load_node_identity()
    }

    #[inline(always)]
    fn save_node_identity(&self, id: &Identity) {
        self.storage.save_node_identity(id)
    }
}

impl<
        NodeStorageImpl: NodeStorage + ?Sized + 'static,
        VL1AuthProviderImpl: VL1AuthProvider + ?Sized + 'static,
        InnerProtocolImpl: InnerProtocol + ?Sized + 'static,
    > VL1AuthProvider for VL1Service<NodeStorageImpl, VL1AuthProviderImpl, InnerProtocolImpl>
{
    #[inline(always)]
    fn should_respond_to(&self, id: &Identity) -> bool {
        self.vl1_auth_provider.should_respond_to(id)
    }

    #[inline(always)]
    fn has_trust_relationship(&self, id: &Identity) -> bool {
        self.vl1_auth_provider.has_trust_relationship(id)
    }
}

impl<
        NodeStorageImpl: NodeStorage + ?Sized + 'static,
        VL1AuthProviderImpl: VL1AuthProvider + ?Sized + 'static,
        InnerProtocolImpl: InnerProtocol + ?Sized + 'static,
    > Drop for VL1Service<NodeStorageImpl, VL1AuthProviderImpl, InnerProtocolImpl>
{
    fn drop(&mut self) {
        let mut state = self.state.write().unwrap();
        state.running = false;
        state.udp_sockets.clear();
        let mut daemons: Vec<JoinHandle<()>> = state.daemons.drain(..).collect();
        drop(state);
        for d in daemons.drain(..) {
            let _ = d.join();
        }
    }
}
