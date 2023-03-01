//! Filesystem API constants, translated into `bitflags` constants.

use crate::backend;

pub use crate::io::FdFlags;
pub use backend::fs::types::{Access, Mode, OFlags};

#[cfg(not(target_os = "redox"))]
pub use backend::fs::types::AtFlags;

#[cfg(any(target_os = "ios", target_os = "macos"))]
pub use backend::fs::types::{CloneFlags, CopyfileFlags};

#[cfg(any(target_os = "android", target_os = "linux"))]
pub use backend::fs::types::{MountFlags, MountPropagationFlags, RenameFlags, ResolveFlags};

#[cfg(not(target_os = "redox"))]
pub use backend::fs::types::Dev;

pub use backend::time::types::{Nsecs, Secs, Timespec};
