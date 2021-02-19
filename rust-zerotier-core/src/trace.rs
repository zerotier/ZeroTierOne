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

use std::str;

use crate::{capi as ztcore, Dictionary, Endpoint, Fingerprint, IpScope, MAC, Address, CredentialType};
use num_derive::{FromPrimitive, ToPrimitive};
use num_traits::FromPrimitive;
use crate::trace::TraceEvent::TryingNewPath;

/*
// Used to construct String instances from constant strings in C. This assumes
// the string is valid UTF8 and may panic or crash otherwise.
fn string_from_static_array<A: AsRef<[u8]>>(a: A) -> &'static str {
    str::from_utf8(a.as_ref()).unwrap()
}

lazy_static! {
    pub static ref TRACE_FIELD_TYPE: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_TYPE);
    pub static ref TRACE_FIELD_CODE_LOCATION: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_CODE_LOCATION);
    pub static ref TRACE_FIELD_ENDPOINT: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_ENDPOINT);
    pub static ref TRACE_FIELD_OLD_ENDPOINT: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_OLD_ENDPOINT);
    pub static ref TRACE_FIELD_NEW_ENDPOINT: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_NEW_ENDPOINT);
    pub static ref TRACE_FIELD_TRIGGER_FROM_ENDPOINT: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_TRIGGER_FROM_ENDPOINT);
    pub static ref TRACE_FIELD_TRIGGER_FROM_PACKET_ID: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_TRIGGER_FROM_PACKET_ID);
    pub static ref TRACE_FIELD_TRIGGER_FROM_PACKET_VERB: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_TRIGGER_FROM_PACKET_VERB);
    pub static ref TRACE_FIELD_TRIGGER_FROM_PEER_FINGERPRINT_HASH: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_TRIGGER_FROM_PEER_FINGERPRINT_HASH);
    pub static ref TRACE_FIELD_MESSAGE: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_MESSAGE);
    pub static ref TRACE_FIELD_RESET_ADDRESS_SCOPE: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_RESET_ADDRESS_SCOPE);
    pub static ref TRACE_FIELD_IDENTITY_FINGERPRINT_HASH: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_IDENTITY_FINGERPRINT_HASH);
    pub static ref TRACE_FIELD_PACKET_ID: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_PACKET_ID);
    pub static ref TRACE_FIELD_PACKET_VERB: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_PACKET_VERB);
    pub static ref TRACE_FIELD_PACKET_HOPS: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_PACKET_HOPS);
    pub static ref TRACE_FIELD_NETWORK_ID: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_NETWORK_ID);
    pub static ref TRACE_FIELD_REASON: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_REASON);
    pub static ref TRACE_FIELD_SOURCE_MAC: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_SOURCE_MAC);
    pub static ref TRACE_FIELD_DEST_MAC: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_DEST_MAC);
    pub static ref TRACE_FIELD_ETHERTYPE: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_ETHERTYPE);
    pub static ref TRACE_FIELD_VLAN_ID: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_VLAN_ID);
    pub static ref TRACE_FIELD_FRAME_LENGTH: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_FRAME_LENGTH);
    pub static ref TRACE_FIELD_FRAME_DATA: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_FRAME_DATA);
    pub static ref TRACE_FIELD_FLAG_CREDENTIAL_REQUEST_SENT: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_FLAG_CREDENTIAL_REQUEST_SENT);
    pub static ref TRACE_FIELD_PRIMARY_RULE_SET_LOG: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_PRIMARY_RULE_SET_LOG);
    pub static ref TRACE_FIELD_MATCHING_CAPABILITY_RULE_SET_LOG: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_MATCHING_CAPABILITY_RULE_SET_LOG);
    pub static ref TRACE_FIELD_MATCHING_CAPABILITY_ID: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_MATCHING_CAPABILITY_ID);
    pub static ref TRACE_FIELD_MATCHING_CAPABILITY_TIMESTAMP: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_MATCHING_CAPABILITY_TIMESTAMP);
    pub static ref TRACE_FIELD_SOURCE_ZT_ADDRESS: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_SOURCE_ZT_ADDRESS);
    pub static ref TRACE_FIELD_DEST_ZT_ADDRESS: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_DEST_ZT_ADDRESS);
    pub static ref TRACE_FIELD_MATCHING_CAPABILITY_ID: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_MATCHING_CAPABILITY_ID);
    pub static ref TRACE_FIELD_RULE_FLAG_NOTEE: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_RULE_FLAG_NOTEE);
    pub static ref TRACE_FIELD_RULE_FLAG_INBOUND: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_RULE_FLAG_INBOUND);
    pub static ref TRACE_FIELD_RULE_FLAG_ACCEPT: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_RULE_FLAG_ACCEPT);
    pub static ref TRACE_FIELD_CREDENTIAL_ID: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_CREDENTIAL_ID);
    pub static ref TRACE_FIELD_CREDENTIAL_TYPE: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_CREDENTIAL_TYPE);
    pub static ref TRACE_FIELD_CREDENTIAL_TIMESTAMP: &'static str = string_from_static_array(ztcore::ZT_TRACE_FIELD_CREDENTIAL_TIMESTAMP);
}
 */

