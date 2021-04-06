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

use std::collections::hash_map::HashMap;
use std::intrinsics::copy_nonoverlapping;
use std::mem::{MaybeUninit, transmute};
use std::os::raw::{c_int, c_uint, c_ulong, c_void};
use std::pin::Pin;
use std::ptr::{null_mut, slice_from_raw_parts};
use std::sync::Mutex;

use num_traits::FromPrimitive;
use serde::{Deserialize, Serialize};

use crate::*;
use crate::capi as ztcore;
use std::marker::PhantomData;

pub const NODE_BACKGROUND_TASKS_MAX_INTERVAL: i64 = 200;

const EMPTY_BYTE_ARRAY: [u8; 0] = [];

#[derive(FromPrimitive, ToPrimitive, PartialEq, Eq, Clone, Copy)]
pub enum Event {
    Up = ztcore::ZT_Event_ZT_EVENT_UP as isize,
    Offline = ztcore::ZT_Event_ZT_EVENT_OFFLINE as isize,
    Online = ztcore::ZT_Event_ZT_EVENT_ONLINE as isize,
    Down = ztcore::ZT_Event_ZT_EVENT_DOWN as isize,
    Trace = ztcore::ZT_Event_ZT_EVENT_TRACE as isize,
    UserMessage = ztcore::ZT_Event_ZT_EVENT_USER_MESSAGE as isize,
}

#[derive(FromPrimitive, ToPrimitive, PartialEq, Eq, Clone, Copy)]
pub enum StateObjectType {
    IdentityPublic = ztcore::ZT_StateObjectType_ZT_STATE_OBJECT_IDENTITY_PUBLIC as isize,
    IdentitySecret = ztcore::ZT_StateObjectType_ZT_STATE_OBJECT_IDENTITY_SECRET as isize,
    Locator = ztcore::ZT_StateObjectType_ZT_STATE_OBJECT_LOCATOR as isize,
    Peer = ztcore::ZT_StateObjectType_ZT_STATE_OBJECT_PEER as isize,
    NetworkConfig = ztcore::ZT_StateObjectType_ZT_STATE_OBJECT_NETWORK_CONFIG as isize,
    TrustStore = ztcore::ZT_StateObjectType_ZT_STATE_OBJECT_TRUST_STORE as isize,
}

impl StateObjectType {
    /// True if this state object should be protected.
    #[inline(always)]
    pub fn is_secret(&self) -> bool {
        *self == StateObjectType::IdentitySecret || *self == StateObjectType::TrustStore
    }
}

/// The status of a ZeroTier node.
#[derive(Serialize, Deserialize, PartialEq, Eq, Clone)]
pub struct NodeStatus {
    pub address: Address,
    pub identity: Identity,
    #[serde(rename = "publicIdentity")]
    pub public_identity: String,
    #[serde(rename = "secretIdentity")]
    pub secret_identity: String,
    pub online: bool,
}

/// An event handler that receives events, frames, and packets from the core.
/// Note that these handlers can be called concurrently from any thread and
/// must be thread safe.
pub trait NodeEventHandler<N: Sync + Send + 'static> {
    /// Called when a configuration change or update should be applied to a network.
    fn virtual_network_config(&self, network_id: NetworkId, network_obj: &N, config_op: VirtualNetworkConfigOperation, config: Option<&VirtualNetworkConfig>);

    /// Called when a frame should be injected into the virtual network (physical -> virtual).
    fn virtual_network_frame(&self, network_id: NetworkId, network_obj: &N, source_mac: MAC, dest_mac: MAC, ethertype: u16, vlan_id: u16, data: &[u8]);

    /// Called when a core ZeroTier event occurs.
    fn event(&self, event: Event, event_data: &[u8]);

    /// Called to store an object into the object store.
    fn state_put(&self, obj_type: StateObjectType, obj_id: &[u64], obj_data: &[u8]) -> std::io::Result<()>;

    /// Called to retrieve an object from the object store.
    fn state_get(&self, obj_type: StateObjectType, obj_id: &[u64]) -> std::io::Result<Vec<u8>>;

    /// Called to send a packet over the physical network (virtual -> physical).
    fn wire_packet_send(&self, local_socket: i64, sock_addr: &InetAddress, data: &[u8], packet_ttl: u32) -> i32;

    /// Called to check and see if a physical address should be used for ZeroTier traffic.
    fn path_check(&self, address: Address, id: &Identity, local_socket: i64, sock_addr: &InetAddress) -> bool;

    /// Called to look up a path to a known node, allowing out of band lookup methods for physical paths to nodes.
    fn path_lookup(&self, address: Address, id: &Identity, desired_family: InetAddressFamily) -> Option<InetAddress>;
}

