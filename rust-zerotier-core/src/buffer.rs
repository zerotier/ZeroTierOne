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

use std::os::raw::c_void;
use crate::capi as ztcore;

/// A reusable buffer for I/O to/from the ZeroTier core.
///
/// The core allocates and manages a pool of these. This provides a Rust
/// interface to that pool. ZT core buffers are used to reduce the need for
/// memory copying by passing buffers around instead of memcpy'ing when
/// packet data is passed into and out of the core.
///
/// IMPORTANT NOTE: when these are fed into the ZeroTier core, drop() is
/// elided via std::mem::forget(). Node does this automatically so usually
/// users of this API do not need to be aware of it, but it's worth mentioning
/// in case someone re-implements calls directly into the core. Dropping this
/// after handing it back to the core could result in mytserious corruption
/// bugs or double-free.
///
/// This does not implement copy or clone because that would result in this
/// memory being dropped more than once. Use Rc or Arc to share.
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

    /// Set the size of the data held by this buffer.
    /// This is usually called after writing data into the buffer.
    #[inline(always)]
    pub fn set_len(&mut self, s: usize) {
        // CAPACITY will always be a power of two, so we can just mask this
        // to make this safe. This is a sanity check to make it impossible to
        // set this to an invalid size.
        self.data_size = s & (Buffer::CAPACITY - 1);
    }
}

impl Drop for Buffer {
    #[inline(always)]
    fn drop(&mut self) {
        unsafe {
            ztcore::ZT_freeBuffer(self.zt_core_buf as *mut c_void);
        }
    }
}
