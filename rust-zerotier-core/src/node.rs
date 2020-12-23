use std::cell::Cell;
use std::mem::MaybeUninit;
use std::os::raw::{c_uint, c_ulong, c_void};
use std::ptr::null_mut;
use std::sync::*;
use std::sync::atomic::*;
use std::time::Duration;

use num_traits::FromPrimitive;
use socket2::SockAddr;

use crate::*;
use crate::bindings::capi as ztcore;

pub struct Node {
    capi: *mut ztcore::ZT_Node,
    background_thread: Cell<Option<std::thread::JoinHandle<()>>>,
    background_thread_run: Arc<AtomicBool>,
    now: PortableAtomicI64,
}

impl Node {
    pub fn new(base_dir: &std::path::Path) -> Arc<Node> {
        let n: Arc<Node> = Arc::new(Node {
            capi: null_mut(), // TODO
            background_thread: Cell::new(None),
            background_thread_run: Arc::new(AtomicBool::new(true)),
            now: PortableAtomicI64::new(now()),
        });

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

        n
    }

    /// This is called periodically from internal background thread.
    /// Don't call directly.
    #[inline(always)]
    fn process_background_tasks(&self) -> i64 {
        let current_time = now();
        self.now.set(current_time);

        let mut next_task_deadline: i64 = current_time;
        unsafe {
            ztcore::ZT_Node_processBackgroundTasks(self.capi, 0 as *mut c_void, current_time, &mut next_task_deadline as *mut i64);
        }
        let mut next_delay = next_task_deadline - current_time;

        if next_delay < 50 {
            next_delay = 50;
        } else if next_delay > 500 {
            next_delay = 500;
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
            let rc = ztcore::ZT_Node_join(self.capi, nwid.0, cfpp, null_mut(), null_mut());
            return ResultCode::from_u32(rc as u32).unwrap();
        }
    }

    pub fn leave(&self, nwid: NetworkId) -> ResultCode {
        unsafe {
            let rc = ztcore::ZT_Node_leave(self.capi, nwid.0, null_mut(), null_mut());
            return ResultCode::from_u32(rc as u32).unwrap();
        }
    }

    #[inline(always)]
    pub fn address(&self) -> Address {
        unsafe {
            return Address(ztcore::ZT_Node_address(self.capi) as u64);
        }
    }

    pub fn identity(&self) -> Identity {
        unsafe {
            return Identity::new_from_capi(ztcore::ZT_Node_identity(self.capi), false);
        }
    }

    #[inline(always)]
    pub fn process_wire_packet(&self, local_socket: i64, remote_address: &SockAddr, data: &mut Buffer) -> ResultCode {
        let current_time = self.now.get();
        let mut next_task_deadline: i64 = current_time;
        unsafe {
            return ResultCode::from_u32(ztcore::ZT_Node_processWirePacket(self.capi, null_mut(), current_time, local_socket, remote_address.as_ptr() as *const ztcore::sockaddr_storage, data.zt_core_buf as *const c_void, data.data_size as u32, 1, &mut next_task_deadline as *mut i64) as u32).unwrap_or(ResultCode::ErrorInternalNonFatal);
        }
    }

    #[inline(always)]
    pub fn process_virtual_network_frame(&self, nwid: &NetworkId, source_mac: &MAC, dest_mac: &MAC, ethertype: u16, vlan_id: u16, data: &mut Buffer) -> ResultCode {
        let current_time = self.now.get();
        let mut next_tick_deadline: i64 = current_time;
        unsafe {
            return ResultCode::from_u32(ztcore::ZT_Node_processVirtualNetworkFrame(self.capi, null_mut(), current_time, nwid.0, source_mac.0, dest_mac.0, ethertype as c_uint, vlan_id as c_uint, data.zt_core_buf as *const c_void, data.data_size as u32, 1, &mut next_tick_deadline as *mut i64) as u32).unwrap_or(ResultCode::ErrorInternalNonFatal);
        }
    }

    pub fn multicast_subscribe(&self, nwid: &NetworkId, multicast_group: &MAC, multicast_adi: u32) -> ResultCode {
        unsafe {
            return ResultCode::from_u32(ztcore::ZT_Node_multicastSubscribe(self.capi, null_mut(), nwid.0, multicast_group.0, multicast_adi as c_ulong) as u32).unwrap_or(ResultCode::ErrorInternalNonFatal);
        }
    }

    pub fn multicast_unsubscribe(&self, nwid: &NetworkId, multicast_group: &MAC, multicast_adi: u32) -> ResultCode {
        unsafe {
            return ResultCode::from_u32(ztcore::ZT_Node_multicastUnsubscribe(self.capi, nwid.0, multicast_group.0, multicast_adi as c_ulong) as u32).unwrap_or(ResultCode::ErrorInternalNonFatal);
        }
    }
}

unsafe impl Sync for Node {}

unsafe impl Send for Node {}

impl Drop for Node {
    fn drop(&mut self) {
        self.background_thread_run.store(false, Ordering::Relaxed);
        std::thread::yield_now();
        let bt = self.background_thread.replace(None);
        if bt.is_some() {
            let bt = bt.unwrap();
            bt.thread().unpark();
            let _ = bt.join();
        }

        unsafe {
            ztcore::ZT_Node_delete(self.capi, 0 as *mut c_void);
        }
    }
}
