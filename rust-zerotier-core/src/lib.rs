/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

use std::os::raw::{c_char, c_int};
use num_derive::{FromPrimitive, ToPrimitive};

mod bindings;

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

use bindings::capi as ztcore;

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

pub const DEFAULT_PORT: u16 = ztcore::ZT_DEFAULT_PORT as u16;

pub const BUF_SIZE: u32 = ztcore::ZT_BUF_SIZE;

pub const MIN_MTU: u32 = ztcore::ZT_MIN_MTU;
pub const MAX_MTU: u32 = ztcore::ZT_MAX_MTU;
pub const DEFAULT_UDP_MTU: u32 = ztcore::ZT_DEFAULT_UDP_MTU;
pub const MAX_UDP_MTU: u32 = ztcore::ZT_MAX_UDP_MTU;

#[allow(non_snake_case,non_upper_case_globals)]
pub mod RulePacketCharacteristicFlags {
    pub const Inbound: u64 = crate::bindings::capi::ZT_RULE_PACKET_CHARACTERISTICS_INBOUND as u64;
    pub const Multicast: u64 = crate::bindings::capi::ZT_RULE_PACKET_CHARACTERISTICS_MULTICAST as u64;
    pub const Broadcast: u64 = crate::bindings::capi::ZT_RULE_PACKET_CHARACTERISTICS_BROADCAST as u64;
    pub const SenderIpAuthenticated: u64 = crate::bindings::capi::ZT_RULE_PACKET_CHARACTERISTICS_SENDER_IP_AUTHENTICATED as u64;
    pub const SenderMacAuthenticated: u64 = crate::bindings::capi::ZT_RULE_PACKET_CHARACTERISTICS_SENDER_MAC_AUTHENTICATED as u64;
    pub const TcpFlagNS: u64 = crate::bindings::capi::ZT_RULE_PACKET_CHARACTERISTICS_TCP_NS as u64;
    pub const TcpFlagCWR: u64 = crate::bindings::capi::ZT_RULE_PACKET_CHARACTERISTICS_TCP_CWR as u64;
    pub const TcpFlagECE: u64 = crate::bindings::capi::ZT_RULE_PACKET_CHARACTERISTICS_TCP_ECE as u64;
    pub const TcpFlagURG: u64 = crate::bindings::capi::ZT_RULE_PACKET_CHARACTERISTICS_TCP_URG as u64;
    pub const TcpFlagACK: u64 = crate::bindings::capi::ZT_RULE_PACKET_CHARACTERISTICS_TCP_ACK as u64;
    pub const TcpFlagPSH: u64 = crate::bindings::capi::ZT_RULE_PACKET_CHARACTERISTICS_TCP_PSH as u64;
    pub const TcpFlagRST: u64 = crate::bindings::capi::ZT_RULE_PACKET_CHARACTERISTICS_TCP_RST as u64;
    pub const TcpFlagSYN: u64 = crate::bindings::capi::ZT_RULE_PACKET_CHARACTERISTICS_TCP_SYN as u64;
    pub const TcpFlagFIN: u64 = crate::bindings::capi::ZT_RULE_PACKET_CHARACTERISTICS_TCP_FIN as u64;
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

#[derive(FromPrimitive, ToPrimitive, PartialEq, Eq)]
pub enum TraceEventType {
    UnexpectedError = ztcore::ZT_TraceEventType_ZT_TRACE_UNEXPECTED_ERROR as isize,
    ResetingPathsInScope = ztcore::ZT_TraceEventType_ZT_TRACE_VL1_RESETTING_PATHS_IN_SCOPE as isize,
    TryingNewPath = ztcore::ZT_TraceEventType_ZT_TRACE_VL1_TRYING_NEW_PATH as isize,
    LearnedNewPath = ztcore::ZT_TraceEventType_ZT_TRACE_VL1_LEARNED_NEW_PATH as isize,
    IncomingPacketDropped = ztcore::ZT_TraceEventType_ZT_TRACE_VL1_INCOMING_PACKET_DROPPED as isize,
    OutgoingFrameDropped = ztcore::ZT_TraceEventType_ZT_TRACE_VL2_OUTGOING_FRAME_DROPPED as isize,
    IncomingFrameDropped = ztcore::ZT_TraceEventType_ZT_TRACE_VL2_INCOMING_FRAME_DROPPED as isize,
    NetworkConfigRequested = ztcore::ZT_TraceEventType_ZT_TRACE_VL2_NETWORK_CONFIG_REQUESTED as isize,
    NetworkFilter = ztcore::ZT_TraceEventType_ZT_TRACE_VL2_NETWORK_FILTER as isize,
}

#[derive(FromPrimitive, ToPrimitive, PartialEq, Eq)]
pub enum TracePacketDropReason {
    Unspecified = ztcore::ZT_TracePacketDropReason_ZT_TRACE_PACKET_DROP_REASON_UNSPECIFIED as isize,
    PeerTooOld = ztcore::ZT_TracePacketDropReason_ZT_TRACE_PACKET_DROP_REASON_PEER_TOO_OLD as isize,
    MalformedPacket = ztcore::ZT_TracePacketDropReason_ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET as isize,
    MacFailed = ztcore::ZT_TracePacketDropReason_ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED as isize,
    RateLimitExceeded = ztcore::ZT_TracePacketDropReason_ZT_TRACE_PACKET_DROP_REASON_RATE_LIMIT_EXCEEDED as isize,
    InvalidObject = ztcore::ZT_TracePacketDropReason_ZT_TRACE_PACKET_DROP_REASON_INVALID_OBJECT as isize,
    InvalidCompressedData = ztcore::ZT_TracePacketDropReason_ZT_TRACE_PACKET_DROP_REASON_INVALID_COMPRESSED_DATA as isize,
    UnrecognizedVerb = ztcore::ZT_TracePacketDropReason_ZT_TRACE_PACKET_DROP_REASON_UNRECOGNIZED_VERB as isize,
    ReplyNotExpected = ztcore::ZT_TracePacketDropReason_ZT_TRACE_PACKET_DROP_REASON_REPLY_NOT_EXPECTED as isize,
}

#[derive(FromPrimitive, ToPrimitive, PartialEq, Eq)]
pub enum TraceFrameDropReason {
    Unspecified = ztcore::ZT_TraceFrameDropReason_ZT_TRACE_FRAME_DROP_REASON_UNSPECIFIED as isize,
    BridgingNotAllowedRemote = ztcore::ZT_TraceFrameDropReason_ZT_TRACE_FRAME_DROP_REASON_BRIDGING_NOT_ALLOWED_REMOTE as isize,
    BridgingNotAllowedLocal = ztcore::ZT_TraceFrameDropReason_ZT_TRACE_FRAME_DROP_REASON_BRIDGING_NOT_ALLOWED_LOCAL as isize,
    MulticastDisabled = ztcore::ZT_TraceFrameDropReason_ZT_TRACE_FRAME_DROP_REASON_MULTICAST_DISABLED as isize,
    BroadcastDisabled = ztcore::ZT_TraceFrameDropReason_ZT_TRACE_FRAME_DROP_REASON_BROADCAST_DISABLED as isize,
    FilterBlocked = ztcore::ZT_TraceFrameDropReason_ZT_TRACE_FRAME_DROP_REASON_FILTER_BLOCKED as isize,
    FilterBlockedAtBridgeReplication = ztcore::ZT_TraceFrameDropReason_ZT_TRACE_FRAME_DROP_REASON_FILTER_BLOCKED_AT_BRIDGE_REPLICATION as isize,
    PermissionDenied = ztcore::ZT_TraceFrameDropReason_ZT_TRACE_FRAME_DROP_REASON_PERMISSION_DENIED as isize,
}

#[derive(FromPrimitive, ToPrimitive, PartialEq, Eq)]
pub enum TraceCredentialRejectionReason {
    SignatureVerificationFailed = ztcore::ZT_TraceCredentialRejectionReason_ZT_TRACE_CREDENTIAL_REJECTION_REASON_SIGNATURE_VERIFICATION_FAILED as isize,
    Revoked = ztcore::ZT_TraceCredentialRejectionReason_ZT_TRACE_CREDENTIAL_REJECTION_REASON_REVOKED as isize,
    OlderThanLatest = ztcore::ZT_TraceCredentialRejectionReason_ZT_TRACE_CREDENTIAL_REJECTION_REASON_OLDER_THAN_LATEST as isize,
    Invalid = ztcore::ZT_TraceCredentialRejectionReason_ZT_TRACE_CREDENTIAL_REJECTION_REASON_INVALID as isize,
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

/// Convenience function to get the number of milliseconds since the Unix epoch.
#[inline(always)]
pub fn now() -> i64 {
    std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).unwrap().as_millis() as i64
}

/// Get a random 64-bit integer using the non-cryptographic PRNG in the ZeroTier core.
#[inline(always)]
pub fn random() -> u64 {
    unsafe {
        return ztcore::ZT_random();
    }
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

/// Macro to implement to_json and new_from_json on types that are Serializable.
#[macro_export(crate)]
macro_rules! implement_to_from_json {
    ($struct_name:ident) => {
        impl $struct_name {
            pub fn new_from_json(json: &str) -> Result<$struct_name, String> {
                let r: serde_json::error::Result<$struct_name> = serde_json::from_str(json);
                if r.is_err() {
                    let e = r.err();
                    if e.is_none() {
                        return Err(String::from("unknown error"));
                    }
                    return Err(e.unwrap().to_string());
                }
                Ok(r.unwrap())
            }

            pub fn to_json(&self) -> String {
                serde_json::to_string_pretty(self).unwrap()
            }
        }
    };
}