pub struct NodeIntl<T: AsRef<H> + Sync + Send + Clone + 'static, N: Sync + Send + 'static, H: NodeEventHandler<N>> {
    event_handler: T,
    capi: *mut ztcore::ZT_Node,
    networks_by_id: Mutex<HashMap<u64, Pin<Box<N>>>>,
    recent_clock: PortableAtomicI64,
    recent_ticks: PortableAtomicI64,
    event_handler_placeholder: PhantomData<H>,
}

/// An instance of the ZeroTier core.
///
/// The event handler is templated as AsRef<H> where H is the concrete type of the actual
/// handler. This allows the handler to be an Arc<>, Box<>, or similar. We do this instead
/// of templating it on "dyn NodeEventHandler" because we want the types to all be concrete
/// to avoid dynamic call overhead. Unfortunately it makes the types here a tad more
/// verbose.
///
/// In most cases you will want the handler to be an Arc<> anyway since most uses will be
/// multithreaded or async.
pub struct Node<T: AsRef<H> + Sync + Send + Clone + 'static, N: Sync + Send + 'static, H: NodeEventHandler<N>> {
    intl: Pin<Box<NodeIntl<T, N, H>>>,
    event_handler_placeholder: PhantomData<H>,
}

/********************************************************************************************************************/

extern "C" fn zt_virtual_network_config_function<T: AsRef<H> + Sync + Send + Clone + 'static, N: Sync + Send + 'static, H: NodeEventHandler<N>>(
    _: *mut ztcore::ZT_Node,
    uptr: *mut c_void,
    _: *mut c_void,
    nwid: u64,
    nptr: *mut *mut c_void,
    op: ztcore::ZT_VirtualNetworkConfigOperation,
    conf: *const ztcore::ZT_VirtualNetworkConfig,
) {
    let _ = VirtualNetworkConfigOperation::from_i32(op as i32).map(|op| {
        let n = unsafe { &*(uptr.cast::<NodeIntl<T, N, H>>()) };
        if conf.is_null() {
            n.event_handler.as_ref().virtual_network_config(NetworkId(nwid), unsafe { &*(nptr.cast::<N>()) }, op, None);
        } else {
            let conf2 = unsafe { VirtualNetworkConfig::new_from_capi(&*conf) };
            n.event_handler.as_ref().virtual_network_config(NetworkId(nwid), unsafe { &*(nptr.cast::<N>()) }, op, Some(&conf2));
        }
    });
}

extern "C" fn zt_virtual_network_frame_function<T: AsRef<H> + Sync + Send + Clone + 'static, N: Sync + Send + 'static, H: NodeEventHandler<N>>(
    _: *mut ztcore::ZT_Node,
    uptr: *mut c_void,
    _: *mut c_void,
    nwid: u64,
    nptr: *mut *mut c_void,
    source_mac: u64,
    dest_mac: u64,
    ethertype: c_uint,
    vlan_id: c_uint,
    data: *const c_void,
    data_size: c_uint,
) {
    if !nptr.is_null() {
        unsafe { &*(uptr.cast::<NodeIntl<T, N, H>>()) }.event_handler.as_ref().virtual_network_frame(
            NetworkId(nwid),
            unsafe { &*(nptr.cast::<N>()) },
            MAC(source_mac),
            MAC(dest_mac),
            ethertype as u16,
            vlan_id as u16,
            unsafe { &*slice_from_raw_parts(data.cast::<u8>(), data_size as usize) });
    }
}

