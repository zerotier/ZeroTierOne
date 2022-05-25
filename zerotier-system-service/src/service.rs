// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::collections::{HashMap, HashSet};
use std::error::Error;
use std::hash::Hash;
use std::path::Path;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::{Arc, Weak};

use zerotier_network_hypervisor::vl1::*;
use zerotier_network_hypervisor::vl2::*;
use zerotier_network_hypervisor::*;

use zerotier_core_crypto::random;

use tokio::time::Duration;

use crate::datadir::DataDir;
use crate::localinterface::LocalInterface;
use crate::udp::{BoundUdpPort, BoundUdpSocket};
use crate::utils::{ms_monotonic, ms_since_epoch};

const UDP_UPDATE_BINDINGS_INTERVAL_MS: Duration = Duration::from_millis(2500);

/// ZeroTier system service, which presents virtual networks as VPN connections.
pub struct Service {
    udp_binding_task: tokio::task::JoinHandle<()>,
    core_background_service_task: tokio::task::JoinHandle<()>,
    internal: Arc<ServiceImpl>,
}

struct ServiceImpl {
    pub rt: tokio::runtime::Handle,
    pub data: DataDir,
    pub local_socket_unique_id_counter: AtomicUsize,
    pub udp_sockets: tokio::sync::RwLock<HashMap<u16, BoundUdpPort>>,
    pub num_listeners_per_socket: usize,
    _core: Option<NetworkHypervisor<Self>>,
}

impl Drop for Service {
    fn drop(&mut self) {
        self.internal.rt.block_on(async {
            // Kill all background tasks associated with this service.
            self.udp_binding_task.abort();
            self.core_background_service_task.abort();

            // Wait for all tasks to actually stop.
            let _ = self.udp_binding_task.await;
            let _ = self.core_background_service_task.await;

            // Drop all bound sockets since these can hold circular Arc<> references to 'internal'.
            self.internal.udp_sockets.write().await.clear();
        });
    }
}

impl Service {
    pub async fn new<P: AsRef<Path>>(rt: tokio::runtime::Handle, base_path: P, auto_upgrade_identity: bool) -> Result<Self, Box<dyn Error>> {
        let mut si = ServiceImpl {
            rt,
            data: DataDir::open(base_path).await.map_err(|e| Box::new(e))?,
            local_socket_unique_id_counter: AtomicUsize::new(1),
            udp_sockets: tokio::sync::RwLock::new(HashMap::with_capacity(4)),
            num_listeners_per_socket: std::thread::available_parallelism().unwrap().get(),
            _core: None,
        };
        let _ = si._core.insert(NetworkHypervisor::new(&si, true, auto_upgrade_identity)?);
        let si = Arc::new(si);

        let (si1, si2) = (si.clone(), si.clone());
        Ok(Self {
            udp_binding_task: si.rt.spawn(si1.udp_binding_task_main()),
            core_background_service_task: si.rt.spawn(si2.core_background_service_task_main()),
            internal: si,
        })
    }
}

impl ServiceImpl {
    #[inline(always)]
    fn core(&self) -> &NetworkHypervisor<ServiceImpl> {
        debug_assert!(self._core.is_some());
        unsafe { self._core.as_ref().unwrap_unchecked() }
    }

    async fn update_bindings_for_port(self: &Arc<Self>, port: u16, interface_prefix_blacklist: &Vec<String>, cidr_blacklist: &Vec<InetAddress>) -> Option<Vec<(LocalInterface, InetAddress, std::io::Error)>> {
        let mut udp_sockets = self.udp_sockets.write().await;
        let bp = udp_sockets.entry(port).or_insert_with(|| BoundUdpPort::new(port));
        let (errors, new_sockets) = bp.update_bindings(interface_prefix_blacklist, cidr_blacklist);
        if bp.sockets.is_empty() {
            return Some(errors);
        }
        drop(udp_sockets); // release lock

        for ns in new_sockets.iter() {
            // We start a task for each CPU core. Tokio multiplexes but since each packet takes a bit of CPU
            // to parse, decrypt, etc. we want to be able to saturate the CPU for any given socket to virtual
            // network path. The alternative would be to use MPMC channels but that would almost certainly be
            // a lot slower as it would involve more sync/atomic bottlenecks and probably extra malloc/free.
            let mut kill_on_drop = ns.kill_on_drop.lock();
            for _ in 0..self.num_listeners_per_socket {
                let self2 = self.clone();
                let socket = ns.socket.clone();
                let interface = ns.interface.clone();
                let local_socket = LocalSocket(Arc::downgrade(ns), self.local_socket_unique_id_counter.fetch_add(1, Ordering::SeqCst));
                kill_on_drop.push(self.rt.spawn(async move {
                    let core = self2.core();
                    loop {
                        let mut buf = core.get_packet_buffer();
                        if let Ok((bytes, source)) = socket.recv_from(unsafe { buf.entire_buffer_mut() }).await {
                            unsafe { buf.set_size_unchecked(bytes) };
                            core.handle_incoming_physical_packet(&self2, &Endpoint::IpUdp(InetAddress::from(source)), &local_socket, &interface, buf);
                        } else {
                            break;
                        }
                    }
                }));
            }
        }

        return None;
    }

