/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::error::Error;
use std::str::FromStr;
use std::time::{Instant, SystemTime, UNIX_EPOCH};

use serde::de::DeserializeOwned;
use serde::{Serialize, Serializer};

use lazy_static::lazy_static;

use tokio::fs::File;
use tokio::io::{AsyncRead, AsyncReadExt};

use crate::jsonformatter::JsonFormatter;

use zerotier_network_hypervisor::vl1::Identity;

lazy_static! {
    static ref STARTUP_INSTANT: Instant = Instant::now();
}

/// Get milliseconds since unix epoch.
pub fn ms_since_epoch() -> i64 {
    SystemTime::now().duration_since(UNIX_EPOCH).unwrap().as_millis() as i64
}

/// Get milliseconds since an arbitrary time in the past, guaranteed to monotonically increase.
pub fn ms_monotonic() -> i64 {
    Instant::now().duration_since(*STARTUP_INSTANT).as_millis() as i64
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

/// Recursively patch a JSON object.
///
/// This is slightly different from a usual JSON merge. For objects in the target their fields
/// are updated by recursively calling json_patch if the same field is present in the source.
/// If the source tries to set an object to something other than another object, this is ignored.
/// Other fields are replaced. This is used for RESTful config object updates. The depth limit
/// field is to prevent stack overflows via the API.
pub fn json_patch(target: &mut serde_json::value::Value, source: &serde_json::value::Value, depth_limit: usize) {
    if target.is_object() {
        if source.is_object() {
            let target = target.as_object_mut().unwrap();
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
///
/// If there are no changes, None is returned. The depth limit is passed through to json_patch and
/// should be set to a sanity check value to prevent overflows.
pub fn json_patch_object<O: Serialize + DeserializeOwned + Eq>(obj: O, patch: &str, depth_limit: usize) -> Result<Option<O>, serde_json::Error> {
    serde_json::from_str::<serde_json::value::Value>(patch).map_or_else(
        |e| Err(e),
        |patch| {
            serde_json::value::to_value(&obj).map_or_else(
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

/// Shortcut to use serde_json to serialize an object, returns "null" on error.
pub fn to_json<O: serde::Serialize>(o: &O) -> String {
    serde_json::to_string(o).unwrap_or("null".into())
}

/// Shortcut to use serde_json to serialize an object, returns "null" on error.
pub fn to_json_pretty<O: serde::Serialize>(o: &O) -> String {
    let mut buf = Vec::new();
    let mut ser = serde_json::Serializer::with_formatter(&mut buf, JsonFormatter::new());
    if o.serialize(&mut ser).is_ok() {
        String::from_utf8(buf).unwrap_or_else(|_| "null".into())
    } else {
        "null".into()
    }
}

/// Convenience function to read up to limit bytes from a file.
///
/// If the file is larger than limit, the excess is not read.
pub async fn read_limit(path: &str, limit: usize) -> std::io::Result<Vec<u8>> {
    let mut f = File::open(path).await?;
    let bytes = f.metadata().await?.len().min(limit as u64) as usize;
    let mut v: Vec<u8> = Vec::with_capacity(bytes);
    v.resize(bytes, 0);
    f.read_exact(v.as_mut_slice()).await?;
    Ok(v)
}

/// Returns true if the file exists and is a regular file (or a link to one).
pub async fn file_exists(path: &str) -> bool {
    tokio::fs::metadata(path).await.is_ok()
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
    if file_exists(input).await {
        read_limit(input, 16384).await.map_or_else(|e| Err(e.to_string()), |v| String::from_utf8(v).map_or_else(|e| Err(e.to_string()), |s| parse_func(s.as_str())))
    } else {
        parse_func(input)
    }
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
