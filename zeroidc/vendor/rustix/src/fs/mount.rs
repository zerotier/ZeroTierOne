//! Linux `mount`.

use crate::backend::fs::types::{
    InternalMountFlags, MountFlags, MountFlagsArg, MountPropagationFlags,
};
use crate::{backend, io, path};

/// `mount(source, target, filesystemtype, mountflags, data)`
///
/// # References
///  - [Linux]
///
/// [Linux]: https://man7.org/linux/man-pages/man2/mount.2.html
#[inline]
pub fn mount<Source: path::Arg, Target: path::Arg, Fs: path::Arg, Data: path::Arg>(
    source: Source,
    target: Target,
    file_system_type: Fs,
    flags: MountFlags,
    data: Data,
) -> io::Result<()> {
    source.into_with_c_str(|source| {
        target.into_with_c_str(|target| {
            file_system_type.into_with_c_str(|file_system_type| {
                data.into_with_c_str(|data| {
                    backend::fs::syscalls::mount(
                        Some(source),
                        target,
                        Some(file_system_type),
                        MountFlagsArg(flags.bits()),
                        Some(data),
                    )
                })
            })
        })
    })
}

/// `mount(null, target, null, MS_REMOUNT | mountflags, data)`
///
/// # References
///  - [Linux]
///
/// [Linux]: https://man7.org/linux/man-pages/man2/mount.2.html
#[inline]
pub fn remount<Target: path::Arg, Data: path::Arg>(
    target: Target,
    flags: MountFlags,
    data: Data,
) -> io::Result<()> {
    target.into_with_c_str(|target| {
        data.into_with_c_str(|data| {
            backend::fs::syscalls::mount(
                None,
                target,
                None,
                MountFlagsArg(InternalMountFlags::REMOUNT.bits() | flags.bits()),
                Some(data),
            )
        })
    })
}

/// `mount(source, target, null, MS_BIND, null)`
///
/// # References
///  - [Linux]
///
/// [Linux]: https://man7.org/linux/man-pages/man2/mount.2.html
#[inline]
pub fn bind_mount<Source: path::Arg, Target: path::Arg>(
    source: Source,
    target: Target,
) -> io::Result<()> {
    source.into_with_c_str(|source| {
        target.into_with_c_str(|target| {
            backend::fs::syscalls::mount(
                Some(source),
                target,
                None,
                MountFlagsArg(MountFlags::BIND.bits()),
                None,
            )
        })
    })
}

/// `mount(source, target, null, MS_BIND | MS_REC, null)`
///
/// # References
///  - [Linux]
///
/// [Linux]: https://man7.org/linux/man-pages/man2/mount.2.html
#[inline]
pub fn recursive_bind_mount<Source: path::Arg, Target: path::Arg>(
    source: Source,
    target: Target,
) -> io::Result<()> {
    source.into_with_c_str(|source| {
        target.into_with_c_str(|target| {
            backend::fs::syscalls::mount(
                Some(source),
                target,
                None,
                MountFlagsArg(MountFlags::BIND.bits() | MountPropagationFlags::REC.bits()),
                None,
            )
        })
    })
}

/// `mount(null, target, null, mountflags, null)`
///
/// # References
///  - [Linux]
///
/// [Linux]: https://man7.org/linux/man-pages/man2/mount.2.html
#[inline]
pub fn change_mount<Target: path::Arg>(
    target: Target,
    flags: MountPropagationFlags,
) -> io::Result<()> {
    target.into_with_c_str(|target| {
        backend::fs::syscalls::mount(None, target, None, MountFlagsArg(flags.bits()), None)
    })
}

/// `mount(source, target, null, MS_MOVE, null)`
///
/// # References
///  - [Linux]
///
/// [Linux]: https://man7.org/linux/man-pages/man2/mount.2.html
#[inline]
pub fn move_mount<Source: path::Arg, Target: path::Arg>(
    source: Source,
    target: Target,
) -> io::Result<()> {
    source.into_with_c_str(|source| {
        target.into_with_c_str(|target| {
            backend::fs::syscalls::mount(
                Some(source),
                target,
                None,
                MountFlagsArg(InternalMountFlags::MOVE.bits()),
                None,
            )
        })
    })
}
