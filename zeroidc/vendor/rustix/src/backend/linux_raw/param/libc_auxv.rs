//! Linux auxv support, using libc.
//!
//! # Safety
//!
//! This uses raw pointers to locate and read the kernel-provided auxv array.
#![allow(unsafe_code)]

#[cfg(any(feature = "param", feature = "runtime"))]
use super::super::c;
use super::super::elf::*;
#[cfg(feature = "param")]
use crate::ffi::CStr;
#[cfg(feature = "runtime")]
use core::slice;

#[cfg(feature = "param")]
#[inline]
pub(crate) fn page_size() -> usize {
    unsafe { libc::sysconf(libc::_SC_PAGESIZE) as usize }
}

#[cfg(feature = "param")]
#[inline]
pub(crate) fn clock_ticks_per_second() -> u64 {
    unsafe { libc::getauxval(libc::AT_CLKTCK) as u64 }
}

#[cfg(feature = "param")]
#[inline]
pub(crate) fn linux_hwcap() -> (usize, usize) {
    unsafe {
        (
            libc::getauxval(libc::AT_HWCAP) as usize,
            libc::getauxval(libc::AT_HWCAP2) as usize,
        )
    }
}

#[cfg(feature = "param")]
#[inline]
pub(crate) fn linux_execfn() -> &'static CStr {
    unsafe {
        let execfn = libc::getauxval(libc::AT_EXECFN) as *const c::c_char;
        CStr::from_ptr(execfn.cast())
    }
}

#[cfg(feature = "runtime")]
#[inline]
pub(crate) fn exe_phdrs() -> (*const c::c_void, usize) {
    unsafe {
        (
            libc::getauxval(libc::AT_PHDR) as *const c::c_void,
            libc::getauxval(libc::AT_PHNUM) as usize,
        )
    }
}

#[cfg(feature = "runtime")]
#[inline]
pub(in super::super) fn exe_phdrs_slice() -> &'static [Elf_Phdr] {
    let (phdr, phnum) = exe_phdrs();

    // Safety: We assume the `AT_PHDR` and `AT_PHNUM` values provided by the
    // kernel form a valid slice.
    unsafe { slice::from_raw_parts(phdr.cast(), phnum) }
}

/// `AT_SYSINFO_EHDR` isn't present on all platforms in all configurations,
/// so if we don't see it, this function returns a null pointer.
#[inline]
pub(in super::super) fn sysinfo_ehdr() -> *const Elf_Ehdr {
    unsafe { libc::getauxval(linux_raw_sys::general::AT_SYSINFO_EHDR.into()) as *const Elf_Ehdr }
}
