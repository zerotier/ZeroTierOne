use crate::fd::OwnedFd;
use crate::{backend, io};

pub use backend::io::types::EventfdFlags;

/// `eventfd(initval, flags)`—Creates a file descriptor for event
/// notification.
///
/// # References
///  - [Linux]
///
/// [Linux]: https://man7.org/linux/man-pages/man2/eventfd.2.html
#[inline]
pub fn eventfd(initval: u32, flags: EventfdFlags) -> io::Result<OwnedFd> {
    backend::io::syscalls::eventfd(initval, flags)
}
