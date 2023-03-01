//! I/O operations.

mod close;
#[cfg(any(target_os = "android", target_os = "linux"))]
pub(crate) mod context;
#[cfg(not(windows))]
mod dup;
mod errno;
#[cfg(any(target_os = "android", target_os = "linux"))]
mod eventfd;
#[cfg(not(windows))]
mod fcntl;
#[cfg(not(feature = "std"))]
pub(crate) mod fd;
mod ioctl;
#[cfg(not(any(windows, target_os = "redox")))]
mod is_read_write;
#[cfg(not(any(windows, target_os = "wasi")))]
mod pipe;
mod poll;
#[cfg(all(feature = "procfs", any(target_os = "android", target_os = "linux")))]
mod procfs;
#[cfg(not(windows))]
mod read_write;
#[cfg(not(feature = "std"))]
mod seek_from;
#[cfg(not(windows))]
mod stdio;

#[cfg(any(target_os = "android", target_os = "linux"))]
pub use crate::backend::io::epoll;
pub use close::close;
#[cfg(not(any(windows, target_os = "aix", target_os = "wasi")))]
pub use dup::{dup, dup2, dup3, DupFlags};
pub use errno::{retry_on_intr, Errno, Result};
#[cfg(any(target_os = "android", target_os = "linux"))]
pub use eventfd::{eventfd, EventfdFlags};
#[cfg(not(any(windows, target_os = "wasi")))]
pub use fcntl::fcntl_dupfd_cloexec;
#[cfg(not(windows))]
pub use fcntl::{fcntl_getfd, fcntl_setfd, FdFlags};
#[cfg(any(target_os = "ios", target_os = "macos"))]
pub use ioctl::ioctl_fioclex;
pub use ioctl::ioctl_fionbio;
#[cfg(not(target_os = "redox"))]
pub use ioctl::ioctl_fionread;
#[cfg(any(target_os = "android", target_os = "linux"))]
pub use ioctl::{ioctl_blkpbszget, ioctl_blksszget};
#[cfg(not(any(windows, target_os = "haiku", target_os = "redox", target_os = "wasi")))]
pub use ioctl::{ioctl_tiocexcl, ioctl_tiocnxcl};
#[cfg(not(any(windows, target_os = "redox")))]
#[cfg(all(feature = "fs", feature = "net"))]
pub use is_read_write::is_read_write;
#[cfg(not(any(windows, target_os = "wasi")))]
pub use pipe::pipe;
#[cfg(not(any(
    windows,
    target_os = "haiku",
    target_os = "illumos",
    target_os = "redox",
    target_os = "solaris",
    target_os = "wasi",
)))]
pub use pipe::PIPE_BUF;
#[cfg(not(any(
    windows,
    target_os = "aix",
    target_os = "haiku",
    target_os = "ios",
    target_os = "macos",
    target_os = "wasi"
)))]
pub use pipe::{pipe_with, PipeFlags};
#[cfg(any(target_os = "android", target_os = "linux"))]
pub use pipe::{splice, vmsplice, IoSliceRaw, SpliceFlags};
pub use poll::{poll, PollFd, PollFlags};
#[cfg(all(feature = "procfs", any(target_os = "android", target_os = "linux")))]
pub use procfs::{
    proc_self_fd, proc_self_fdinfo_fd, proc_self_maps, proc_self_pagemap, proc_self_status,
};
#[cfg(not(windows))]
pub use read_write::{pread, pwrite, read, readv, write, writev, IoSlice, IoSliceMut};
#[cfg(not(any(
    windows,
    target_os = "haiku",
    target_os = "redox",
    target_os = "solaris"
)))]
pub use read_write::{preadv, pwritev};
#[cfg(any(target_os = "android", target_os = "linux"))]
pub use read_write::{preadv2, pwritev2, ReadWriteFlags};
#[cfg(not(feature = "std"))]
pub use seek_from::SeekFrom;
#[cfg(feature = "std")]
pub use std::io::SeekFrom;
#[cfg(not(windows))]
pub use stdio::{
    raw_stderr, raw_stdin, raw_stdout, stderr, stdin, stdout, take_stderr, take_stdin, take_stdout,
};
