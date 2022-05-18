/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::collections::HashMap;
use std::hash::Hash;
use std::num::NonZeroI64;
use std::sync::atomic::AtomicUsize;
use std::sync::{Arc, Weak};

use tokio::net::UdpSocket;

use zerotier_network_hypervisor::vl1::*;
use zerotier_network_hypervisor::vl2::*;
use zerotier_network_hypervisor::*;

use crate::datadir::DataDir;
use crate::udp::BoundUdpSocket;
use crate::utils::{ms_monotonic, ms_since_epoch};

pub type DynamicError = Box<dyn Error>;

pub struct Service {
    pub rt: tokio::runtime::Handle,
    pub data: DataDir,
    pub local_socket_unique_id_counter: AtomicUsize,
    pub udp_sockets: parking_lot::RwLock<HashMap<u16, Vec<Arc<BoundUdpSocket>>>>,
    pub core: Option<NetworkHypervisor<Self>>,
}

impl Service {
    pub async fn new(base_path: &str) -> Result<Self, DynamicError> {
        let mut svc = Self {
            rt: tokio::runtime::Handle::current(),
            data: DataDir::open(base_path).await.map_err(|e| Box::new(e))?,
            local_socket_unique_id_counter: AtomicUsize::new(1),
            udp_sockets: parking_lot::RwLock::new(HashMap::with_capacity(4)),
            core: None,
        };
        let _ = svc.core.insert(NetworkHypervisor::new(&svc, true).map_err(|e| Box::new(e))?);

        let config = svc.data.config().await;

        Ok(svc)
    }
}

/// Local socket wrapper implementing equality and hash in terms of an arbitrary unique ID.
#[derive(Clone)]
struct LocalSocket(Weak<BoundUdpSocket>, usize);

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

impl SystemInterface for Service {
    type LocalSocket = crate::service::LocalSocket;

    type LocalInterface = String;

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
        todo!()
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

impl SwitchInterface for Service {}

impl Interface for Service {}
