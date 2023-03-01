//! Bindings for the FreeBSD `procctl` system call.
//!
//! There are similarities (but also differences) with Linux's `prctl` system call, whose interface
//! is located in the `prctl.rs` file.

#![allow(unsafe_code)]

use core::mem::MaybeUninit;

use crate::backend::c::{c_int, c_uint, c_void};
use crate::backend::process::syscalls;
use crate::backend::process::types::{RawId, Signal};
use crate::io;
use crate::process::{Pid, RawPid};

//
// Helper functions.
//

/// Subset of `idtype_t` C enum, with only the values allowed by `procctl`.
#[repr(i32)]
pub enum IdType {
    /// Process id.
    Pid = 0,
    /// Process group id.
    Pgid = 2,
}

/// A process selector for use with the `procctl` interface.
///
/// `None` represents the current process. `Some((IdType::Pid, pid))` represents the process
/// with pid `pid`. `Some((IdType::Pgid, pgid))` represents the control processes belonging to
/// the process group with id `pgid`.
pub type ProcSelector = Option<(IdType, Pid)>;
fn proc_selector_to_raw(selector: ProcSelector) -> (IdType, RawPid) {
    match selector {
        Some((idtype, id)) => (idtype, id.as_raw_nonzero().get()),
        None => (IdType::Pid, 0),
    }
}

#[inline]
pub(crate) unsafe fn procctl(
    option: c_int,
    process: ProcSelector,
    data: *mut c_void,
) -> io::Result<()> {
    let (idtype, id) = proc_selector_to_raw(process);
    syscalls::procctl(idtype as c_uint, id as RawId, option, data)
}

#[inline]
pub(crate) unsafe fn procctl_set<P>(
    option: c_int,
    process: ProcSelector,
    data: &P,
) -> io::Result<()> {
    procctl(option, process, (data as *const P as *mut P).cast())
}

#[inline]
pub(crate) unsafe fn procctl_get_optional<P>(
    option: c_int,
    process: ProcSelector,
) -> io::Result<P> {
    let mut value: MaybeUninit<P> = MaybeUninit::uninit();
    procctl(option, process, value.as_mut_ptr().cast())?;
    Ok(value.assume_init())
}

//
// PROC_PDEATHSIG_STATUS/PROC_PDEATHSIG_CTL
//

const PROC_PDEATHSIG_STATUS: c_int = 12;

/// Get the current value of the parent process death signal.
///
/// # References
/// - [Linux: `prctl(PR_GET_PDEATHSIG,...)`]
/// - [FreeBSD: `procctl(PROC_PDEATHSIG_STATUS,...)`]
///
/// [Linux: `prctl(PR_GET_PDEATHSIG,...)`]: https://man7.org/linux/man-pages/man2/prctl.2.html
/// [FreeBSD: `procctl(PROC_PDEATHSIG_STATUS,...)`]: https://www.freebsd.org/cgi/man.cgi?query=procctl&sektion=2
#[inline]
pub fn parent_process_death_signal() -> io::Result<Option<Signal>> {
    unsafe { procctl_get_optional::<c_int>(PROC_PDEATHSIG_STATUS, None) }.map(Signal::from_raw)
}

const PROC_PDEATHSIG_CTL: c_int = 11;

/// Set the parent-death signal of the calling process.
///
/// # References
/// - [Linux: `prctl(PR_SET_PDEATHSIG,...)`]
/// - [FreeBSD: `procctl(PROC_PDEATHSIG_CTL,...)`]
///
/// [Linux: `prctl(PR_SET_PDEATHSIG,...)`]: https://man7.org/linux/man-pages/man2/prctl.2.html
/// [FreeBSD: `procctl(PROC_PDEATHSIG_CTL,...)`]: https://www.freebsd.org/cgi/man.cgi?query=procctl&sektion=2
#[inline]
pub fn set_parent_process_death_signal(signal: Option<Signal>) -> io::Result<()> {
    let signal = signal.map_or(0, |signal| signal as c_int);
    unsafe { procctl_set::<c_int>(PROC_PDEATHSIG_CTL, None, &signal) }
}

//
// PROC_TRACE_CTL
//

const PROC_TRACE_CTL: c_int = 7;

const PROC_TRACE_CTL_ENABLE: i32 = 1;
const PROC_TRACE_CTL_DISABLE: i32 = 2;
const PROC_TRACE_CTL_DISABLE_EXEC: i32 = 3;

/// `PROC_TRACE_CTL_*`.
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
#[repr(i32)]
pub enum DumpableBehavior {
    /// Not dumpable.
    NotDumpable = PROC_TRACE_CTL_DISABLE,
    /// Dumpable.
    Dumpable = PROC_TRACE_CTL_ENABLE,
    /// Not dumpable, and this behaviour is preserved across `execve` calls.
    NotDumpableExecPreserved = PROC_TRACE_CTL_DISABLE_EXEC,
}

/// Set the state of the `dumpable` attribute for the process indicated by `idtype` and `id`.
/// This determines whether the process can be traced and whether core dumps are produced for
/// the process upon delivery of a signal whose default behavior is to produce a core dump.
///
/// This is similar to `set_dumpable_behavior` on Linux, with the exception that on FreeBSD
/// there is an extra argument `process`. When `process` is set to `None`, the operation is
/// performed for the current process, like on Linux.
///
/// # References
/// - [`procctl(PROC_TRACE_CTL,...)`]
///
/// [`procctl(PROC_TRACE_CTL,...)`]: https://www.freebsd.org/cgi/man.cgi?query=procctl&sektion=2
#[inline]
pub fn set_dumpable_behavior(process: ProcSelector, config: DumpableBehavior) -> io::Result<()> {
    unsafe { procctl(PROC_TRACE_CTL, process, config as usize as *mut _) }
}

//
// PROC_TRACE_STATUS
//

const PROC_TRACE_STATUS: c_int = 8;

/// Tracing status as returned by [`trace_status`].
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum TracingStatus {
    /// Tracing is disabled for the process.
    NotTraceble,
    /// Tracing is not disabled for the process, but not debugger/tracer is attached.
    Tracable,
    /// The process is being traced by the process whose pid is stored in the first
    /// component of this variant.
    BeingTraced(Pid),
}

/// Get the tracing status of the process indicated by `idtype` and `id`.
///
/// # References
/// - [`procctl(PROC_TRACE_STATUS,...)`]
///
/// [`procctl(PROC_TRACE_STATUS,...)`]: https://www.freebsd.org/cgi/man.cgi?query=procctl&sektion=2
#[inline]
pub fn trace_status(process: ProcSelector) -> io::Result<TracingStatus> {
    let val = unsafe { procctl_get_optional::<c_int>(PROC_TRACE_STATUS, process) }?;
    match val {
        -1 => Ok(TracingStatus::NotTraceble),
        0 => Ok(TracingStatus::Tracable),
        pid => {
            let pid = unsafe { Pid::from_raw(pid as RawPid) }.ok_or(io::Errno::RANGE)?;
            Ok(TracingStatus::BeingTraced(pid))
        }
    }
}
