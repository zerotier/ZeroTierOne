/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

use std::collections::BTreeMap;
use std::net::{SocketAddr, Ipv4Addr, IpAddr, Ipv6Addr};
use std::str::FromStr;
use std::sync::{Arc, Mutex, Weak};
use std::sync::atomic::{AtomicBool, Ordering, AtomicPtr};
use std::time::Duration;

use zerotier_core::*;
use zerotier_core::trace::{TraceEvent, TraceEventLayer};
use futures::StreamExt;

use crate::fastudpsocket::*;
use crate::getifaddrs;
use crate::localconfig::*;
use crate::log::Log;
use crate::network::Network;
use crate::store::Store;
use crate::utils::ms_since_epoch;
use crate::weblistener::WebListener;

/// How often to check for major configuration changes. This shouldn't happen
/// too often since it uses a bit of CPU.
const CONFIG_CHECK_INTERVAL: i64 = 5000;

struct ServiceIntl {
    interrupt: Mutex<futures::channel::mpsc::Sender<()>>,
    local_config: Mutex<Arc<LocalConfig>>,
    store: Arc<Store>,
    run: AtomicBool,
    online: AtomicBool,
}

unsafe impl Send for ServiceIntl {}

unsafe impl Sync for ServiceIntl {}

/// Core ZeroTier service, which is sort of just a container for all the things.
#[derive(Clone)]
pub(crate) struct Service {
    pub(crate) log: Arc<Log>,
    _node: Weak<Node<Service, Network>>,
    intl: Arc<ServiceIntl>,
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
                self.intl.online.store(false, Ordering::Relaxed);
            }

            Event::Online => {
                d!(self.log, "node is online.");
                self.intl.online.store(true, Ordering::Relaxed);
            }

            Event::Offline => {
                d!(self.log, "node is offline.");
                self.intl.online.store(false, Ordering::Relaxed);
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
            self.intl.store.store_object(&obj_type, obj_id, obj_data)
        } else {
            self.intl.store.erase_object(&obj_type, obj_id);
            Ok(())
        }
    }

    #[inline(always)]
    fn state_get(&self, obj_type: StateObjectType, obj_id: &[u64]) -> std::io::Result<Vec<u8>> {
        self.intl.store.load_object(&obj_type, obj_id)
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
    #[inline(always)]
    pub fn local_config(&self) -> Arc<LocalConfig> {
        self.intl.local_config.lock().unwrap().clone()
    }

    #[inline(always)]
    pub fn set_local_config(&self, new_lc: LocalConfig) {
        *(self.intl.local_config.lock().unwrap()) = Arc::new(new_lc);
    }

    /// Get the node running with this service.
    /// This can return None if we are in the midst of shutdown. In this case
    /// whatever operation is in progress should abort. None will never be
    /// returned during normal operation.
    #[inline(always)]
    pub fn node(&self) -> Option<Arc<Node<Service, Network>>> {
        self._node.upgrade()
    }

    #[inline(always)]
    pub fn store(&self) -> &Arc<Store> {
        &self.intl.store
    }

    #[inline(always)]
    pub fn online(&self) -> bool {
        self.intl.online.load(Ordering::Relaxed)
    }

    pub fn shutdown(&self) {
        self.intl.run.store(false, Ordering::Relaxed);
        let _ = self.intl.interrupt.lock().unwrap().try_send(());
    }
}

unsafe impl Send for Service {}

unsafe impl Sync for Service {}

