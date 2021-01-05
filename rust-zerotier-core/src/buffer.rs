use std::os::raw::c_void;
use std::ptr::null_mut;
use std::slice::{from_raw_parts, from_raw_parts_mut};

use crate::bindings::capi as ztcore;

pub struct Buffer {
    pub(crate) zt_core_buf: *mut u8,
    pub(crate) data_size: u32
}

impl Buffer {
    pub const CAPACITY: u32 = ztcore::ZT_BUF_SIZE;

    #[inline(always)]
    pub fn new() -> Buffer {
        let b = unsafe { ztcore::ZT_getBuffer() as *mut u8 };
        if b.is_null() {
            panic!("out of memory calling ZT_getBuffer()");
        }
        return Buffer {
            zt_core_buf: b,
            data_size: ztcore::ZT_BUF_SIZE
        };
    }

    #[inline(always)]
    pub fn len(&self) -> u32 {
        self.data_size
    }

    /// Set the size of the data held by this buffer. This is unsafe because
    /// setting it to a value larger than CAPACITY will place the buffer into
    /// an invalid state.
    #[inline(always)]
    pub unsafe fn set_len(&mut self, s: u32) {
        self.data_size = s;
    }
}

impl AsRef<&[u8]> for Buffer {
    #[inline(always)]
    fn as_ref(&self) -> &[u8] {
        unsafe {
            return from_raw_parts(self.zt_core_buf, self.data_size as usize);
        }
    }
}

impl AsMut<&[u8]> for Buffer {
    #[inline(always)]
    fn as_mut(&mut self) -> &mut [u8] {
        unsafe {
            return from_raw_parts_mut(self.zt_core_buf, self.data_size as usize);
        }
    }
}

impl Drop for Buffer {
    #[inline(always)]
    fn drop(&mut self) {
        // NOTE: in node.rs std::mem::forget() is used to prevent this from
        // being called on buffers that have been returned via one of the
        // process_XX() methods on ZT_Node. This destructor only exists to
        // return buffers that were not consumed normally.
        unsafe {
            ztcore::ZT_freeBuffer(self.zt_core_buf as *mut c_void);
        }
    }
}
