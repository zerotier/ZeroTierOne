/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

use std::borrow::Borrow;
use std::fs::File;
use std::io::Read;
use std::path::Path;

use zerotier_core::{Identity, Locator};

use serde::Serialize;
use serde::de::DeserializeOwned;

use crate::osdep;

#[inline(always)]
pub(crate) fn ms_since_epoch() -> i64 {
    unsafe { osdep::msSinceEpoch() }
}

#[inline(always)]
pub(crate) fn ms_monotonic() -> i64 {
    unsafe { osdep::msMonotonic() }
}

/// Convenience function to read up to limit bytes from a file.
/// If the file is larger than limit, the excess is not read.
pub(crate) fn read_limit<P: AsRef<Path>>(path: P, limit: usize) -> std::io::Result<Vec<u8>> {
    let mut v: Vec<u8> = Vec::new();
    let _ = File::open(path)?.take(limit as u64).read_to_end(&mut v)?;
    Ok(v)
}

/// Read an identity as either a literal or from a file.
pub(crate) fn read_identity(input: &str, validate: bool) -> Result<Identity, String> {
    let parse_func = |s: &str| {
        Identity::new_from_string(s).map_or_else(|e| {
            Err(format!("invalid identity: {}", e.to_str()))
        }, |id| {
            if !validate || id.validate() {
                Ok(id)
            } else {
                Err(String::from("invalid identity: local validation failed"))
            }
        })
    };
    if Path::new(input).exists() {
        read_limit(input, 16384).map_or_else(|e| {
            Err(e.to_string())
        }, |v| {
            String::from_utf8(v).map_or_else(|e| {
                Err(e.to_string())
            }, |s| {
                parse_func(s.as_str())
            })
        })
    } else {
        parse_func(input)
    }
}

/// Read a locator as either a literal or from a file.
pub(crate) fn read_locator(input: &str) -> Result<Locator, String> {
    let parse_func = |s: &str| {
        Locator::new_from_string(s).map_or_else(|e| {
            Err(format!("invalid locator: {}", e.to_str()))
        }, |loc| {
            Ok(loc)
        })
    };
    if Path::new(input).exists() {
        read_limit(input, 16384).map_or_else(|e| {
            Err(e.to_string())
        }, |v| {
            String::from_utf8(v).map_or_else(|e| {
                Err(e.to_string())
            }, |s| {
                parse_func(s.as_str())
            })
        })
    } else {
        parse_func(input)
    }
}

/// Create a new HTTP authorization nonce by encrypting the current time.
/// The key used to encrypt the current time is random and is re-created for
/// each execution of the process. By decrypting this nonce when it is returned,
/// the client and server may check the age of a digest auth exchange.
pub(crate) fn create_http_auth_nonce(timestamp: i64) -> String {
    let mut nonce_plaintext: [u64; 2] = [timestamp as u64, 12345]; // the second u64 is arbitrary and unused
    unsafe {
        osdep::encryptHttpAuthNonce(nonce_plaintext.as_mut_ptr().cast());
        hex::encode(*nonce_plaintext.as_ptr().cast::<[u8; 16]>())
    }
}

/// Decrypt HTTP auth nonce encrypted by this process and return the timestamp.
/// This returns zero if the input was not valid.
pub(crate) fn decrypt_http_auth_nonce(nonce: &str) -> i64 {
    let nonce = hex::decode(nonce.trim());
    if nonce.is_err() {
        return 0;
    }
    let mut nonce = nonce.unwrap();
    if nonce.len() != 16 {
        return 0;
    }
    unsafe {
        osdep::decryptHttpAuthNonce(nonce.as_mut_ptr().cast());
        let nonce = *nonce.as_ptr().cast::<[u64; 2]>();
        nonce[0] as i64
    }
}

/// Shortcut to use serde_json to serialize an object, returns "null" on error.
pub(crate) fn to_json<O: serde::Serialize>(o: &O) -> String {
    serde_json::to_string(o).unwrap_or("null".into())
}

/// Shortcut to use serde_json to serialize an object, returns "null" on error.
pub(crate) fn to_json_pretty<O: serde::Serialize>(o: &O) -> String {
    serde_json::to_string_pretty(o).unwrap_or("null".into())
}

/// Recursively patch a JSON object.
/// This is slightly different from a usual JSON merge. For objects in the target their fields
/// are updated by recursively calling json_patch if the same field is present in the source.
/// If the source tries to set an object to something other than another object, this is ignored.
/// Other fields are replaced. This is used for RESTful config object updates. The depth limit
/// field is to prevent stack overflows via the API.
pub(crate) fn json_patch(target: &mut serde_json::value::Value, source: &serde_json::value::Value, depth_limit: usize) {
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
pub(crate) fn json_patch_object<O: Serialize + DeserializeOwned + Eq>(obj: O, patch: &str, depth_limit: usize) -> Result<Option<O>, serde_json::Error> {
    serde_json::from_str::<serde_json::value::Value>(patch).map_or_else(|e| Err(e), |patch| {
        serde_json::value::to_value(obj.borrow()).map_or_else(|e| Err(e), |mut obj_value| {
            json_patch(&mut obj_value, &patch, depth_limit);
            serde_json::value::from_value::<O>(obj_value).map_or_else(|e| Err(e), |obj_merged| {
                if obj == obj_merged {
                    Ok(None)
                } else {
                    Ok(Some(obj_merged))
                }
            })
        })
    })
}