extern "C" fn zt_event_callback<T: AsRef<H> + Sync + Send + Clone + 'static, N: Sync + Send + 'static, H: NodeEventHandler<N>>(
    _: *mut ztcore::ZT_Node,
    uptr: *mut c_void,
    _: *mut c_void,
    ev: ztcore::ZT_Event,
    data: *const c_void,
    data_size: c_uint,
) {
    let _ = Event::from_i32(ev as i32).map(|ev: Event| {
        let n = unsafe { &*(uptr.cast::<NodeIntl<T, N, H>>()) };
        if data.is_null() {
            n.event_handler.as_ref().event(ev, &EMPTY_BYTE_ARRAY);
        } else {
            n.event_handler.as_ref().event(ev, unsafe { &*slice_from_raw_parts(data.cast::<u8>(), data_size as usize) });
        }
    });
}

extern "C" fn zt_state_put_function<T: AsRef<H> + Sync + Send + Clone + 'static, N: Sync + Send + 'static, H: NodeEventHandler<N>>(
    _: *mut ztcore::ZT_Node,
    uptr: *mut c_void,
    _: *mut c_void,
    obj_type: ztcore::ZT_StateObjectType,
    obj_id: *const u64,
    obj_id_len: c_uint,
    obj_data: *const c_void,
    obj_data_len: c_int,
) {
    let _ = StateObjectType::from_i32(obj_type as i32).map(|obj_type| {
        let n = unsafe { &*(uptr.cast::<NodeIntl<T, N, H>>()) };
        let _ = n.event_handler.as_ref().state_put(obj_type, unsafe { &*slice_from_raw_parts(obj_id, obj_id_len as usize) }, unsafe { &*slice_from_raw_parts(obj_data.cast::<u8>(), obj_data_len as usize) });
    });
}

extern "C" fn zt_state_get_function<T: AsRef<H> + Sync + Send + Clone + 'static, N: Sync + Send + 'static, H: NodeEventHandler<N>>(
    _: *mut ztcore::ZT_Node,
    uptr: *mut c_void,
    _: *mut c_void,
    obj_type: ztcore::ZT_StateObjectType,
    obj_id: *const u64,
    obj_id_len: c_uint,
    obj_data: *mut *mut c_void,
    obj_data_free_function: *mut *mut c_void,
) -> c_int {
    if obj_data.is_null() || obj_data_free_function.is_null() {
        -1 as c_int
    } else {
        unsafe {
            *obj_data = null_mut();
            *obj_data_free_function = transmute(ztcore::free as *const ());
        }
        StateObjectType::from_i32(obj_type as i32).map_or_else(|| {
            -1 as c_int
        }, |obj_type| {
            unsafe { &*(uptr.cast::<NodeIntl<T, N, H>>()) }.event_handler.as_ref().state_get(obj_type, unsafe { &*slice_from_raw_parts(obj_id, obj_id_len as usize) }).map_or_else(|_| {
                -1 as c_int
            }, |obj_data_result| {
                let obj_data_len = obj_data_result.len() as c_int;
                if obj_data_len > 0 {
                    unsafe {
                        let obj_data_raw = ztcore::malloc(obj_data_len as c_ulong);
                        if obj_data_raw.is_null() {
                            -1 as c_int
                        } else {
                            copy_nonoverlapping(obj_data_result.as_ptr(), obj_data_raw.cast::<u8>(), obj_data_len as usize);
                            *obj_data = obj_data_raw;
                            obj_data_len
                        }
                    }
                } else {
                    -1 as c_int
                }
            })
        })
    }
}

