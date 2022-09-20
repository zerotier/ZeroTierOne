// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::path::Path;
use std::str::FromStr;

use tokio::fs::File;
use tokio::io::AsyncReadExt;

use zerotier_network_hypervisor::vl1::Identity;

/// Default sanity limit parameter for read_limit() used throughout the service.
pub const DEFAULT_FILE_IO_READ_LIMIT: usize = 1048576;

/// Convenience function to read up to limit bytes from a file.
///
/// If the file is larger than limit, the excess is not read.
pub async fn read_limit<P: AsRef<Path>>(path: P, limit: usize) -> std::io::Result<Vec<u8>> {
    let mut f = File::open(path).await?;
    let bytes = f.metadata().await?.len().min(limit as u64) as usize;
    let mut v: Vec<u8> = Vec::with_capacity(bytes);
    v.resize(bytes, 0);
    f.read_exact(v.as_mut_slice()).await?;
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

/// Returns true if the string starts with [yY1tT] or false for [nN0fF].
pub fn parse_bool(v: &str) -> Result<bool, String> {
    if !v.is_empty() {
        match v.trim().chars().next().unwrap() {
            'y' | 'Y' | '1' | 't' | 'T' => {
                return Ok(true);
            }
            'n' | 'N' | '0' | 'f' | 'F' => {
                return Ok(false);
            }
            _ => {}
        }
    }
    Err(format!("invalid boolean value: '{}'", v))
}

/// Returns a non-error if a string is a valid boolean.
pub fn is_valid_bool(v: &str) -> Result<(), String> {
    parse_bool(v).map(|_| ())
}

/// Returns a non-error if the string is a valid port number.
pub fn is_valid_port(v: &str) -> Result<(), String> {
    let i = isize::from_str(v).unwrap_or(0);
    if i >= 0x0001 && i <= 0xffff {
        return Ok(());
    }
    Err(format!("invalid TCP/IP port number: {}", v))
}

/// Read an identity as either a literal or from a file.
pub async fn parse_cli_identity(input: &str, validate: bool) -> Result<Identity, String> {
    let parse_func = |s: &str| {
        Identity::from_str(s).map_or_else(
            |e| Err(format!("invalid identity: {}", e.to_string())),
            |id| {
                if !validate || id.validate_identity() {
                    Ok(id)
                } else {
                    Err(String::from("invalid identity: local validation failed"))
                }
            },
        )
    };

    let input_p = Path::new(input);
    if input_p.is_file() {
        read_limit(input_p, 16384).await.map_or_else(
            |e| Err(e.to_string()),
            |v| String::from_utf8(v).map_or_else(|e| Err(e.to_string()), |s| parse_func(s.as_str())),
        )
    } else {
        parse_func(input)
    }
}

//#[cfg(unix)]
//pub fn c_strerror() -> String {
//    unsafe { std::ffi::CStr::from_ptr(libc::strerror(*libc::__error()).cast()).to_string_lossy().to_string() }
//}
