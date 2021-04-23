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

use std::os::raw::{c_char, c_int};

use num_derive::{FromPrimitive, ToPrimitive};
#[macro_use] extern crate base64_serde;

#[allow(non_snake_case,non_upper_case_globals,non_camel_case_types,dead_code,improper_ctypes)]
mod capi; // bindgen generated

mod identity;
mod address;
mod fingerprint;
mod endpoint;
mod certificate;
mod networkid;
mod inetaddress;
mod locator;
mod path;
mod peer;
mod node;
mod mac;
mod buffer;
mod portableatomici64;
mod virtualnetworkconfig;
mod multicastgroup;
mod dictionary;
pub mod trace;

use crate::capi as ztcore;

pub use identity::*;
pub use address::Address;
pub use fingerprint::Fingerprint;
pub use endpoint::*;
pub use certificate::*;
pub use networkid::NetworkId;
pub use inetaddress::*;
pub use locator::*;
pub use path::Path;
pub use peer::Peer;
pub use node::*;
pub use mac::MAC;
pub use buffer::Buffer;
pub use portableatomici64::PortableAtomicI64;
pub use virtualnetworkconfig::*;
pub use multicastgroup::MulticastGroup;
pub use dictionary::*;

base64_serde_type!(Base64URLSafeNoPad, base64::URL_SAFE_NO_PAD);

/// Recommended minimum thread stack size for background threads.
pub const RECOMMENDED_THREAD_STACK_SIZE: usize = 524288;

/// Default TCP and UDP port.
pub const DEFAULT_PORT: u16 = ztcore::ZT_DEFAULT_PORT as u16;

/// Default secondary UDP port if enabled.
pub const DEFAULT_SECONDARY_PORT: u16 = ztcore::ZT_DEFAULT_SECONDARY_PORT as u16;

/// Size of a ZeroTier core "Buffer" in bytes.
pub const BUF_SIZE: usize = ztcore::ZT_BUF_SIZE as usize;

/// Minimum physical MTU.
pub const MIN_MTU: u32 = ztcore::ZT_MIN_MTU;

/// Maximum physical MTU.
pub const MAX_MTU: u32 = ztcore::ZT_MAX_MTU;

/// Default physica UDP MTU (not including IP or UDP headers).
pub const DEFAULT_UDP_MTU: u32 = ztcore::ZT_DEFAULT_UDP_MTU;

/// Maximum UDP MTU (we never actually get this high).
pub const MAX_UDP_MTU: u32 = ztcore::ZT_MAX_UDP_MTU;

/// Base64 encode using the URL-safe with no padding configuration.
pub fn base64_encode<T: AsRef<[u8]>>(t: &T) -> String {
    base64::encode_config(t, base64::URL_SAFE_NO_PAD)
}

/// Base64 decode using the URL-safe with no padding configuration.
pub fn base64_decode<T: AsRef<[u8]>>(t: &T) -> Result<Vec<u8>, base64::DecodeError> {
    base64::decode_config(t, base64::URL_SAFE_NO_PAD)
}

#[allow(non_snake_case,non_upper_case_globals)]
pub mod RulePacketCharacteristicFlags {
    pub const Inbound: u64 = crate::capi::ZT_RULE_PACKET_CHARACTERISTICS_INBOUND as u64;
    pub const Multicast: u64 = crate::capi::ZT_RULE_PACKET_CHARACTERISTICS_MULTICAST as u64;
    pub const Broadcast: u64 = crate::capi::ZT_RULE_PACKET_CHARACTERISTICS_BROADCAST as u64;
    pub const SenderIpAuthenticated: u64 = crate::capi::ZT_RULE_PACKET_CHARACTERISTICS_SENDER_IP_AUTHENTICATED as u64;
    pub const SenderMacAuthenticated: u64 = crate::capi::ZT_RULE_PACKET_CHARACTERISTICS_SENDER_MAC_AUTHENTICATED as u64;
    pub const TcpFlagNS: u64 = crate::capi::ZT_RULE_PACKET_CHARACTERISTICS_TCP_NS as u64;
    pub const TcpFlagCWR: u64 = crate::capi::ZT_RULE_PACKET_CHARACTERISTICS_TCP_CWR as u64;
    pub const TcpFlagECE: u64 = crate::capi::ZT_RULE_PACKET_CHARACTERISTICS_TCP_ECE as u64;
    pub const TcpFlagURG: u64 = crate::capi::ZT_RULE_PACKET_CHARACTERISTICS_TCP_URG as u64;
    pub const TcpFlagACK: u64 = crate::capi::ZT_RULE_PACKET_CHARACTERISTICS_TCP_ACK as u64;
    pub const TcpFlagPSH: u64 = crate::capi::ZT_RULE_PACKET_CHARACTERISTICS_TCP_PSH as u64;
    pub const TcpFlagRST: u64 = crate::capi::ZT_RULE_PACKET_CHARACTERISTICS_TCP_RST as u64;
    pub const TcpFlagSYN: u64 = crate::capi::ZT_RULE_PACKET_CHARACTERISTICS_TCP_SYN as u64;
    pub const TcpFlagFIN: u64 = crate::capi::ZT_RULE_PACKET_CHARACTERISTICS_TCP_FIN as u64;
}

