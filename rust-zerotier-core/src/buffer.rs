/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

use std::os::raw::c_void;

use crate::bindings::capi as ztcore;

/// A reusable buffer for I/O to/from the ZeroTier core.
/// The core allocates and manages a pool of these. This provides a Rust
/// interface to that pool. ZT core buffers are used to reduce the need for
/// memory copying by passing buffers around instead of memcpy'ing when
/// packet data is passed into and out of the core.
pub struct Buffer {
    pub(crate) zt_core_buf: *mut u8,
    pub(crate) data_size: usize,
}

impl Buffer {
    /// Maximum capacity of a ZeroTier reusable buffer.
    pub const CAPACITY: usize = ztcore::ZT_BUF_SIZE as usize;

    /// Obtain a new buffer from the core and set the size of its data to CAPACITY.
    /// The contents of the buffer are not defined.
    #[inline(always)]
    pub fn new() -> Buffer {
        let b = unsafe { ztcore::ZT_getBuffer() as *mut u8 };
        if b.is_null() {
            panic!("out of memory calling ZT_getBuffer()");
        }
        Buffer {
            zt_core_buf: b,
            data_size: ztcore::ZT_BUF_SIZE as usize
        }
    }

    /// Get the current size of the data held by this buffer.
    /// Initially this is equal to CAPACITY.
    #[inline(always)]
    pub fn len(&self) -> usize {
        self.data_size
    }

    #[inline(always)]
    pub fn as_ptr(&self) -> *const u8 {
        self.zt_core_buf
    }

    #[inline(always)]
    pub fn as_mut_ptr(&mut self) -> *mut u8 {
        self.zt_core_buf
    }

    /// Set the size of the data held by this buffer. This is unsafe because
    /// setting it to a value larger than CAPACITY will place the buffer into
    /// an invalid state.
    #[inline(always)]
    pub unsafe fn set_len(&mut self, s: usize) {
        self.data_size = s;
    }
}

impl Drop for Buffer {
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
