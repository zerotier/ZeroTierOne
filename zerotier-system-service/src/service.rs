/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::cell::Cell;
use std::collections::BTreeMap;
use std::net::{SocketAddr, Ipv4Addr, IpAddr, Ipv6Addr};
use std::sync::{Arc, Mutex, Weak};
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Duration;

use zerotier_network_hypervisor::vl1::{Address, Identity, InetAddress, MAC, PacketBuffer};

use futures::StreamExt;
use serde::{Serialize, Deserialize};

use crate::fastudpsocket::*;
use crate::getifaddrs;
use crate::localconfig::*;
use crate::log::Log;
use crate::network::Network;
use crate::store::Store;
use crate::utils::{ms_since_epoch, ms_monotonic};
use crate::httplistener::HttpListener;
use zerotier_network_hypervisor::vl1::inetaddress::IpScope;

/// How often to check for major configuration changes. This shouldn't happen
/// too often since it uses a bit of CPU.
const CONFIG_CHECK_INTERVAL: i64 = 5000;

/// ServiceStatus is the object returned by the API /status endpoint
#[derive(Serialize, Deserialize, Clone, PartialEq, Eq)]
pub struct ServiceStatus {
    #[serde(rename = "objectType")]
    pub object_type: String,
    pub address: Address,
    pub clock: i64,
    #[serde(rename = "startTime")]
    pub start_time: i64,
    pub uptime: i64,
    pub config: LocalConfig,
    pub online: bool,
    #[serde(rename = "publicIdentity")]
    pub public_identity: Identity,
    pub version: String,
    #[serde(rename = "versionMajor")]
    pub version_major: i32,
    #[serde(rename = "versionMinor")]
    pub version_minor: i32,
    #[serde(rename = "versionRev")]
    pub version_revision: i32,
    #[serde(rename = "versionBuild")]
    pub version_build: i32,
    #[serde(rename = "udpLocalEndpoints")]
    pub udp_local_endpoints: Vec<InetAddress>,
    #[serde(rename = "httpLocalEndpoints")]
    pub http_local_endpoints: Vec<InetAddress>,
}

/// Core ZeroTier service, which is sort of just a container for all the things.
pub(crate) struct Service {
    pub(crate) log: Log,
    _node: Cell<Weak<Node<Arc<Service>, Network, Service>>>, // never modified after node is created
    udp_local_endpoints: Mutex<Vec<InetAddress>>,
    http_local_endpoints: Mutex<Vec<InetAddress>>,
    interrupt: Mutex<futures::channel::mpsc::Sender<()>>,
    local_config: Mutex<Arc<LocalConfig>>,
    store: Arc<Store>,
    startup_time: i64,
    startup_time_monotonic: i64,
    run: AtomicBool,
    online: AtomicBool,
}

impl NodeEventHandler<Network> for Service {
    #[inline(always)]
    fn virtual_network_config(&self, network_id: NetworkId, network_obj: &Network, config_op: VirtualNetworkConfigOperation, config: Option<&VirtualNetworkConfig>) {}

    #[inline(always)]
    fn virtual_network_frame(&self, network_id: NetworkId, network_obj: &Network, source_mac: MAC, dest_mac: MAC, ethertype: u16, vlan_id: u16, data: &[u8]) {}