#[derive(FromPrimitive, ToPrimitive, PartialEq, Eq)]
pub enum CredentialType {
    Null = ztcore::ZT_CredentialType_ZT_CREDENTIAL_TYPE_NULL as isize,
    CertificateOfMembership = ztcore::ZT_CredentialType_ZT_CREDENTIAL_TYPE_COM as isize,
    Capability = ztcore::ZT_CredentialType_ZT_CREDENTIAL_TYPE_CAPABILITY as isize,
    Tag = ztcore::ZT_CredentialType_ZT_CREDENTIAL_TYPE_TAG as isize,
    CertificateOfOwnership = ztcore::ZT_CredentialType_ZT_CREDENTIAL_TYPE_COO as isize,
    Revocation = ztcore::ZT_CredentialType_ZT_CREDENTIAL_TYPE_REVOCATION as isize,
}

impl CredentialType {
    pub fn to_str(&self) -> &'static str {
        match *self {
            CredentialType::Null => "Null",
            CredentialType::CertificateOfMembership => "CertificateOfMembership",
            CredentialType::Capability => "Capability",
            CredentialType::Tag => "Tag",
            CredentialType::CertificateOfOwnership => "CertificateOfOwnership",
            CredentialType::Revocation => "Revocation",
        }
    }
}

#[derive(FromPrimitive, ToPrimitive, PartialEq, Eq)]
pub enum ResultCode {
    Ok = ztcore::ZT_ResultCode_ZT_RESULT_OK as isize,
    FatalErrorOutOfMemory = ztcore::ZT_ResultCode_ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY as isize,
    FatalErrorDataStoreFailed = ztcore::ZT_ResultCode_ZT_RESULT_FATAL_ERROR_DATA_STORE_FAILED as isize,
    FatalErrorInternal = ztcore::ZT_ResultCode_ZT_RESULT_FATAL_ERROR_INTERNAL as isize,
    ErrorNetworkNotFound = ztcore::ZT_ResultCode_ZT_RESULT_ERROR_NETWORK_NOT_FOUND as isize,
    ErrorUnsupportedOperation = ztcore::ZT_ResultCode_ZT_RESULT_ERROR_UNSUPPORTED_OPERATION as isize,
    ErrorBadParameter = ztcore::ZT_ResultCode_ZT_RESULT_ERROR_BAD_PARAMETER as isize,
    ErrorInvalidCredential = ztcore::ZT_ResultCode_ZT_RESULT_ERROR_INVALID_CREDENTIAL as isize,
    ErrorCollidingObject = ztcore::ZT_ResultCode_ZT_RESULT_ERROR_COLLIDING_OBJECT as isize,
    ErrorInternalNonFatal = ztcore::ZT_ResultCode_ZT_RESULT_ERROR_INTERNAL as isize,
}

impl ResultCode {
    pub fn to_str(&self) -> &'static str {
        match *self {
            ResultCode::Ok => "Ok",
            ResultCode::FatalErrorOutOfMemory => "FatalErrorOutOfMemory",
            ResultCode::FatalErrorDataStoreFailed => "FatalErrorDataStoreFailed",
            ResultCode::FatalErrorInternal => "FatalErrorInternal",
            ResultCode::ErrorNetworkNotFound => "ErrorNetworkNotFound",
            ResultCode::ErrorUnsupportedOperation => "ErrorUnsupportedOperation",
            ResultCode::ErrorBadParameter => "ErrorBadParameter",
            ResultCode::ErrorInvalidCredential => "ErrorInvalidCredential",
            ResultCode::ErrorCollidingObject => "ErrorCollidingObject",
            ResultCode::ErrorInternalNonFatal => "ErrorInternalNonFatal",
        }
    }
}

/// Returns a tuple of major, minor, revision, and build version numbers from the ZeroTier core.
pub fn version() -> (i32, i32, i32, i32) {
    let mut major: c_int = 0;
    let mut minor: c_int = 0;
    let mut revision: c_int = 0;
    let mut build: c_int = 0;
    unsafe {
        ztcore::ZT_version(&mut major as *mut c_int, &mut minor as *mut c_int, &mut revision as *mut c_int, &mut build as *mut c_int);
    }
    (major as i32, minor as i32, revision as i32, build as i32)
}

/// Get a random 64-bit integer using the non-cryptographic PRNG in the ZeroTier core.
#[inline(always)]
pub fn random() -> u64 {
    unsafe {
        return ztcore::ZT_random();
    }
}

/// Test whether this byte array or slice is all zeroes.
pub fn is_all_zeroes<B: AsRef<[u8]>>(b: B) -> bool {
    for c in b.as_ref().iter() {
        if *c != 0 {
            return false;
        }
    }
    true
}

/// The CStr stuff is cumbersome, so this is an easier to use function to turn a C string into a String.
/// This returns an empty string on a null pointer or invalid UTF-8. It's unsafe because it can crash if
/// the string is not zero-terminated. A size limit can be passed in if available to reduce this risk, or
/// the max_len parameter can be -1 if there is no known limit.
pub unsafe fn cstr_to_string(cstr: *const c_char, max_len: isize) -> String {
    if !cstr.is_null() {
        let mut cstr_len: isize = 0;
        while max_len < 0 || cstr_len < max_len {
            if *cstr.offset(cstr_len) == 0 {
                break;
            }
            cstr_len += 1;
        }
        return String::from(std::str::from_utf8(std::slice::from_raw_parts(cstr as *const u8, cstr_len as usize)).unwrap_or(""));
    }
    String::new()
}

/*
#[macro_export(crate)]
macro_rules! enum_str {
    (enum $name:ident {
        $($variant:ident = $val:expr),*,
    }) => {
        enum $name {
            $($variant = $val),*
        }
        impl $name {
            fn name(&self) -> &'static str {
                match self {
                    $($name::$variant => stringify!($variant)),*
                }
            }
        }
    };
}
*/