extern "C" fn zt_wire_packet_send_function<T: AsRef<H> + Sync + Send + Clone + 'static, N: Sync + Send + 'static, H: NodeEventHandler<N>>(
    _: *mut ztcore::ZT_Node,
    uptr: *mut c_void,
    _: *mut c_void,
    local_socket: i64,
    sock_addr: *const ztcore::ZT_InetAddress,
    data: *const c_void,
    data_size: c_uint,
    packet_ttl: c_uint,
) -> c_int {
    unsafe { &*(uptr.cast::<NodeIntl<T, N, H>>()) }.event_handler.as_ref().wire_packet_send(local_socket, InetAddress::transmute_capi(unsafe { &*sock_addr }), unsafe { &*slice_from_raw_parts(data.cast::<u8>(), data_size as usize) }, packet_ttl as u32) as c_int
}

extern "C" fn zt_path_check_function<T: AsRef<H> + Sync + Send + Clone + 'static, N: Sync + Send + 'static, H: NodeEventHandler<N>>(
    _: *mut ztcore::ZT_Node,
    uptr: *mut c_void,
    _: *mut c_void,
    address: u64,
    identity: *const ztcore::ZT_Identity,
    local_socket: i64,
    sock_addr: *const ztcore::ZT_InetAddress,
) -> c_int {
    let id = Identity::new_from_capi(identity, false);
    unsafe { &*(uptr.cast::<NodeIntl<T, N, H>>()) }.event_handler.as_ref().path_check(Address(address), &id, local_socket, InetAddress::transmute_capi(unsafe { &*sock_addr })) as c_int
}

extern "C" fn zt_path_lookup_function<T: AsRef<H> + Sync + Send + Clone + 'static, N: Sync + Send + 'static, H: NodeEventHandler<N>>(
    _: *mut ztcore::ZT_Node,
    uptr: *mut c_void,
    _: *mut c_void,
    address: u64,
    identity: *const ztcore::ZT_Identity,
    sock_family: c_int,
    sock_addr: *mut ztcore::ZT_InetAddress,
) -> c_int {
    if sock_addr.is_null() {
        return 0;
    }
    let sock_family2: InetAddressFamily;
    unsafe {
        if sock_family == ztcore::ZT_AF_INET {
            sock_family2 = InetAddressFamily::IPv4;
        } else if sock_family == ztcore::ZT_AF_INET6 {
            sock_family2 = InetAddressFamily::IPv6;
        } else {
            return 0;
        }
    }

    let id = Identity::new_from_capi(identity, false);
    unsafe { &*(uptr.cast::<NodeIntl<T, N, H>>()) }.event_handler.as_ref().path_lookup(Address(address), &id, sock_family2).map_or_else(|| {
        0 as c_int
    }, |result| {
        let result_ptr = &result as *const InetAddress;
        unsafe {
            copy_nonoverlapping(result_ptr.cast::<ztcore::ZT_InetAddress>(), sock_addr, 1);
        }
        1 as c_int
    })
}

/********************************************************************************************************************/

impl<T: AsRef<H> + Sync + Send + Clone + 'static, N: Sync + Send + 'static, H: NodeEventHandler<N>> Node<T, N, H> {
    /// Create a new Node with a given event handler.
    #[allow(unused_mut)]
    pub fn new(event_handler: T, clock: i64, ticks: i64) -> Result<Node<T, N, H>, ResultCode> {
        let mut n = Node {
            intl: Box::pin(NodeIntl {
                event_handler: event_handler.clone(),
                capi: null_mut(),
                networks_by_id: Mutex::new(HashMap::new()),
                event_handler_placeholder: PhantomData::default(),
                recent_clock: PortableAtomicI64::new(clock),
                recent_ticks: PortableAtomicI64::new(ticks),
            }),
            event_handler_placeholder: PhantomData::default(),
        };

        let rc = unsafe {
            let callbacks = ztcore::ZT_Node_Callbacks {
                statePutFunction: transmute(zt_state_put_function::<T, N, H> as *const ()),
                stateGetFunction: transmute(zt_state_get_function::<T, N, H> as *const ()),
                wirePacketSendFunction: transmute(zt_wire_packet_send_function::<T, N, H> as *const ()),
                virtualNetworkFrameFunction: transmute(zt_virtual_network_frame_function::<T, N, H> as *const ()),
                virtualNetworkConfigFunction: transmute(zt_virtual_network_config_function::<T, N, H> as *const ()),
                eventCallback: transmute(zt_event_callback::<T, N, H> as *const ()),
                pathCheckFunction: transmute(zt_path_check_function::<T, N, H> as *const ()),
                pathLookupFunction: transmute(zt_path_lookup_function::<T, N, H> as *const ()),
            };
            ztcore::ZT_Node_new(transmute(&(n.intl.capi) as *const *mut ztcore::ZT_Node), clock, ticks, null_mut(), transmute(&*n.intl as *const NodeIntl<T, N, H>), &callbacks)
        };

        if rc == 0 {
            assert!(!n.intl.capi.is_null());
            Ok(n)
        } else {
            Err(ResultCode::from_i32(rc as i32).unwrap_or(ResultCode::FatalErrorInternal))
        }
    }