/*
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

*/

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

#[derive(PartialEq, Eq)]
pub enum TraceAccept {
    Reject,
    Accept,
    SuperAccept,
}

pub enum TraceEvent {
    UnexpectedError {
        code_location: u32,
        message: String,
    },
    ResetingPathsInScope {
        code_location: u32,
        reporter: Option<Fingerprint>,
        reporter_endpoint: Option<Endpoint>,
        my_old_external: Option<Endpoint>,
        my_new_external: Option<Endpoint>,
        scope: IpScope,
    },
    TryingNewPath {
        code_location: u32,
        trying: Fingerprint,
        trigger_peer: Option<Fingerproint>,
        trigger_packet_from: Option<Endpoint>,
        trigger_packet_id: u64,
        trigger_packet_verb: i32,
    },
    LearnedNewPath {
        code_location: u32,
        learned_from_packet_id: u64,
        peer: Fingerprint,
        new_address: Option<Endpoint>,
        replaced_address: Option<Endpoint>,
    },
    IncomingPacketDropped {
        code_location: u32,
        packet_id: u64,
        network_id: u64,
        peer: Option<Fingerprint>,
        peer_address: Option<Endpoint>,
        hops: i32,
        verb: i32,
        reason: TracePacketDropReason,
    },
    OutgoingFrameDropped {
        code_location: u32,
        network_id: u64,
        source_mac: MAC,
        dest_mac: MAC,
        ethertype: u16,
        frame_length: u32,
        frame_data: Vec<u8>,
        reason: TraceFrameDropReason,
    },
    IncomingFrameDropped {
        code_location: u32,
        source_mac: MAC,
        dest_mac: MAC,
        ethertype: u16,
        peer: Fingerprint,
        peer_address: Option<Endpoint>,
        hops: i32,
        verb: i32,
        frame_length: u32,
        frame_data: Vec<u8>,
        credential_request_sent: bool,
        reason: TraceFrameDropReason,
    },
    NetworkConfigRequested {
        code_location: u32,
        network_id: u64,
    },
    NetworkFilter {
        code_location: u32,
        network_id: u64,
        primary_rule_set_log: Vec<u8>,
        matching_capability_rule_set_log: Vec<u8>,
        matching_capability_id: u32,
        matching_capability_timestamp: i64,
        source_address: Address,
        dest_address: Address,
        source_mac: MAC,
        dest_mac: MAC,
        frame_length: u32,
        frame_data: Vec<u8>,
        ethertype: u16,
        vlan_id: u16,
        rule_flag_notee: bool,
        rule_flag_inbound: bool,
        rule_flag_accept: TraceAccept,
    },
    NetworkCredentialRejected {
        code_location: u32,
        network_id: u64,
        from_peer: Fingerprint,
        credential_id: u32,
        credential_timestamp: i64,
        credential_type: CredentialType,
        reason: TraceCredentialRejectionReason,
    },
}

fn trace_optional_endpoint(bytes: Option<&Vec<u8>>) -> Option<Endpoint> {
    bytes.map_or(None, |ep| {
        Endpoint::new_from_bytes(ep.as_slice()).map_or(None, |ep| {
            Some(ep)
        })
    })
}

fn trace_optional_fingerprint(bytes: Option<&Vec<u8>>) -> Option<Fingerprint> {
    bytes.map_or(None, |fp| {
        Fingerprint::new_from_bytes(fp).map_or(None, |fp| {
            Some(fp)
        })
    })
}

