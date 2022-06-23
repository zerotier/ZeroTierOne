// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::collections::{HashMap, HashSet};
use std::error::Error;
use std::path::Path;
use std::sync::Arc;

use async_trait::async_trait;

use zerotier_network_hypervisor::vl1::*;
use zerotier_network_hypervisor::vl2::*;
use zerotier_network_hypervisor::*;

use zerotier_core_crypto::random;

use tokio::time::Duration;

use crate::datadir::DataDir;
use crate::localinterface::LocalInterface;
use crate::localsocket::LocalSocket;
use crate::udp::*;
use crate::utils::{ms_monotonic, ms_since_epoch};

const UDP_UPDATE_BINDINGS_INTERVAL_MS: Duration = Duration::from_millis(2500);

/// ZeroTier system service, which presents virtual networks as VPN connections on Windows/macOS/Linux/BSD/etc.
pub struct Service {
    udp_binding_task: tokio::task::JoinHandle<()>,
    core_background_service_task: tokio::task::JoinHandle<()>,
    internal: Arc<ServiceImpl>,
}

struct ServiceImpl {
    pub rt: tokio::runtime::Handle,
    pub data: DataDir,
    pub udp_sockets: tokio::sync::RwLock<HashMap<u16, BoundUdpPort>>,
    pub num_listeners_per_socket: usize,
    _core: Option<NetworkHypervisor<Self>>,
}

impl Drop for Service {
    fn drop(&mut self) {
        // Kill all background tasks associated with this service.
        self.udp_binding_task.abort();
        self.core_background_service_task.abort();

        // Drop all bound sockets since these can hold circular Arc<> references to 'internal'.
        // This shouldn't have to loop much if at all to acquire the lock, but it might if something
        // is still completing somewhere in an aborting task.
        loop {
            if let Ok(mut udp_sockets) = self.internal.udp_sockets.try_write() {
                udp_sockets.clear();
                break;
            }
            std::thread::sleep(Duration::from_millis(5));
        }
    }
}

impl Service {
    /// Start ZeroTier service.
    ///
    /// This launches a number of background tasks in the async runtime that will run as long as this object exists.
    /// When this is dropped these tasks are killed.
    pub async fn new<P: AsRef<Path>>(rt: tokio::runtime::Handle, base_path: P, auto_upgrade_identity: bool) -> Result<Self, Box<dyn Error>> {
        let mut si = ServiceImpl {
            rt,
            data: DataDir::open(base_path).await.map_err(|e| Box::new(e))?,
            udp_sockets: tokio::sync::RwLock::new(HashMap::with_capacity(4)),
            num_listeners_per_socket: std::thread::available_parallelism().unwrap().get(),
            _core: None,
        };
        let _ = si._core.insert(NetworkHypervisor::new(&si, true, auto_upgrade_identity).await?);
        let si = Arc::new(si);

        si._core.as_ref().unwrap().add_update_default_root_set_if_none();

        Ok(Self {
            udp_binding_task: si.rt.spawn(si.clone().udp_binding_task_main()),
            core_background_service_task: si.rt.spawn(si.clone().core_background_service_task_main()),
            internal: si,
        })
    }
}

impl ServiceImpl {
    #[inline(always)]
    fn core(&self) -> &NetworkHypervisor<ServiceImpl> {
        self._core.as_ref().unwrap()
    }

    /// Called in udp_binding_task_main() to service a particular UDP port.
    async fn update_udp_bindings_for_port(self: &Arc<Self>, port: u16, interface_prefix_blacklist: &Vec<String>, cidr_blacklist: &Vec<InetAddress>) -> Option<Vec<(LocalInterface, InetAddress, std::io::Error)>> {
        for ns in {
            let mut udp_sockets = self.udp_sockets.write().await;
            let bp = udp_sockets.entry(port).or_insert_with(|| BoundUdpPort::new(port));
            let (errors, new_sockets) = bp.update_bindings(interface_prefix_blacklist, cidr_blacklist);
            if bp.sockets.is_empty() {
                return Some(errors);
            }
            new_sockets
        }
        .iter()
        {
            /*
             * Start a task (not actual thread) for each CPU core.
             *
             * The async runtime is itself multithreaded but each packet takes a little bit of CPU to handle.
             * This makes sure that when one packet is in processing the async runtime is immediately able to
             * cue up another receiver for this socket.
             */
            for _ in 0..self.num_listeners_per_socket {
                let self2 = self.clone();
                let socket = ns.socket.clone();
                let interface = ns.interface.clone();
                let local_socket = LocalSocket::new(ns);
                ns.socket_associated_tasks.lock().push(self.rt.spawn(async move {
                    let core = self2.core();
                    loop {
                        let mut buf = core.get_packet_buffer();
                        if let Ok((bytes, source)) = socket.recv_from(unsafe { buf.entire_buffer_mut() }).await {
                            unsafe { buf.set_size_unchecked(bytes) };
                            core.handle_incoming_physical_packet(&self2, &Endpoint::IpUdp(InetAddress::from(source)), &local_socket, &interface, buf).await;
                        } else {
                            break;
                        }
                    }
                }));
            }
        }
        return None;
    }