    /// Perform periodic background tasks.
    /// The first call should happen no more than NODE_BACKGROUND_TASKS_MAX_INTERVAL milliseconds
    /// since the node was created, and after this runs it returns the amount of time the caller
    /// should wait before calling it again.
    pub fn process_background_tasks(&self, clock: i64, ticks: i64) -> i64 {
        self.intl.recent_clock.set(clock);
        self.intl.recent_ticks.set(ticks);
        let mut next_task_deadline: i64 = ticks;
        unsafe {
            ztcore::ZT_Node_processBackgroundTasks(self.intl.capi, clock, ticks, null_mut(), (&mut next_task_deadline as *mut i64).cast());
        }
        (next_task_deadline - ticks).clamp(1_i64, NODE_BACKGROUND_TASKS_MAX_INTERVAL)
    }

    /// Join a network, associating network_obj with it.
    /// If a fingerprint is supplied it will be used as a full sha384 fingerprint of the
    /// network's controller.
    pub fn join(&self, clock: i64, ticks: i64, nwid: NetworkId, controller_fingerprint: Option<Fingerprint>, network_obj: N) -> ResultCode {
        let mut cfp: MaybeUninit<ztcore::ZT_Fingerprint> = MaybeUninit::uninit();
        let mut cfpp: *mut ztcore::ZT_Fingerprint = null_mut();
        if controller_fingerprint.is_some() {
            let cfp2 = controller_fingerprint.unwrap();
            cfpp = cfp.as_mut_ptr();
            unsafe {
                (*cfpp).address = cfp2.address.0;
                (*cfpp).hash = cfp2.hash;
            }
        }

        let network_obj = Box::pin(network_obj);
        let rc = unsafe { ztcore::ZT_Node_join(self.intl.capi, clock, ticks, null_mut(), transmute((&*network_obj) as *const N), nwid.0, cfpp) };
        if rc == ztcore::ZT_ResultCode_ZT_RESULT_OK {
            self.intl.networks_by_id.lock().unwrap().insert(nwid.0, network_obj);
            ResultCode::Ok
        } else {
            ResultCode::from_i32(rc as i32).unwrap_or(ResultCode::ErrorInternalNonFatal)
        }
    }

    /// Leave a network.
    pub fn leave(&self, clock: i64, ticks: i64, nwid: NetworkId) -> ResultCode {
        self.intl.networks_by_id.lock().unwrap().remove(&nwid.0).map_or_else(|| {
            ResultCode::ErrorNetworkNotFound
        }, |_| {
            unsafe { ResultCode::from_i32(ztcore::ZT_Node_leave(self.intl.capi, clock, ticks, null_mut(), null_mut(), nwid.0) as i32).unwrap_or(ResultCode::ErrorInternalNonFatal) }
        })
    }

