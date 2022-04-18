/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::borrow::Borrow;
use std::fs::File;
use std::io::Read;
use std::path::Path;
use std::str::FromStr;

use serde::de::DeserializeOwned;
use serde::Serialize;

use zerotier_core_crypto::hex;
use zerotier_network_hypervisor::vl1::Identity;

use crate::osdep;

pub fn ms_since_epoch() -> i64 {
    std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).unwrap().as_millis() as i64
}

#[cfg(any(target_os = "macos", target_os = "ios"))]
pub fn ms_monotonic() -> i64 {
    unsafe {
        let mut tb: mach::mach_time::mach_timebase_info_data_t = std::mem::zeroed();
        if mach::mach_time::mach_timebase_info(&mut tb) == 0 {
            let mt = mach::mach_time::mach_continuous_approximate_time(); // ZT doesn't need it to be *that* exact, and this is faster
            (((mt as u128) * tb.numer as u128 * 1000000_u128) / (tb.denom as u128)) as i64
        // milliseconds since X
        } else {
            panic!("FATAL: mach_timebase_info() failed");
        }
    }
}

#[cfg(not(any(target_os = "macos", target_os = "ios")))]
pub fn ms_monotonic() -> i64 {}

pub fn parse_bool(v: &str) -> Result<bool, String> {
    if !v.is_empty() {
        match v.chars().next().unwrap() {
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

pub fn is_valid_bool(v: String) -> Result<(), String> {
    parse_bool(v.as_str()).map(|_| ())
}

pub fn is_valid_port(v: String) -> Result<(), String> {
    let i = u16::from_str(v.as_str()).unwrap_or(0);
    if i >= 1 {
        return Ok(());
    }
    Err(format!("invalid TCP/IP port number: {}", v))
}

/// Convenience function to read up to limit bytes from a file.
/// If the file is larger than limit, the excess is not read.
pub fn read_limit<P: AsRef<Path>>(path: P, limit: usize) -> std::io::Result<Vec<u8>> {
    let mut v: Vec<u8> = Vec::new();
    let _ = File::open(path)?.take(limit as u64).read_to_end(&mut v)?;
    Ok(v)
}

/// Read an identity as either a literal or from a file.
pub fn parse_cli_identity(input: &str, validate: bool) -> Result<Identity, String> {
    let parse_func = |s: &str| {
        Identity::new_from_string(s).map_or_else(
            |e| Err(format!("invalid identity: {}", e.to_str())),
            |id| {
                if !validate || id.validate() {
                    Ok(id)
                } else {
                    Err(String::from("invalid identity: local validation failed"))
                }
            },
        )
    };
    if Path::new(input).exists() {
        read_limit(input, 16384).map_or_else(|e| Err(e.to_string()), |v| String::from_utf8(v).map_or_else(|e| Err(e.to_string()), |s| parse_func(s.as_str())))
    } else {
        parse_func(input)
    }
}

/// Create a new HTTP authorization nonce by encrypting the current time.
/// The key used to encrypt the current time is random and is re-created for
/// each execution of the process. By decrypting this nonce when it is returned,
/// the client and server may check the age of a digest auth exchange.
pub fn create_http_auth_nonce(timestamp: i64) -> String {
    let mut nonce_plaintext: [u64; 2] = [timestamp as u64, timestamp as u64];
    unsafe {
        osdep::encryptHttpAuthNonce(nonce_plaintext.as_mut_ptr().cast());
        hex::encode(*nonce_plaintext.as_ptr().cast::<[u8; 16]>())
    }
}

/// Decrypt HTTP auth nonce encrypted by this process and return the timestamp.
/// This returns zero if the input was not valid.
pub fn decrypt_http_auth_nonce(nonce: &str) -> i64 {
    let nonce = hex::decode(nonce.trim());
    if !nonce.is_err() {
        let mut nonce = nonce.unwrap();
        if nonce.len() == 16 {
            unsafe {
                osdep::decryptHttpAuthNonce(nonce.as_mut_ptr().cast());
                let nonce = *nonce.as_ptr().cast::<[u64; 2]>();
                if nonce[0] == nonce[1] {
                    return nonce[0] as i64;
                }
            }
        }
    }
    return 0;
}

/// Shortcut to use serde_json to serialize an object, returns "null" on error.
pub fn to_json<O: serde::Serialize>(o: &O) -> String {
    serde_json::to_string(o).unwrap_or("null".into())
}

/// Shortcut to use serde_json to serialize an object, returns "null" on error.
pub fn to_json_pretty<O: serde::Serialize>(o: &O) -> String {
    serde_json::to_string_pretty(o).unwrap_or("null".into())
}

/// Recursively patch a JSON object.
/// This is slightly different from a usual JSON merge. For objects in the target their fields
/// are updated by recursively calling json_patch if the same field is present in the source.
/// If the source tries to set an object to something other than another object, this is ignored.
/// Other fields are replaced. This is used for RESTful config object updates. The depth limit
/// field is to prevent stack overflows via the API.
pub fn json_patch(target: &mut serde_json::value::Value, source: &serde_json::value::Value, depth_limit: usize) {
    if target.is_object() {
        if source.is_object() {
            let mut target = target.as_object_mut().unwrap();
            let source = source.as_object().unwrap();
            for kv in target.iter_mut() {
                let _ = source.get(kv.0).map(|new_value| {
                    if depth_limit > 0 {
                        json_patch(kv.1, new_value, depth_limit - 1)
                    }
                });
            }
            for kv in source.iter() {
                if !target.contains_key(kv.0) && !kv.1.is_null() {
                    target.insert(kv.0.clone(), kv.1.clone());
                }
            }
        }
    } else if *target != *source {
        *target = source.clone();
    }
}

/// Patch a serializable object with the fields present in a JSON object.
/// If there are no changes, None is returned. The depth limit is passed through to json_patch and
/// should be set to a sanity check value to prevent overflows.
pub fn json_patch_object<O: Serialize + DeserializeOwned + Eq>(obj: O, patch: &str, depth_limit: usize) -> Result<Option<O>, serde_json::Error> {
    serde_json::from_str::<serde_json::value::Value>(patch).map_or_else(
        |e| Err(e),
        |patch| {
            serde_json::value::to_value(obj.borrow()).map_or_else(
                |e| Err(e),
                |mut obj_value| {
                    json_patch(&mut obj_value, &patch, depth_limit);
                    serde_json::value::from_value::<O>(obj_value).map_or_else(
                        |e| Err(e),
                        |obj_merged| {
                            if obj == obj_merged {
                                Ok(None)
                            } else {
                                Ok(Some(obj_merged))
                            }
                        },
                    )
                },
            )
        },
    )
}

#[cfg(test)]
mod tests {
    use crate::utils::ms_monotonic;
    use std::time::Duration;

    #[test]
    fn monotonic_clock_sanity_check() {
        let start = ms_monotonic();
        std::thread::sleep(Duration::from_millis(500));
        let end = ms_monotonic();
        assert!((end - start).abs() > 450);
        assert!((end - start).abs() < 550);
    }
}