    /// Background task to update per-interface/per-port bindings if system interface configuration changes.
    async fn udp_binding_task_main(self: Arc<Self>) {
        loop {
            let config = self.data.config().await;

            if let Some(errors) = self.update_udp_bindings_for_port(config.settings.primary_port, &config.settings.interface_prefix_blacklist, &config.settings.cidr_blacklist).await {
                for e in errors.iter() {
                    println!("BIND ERROR: {} {} {}", e.0.to_string(), e.1.to_string(), e.2.to_string());
                }
                // TODO: report errors properly
            }

            tokio::time::sleep(UDP_UPDATE_BINDINGS_INTERVAL_MS).await;
        }
    }

    /// Periodically calls do_background_tasks() in the ZeroTier core.
    async fn core_background_service_task_main(self: Arc<Self>) {
        tokio::time::sleep(Duration::from_secs(1)).await;
        loop {
            tokio::time::sleep(self.core().do_background_tasks(&self).await).await;
        }
    }
}

#[async_trait]
impl SystemInterface for ServiceImpl {
    type LocalSocket = crate::service::LocalSocket;
    type LocalInterface = crate::localinterface::LocalInterface;

    fn event(&self, event: Event) {
        println!("{}", event.to_string());
        match event {
            _ => {}
        }
    }

    async fn user_message(&self, _source: &Identity, _message_type: u64, _message: &[u8]) {}

    #[inline(always)]
    fn local_socket_is_valid(&self, socket: &Self::LocalSocket) -> bool {
        socket.0.strong_count() > 0
    }

    async fn load_node_identity(&self) -> Option<Identity> {
        self.data.load_identity().await.map_or(None, |i| Some(i))
    }

    async fn save_node_identity(&self, id: &Identity) {
        assert!(self.data.save_identity(id).await.is_ok())
    }

    async fn wire_send(&self, endpoint: &Endpoint, local_socket: Option<&Self::LocalSocket>, local_interface: Option<&Self::LocalInterface>, data: &[&[u8]], packet_ttl: u8) -> bool {
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

                // Otherwise we try to send from one socket on every interface or from the specified interface.
                // This path only happens when the core is trying new endpoints. The fast path is for most packets.
                let sockets = self.udp_sockets.read().await;
                if !sockets.is_empty() {
                    if let Some(specific_interface) = local_interface {
                        for (_, p) in sockets.iter() {
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
                        let bound_ports: Vec<&u16> = sockets.keys().collect();
                        let mut sent_on_interfaces = HashSet::with_capacity(4);
                        let rn = random::xorshift64_random() as usize;
                        for i in 0..bound_ports.len() {
                            let p = sockets.get(*bound_ports.get(rn.wrapping_add(i) % bound_ports.len()).unwrap()).unwrap();
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

    async fn check_path(&self, _id: &Identity, endpoint: &Endpoint, _local_socket: Option<&Self::LocalSocket>, _local_interface: Option<&Self::LocalInterface>) -> bool {
        let config = self.data.config().await;
        if let Some(pps) = config.physical.get(endpoint) {
            !pps.blacklist
        } else {
            true
        }
    }

    async fn get_path_hints(&self, id: &Identity) -> Option<Vec<(Endpoint, Option<Self::LocalSocket>, Option<Self::LocalInterface>)>> {
        let config = self.data.config().await;
        if let Some(vns) = config.virtual_.get(&id.address) {
            Some(vns.try_.iter().map(|ep| (ep.clone(), None, None)).collect())
        } else {
            None
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

impl SwitchInterface for ServiceImpl {}

impl Interface for ServiceImpl {}