    /// Access a network's associated network object.
    /// This executes the supplied function or closure if we are joined to
    /// this network, providing its associated network object as a parameter.
    /// This happens while the internal data structure is locked, so do not
    /// do anything time consuming while inside this function. The return value
    /// (if any) of this function is returned, or None if we are not joined to
    /// this network.
    #[inline(always)]
    pub fn with_network<F: FnOnce(&N) -> R, R>(&self, nwid: NetworkId, f: F) -> Option<R> {
        self.intl.networks_by_id.lock().unwrap().get(&nwid.0).map_or(None, |nw| {
            Some(f(&*nw))
        })
    }

    #[inline(always)]
    pub fn address(&self) -> Address {
        unsafe { Address(ztcore::ZT_Node_address(self.intl.capi) as u64) }
    }

    #[inline(always)]
    pub fn process_wire_packet(&self, clock: i64, ticks: i64, local_socket: i64, remote_address: &InetAddress, data: Buffer, next_task_deadline: &mut i64) -> ResultCode {
        let intl = &*self.intl;
        let rc = unsafe { ResultCode::from_i32(ztcore::ZT_Node_processWirePacket(intl.capi, clock, ticks, null_mut(), local_socket, remote_address.as_capi_ptr(), data.zt_core_buf as *const c_void, data.data_size as u32, 1, (next_task_deadline as *mut i64).cast()) as i32).unwrap_or(ResultCode::ErrorInternalNonFatal) };
        std::mem::forget(data); // prevent Buffer from being returned to ZT core twice, see comment in drop() in buffer.rs
        rc
    }

    #[inline(always)]
    pub fn process_virtual_network_frame(&self, clock: i64, ticks: i64, nwid: &NetworkId, source_mac: &MAC, dest_mac: &MAC, ethertype: u16, vlan_id: u16, data: Buffer, next_task_deadline: &mut i64) -> ResultCode {
        let intl = &*self.intl;
        let rc = unsafe { ResultCode::from_i32(ztcore::ZT_Node_processVirtualNetworkFrame(intl.capi, clock, ticks, null_mut(), nwid.0, source_mac.0, dest_mac.0, ethertype as c_uint, vlan_id as c_uint, data.zt_core_buf as *const c_void, data.data_size as u32, 1, (next_task_deadline as *mut i64).cast()) as i32).unwrap_or(ResultCode::ErrorInternalNonFatal) };
        std::mem::forget(data); // prevent Buffer from being returned to ZT core twice, see comment in drop() in buffer.rs
        rc
    }

    #[inline(always)]
    pub fn multicast_subscribe(&self, clock: i64, ticks: i64, nwid: &NetworkId, multicast_group: &MAC, multicast_adi: u32) -> ResultCode {
        unsafe { ResultCode::from_i32(ztcore::ZT_Node_multicastSubscribe(self.intl.capi, clock, ticks, null_mut(), nwid.0, multicast_group.0, multicast_adi as c_ulong) as i32).unwrap_or(ResultCode::ErrorInternalNonFatal) }
    }

    #[inline(always)]
    pub fn multicast_unsubscribe(&self, clock: i64, ticks: i64, nwid: &NetworkId, multicast_group: &MAC, multicast_adi: u32) -> ResultCode {
        unsafe { ResultCode::from_i32(ztcore::ZT_Node_multicastUnsubscribe(self.intl.capi, clock, ticks, null_mut(), nwid.0, multicast_group.0, multicast_adi as c_ulong) as i32).unwrap_or(ResultCode::ErrorInternalNonFatal) }
    }

    /// Get a copy of this node's identity.
    #[inline(always)]
    pub fn identity(&self) -> Identity {
        unsafe { self.identity_fast().clone() }
    }

    /// Get an identity that simply holds a pointer to the underlying node's identity.
    /// This is unsafe because the identity object becomes invalid if the node ceases
    /// to exist the Identity becomes invalid. Use clone() on it to get a copy.
    #[inline(always)]
    pub(crate) unsafe fn identity_fast(&self) -> Identity {
        Identity::new_from_capi(ztcore::ZT_Node_identity(self.intl.capi), false)
    }

