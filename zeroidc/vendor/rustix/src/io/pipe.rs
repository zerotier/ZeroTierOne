#![allow(unsafe_code)]

use crate::fd::OwnedFd;
use crate::{backend, io};
#[cfg(any(target_os = "android", target_os = "linux"))]
use backend::fd::AsFd;

#[cfg(not(any(target_os = "ios", target_os = "macos")))]
pub use backend::io::types::PipeFlags;

#[cfg(any(target_os = "android", target_os = "linux"))]
pub use backend::io::types::{IoSliceRaw, SpliceFlags};

/// `PIPE_BUF`—The maximum length at which writes to a pipe are atomic.
///
/// # References
///  - [Linux]
///  - [POSIX]
///
/// [Linux]: https://man7.org/linux/man-pages/man7/pipe.7.html
/// [POSIX]: https://pubs.opengroup.org/onlinepubs/9699919799/functions/write.html
#[cfg(not(any(
    windows,
    target_os = "haiku",
    target_os = "illumos",
    target_os = "redox",
    target_os = "solaris",
    target_os = "wasi",
)))]
pub const PIPE_BUF: usize = backend::io::types::PIPE_BUF;

/// `pipe()`—Creates a pipe.
///
/// This function creates a pipe and returns two file descriptors, for the
/// reading and writing ends of the pipe, respectively.
///
/// # References
///  - [POSIX]
///  - [Linux]
///
/// [POSIX]: https://pubs.opengroup.org/onlinepubs/9699919799/functions/pipe.html
/// [Linux]: https://man7.org/linux/man-pages/man2/pipe.2.html
#[inline]
pub fn pipe() -> io::Result<(OwnedFd, OwnedFd)> {
    backend::io::syscalls::pipe()
}

/// `pipe2(flags)`—Creates a pipe, with flags.
///
/// This function creates a pipe and returns two file descriptors, for the
/// reading and writing ends of the pipe, respectively.
///
/// # References
///  - [Linux]
///
/// [Linux]: https://man7.org/linux/man-pages/man2/pipe2.2.html
#[cfg(not(any(
    target_os = "aix",
    target_os = "haiku",
    target_os = "ios",
    target_os = "macos"
)))]
#[inline]
#[doc(alias = "pipe2")]
pub fn pipe_with(flags: PipeFlags) -> io::Result<(OwnedFd, OwnedFd)> {
    backend::io::syscalls::pipe_with(flags)
}

/// `splice(fd_in, off_in, fd_out, off_out, len, flags)`—Transfer data between a file and a pipe.
///
/// This function transfers up to `len` bytes of data from the file descriptor `fd_in`
/// to the file descriptor `fd_out`, where one of the file descriptors
/// must refer to a pipe.
///
/// `off_*` must be `None` if the corresponding fd refers to a pipe.
/// Otherwise its value points to the starting offset to the file,
/// from which the data is read/written.
/// on success the number of bytes read/written is added to the offset.
///
/// passing `None` causes the read/write to start from the file offset,
/// and the file offset is adjusted appropriately.
///
/// # References
///  - [Linux]
///
/// [Linux]: https://man7.org/linux/man-pages/man2/splice.2.html
#[cfg(any(target_os = "android", target_os = "linux"))]
#[inline]
pub fn splice<FdIn: AsFd, FdOut: AsFd>(
    fd_in: FdIn,
    off_in: Option<&mut u64>,
    fd_out: FdOut,
    off_out: Option<&mut u64>,
    len: usize,
    flags: SpliceFlags,
) -> io::Result<usize> {
    backend::io::syscalls::splice(fd_in.as_fd(), off_in, fd_out.as_fd(), off_out, len, flags)
}

/// `vmsplice(fd, bufs, flags)`—Transfer data between memory and a pipe.
///
/// If `fd` is the write end of the pipe,
/// the function maps the memory pointer at by `bufs` to the pipe.
///
/// If `fd` is the read end of the pipe,
/// the function writes data from the pipe to said memory.
///
/// # Safety
///
/// If the memory must not be mutated (such as when `bufs` were originally immutable slices),
/// it is up to the caller to ensure that the write end of the pipe is placed in `fd`.
///
/// Additionally if `SpliceFlags::GIFT` is set, the caller must also ensure
/// that the contents of `bufs` in never modified following the call,
/// and that all of the pointers in `bufs` are page aligned,
/// and the lengths are multiples of a page size in bytes.
///
/// # References
///  - [Linux]
///
/// [Linux]: https://man7.org/linux/man-pages/man2/vmsplice.2.html
#[cfg(any(target_os = "android", target_os = "linux"))]
#[inline]
pub unsafe fn vmsplice<PipeFd: AsFd>(
    fd: PipeFd,
    bufs: &[io::IoSliceRaw],
    flags: SpliceFlags,
) -> io::Result<usize> {
    backend::io::syscalls::vmsplice(fd.as_fd(), bufs, flags)
}
