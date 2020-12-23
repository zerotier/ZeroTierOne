use crate::bindings::capi as ztcore;
use std::os::raw::c_void;
use std::slice::{from_raw_parts_mut};
use std::ptr::null_mut;

pub struct Buffer {
    pub(crate) zt_core_buf: *mut u8,
    pub(crate) data_size: usize
}

impl Buffer {
    pub fn new() -> Buffer {
        unsafe {
            let b = ztcore::ZT_getBuffer() as *mut u8;
            if b.is_null() {
                panic!("out of memory calling ZT_getBuffer()");
            }
            return Buffer {
                zt_core_buf: b,
                data_size: ztcore::ZT_BUF_SIZE as usize
            };
        }
    }

    #[inline(always)]
    pub fn get(&mut self) -> &mut [u8] {
        unsafe {
            return from_raw_parts_mut(self.zt_core_buf, ztcore::ZT_BUF_SIZE as usize);
        }
    }

    #[inline(always)]
    pub fn set_data_size(&mut self, s: usize) {
        self.data_size = s & (ztcore::ZT_BUF_SIZE - 1) as usize;
    }

    #[inline(always)]
    pub(crate) fn mark_consumed(&mut self) {
        self.zt_core_buf = null_mut();
    }
}

impl Drop for Buffer {
    #[inline(always)]
    fn drop(&mut self) {
        // ZT_freeBuffer() does nothing if passed a null pointer.
        unsafe {
            ztcore::ZT_freeBuffer(self.zt_core_buf as *mut c_void);
        }
    }
}
