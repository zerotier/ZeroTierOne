use std::sync::atomic::{AtomicI64, Ordering};

use crate::vl1::Endpoint;

pub struct Path {
    pub(crate) endpoint: Endpoint,
    pub(crate) local_socket: i64,
    pub(crate) local_interface: i64,
    last_send_time_ticks: AtomicI64,
    last_receive_time_ticks: AtomicI64,
}

impl Path {
    #[inline(always)]
    pub fn new(endpoint: Endpoint, local_socket: i64, local_interface: i64) -> Self {
        Self {
            endpoint,
            local_socket,
            local_interface,
            last_send_time_ticks: AtomicI64::new(0),
            last_receive_time_ticks: AtomicI64::new(0),
        }
    }

    #[inline(always)]
    pub fn last_send_time_ticks(&self) -> i64 {
        self.last_send_time_ticks.load(Ordering::Relaxed)
    }

    #[inline(always)]
    pub fn send_receive_time_ticks(&self) -> i64 {
        self.last_receive_time_ticks.load(Ordering::Relaxed)
    }
}

unsafe impl Send for Path {}

unsafe impl Sync for Path {}