    async fn udp_binding_task_main(self: Arc<Self>) {
        loop {
            let config = self.data.config().await;

            if let Some(errors) = self.update_bindings_for_port(config.settings.primary_port, &config.settings.interface_prefix_blacklist, &config.settings.cidr_blacklist).await {
                for e in errors.iter() {
                    println!("BIND ERROR: {} {} {}", e.0.to_string(), e.1.to_string(), e.2.to_string());
                }
                // TODO: report errors properly
            }

            tokio::time::sleep(UDP_UPDATE_BINDINGS_INTERVAL_MS).await;
        }
    }

    async fn core_background_service_task_main(self: Arc<Self>) {
        tokio::time::sleep(Duration::from_secs(1)).await;
        loop {
            tokio::time::sleep(self.core().do_background_tasks(&self)).await;
        }
    }
}

impl SystemInterface for ServiceImpl {
    type LocalSocket = crate::service::LocalSocket;
    type LocalInterface = crate::localinterface::LocalInterface;

    fn event_node_is_up(&self) {}

    fn event_node_is_down(&self) {}

    fn event_online_status_change(&self, online: bool) {}

    fn event_user_message(&self, source: &Identity, message_type: u64, message: &[u8]) {}

    fn event_security_warning(&self, warning: &str) {}

    fn local_socket_is_valid(&self, socket: &Self::LocalSocket) -> bool {
        socket.0.strong_count() > 0
    }

    fn load_node_identity(&self) -> Option<Identity> {
        self.rt.block_on(async { self.data.load_identity().await.map_or(None, |i| Some(i)) })
    }

    fn save_node_identity(&self, id: &Identity) {
        self.rt.block_on(async { assert!(self.data.save_identity(id).await.is_ok()) });
    }

    fn wire_send(&self, endpoint: &Endpoint, local_socket: Option<&Self::LocalSocket>, local_interface: Option<&Self::LocalInterface>, data: &[&[u8]], packet_ttl: u8) -> bool {
        match endpoint {
            Endpoint::IpUdp(address) => {
                // This is the fast path -- the socket is known to the core so just send it.
                if let Some(s) = local_socket {
                    if let Some(s) = s.0.upgrade() {
                        return s.send_sync_nonblock(&self.rt, address, data, packet_ttl);
                    } else {
                        return false;
                    }
                }

                // Otherwise we try to send from one socket on every interface or from the specified interface.
                // This path only happens when the core is trying new endpoints. The fast path is for most packets.
                return self.rt.block_on(async {
                    let sockets = self.udp_sockets.read().await;
                    if !sockets.is_empty() {
                        if let Some(specific_interface) = local_interface {
                            for (_, p) in sockets.iter() {
                                for s in p.sockets.iter() {
                                    if s.interface.eq(specific_interface) {
                                        if s.send_async(&self.rt, address, data, packet_ttl).await {
                                            return true;
                                        }
                                    }
                                }
                            }
                        } else {
                            let bound_ports: Vec<&u16> = sockets.keys().collect();
                            let mut sent_on_interfaces = HashSet::with_capacity(4);
                            let rn = random::xorshift64_random() as usize;
                            for i in 0..bound_ports.len() {
                                let p = sockets.get(*bound_ports.get(rn.wrapping_add(i) % bound_ports.len()).unwrap()).unwrap();
                                for s in p.sockets.iter() {
                                    if !sent_on_interfaces.contains(&s.interface) {
                                        if s.send_async(&self.rt, address, data, packet_ttl).await {
                                            sent_on_interfaces.insert(s.interface.clone());
                                        }
                                    }
                                }
                            }
                            return !sent_on_interfaces.is_empty();
                        }
                    }
                    return false;
                });
            }
            _ => {}
        }
        return false;
    }

    fn check_path(&self, id: &Identity, endpoint: &Endpoint, local_socket: Option<&Self::LocalSocket>, local_interface: Option<&Self::LocalInterface>) -> bool {
        true
    }

    fn get_path_hints(&self, id: &Identity) -> Option<Vec<(Endpoint, Option<Self::LocalSocket>, Option<Self::LocalInterface>)>> {
        None
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

impl SwitchInterface for ServiceImpl {}

impl Interface for ServiceImpl {}

/// Local socket wrapper to provide to the core.
///
/// This implements very fast hash and equality in terms of an arbitrary unique ID assigned at
/// construction and holds a weak reference to the bound socket so dead sockets will silently
/// cease to exist or work. This also means that this code can check the weak count to determine
/// if the core is currently holding/using a socket for any reason.
#[derive(Clone)]
pub struct LocalSocket(Weak<BoundUdpSocket>, usize);

impl LocalSocket {
    /// Returns true if the wrapped socket appears to be in use by the core.
    #[inline(always)]
    pub fn in_use(&self) -> bool {
        self.0.weak_count() > 0
    }

    #[inline(always)]
    pub fn socket(&self) -> Option<Arc<BoundUdpSocket>> {
        self.0.upgrade()
    }
}

impl PartialEq for LocalSocket {
    #[inline(always)]
    fn eq(&self, other: &Self) -> bool {
        self.1 == other.1
    }
}

impl Eq for LocalSocket {}

impl Hash for LocalSocket {
    #[inline(always)]
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.1.hash(state)
    }
}

impl ToString for LocalSocket {
    fn to_string(&self) -> String {
        if let Some(s) = self.0.upgrade() {
            s.address.to_string()
        } else {
            "(closed socket)".into()
        }
    }
}