    pub fn status(&self, clock: i64, ticks: i64) -> NodeStatus {
        let mut ns: MaybeUninit<ztcore::ZT_NodeStatus> = MaybeUninit::zeroed();
        unsafe {
            ztcore::ZT_Node_status(self.intl.capi, clock, ticks, null_mut(), ns.as_mut_ptr());
            let ns = ns.assume_init();
            if ns.identity.is_null() {
                panic!("ZT_Node_status() returned null identity");
            }
            NodeStatus {
                address: Address(ns.address),
                identity: Identity::new_from_capi(&*ns.identity, false).clone(),
                public_identity: cstr_to_string(ns.publicIdentity, -1),
                secret_identity: cstr_to_string(ns.secretIdentity, -1),
                online: ns.online != 0,
            }
        }
    }

    pub fn peers(&self, clock: i64, ticks: i64) -> Vec<Peer> {
        let mut p: Vec<Peer> = Vec::new();
        unsafe {
            let pl = ztcore::ZT_Node_peers(self.intl.capi, clock, ticks, null_mut());
            if !pl.is_null() {
                let peer_count = (*pl).peerCount as usize;
                p.reserve(peer_count);
                for i in 0..peer_count as isize {
                    p.push(Peer::new_from_capi(&*(*pl).peers.offset(i)));
                }
                ztcore::ZT_freeQueryResult(pl as *const c_void);
            }
        }
        p
    }

    pub fn networks(&self) -> Vec<VirtualNetworkConfig> {
        let mut n: Vec<VirtualNetworkConfig> = Vec::new();
        unsafe {
            let nl = ztcore::ZT_Node_networks(self.intl.capi);
            if !nl.is_null() {
                let net_count = (*nl).networkCount as usize;
                n.reserve(net_count);
                for i in 0..net_count as isize {
                    n.push(VirtualNetworkConfig::new_from_capi(&*(*nl).networks.offset(i)));
                }
                ztcore::ZT_freeQueryResult(nl as *const c_void);
            }
        }
        n
    }

    pub fn certificates(&self, clock: i64, ticks: i64) -> Vec<(Certificate, u32)> {
        let mut c: Vec<(Certificate, u32)> = Vec::new();
        unsafe {
            let cl = ztcore::ZT_Node_listCertificates(self.intl.capi, clock, ticks, null_mut());
            if !cl.is_null() {
                let cert_count = (*cl).certCount as usize;
                c.reserve(cert_count);
                for i in 0..cert_count as isize {
                    c.push((Certificate::new_from_capi(&**(*cl).certs.offset(i)), *(*cl).localTrust.offset(i)));
                }
                ztcore::ZT_freeQueryResult(cl as *const c_void);
            }
        }
        c
    }
}

impl<T: AsRef<H> + Sync + Send + Clone + 'static, N: Sync + Send + Clone + 'static, H: NodeEventHandler<N>> Node<T, N, H> {
    /// Get a copy of this network's associated object.
    /// This is only available if N implements Clone.
    pub fn network(&self, nwid: NetworkId) -> Option<N> {
        self.intl.networks_by_id.lock().unwrap().get(&nwid.0).map_or(None, |nw| Some((**nw).clone()))
    }
}

unsafe impl<T: AsRef<H> + Sync + Send + Clone + 'static, N: Sync + Send + 'static, H: NodeEventHandler<N>> Sync for Node<T, N, H> {}

unsafe impl<T: AsRef<H> + Sync + Send + Clone + 'static, N: Sync + Send + 'static, H: NodeEventHandler<N>> Send for Node<T, N, H> {}

impl<T: AsRef<H> + Sync + Send + Clone + 'static, N: Sync + Send + 'static, H: NodeEventHandler<N>> Drop for Node<T, N, H> {
    fn drop(&mut self) {
        unsafe {
            ztcore::ZT_Node_delete(self.intl.capi, self.intl.recent_clock.get(), self.intl.recent_ticks.get(), null_mut());
        }
    }
}