impl TraceEvent {
    pub fn parse_message(msg: &Dictionary) -> Option<TraceEvent> {
        msg.get_ui(ztcore::ZT_TRACE_FIELD_TYPE).map_or(None, |mt: u64| -> Option<TraceEvent> {
            let cl = msg.get_ui(ztcore::ZT_TRACE_FIELD_CODE_LOCATION).unwrap_or(0) as u32;
            match mt as u32 {
                ztcore::ZT_TraceEventType_ZT_TRACE_UNEXPECTED_ERROR => {
                    Some(TraceEvent::UnexpectedError {
                        code_location: cl,
                        message: msg.get_string_or_empty(ztcore::ZT_TRACE_FIELD_MESSAGE),
                    })
                },
                ztcore::ZT_TraceEventType_ZT_TRACE_VL1_RESETTING_PATHS_IN_SCOPE => {
                    Some(TraceEvent::ResetingPathsInScope {
                        code_location: cl,
                        reporter: trace_optional_fingerprint(msg.get(ztcore::ZT_TRACE_FIELD_IDENTITY_FINGERPRINT)),
                        reporter_endpoint: trace_optional_endpoint(msg.get(ztcore::ZT_TRACE_FIELD_TRIGGER_FROM_ENDPOINT)),
                        my_old_external: trace_optional_endpoint(msg.get(ztcore::ZT_TRACE_FIELD_OLD_ENDPOINT)),
                        my_new_external: trace_optional_endpoint(msg.get(ztcore::ZT_TRACE_FIELD_NEW_ENDPOINT)),
                        scope: IpScope::from_i32(msg.get_ui(ztcore::ZT_TRACE_FIELD_RESET_ADDRESS_SCOPE).unwrap_or(0) as i32).unwrap_or(IpScope::None),
                    })
                },
                ztcore::ZT_TraceEventType_ZT_TRACE_VL1_TRYING_NEW_PATH => {
                    let tf = msg.get(ztcore::ZT_TRACE_FIELD_IDENTITY_FINGERPRINT);
                    if tf.is_some() {
                        let tf = Fingerprint::new_from_bytes(tf.unwrap().as_slice()).ok();
                        if tf.is_some() {
                            return Some(TraceEvent::TryingNewPath {
                                code_location: cl,
                                trying: tf.unwrap(),
                                trigger_peer: trace_optional_fingerprint(msg.get(ztcore::ZT_TRACE_FIELD_TRIGGER_FROM_PEER_FINGERPRINT)),
                                trigger_packet_from: trace_optional_endpoint(msg.get(ztcore::ZT_TRACE_FIELD_TRIGGER_FROM_ENDPOINT)),
                                trigger_packet_id: msg.get_ui(ztcore::ZT_TRACE_FIELD_TRIGGER_FROM_PACKET_ID).unwrap_or(0),
                                trigger_packet_verb: msg.get_ui(ztcore::ZT_TRACE_FIELD_TRIGGER_FROM_PACKET_VERB).unwrap_or(0) as i32,
                            });
                        }
                    }
                    None
                },
                ztcore::ZT_TraceEventType_ZT_TRACE_VL1_LEARNED_NEW_PATH => {
                    let fp = msg.get(ztcore::ZT_TRACE_FIELD_IDENTITY_FINGERPRINT);
                    if fp.is_some() {
                        let fp = Fingerprint::new_from_bytes(fp.unwrap().as_slice()).ok();
                        if fp.is_some() {
                            return Some(TraceEvent::LearnedNewPath {
                                code_location: cl,
                                learned_from_packet_id: msg.get_ui(ztcore::ZT_TRACE_FIELD_PACKET_ID).unwrap_or(0),
                                peer: fp.unwrap(),
                                new_address: trace_optional_endpoint(msg.get(ztcore::ZT_TRACE_FIELD_ENDPOINT)),
                                replaced_address: trace_optional_endpoint(msg.get(ztcore::ZT_TRACE_FIELD_OLD_ENDPOINT)),
                            });
                        }
                    }
                    None
                },
                ztcore::ZT_TraceEventType_ZT_TRACE_VL1_INCOMING_PACKET_DROPPED => {
                    Some(TraceEvent::IncomingPacketDropped {
                        code_location: cl,
                        packet_id: msg.get_ui(ztcore::ZT_TRACE_FIELD_PACKET_ID).unwrap_or(0),
                        network_id: msg.get_ui(ztcore::ZT_TRACE_FIELD_NETWORK_ID).unwrap_or(0),
                        peer: trace_optional_fingerprint(msg.get(ztcore::ZT_TRACE_FIELD_IDENTITY_FINGERPRINT)),
                        peer_address: trace_optional_endpoint(msg.get(ztcore::ZT_TRACE_FIELD_ENDPOINT)),
                        hops: msg.get_ui(ztcore::ZT_TRACE_FIELD_PACKET_HOPS).unwrap_or(0) as i32,
                        verb: msg.get_ui(ztcore::ZT_TRACE_FIELD_PACKET_VERB).unwrap_or(0) as i32,
                        reason: TracePacketDropReason.from_i32(msg.get_ui(ztcore::ZT_TRACE_FIELD_REASON).unwrap_or(0) as i32).unwrap_or(TracePacketDropReason::Unspecified),
                    })
                },
                ztcore::ZT_TraceEventType_ZT_TRACE_VL2_OUTGOING_FRAME_DROPPED => {
                    Some(TraceEvent::OutgoingFrameDropped {
                        code_location: ci,
                        network_id: msg.get_ui(ztcore::ZT_TRACE_FIELD_NETWORK_ID).unwrap_or(0),
                        source_mac: MAC(msg.get_ui(ztcore::ZT_TRACE_FIELD_SOURCE_MAC).unwrap_or(0)),
                        dest_mac: MAC(msg.get_ui(ztcore::ZT_TRACE_FIELD_DEST_MAC).unwrap_or(0)),
                        ethertype: msg.get_ui(ztcore::ZT_TRACE_FIELD_ETHERTYPE).unwrap_or(0) as u16,
                        frame_length: msg.get_ui(ztcore::ZT_TRACE_FIELD_FRAME_LENGTH).unwrap_or(0) as u32,
                        frame_data: msg.get(ztcore::ZT_TRACE_FIELD_FRAME_DATA).map_or_else(|| -> Vec<u8> {
                            Vec::new()
                        },|d: &Vec<u8>| -> Vec<u8> {
                            d.clone()
                        }),
                        reason: TraceFrameDropReason::from_i32(msg.get_ui(ztcore::ZT_TRACE_FIELD_REASON).unwrap_or(0) as i32).unwrap_or(TraceFrameDropReason::Unspecified),
                    })
                },
                ztcore::ZT_TraceEventType_ZT_TRACE_VL2_INCOMING_FRAME_DROPPED => {
                    let fp = msg.get(ztcore::ZT_TRACE_FIELD_IDENTITY_FINGERPRINT);
                    if fp.is_some() {
                        let fp = Fingerprint::new_from_bytes(fp.unwrap().as_slice()).ok();
                        if fp.is_some() {
                            return Some(TraceEvent::IncomingFrameDropped {
                                code_location: cl,
                                source_mac: MAC(msg.get_ui(ztcore::ZT_TRACE_FIELD_SOURCE_MAC).unwrap_or(0)),
                                dest_mac: MAC(msg.get_ui(ztcore::ZT_TRACE_FIELD_DEST_MAC).unwrap_or(0)),
                                ethertype: msg.get_ui(ztcore::ZT_TRACE_FIELD_ETHERTYPE).unwrap_or(0) as u16,
                                peer: fp.unwrap(),
                                peer_address: trace_optional_endpoint(msg.get(ztcore::ZT_TRACE_FIELD_ENDPOINT)),
                                hops: msg.get_ui(ztcore::ZT_TRACE_FIELD_PACKET_HOPS).unwrap_or(0) as i32,
                                verb: msg.get_ui(ztcore::ZT_TRACE_FIELD_PACKET_VERB).unwrap_or(0) as i32,
                                frame_length: msg.get_ui(ztcore::ZT_TRACE_FIELD_FRAME_LENGTH).unwrap_or(0) as u32,
                                frame_data: msg.get(ztcore::ZT_TRACE_FIELD_FRAME_DATA).map_or_else(|| -> Vec<u8> {
                                    Vec::new()
                                },|d: &Vec<u8>| -> Vec<u8> {
                                    d.clone()
                                }),
                                credential_request_sent: msg.get_ui(ztcore::ZT_TRACE_FIELD_FLAG_CREDENTIAL_REQUEST_SENT).unwrap_or(0) != 0,
                                reason: TraceFrameDropReason::from_i32(msg.get_ui(ztcore::ZT_TRACE_FIELD_REASON).unwrap_or(0) as i32).unwrap_or(TraceFrameDropReason::Unspecified),
                            })
                        }
                    }
                    None
                },
                ztcore::ZT_TraceEventType_ZT_TRACE_VL2_NETWORK_CONFIG_REQUESTED => {
                    Some(TraceEvent::NetworkConfigRequested {
                        code_location: cl,
                        network_id: msg.get_ui(ztcore::ZT_TRACE_FIELD_NETWORK_ID).unwrap_or(0),
                    })
                },
                ztcore::ZT_TraceEventType_ZT_TRACE_VL2_NETWORK_FILTER => {
                    Some(TraceEvent::NetworkFilter {
                        code_location: cl,
                        network_id: msg.get_ui(ztcore::ZT_TRACE_FIELD_NETWORK_ID).unwrap_or(0),
                        primary_rule_set_log: msg.get(ztcore::ZT_TRACE_FIELD_PRIMARY_RULE_SET_LOG).map_or_else(|| {
                            Vec::new()
                        },|l| {
                            l.clone()
                        }),
                        matching_capability_rule_set_log: msg.get(ztcore::ZT_TRACE_FIELD_MATCHING_CAPABILITY_RULE_SET_LOG).map_or_else(|| {
                            Vec::new()
                        },|l| {
                            l.clone()
                        }),
                        matching_capability_id: msg.get_ui(ztcore::ZT_TRACE_FIELD_MATCHING_CAPABILITY_ID).unwrap_or(0) as u32,
                        matching_capability_timestamp: msg.get_ui(ztcore::ZT_TRACE_FIELD_MATCHING_CAPABILITY_TIMESTAMP).unwrap_or(0) as i64,
                        source_address: Address(msg.get_ui(ztcore::ZT_TRACE_FIELD_SOURCE_ZT_ADDRESS).unwrap_or(0)),
                        dest_address: Address(msg.get_ui(ztcore::ZT_TRACE_FIELD_DEST_ZT_ADDRESS).unwrap_or(0)),
                        source_mac: MAC(msg.get_ui(ztcore::ZT_TRACE_FIELD_SOURCE_MAC).unwrap_or(0)),
                        dest_mac: MAC(msg.get_ui(ztcore::ZT_TRACE_FIELD_DEST_MAC).unwrap_or(0)),
                        frame_length: msg.get_ui(ztcore::ZT_TRACE_FIELD_FRAME_LENGTH).unwrap_or(0) as u32,
                        frame_data: msg.get(ztcore::ZT_TRACE_FIELD_FRAME_DATA).map_or_else(|| -> Vec<u8> {
                            Vec::new()
                        },|d: &Vec<u8>| -> Vec<u8> {
                            d.clone()
                        }),
                        ethertype: msg.get_ui(ztcore::ZT_TRACE_FIELD_ETHERTYPE).unwrap_or(0) as u16,
                        vlan_id: msg.get_ui(ztcore::ZT_TRACE_FIELD_VLAN_ID).unwrap_or(0) as u16,
                        rule_flag_notee: msg.get_ui(ztcore::ZT_TRACE_FIELD_RULE_FLAG_NOTEE).unwrap_or(0) != 0,
                        rule_flag_inbound: msg.get_ui(ztcore::ZT_TRACE_FIELD_RULE_FLAG_INBOUND).unwrap_or(0) != 0,
                        rule_flag_accept: match msg.get(ztcore::ZT_TRACE_FIELD_RULE_FLAG_ACCEPT).map_or_else(|| -> i32 { 0 as i32 },|a| -> i32 { i32::from_str_radix(str::from_utf8(a).unwrap_or("0"), 16).unwrap_or(0) }) {
                            1 => { TraceAccept::Accept },
                            2 => { TraceAccept::SuperAccept },
                            _ => { TraceAccept::Reject },
                        }
                    })
                },
                ztcore::ZT_TraceEventType_ZT_TRACE_VL2_NETWORK_CREDENTIAL_REJECTED => {
                    let fp = msg.get(ztcore::ZT_TRACE_FIELD_IDENTITY_FINGERPRINT);
                    if fp.is_some() {
                        let fp = Fingerprint::new_from_bytes(fp.unwrap().as_slice()).ok();
                        if fp.is_some() {
                            return Some(TraceEvent::NetworkCredentialRejected {
                                code_location: cl,
                                network_id: msg.get_ui(ztcore::ZT_TRACE_FIELD_NETWORK_ID).unwrap_or(0),
                                from_peer: fp.unwrap(),
                                credential_id: msg.get_ui(ztcore::ZT_TRACE_FIELD_CREDENTIAL_ID).unwrap_or(0) as u32,
                                credential_timestamp: msg.get_ui(ztcore::ZT_TRACE_FIELD_CREDENTIAL_TIMESTAMP).unwrap_or(0) as i64,
                                credential_type: CredentialType::from_i32(msg.get_ui(ztcore::ZT_TRACE_FIELD_CREDENTIAL_TYPE).unwrap_or(0) as i32).unwrap_or(CredentialType::Null),
                                reason: TraceCredentialRejectionReason::from_i32(msg.get_ui(ztcore::ZT_TRACE_FIELD_REASON).unwrap_or(0) as i32).unwrap_or(TraceCredentialRejectionReason::Invalid),
                            });
                        }
                    }
                    None
                },
                _ => None,
            }
        })
    }
}
