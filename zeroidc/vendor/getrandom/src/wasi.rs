// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Implementation for WASI
use crate::Error;
use core::num::NonZeroU32;
use wasi::random_get;

pub fn getrandom_inner(dest: &mut [u8]) -> Result<(), Error> {
    unsafe {
        random_get(dest.as_mut_ptr(), dest.len()).map_err(|e: wasi::Error| {
            // convert wasi's Error into getrandom's NonZeroU32 error
            // SAFETY: `wasi::Error` is `NonZeroU16` internally, so `e.raw_error()`
            // will never return 0
            NonZeroU32::new_unchecked(e.raw_error() as u32).into()
        })
    }
}
