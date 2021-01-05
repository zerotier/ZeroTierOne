use std::cell::Cell;
use std::ffi::CStr;
use std::mem::MaybeUninit;
use std::mem::transmute;
use std::os::raw::{c_int, c_uint, c_ulong, c_void};
use std::ptr::null_mut;
use std::sync::*;
use std::sync::atomic::*;
use std::time::Duration;

use num_traits::FromPrimitive;
use serde::{Deserialize, Serialize};

use crate::*;
use crate::bindings::capi as ztcore;

const NODE_BACKGROUND_MIN_DELAY: i64 = 250;

#[derive(Serialize, Deserialize)]
pub struct NodeStatus {
    pub address: Address,
    pub identity: Identity,
    #[serde(rename = "publicIdentity")]
    pub public_identity: String,
    #[serde(rename = "secretIdentity")]
    pub secret_identity: String,
    pub online: bool,
}

pub trait NodeEventHandler {
    fn virtual_network_config(&self);
    fn virtual_network_frame(&self);
    fn event(&self);
    fn state_put(&self);
    fn state_get(&self);
    fn wire_packet_send(&self);
    fn path_check(&self);
    fn path_lookup(&self);
}

pub struct Node<T: NodeEventHandler + 'static> {
    event_handler: Arc<T>,
    capi: Cell<*mut ztcore::ZT_Node>,
    background_thread: Cell<Option<std::thread::JoinHandle<()>>>,
    background_thread_run: Arc<AtomicBool>,
    now: PortableAtomicI64,
}

macro_rules! node_from_raw_ptr {
    ($uptr:ident) => {
        unsafe {
            let ntmp: *const Node<T> = transmute($uptr);
            let ntmp: &Node<T> = &*ntmp;
            ntmp
        }
    }
}

#[no_mangle]
extern "C" fn zt_virtual_network_config_function<T: NodeEventHandler + 'static>(
    capi: *mut ztcore::ZT_Node,
    uptr: *mut c_void,
    tptr: *mut c_void,
    nwid: u64,
    nptr: *mut *mut c_void,
    op: ztcore::ZT_VirtualNetworkConfigOperation,
    conf: *const ztcore::ZT_VirtualNetworkConfig,
) {
    let n = node_from_raw_ptr!(uptr);
    n.event_handler.virtual_network_config();
}

#[no_mangle]
extern "C" fn zt_virtual_network_frame_function<T: NodeEventHandler + 'static>(
    capi: *mut ztcore::ZT_Node,
    uptr: *mut c_void,
    tptr: *mut c_void,
    nwid: u64,
    nptr: *mut *mut c_void,
    source_mac: u64,
    dest_mac: u64,
    ethertype: c_uint,
    vlan_id: c_uint,
    data: *const c_void,
    data_size: c_uint,
) {
    let n = node_from_raw_ptr!(uptr);
    n.event_handler.virtual_network_frame();
}

#[no_mangle]
extern "C" fn zt_event_callback<T: NodeEventHandler + 'static>(
    capi: *mut ztcore::ZT_Node,
    uptr: *mut c_void,
    tptr: *mut c_void,
    ev: ztcore::ZT_Event,
    data: *const c_void,
) {
    let n = node_from_raw_ptr!(uptr);
    n.event_handler.event();
}

#[no_mangle]
extern "C" fn zt_state_put_function<T: NodeEventHandler + 'static>(
    capi: *mut ztcore::ZT_Node,
    uptr: *mut c_void,
    tptr: *mut c_void,
    obj_type: ztcore::ZT_StateObjectType,
    obj_id: *const u64,
    obj_data: *const c_void,
    obj_data_len: c_int,
) {
    let n = node_from_raw_ptr!(uptr);
    n.event_handler.state_put();
}

#[no_mangle]
extern "C" fn zt_state_get_function<T: NodeEventHandler + 'static>(
    capi: *mut ztcore::ZT_Node,
    uptr: *mut c_void,
    tptr: *mut c_void,
    obj_type: ztcore::ZT_StateObjectType,
    obj_id: *const u64,
    obj_data: *mut *mut c_void,
    obj_data_free_function: *mut *mut c_void,
) {
    let n = node_from_raw_ptr!(uptr);
    n.event_handler.state_get();
}

#[no_mangle]
extern "C" fn zt_wire_packet_send_function<T: NodeEventHandler + 'static>(
    capi: *mut ztcore::ZT_Node,
    uptr: *mut c_void,
    tptr: *mut c_void,
    local_socket: i64,
    sock_addr: *const ztcore::ZT_InetAddress,
    data: *const c_void,
    data_size: c_uint,
    packet_ttl: c_uint,
) {
    let n = node_from_raw_ptr!(uptr);
    n.event_handler.wire_packet_send();
}