    #[inline(always)]
    fn event(&self, event: Event, event_data: &[u8]) {
        match event {
            Event::Up => {
                d!(self.log, "node startup event received.");
            }

            Event::Down => {
                d!(self.log, "node shutdown event received.");
                self.online.store(false, Ordering::Relaxed);
            }

            Event::Online => {
                d!(self.log, "node is online.");
                self.online.store(true, Ordering::Relaxed);
            }

            Event::Offline => {
                d!(self.log, "node is offline.");
                self.online.store(false, Ordering::Relaxed);
            }

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
                                _ => true,
                            } {
                                self.log.log(tm.to_string());
                            }
                        });
                    });
                }
            }

            Event::UserMessage => {}
        }
    }

    #[inline(always)]
    fn state_put(&self, obj_type: StateObjectType, obj_id: &[u64], obj_data: &[u8]) -> std::io::Result<()> {
        if !obj_data.is_empty() {
            self.store.store_object(&obj_type, obj_id, obj_data)
        } else {
            self.store.erase_object(&obj_type, obj_id);
            Ok(())
        }
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
        lc.virtual_.get(&address).map_or(None, |c: &LocalConfigVirtualConfig| {
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
    pub fn local_config(&self) -> Arc<LocalConfig> {
        self.local_config.lock().unwrap().clone()
    }

    pub fn set_local_config(&self, new_lc: LocalConfig) {
        *(self.local_config.lock().unwrap()) = Arc::new(new_lc);
    }

    /// Get the node running with this service.
    /// This can return None during shutdown because Service holds a weak
    /// reference to Node to avoid circular Arc<> pointers. This will only
    /// return None during shutdown, in which case whatever is happening
    /// should abort as quietly as possible.
    pub fn node(&self) -> Option<Arc<Node<Arc<Service>, Network, Service>>> {
        unsafe { &*self._node.as_ptr() }.upgrade()
    }

    #[inline(always)]
    pub fn store(&self) -> &Arc<Store> {
        &self.store
    }

    pub fn online(&self) -> bool {
        self.online.load(Ordering::Relaxed)
    }

    pub fn shutdown(&self) {
        self.run.store(false, Ordering::Relaxed);
        let _ = self.interrupt.lock().unwrap().try_send(());
    }

    /// Get service status for API, or None if a shutdown is in progress.
    pub fn status(&self) -> Option<ServiceStatus> {
        let ver = zerotier_core::version();
        self.node().map(|node| {
            ServiceStatus {
                object_type: "status".to_owned(),
                address: node.address(),
                clock: ms_since_epoch(),
                start_time: self.startup_time,
                uptime: ms_monotonic() - self.startup_time_monotonic,
                config: (*self.local_config()).clone(),
                online: self.online(),
                public_identity: node.identity().clone(),
                version: format!("{}.{}.{}", ver.0, ver.1, ver.2),
                version_major: ver.0,
                version_minor: ver.1,
                version_revision: ver.2,
                version_build: ver.3,
                udp_local_endpoints: self.udp_local_endpoints.lock().unwrap().clone(),
                http_local_endpoints: self.http_local_endpoints.lock().unwrap().clone(),
            }
        })
    }
}

unsafe impl Send for Service {}

unsafe impl Sync for Service {}

async fn run_async(store: Arc<Store>, local_config: Arc<LocalConfig>) -> i32 {
    let process_exit_value: i32 = 0;

    let mut udp_sockets: BTreeMap<InetAddress, FastUDPSocket> = BTreeMap::new();
    let mut http_listeners: BTreeMap<InetAddress, HttpListener> = BTreeMap::new();
    let mut loopback_http_listeners: (Option<HttpListener>, Option<HttpListener>) = (None, None); // 127.0.0.1, ::1

    let (interrupt_tx, mut interrupt_rx) = futures::channel::mpsc::channel::<()>(1);
    let service = Arc::new(Service {
        log: Log::new(
            if local_config.settings.log.path.as_ref().is_some() {
                local_config.settings.log.path.as_ref().unwrap().as_str()
            } else {
                store.default_log_path.to_str().unwrap()
            },
            local_config.settings.log.max_size,
            local_config.settings.log.stderr,
            local_config.settings.log.debug,
            "",
        ),
        _node: Cell::new(Weak::new()),
        udp_local_endpoints: Mutex::new(Vec::new()),
        http_local_endpoints: Mutex::new(Vec::new()),
        interrupt: Mutex::new(interrupt_tx),
        local_config: Mutex::new(local_config),
        store: store.clone(),
        startup_time: ms_since_epoch(),
        startup_time_monotonic: ms_monotonic(),
        run: AtomicBool::new(true),
        online: AtomicBool::new(false),
    });

    let node = Node::new(service.clone(), ms_since_epoch(), ms_monotonic());
    if node.is_err() {
        service.log.fatal(format!("error initializing node: {}", node.err().unwrap().to_str()));
        return 1;
    }
    let node = Arc::new(node.ok().unwrap());
    service._node.replace(Arc::downgrade(&node));

    let mut local_config = service.local_config();

    let mut ticks: i64 = ms_monotonic();
    let mut loop_delay = zerotier_core::NODE_BACKGROUND_TASKS_MAX_INTERVAL;
    let mut last_checked_config: i64 = 0;
    while service.run.load(Ordering::Relaxed) {
        let loop_delay_start = ms_monotonic();
        tokio::select! {
            _ = tokio::time::sleep(Duration::from_millis(loop_delay as u64)) => {
                ticks = ms_monotonic();
                let actual_delay = ticks - loop_delay_start;
                if actual_delay > ((loop_delay as i64) * 4_i64) {
                    l!(service.log, "likely sleep/wake detected due to excessive loop delay, cycling links...");
                    // TODO: handle likely sleep/wake or other system interruption
                }
            },
            _ = interrupt_rx.next() => {
                d!(service.log, "inner loop delay interrupted!");
                if !service.run.load(Ordering::Relaxed) {
                    break;
                }
                ticks = ms_monotonic();
            },
            _ = tokio::signal::ctrl_c() => {
                l!(service.log, "exit signal received, shutting down...");
                service.run.store(false, Ordering::Relaxed);
                break;
            },
        }

        if (ticks - last_checked_config) >= CONFIG_CHECK_INTERVAL {
            last_checked_config = ticks;

            let mut bindings_changed = false;

            let _ = store.read_local_conf(true).map(|new_config| new_config.map(|new_config| {
                d!(service.log, "local.conf changed on disk, reloading.");
                service.set_local_config(new_config);
            }));

            let next_local_config = service.local_config();
            if local_config.settings.primary_port != next_local_config.settings.primary_port {
                loopback_http_listeners.0 = None;
                loopback_http_listeners.1 = None;
                bindings_changed = true;
            }
            if local_config.settings.log.max_size != next_local_config.settings.log.max_size {
                service.log.set_max_size(next_local_config.settings.log.max_size);
            }
            if local_config.settings.log.stderr != next_local_config.settings.log.stderr {
                service.log.set_log_to_stderr(next_local_config.settings.log.stderr);
            }
            if local_config.settings.log.debug != next_local_config.settings.log.debug {
                service.log.set_debug(next_local_config.settings.log.debug);
            }
            local_config = next_local_config;

            let mut loopback_dev_name = String::new();
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
                    },
                    IpScope::Loopback => {
                        if loopback_dev_name.is_empty() {
                            loopback_dev_name.push_str(dev);
                        }
                    },
                    _ => {},
                }
            });

            // TODO: need to also inform the core about these IPs...

            for k in udp_sockets.keys().filter_map(|a| if system_addrs.contains_key(a) { None } else { Some(a.clone()) }).collect::<Vec<InetAddress>>().iter() {
                l!(service.log, "unbinding UDP socket at {} (address no longer exists on system or port has changed)", k.to_string());
                udp_sockets.remove(k);
                bindings_changed = true;
            }
            for a in system_addrs.iter() {
                if !udp_sockets.contains_key(a.0) {
                    let _ = FastUDPSocket::new(a.1.as_str(), a.0, |raw_socket: &FastUDPRawOsSocket, from_address: &InetAddress, data: PacketBuffer| {
                        // TODO: incoming packet handler
                    }).map_or_else(|e| {
                        l!(service.log, "error binding UDP socket to {}: {}", a.0.to_string(), e.to_string());
                    }, |s| {
                        l!(service.log, "bound UDP socket at {}", a.0.to_string());
                        udp_sockets.insert(a.0.clone(), s);
                        bindings_changed = true;
                    });
                }
            }

            let mut udp_primary_port_bind_failure = true;
            let mut udp_secondary_port_bind_failure = local_config.settings.secondary_port.is_some();
            for s in udp_sockets.iter() {
                if s.0.port() == local_config.settings.primary_port {
                    udp_primary_port_bind_failure = false;
                    if !udp_secondary_port_bind_failure {
                        break;
                    }
                }
                if s.0.port() == local_config.settings.secondary_port.unwrap() {
                    udp_secondary_port_bind_failure = false;
                    if !udp_primary_port_bind_failure {
                        break;
                    }
                }
            }
            if udp_primary_port_bind_failure {
                if local_config.settings.auto_port_search {
                    // TODO: port hunting
                } else {
                    l!(service.log, "WARNING: failed to bind to any address at primary port {}", local_config.settings.primary_port);
                }
            }
            if udp_secondary_port_bind_failure {
                if local_config.settings.auto_port_search {
                    // TODO: port hunting
                } else {
                    l!(service.log, "WARNING: failed to bind to any address at secondary port {}", local_config.settings.secondary_port.unwrap_or(0));
                }
            }

            for k in http_listeners.keys().filter_map(|a| if system_addrs.contains_key(a) { None } else { Some(a.clone()) }).collect::<Vec<InetAddress>>().iter() {
                l!(service.log, "closing HTTP listener at {} (address no longer exists on system or port has changed)", k.to_string());
                http_listeners.remove(k);
                bindings_changed = true;
            }
            for a in system_addrs.iter() {
                if !http_listeners.contains_key(a.0) {
                    let sa = a.0.to_socketaddr();
                    if sa.is_some() {
                        let wl = HttpListener::new(a.1.as_str(), sa.unwrap(), &service).await.map_or_else(|e| {
                            l!(service.log, "error creating HTTP listener at {}: {}", a.0.to_string(), e.to_string());
                        }, |l| {
                            l!(service.log, "created HTTP listener at {}", a.0.to_string());
                            http_listeners.insert(a.0.clone(), l);
                            bindings_changed = true;
                        });
                    }
                }
            }

            if loopback_http_listeners.0.is_none() {
                let _ = HttpListener::new(loopback_dev_name.as_str(), SocketAddr::new(IpAddr::from(Ipv4Addr::LOCALHOST), local_config.settings.primary_port), &service).await.map(|wl| {
                    loopback_http_listeners.0 = Some(wl);
                    let _ = store.write_uri(format!("http://127.0.0.1:{}/", local_config.settings.primary_port).as_str());
                    bindings_changed = true;
                });
            }
            if loopback_http_listeners.1.is_none() {
                let _ = HttpListener::new(loopback_dev_name.as_str(), SocketAddr::new(IpAddr::from(Ipv6Addr::LOCALHOST), local_config.settings.primary_port), &service).await.map(|wl| {
                    loopback_http_listeners.1 = Some(wl);
                    if loopback_http_listeners.0.is_none() {
                        let _ = store.write_uri(format!("http://[::1]:{}/", local_config.settings.primary_port).as_str());
                    }
                    bindings_changed = true;
                });
            }
            if loopback_http_listeners.0.is_none() && loopback_http_listeners.1.is_none() {
                // TODO: port hunting
                l!(service.log, "CRITICAL: unable to create HTTP endpoint on 127.0.0.1/{} or ::1/{}, service control API will not work!", local_config.settings.primary_port, local_config.settings.primary_port);
            }

            if bindings_changed {
                {
                    let mut udp_local_endpoints = service.udp_local_endpoints.lock().unwrap();
                    udp_local_endpoints.clear();
                    for ep in udp_sockets.iter() {
                        udp_local_endpoints.push(ep.0.clone());
                    }
                    udp_local_endpoints.sort();
                }
                {
                    let mut http_local_endpoints = service.http_local_endpoints.lock().unwrap();
                    http_local_endpoints.clear();
                    for ep in http_listeners.iter() {
                        http_local_endpoints.push(ep.0.clone());
                    }
                    if loopback_http_listeners.0.is_some() {
                        http_local_endpoints.push(InetAddress::new_ipv4_loopback(loopback_http_listeners.0.as_ref().unwrap().address.port()));
                    }
                    if loopback_http_listeners.1.is_some() {
                        http_local_endpoints.push(InetAddress::new_ipv6_loopback(loopback_http_listeners.1.as_ref().unwrap().address.port()));
                    }
                    http_local_endpoints.sort();
                }
            }
        }

        // Run background task handler in ZeroTier core.
        loop_delay = node.process_background_tasks(ms_since_epoch(), ticks);
    }

    l!(service.log, "shutting down normally.");

    drop(udp_sockets);
    drop(http_listeners);
    drop(loopback_http_listeners);
    drop(node);
    drop(service);

    process_exit_value
}

pub(crate) fn run(store: Arc<Store>) -> i32 {
    let local_config = Arc::new(store.read_local_conf_or_default());

    if store.auth_token(true).is_err() {
        eprintln!("FATAL: error writing new web API authorization token (likely permission problem).");
        return 1;
    }
    if store.write_pid().is_err() {
        eprintln!("FATAL: error writing to directory '{}': unable to write zerotier.pid (likely permission problem).", store.base_path.to_str().unwrap());
        return 1;
    }

    let rt = tokio::runtime::Builder::new_current_thread().enable_all().build().unwrap();
    let store2 = store.clone();
    let process_exit_value = rt.block_on(async move { run_async(store2, local_config).await });
    rt.shutdown_timeout(Duration::from_millis(500));

    store.erase_pid();

    process_exit_value
}
