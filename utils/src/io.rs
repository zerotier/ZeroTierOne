// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::fs::File;
use std::io::Read;
use std::path::Path;

/// Default sanity limit parameter for read_limit() used throughout the service.
pub const DEFAULT_FILE_IO_READ_LIMIT: usize = 262144;

/// Convenience function to read up to limit bytes from a file.
///
/// If the file is larger than limit, the excess is not read.
pub fn read_limit<P: AsRef<Path>>(path: P, limit: usize) -> std::io::Result<Vec<u8>> {
    let mut f = File::open(path)?;
    let bytes = f.metadata()?.len().min(limit as u64) as usize;
    let mut v: Vec<u8> = Vec::with_capacity(bytes);
    v.resize(bytes, 0);
    f.read_exact(v.as_mut_slice())?;
    Ok(v)
}

/// Set permissions on a file or directory to be most restrictive (visible only to the service's user).
#[cfg(unix)]
pub fn fs_restrict_permissions<P: AsRef<Path>>(path: P) -> bool {
    unsafe {
        let c_path = std::ffi::CString::new(path.as_ref().to_str().unwrap()).unwrap();
        libc::chmod(
            c_path.as_ptr(),
            if path.as_ref().is_dir() {
                0o700
            } else {
                0o600
            },
        ) == 0
    }
}