#[no_mangle]
extern "C" fn zt_path_check_function<T: NodeEventHandler + 'static>(
    capi: *mut ztcore::ZT_Node,
    uptr: *mut c_void,
    tptr: *mut c_void,
    address: u64,
    identity: *const ztcore::ZT_Identity,
    local_socket: i64,
    sock_addr: *const ztcore::ZT_InetAddress,
) {
    let n = node_from_raw_ptr!(uptr);
    n.event_handler.path_check();
}

#[no_mangle]
extern "C" fn zt_path_lookup_function<T: NodeEventHandler + 'static>(
    capi: *mut ztcore::ZT_Node,
    uptr: *mut c_void,
    tptr: *mut c_void,
    address: u64,
    identity: *const ztcore::ZT_Identity,
    sock_family: c_int,
    sock_addr: *mut ztcore::ZT_InetAddress,
) {
    let n = node_from_raw_ptr!(uptr);
    n.event_handler.path_lookup();
}

impl<T: NodeEventHandler + 'static> Node<T> {
    /// Create a new Node with a given event handler.
    pub fn new(event_handler: Arc<T>) -> Result<Arc<Node<T>>, ResultCode> {
        let now = now();

        let n = Arc::new(Node {
            event_handler: event_handler.clone(),
            capi: Cell::new(null_mut()),
            background_thread: Cell::new(None),
            background_thread_run: Arc::new(AtomicBool::new(true)),
            now: PortableAtomicI64::new(now),
        });

        let mut capi: *mut ztcore::ZT_Node = null_mut();
        unsafe {
            let callbacks = ztcore::ZT_Node_Callbacks {
                statePutFunction: transmute(zt_state_put_function::<T> as *const ()),
                stateGetFunction: transmute(zt_state_get_function::<T> as *const ()),
                wirePacketSendFunction: transmute(zt_wire_packet_send_function::<T> as *const ()),
                virtualNetworkFrameFunction: transmute(zt_virtual_network_frame_function::<T> as *const ()),
                virtualNetworkConfigFunction: transmute(zt_virtual_network_config_function::<T> as *const ()),
                eventCallback: transmute(zt_event_callback::<T> as *const ()),
                pathCheckFunction: transmute(zt_path_check_function::<T> as *const ()),
                pathLookupFunction: transmute(zt_path_lookup_function::<T> as *const ()),
            };

            let rc = ztcore::ZT_Node_new(&mut capi as *mut *mut ztcore::ZT_Node, transmute(Arc::as_ptr(&n)), null_mut(), &callbacks as *const ztcore::ZT_Node_Callbacks, now);
            if rc != 0 {
                return Err(ResultCode::from_u32(rc as u32).unwrap_or(ResultCode::FatalErrorInternal));
            } else if capi.is_null() {
                return Err(ResultCode::FatalErrorInternal);
            }
        }
        n.capi.replace(capi);

        let wn = Arc::downgrade(&n);
        let run = n.background_thread_run.clone();
        n.background_thread.replace(Some(std::thread::spawn(move || {
            let mut loop_delay = Duration::from_millis(500);
            while run.load(Ordering::Relaxed) {
                std::thread::park_timeout(loop_delay);
                if run.load(Ordering::Relaxed) {
                    let nn = wn.upgrade();
                    if nn.is_some() {
                        loop_delay = Duration::from_millis(nn.unwrap().process_background_tasks() as u64);
                    } else {
                        break;
                    }
                } else {
                    break;
                }
            }
        })));

        Ok(n)
    }

    /// This is called periodically from the background service thread.
    #[inline(always)]
    fn process_background_tasks(&self) -> i64 {
        let current_time = now();
        self.now.set(current_time);

        let mut next_task_deadline: i64 = current_time;
        unsafe {
            ztcore::ZT_Node_processBackgroundTasks(self.capi.get(), null_mut(), current_time, &mut next_task_deadline as *mut i64);
        }
        let mut next_delay = next_task_deadline - current_time;

        if next_delay < 10 {
            next_delay = 10;
        } else if next_delay > NODE_BACKGROUND_MIN_DELAY {
            next_delay = NODE_BACKGROUND_MIN_DELAY;
        }
        next_delay
    }

    pub fn join(&self, nwid: NetworkId, controller_fingerprint: Option<Fingerprint>) -> ResultCode {
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
        unsafe {
            let rc = ztcore::ZT_Node_join(self.capi.get(), nwid.0, cfpp, null_mut(), null_mut());
            return ResultCode::from_u32(rc as u32).unwrap();
        }
    }

    pub fn leave(&self, nwid: NetworkId) -> ResultCode {
        unsafe {
            return ResultCode::from_u32(ztcore::ZT_Node_leave(self.capi.get(), nwid.0, null_mut(), null_mut()) as u32).unwrap();
        }
    }

    #[inline(always)]
    pub fn address(&self) -> Address {
        unsafe {
            return Address(ztcore::ZT_Node_address(self.capi.get()) as u64);
        }
    }

    #[inline(always)]
    pub fn process_wire_packet<A>(&self, local_socket: i64, remote_address: &InetAddress, data: Buffer) -> ResultCode {
        let current_time = self.now.get();
        let mut next_task_deadline: i64 = current_time;
        let rc = unsafe { ResultCode::from_u32(ztcore::ZT_Node_processWirePacket(self.capi.get(), null_mut(), current_time, local_socket, remote_address.as_capi_ptr(), data.zt_core_buf as *const c_void, data.data_size as u32, 1, &mut next_task_deadline as *mut i64) as u32).unwrap_or(ResultCode::ErrorInternalNonFatal) };
        std::mem::forget(data); // prevent Buffer from being returned to ZT core twice, see comment in drop() in buffer.rs
        rc
    }

    #[inline(always)]
    pub fn process_virtual_network_frame(&self, nwid: &NetworkId, source_mac: &MAC, dest_mac: &MAC, ethertype: u16, vlan_id: u16, data: Buffer) -> ResultCode {
        let current_time = self.now.get();
        let mut next_tick_deadline: i64 = current_time;
        let rc = unsafe { ResultCode::from_u32(ztcore::ZT_Node_processVirtualNetworkFrame(self.capi.get(), null_mut(), current_time, nwid.0, source_mac.0, dest_mac.0, ethertype as c_uint, vlan_id as c_uint, data.zt_core_buf as *const c_void, data.data_size as u32, 1, &mut next_tick_deadline as *mut i64) as u32).unwrap_or(ResultCode::ErrorInternalNonFatal) };
        std::mem::forget(data); // prevent Buffer from being returned to ZT core twice, see comment in drop() in buffer.rs
        rc
    }

    pub fn multicast_subscribe(&self, nwid: &NetworkId, multicast_group: &MAC, multicast_adi: u32) -> ResultCode {
        unsafe {
            return ResultCode::from_u32(ztcore::ZT_Node_multicastSubscribe(self.capi.get(), null_mut(), nwid.0, multicast_group.0, multicast_adi as c_ulong) as u32).unwrap_or(ResultCode::ErrorInternalNonFatal);
        }
    }

    pub fn multicast_unsubscribe(&self, nwid: &NetworkId, multicast_group: &MAC, multicast_adi: u32) -> ResultCode {
        unsafe {
            return ResultCode::from_u32(ztcore::ZT_Node_multicastUnsubscribe(self.capi.get(), nwid.0, multicast_group.0, multicast_adi as c_ulong) as u32).unwrap_or(ResultCode::ErrorInternalNonFatal);
        }
    }

    pub fn identity(&self) -> Identity {
        unsafe {
            let mut id = ztcore::ZT_Node_identity(self.capi.get());
            return Identity::new_from_capi(id, false).clone();
        }
    }

    pub fn status(&self) -> NodeStatus {
        unsafe {
            let mut ns: MaybeUninit<ztcore::ZT_NodeStatus> = MaybeUninit::zeroed();
            ztcore::ZT_Node_status(self.capi.get(), ns.as_mut_ptr());
            let ns = ns.assume_init();
            if ns.identity.is_null() {
                panic!("ZT_Node_status() returned null identity");
            }
            return NodeStatus {
                address: Address(ns.address),
                identity: Identity::new_from_capi(&*ns.identity, false).clone(),
                public_identity: String::from(CStr::from_ptr(ns.publicIdentity).to_str().unwrap()),
                secret_identity: String::from(CStr::from_ptr(ns.secretIdentity).to_str().unwrap()),
                online: ns.online != 0,
            };
        }
    }

    pub fn peers(&self) -> Vec<Peer> {
        let mut p: Vec<Peer> = Vec::new();
        unsafe {
            let pl = ztcore::ZT_Node_peers(self.capi.get());
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
            let nl = ztcore::ZT_Node_networks(self.capi.get());
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

    pub fn certificates(&self) -> Vec<(Certificate, u32)> {
        let mut c: Vec<(Certificate, u32)> = Vec::new();
        unsafe {
            let cl = ztcore::ZT_Node_listCertificates(self.capi.get());
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

unsafe impl<T: NodeEventHandler + 'static> Sync for Node<T> {}

unsafe impl<T: NodeEventHandler + 'static> Send for Node<T> {}

impl<T: NodeEventHandler + 'static> Drop for Node<T> {
    fn drop(&mut self) {
        self.background_thread_run.store(false, Ordering::Relaxed);
        let bt = self.background_thread.replace(None);
        if bt.is_some() {
            let bt = bt.unwrap();
            bt.thread().unpark();
            let _ = bt.join();
        }

        unsafe {
            ztcore::ZT_Node_delete(self.capi.get(), null_mut());
        }
    }
}