async fn run_async(store: Arc<Store>, log: Arc<Log>, local_config: Arc<LocalConfig>) -> i32 {
    let mut process_exit_value: i32 = 0;

    let mut udp_sockets: BTreeMap<InetAddress, FastUDPSocket> = BTreeMap::new();
    let mut web_listeners: BTreeMap<InetAddress, WebListener> = BTreeMap::new();
    let mut local_web_listeners: (Option<WebListener>, Option<WebListener>) = (None, None); // IPv4, IPv6

    let (interrupt_tx, mut interrupt_rx) = futures::channel::mpsc::channel::<()>(1);
    let mut service = Service {
        log: log.clone(),
        _node: Weak::new(),
        intl: Arc::new(ServiceIntl {
            interrupt: Mutex::new(interrupt_tx),
            local_config: Mutex::new(local_config),
            store: store.clone(),
            run: AtomicBool::new(true),
            online: AtomicBool::new(false),
        }),
    };

    let node = Node::new(service.clone(), ms_since_epoch());
    if node.is_err() {
        log.fatal(format!("error initializing node: {}", node.err().unwrap().to_str()));
        return 1;
    }
    let node = Arc::new(node.ok().unwrap());

    service._node = Arc::downgrade(&node);
    let service = service; // make immutable after setting node

    let mut local_config = service.local_config();

    let mut now: i64 = ms_since_epoch();
    let mut loop_delay = zerotier_core::NODE_BACKGROUND_TASKS_MAX_INTERVAL;
    let mut last_checked_config: i64 = 0;
    while service.intl.run.load(Ordering::Relaxed) {
        let loop_start = ms_since_epoch();

        tokio::select! {
            _ = tokio::time::sleep(Duration::from_millis(loop_delay as u64)) => {
                now = ms_since_epoch();
                let actual_delay = now - loop_start;
                if actual_delay > ((loop_delay as i64) * 4_i64) {
                    l!(log, "likely sleep/wake detected due to excess delay, reestablishing links...");
                    // TODO: handle likely sleep/wake or other system interruption
                }
            },
            _ = interrupt_rx.next() => {
                d!(log, "inner loop delay interrupted!");
                if !service.intl.run.load(Ordering::Relaxed) {
                    break;
                }
                now = ms_since_epoch();
            },
            _ = tokio::signal::ctrl_c() => {
                l!(log, "exit signal received, shutting down...");
                service.intl.run.store(false, Ordering::Relaxed);
                break;
            },
        }

        if (now - last_checked_config) >= CONFIG_CHECK_INTERVAL {
            last_checked_config = now;

            let new_config = store.read_local_conf(true);
            if new_config.is_ok() {
                d!(log, "local.conf changed on disk, reloading.");
                service.set_local_config(new_config.unwrap());
            }

            let next_local_config = service.local_config();
            if local_config.settings.primary_port != next_local_config.settings.primary_port {
                local_web_listeners.0 = None;
                local_web_listeners.1 = None;
            }
            if local_config.settings.log.max_size != next_local_config.settings.log.max_size {
                log.set_max_size(next_local_config.settings.log.max_size);
            }
            if local_config.settings.log.stderr != next_local_config.settings.log.stderr {
                log.set_log_to_stderr(next_local_config.settings.log.stderr);
            }
            if local_config.settings.log.debug != next_local_config.settings.log.debug {
                log.set_debug(next_local_config.settings.log.debug);
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

            let mut udp_sockets_to_close: Vec<InetAddress> = Vec::new();
            for sock in udp_sockets.iter() {
                if !system_addrs.contains_key(sock.0) {
                    udp_sockets_to_close.push(sock.0.clone());
                }
            }
            for k in udp_sockets_to_close.iter() {
                l!(log, "unbinding UDP socket at {}", k.to_string());
                udp_sockets.remove(k);
            }

            for addr in system_addrs.iter() {
                if !udp_sockets.contains_key(addr.0) {
                    let _ = FastUDPSocket::new(addr.1.as_str(), addr.0, move |raw_socket: &FastUDPRawOsSocket, from_address: &InetAddress, data: Buffer| {
                        // TODO: incoming packet handler
                    }).map_or_else(|e| {
                        l!(log, "error binding UDP socket to {}: {}", addr.0.to_string(), e.to_string());
                    }, |s| {
                        l!(log, "bound UDP socket at {}", addr.0.to_string());
                        udp_sockets.insert(addr.0.clone(), s);
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
                    l!(log, "WARNING: failed to bind to any address at primary port {}", local_config.settings.primary_port);
                }
            }
            if udp_secondary_port_bind_failure {
                if local_config.settings.auto_port_search {
                    // TODO: port hunting
                } else {
                    l!(log, "WARNING: failed to bind to any address at secondary port {}", local_config.settings.secondary_port.unwrap_or(0));
                }
            }

            let mut web_listeners_to_close: Vec<InetAddress> = Vec::new();
            for l in web_listeners.iter() {
                if !system_addrs.contains_key(l.0) {
                    web_listeners_to_close.push(l.0.clone());
                }
            }
            for k in web_listeners_to_close.iter() {
                l!(log, "closing HTTP listener at {}", k.to_string());
                web_listeners.remove(k);
            }

            for addr in system_addrs.iter() {
                if addr.0.port() == local_config.settings.primary_port && !web_listeners.contains_key(addr.0) {
                    let sa = addr.0.to_socketaddr();
                    if sa.is_some() {
                        let wl = WebListener::new(addr.1.as_str(), sa.unwrap(), &service).await.map_or_else(|e| {
                            l!(log, "error creating HTTP listener at {}: {}", addr.0.to_string(), e.to_string());
                        }, |l| {
                            l!(log, "created HTTP listener at {}", addr.0.to_string());
                            web_listeners.insert(addr.0.clone(), l);
                        });
                    }
                }
            }

            if local_web_listeners.0.is_none() {
                let _ = WebListener::new(loopback_dev_name.as_str(), SocketAddr::new(IpAddr::from(Ipv4Addr::LOCALHOST), local_config.settings.primary_port), &service).await.map(|wl| {
                    local_web_listeners.0 = Some(wl);
                    let _ = store.write_uri(format!("http://127.0.0.1:{}/", local_config.settings.primary_port).as_str());
                });
            }
            if local_web_listeners.1.is_none() {
                let _ = WebListener::new(loopback_dev_name.as_str(), SocketAddr::new(IpAddr::from(Ipv6Addr::LOCALHOST), local_config.settings.primary_port), &service).await.map(|wl| {
                    local_web_listeners.1 = Some(wl);
                    if local_web_listeners.0.is_none() {
                        let _ = store.write_uri(format!("http://[::1]:{}/", local_config.settings.primary_port).as_str());
                    }
                });
            }
            if local_web_listeners.0.is_none() && local_web_listeners.1.is_none() {
                l!(log, "error creating HTTP listener on 127.0.0.1/{} or ::1/{}", local_config.settings.primary_port, local_config.settings.primary_port);
            }
        }

        // Run background task handler in ZeroTier core.
        loop_delay = node.process_background_tasks(now);
    }

    l!(log, "shutting down normally.");

    drop(udp_sockets);
    drop(web_listeners);
    drop(local_web_listeners);
    drop(node);
    drop(service);

    d!(log, "shutdown complete.");

    process_exit_value
}

pub(crate) fn run(store: Arc<Store>) -> i32 {
    let local_config = Arc::new(store.read_local_conf(false).unwrap_or_else(|_| { LocalConfig::default() }));

    let log = Arc::new(Log::new(
        if local_config.settings.log.path.as_ref().is_some() {
            local_config.settings.log.path.as_ref().unwrap().as_str()
        } else {
            store.default_log_path.to_str().unwrap()
        },
        local_config.settings.log.max_size,
        local_config.settings.log.stderr,
        local_config.settings.log.debug,
        "",
    ));

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
    let process_exit_value = rt.block_on(async move { run_async(store2, log, local_config).await });
    rt.shutdown_timeout(Duration::from_millis(500));

    store.erase_pid();

    process_exit_value
}
