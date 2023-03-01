//! The Unix `fcntl` function is effectively lots of different functions
//! hidden behind a single dynamic dispatch interface. In order to provide
//! a type-safe API, rustix makes them all separate functions so that they
//! can have dedicated static type signatures.

use crate::{backend, io};
use backend::fd::AsFd;
use backend::fs::types::OFlags;

// These `fcntl` functions like in the `io` module because they're not specific
// to files, directories, or memfd objects. We re-export them here in the `fs`
// module because the other the `fcntl` functions are here.
#[cfg(not(target_os = "wasi"))]
pub use crate::io::fcntl_dupfd_cloexec;
pub use crate::io::{fcntl_getfd, fcntl_setfd};

/// `fcntl(fd, F_GETFL)`—Returns a file descriptor's access mode and status.
///
/// # References
///  - [POSIX]
///  - [Linux]
///
/// [POSIX]: https://pubs.opengroup.org/onlinepubs/9699919799/functions/fcntl.html
/// [Linux]: https://man7.org/linux/man-pages/man2/fcntl.2.html
#[inline]
#[doc(alias = "F_GETFL")]
pub fn fcntl_getfl<Fd: AsFd>(fd: Fd) -> io::Result<OFlags> {
    backend::fs::syscalls::fcntl_getfl(fd.as_fd())
}

/// `fcntl(fd, F_SETFL, flags)`—Sets a file descriptor's status.
///
/// # References
///  - [POSIX]
///  - [Linux]
///
/// [POSIX]: https://pubs.opengroup.org/onlinepubs/9699919799/functions/fcntl.html
/// [Linux]: https://man7.org/linux/man-pages/man2/fcntl.2.html
#[inline]
#[doc(alias = "F_SETFL")]
pub fn fcntl_setfl<Fd: AsFd>(fd: Fd, flags: OFlags) -> io::Result<()> {
    backend::fs::syscalls::fcntl_setfl(fd.as_fd(), flags)
}

/// `fcntl(fd, F_GET_SEALS)`
///
/// # References
///  - [Linux]
///
/// [Linux]: https://man7.org/linux/man-pages/man2/fcntl.2.html
#[cfg(any(
    target_os = "android",
    target_os = "freebsd",
    target_os = "fuchsia",
    target_os = "linux",
))]
#[inline]
#[doc(alias = "F_GET_SEALS")]
pub fn fcntl_get_seals<Fd: AsFd>(fd: Fd) -> io::Result<SealFlags> {
    backend::fs::syscalls::fcntl_get_seals(fd.as_fd())
}

#[cfg(any(
    target_os = "android",
    target_os = "freebsd",
    target_os = "fuchsia",
    target_os = "linux",
))]
pub use backend::fs::types::SealFlags;

/// `fcntl(fd, F_ADD_SEALS)`
///
/// # References
///  - [Linux]
///
/// [Linux]: https://man7.org/linux/man-pages/man2/fcntl.2.html
#[cfg(any(
    target_os = "android",
    target_os = "freebsd",
    target_os = "fuchsia",
    target_os = "linux",
))]
#[inline]
#[doc(alias = "F_ADD_SEALS")]
pub fn fcntl_add_seals<Fd: AsFd>(fd: Fd, seals: SealFlags) -> io::Result<()> {
    backend::fs::syscalls::fcntl_add_seals(fd.as_fd(), seals)
}
