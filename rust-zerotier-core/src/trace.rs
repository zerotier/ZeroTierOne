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

#[derive(FromPrimitive, PartialEq, Eq)]
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

impl TracePacketDropReason {
    pub fn to_str(&self) -> &'static str {
        match *self {
            TracePacketDropReason::Unspecified => "Unspecified",
            TracePacketDropReason::PeerTooOld => "PeerTooOld",
            TracePacketDropReason::MalformedPacket => "MalformedPacket",
            TracePacketDropReason::MacFailed => "MacFailed",
            TracePacketDropReason::RateLimitExceeded => "RateLimitExceeded",
            TracePacketDropReason::InvalidObject => "InvalidObject",
            TracePacketDropReason::InvalidCompressedData => "InvalidCompressedData",
            TracePacketDropReason::UnrecognizedVerb => "UnrecognizedVerb",
            TracePacketDropReason::ReplyNotExpected => "ReplyNotExpected",
        }
    }
}

#[derive(FromPrimitive, PartialEq, Eq)]
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

impl TraceFrameDropReason {
    pub fn to_str(&self) -> &'static str {
        match *self {
            TraceFrameDropReason::Unspecified => "Unspecified",
            TraceFrameDropReason::BridgingNotAllowedRemote => "BridgingNotAllowedRemote",
            TraceFrameDropReason::BridgingNotAllowedLocal => "BridgingNotAllowedLocal",
            TraceFrameDropReason::MulticastDisabled => "MulticastDisabled",
            TraceFrameDropReason::BroadcastDisabled => "BroadcastDisabled",
            TraceFrameDropReason::FilterBlocked => "FilterBlocked",
            TraceFrameDropReason::FilterBlockedAtBridgeReplication => "FilterBlockedAtBridgeReplication",
            TraceFrameDropReason::PermissionDenied => "PermissionDenied",
        }
    }
}

#[derive(FromPrimitive, PartialEq, Eq)]
pub enum TraceCredentialRejectionReason {
    SignatureVerificationFailed = ztcore::ZT_TraceCredentialRejectionReason_ZT_TRACE_CREDENTIAL_REJECTION_REASON_SIGNATURE_VERIFICATION_FAILED as isize,
    Revoked = ztcore::ZT_TraceCredentialRejectionReason_ZT_TRACE_CREDENTIAL_REJECTION_REASON_REVOKED as isize,
    OlderThanLatest = ztcore::ZT_TraceCredentialRejectionReason_ZT_TRACE_CREDENTIAL_REJECTION_REASON_OLDER_THAN_LATEST as isize,
    Invalid = ztcore::ZT_TraceCredentialRejectionReason_ZT_TRACE_CREDENTIAL_REJECTION_REASON_INVALID as isize,
}

impl TraceCredentialRejectionReason {
    pub fn to_str(&self) -> &'static str {
        match *self {
            TraceCredentialRejectionReason::SignatureVerificationFailed => "SignatureVerificationFailed",
            TraceCredentialRejectionReason::Revoked => "Revoked",
            TraceCredentialRejectionReason::OlderThanLatest => "OlderThanLatest",
            TraceCredentialRejectionReason::Invalid => "Invalid",
        }
    }
}

#[derive(PartialEq, Eq)]
pub enum TraceFilterResult {
    Reject,
    Accept,
    SuperAccept,
}

impl TraceFilterResult {
    pub fn to_str(&self) -> &'static str {
        match *self {
            TraceFilterResult::Reject => "Reject",
            TraceFilterResult::Accept => "Accept",
            TraceFilterResult::SuperAccept => "SuperAccept",
        }
    }
}

#[derive(PartialEq, Eq)]
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
        trying_peer: Fingerprint,
        trying_endpoint: Endpoint,
        trigger_peer: Option<Fingerprint>,
        trigger_packet_from: Option<Endpoint>,
        trigger_packet_id: u64,
        trigger_packet_verb: i32,
    },
    LearnedNewPath {
        code_location: u32,
        learned_from_packet_id: u64,
        peer: Fingerprint,
        new_endpoint: Option<Endpoint>,
        old_endpoint: Option<Endpoint>,
    },
    IncomingPacketDropped {
        code_location: u32,
        packet_id: u64,
        network_id: u64,
        peer: Option<Fingerprint>,
        peer_endpoint: Option<Endpoint>,
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
        network_id: u64,
        source_mac: MAC,
        dest_mac: MAC,
        ethertype: u16,
        peer: Fingerprint,
        peer_endpoint: Option<Endpoint>,
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
        flag_notee: bool,
        inbound: bool,
        result: TraceFilterResult,
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

fn trace_to_string_optional<T: ToString>(x: &Option<T>) -> String {
    x.as_ref().map_or_else(|| { "<unknown>".to_string() }, |xx| { xx.to_string() })
}

fn trace_peer_address_to_string_optional(p: &Option<Fingerprint>) -> String {
    p.as_ref().map_or_else(|| { "<unknown>".to_string() }, |pp| { pp.address.to_string() })
}

impl ToString for TraceEvent {
    fn to_string(&self) -> String {
        match self {
            TraceEvent::UnexpectedError { code_location, message } => {
                format!("UnexpectedError: {} ({:0>8x})", message, code_location)
            }
            TraceEvent::ResetingPathsInScope { code_location, reporter, reporter_endpoint, my_old_external, my_new_external, scope } => {
                format!(
                    "VL1 ResettingPathsInScope: resetting scope {} because {}@{} reported that my address changed from {} to {} ({:0>8x})",
                    scope.to_str(),
                    trace_peer_address_to_string_optional(reporter),
                    trace_to_string_optional(reporter_endpoint),
                    trace_to_string_optional(my_old_external),
                    trace_to_string_optional(my_new_external),
                    code_location,
                )
            }
            TraceEvent::TryingNewPath { code_location, trying_peer, trying_endpoint, trigger_peer, trigger_packet_from, trigger_packet_id, .. } => {
                format!(
                    "VL1 TryingNewPath: trying {}@{} triggered by packet {:0>16x} from {}@{} ({:0>8x})",
                    trying_peer.address.to_string(),
                    trying_endpoint.to_string(),
                    trigger_packet_id,
                    trace_peer_address_to_string_optional(trigger_peer),
                    trace_to_string_optional(trigger_packet_from),
                    code_location,
                )
            }
            TraceEvent::LearnedNewPath { code_location, learned_from_packet_id, peer, old_endpoint, new_endpoint } => {
                format!(
                    "VL1 LearnedNewPath: {} is now at {}, was at {}, learned from packet {:0>16x} ({:0>8x})",
                    peer.address.to_string(),
                    trace_to_string_optional(new_endpoint),
                    trace_to_string_optional(old_endpoint),
                    learned_from_packet_id,
                    code_location,
                )
            }
            TraceEvent::IncomingPacketDropped { code_location, packet_id, peer, peer_endpoint, hops, verb, reason, .. } => {
                format!(
                    "VL1 IncomingPacketDropped: packet {:0>16x} from {}@{} (hops: {}, verb: {}) dropped: {} ({:0>8x})",
                    packet_id,
                    trace_peer_address_to_string_optional(peer),
                    trace_to_string_optional(peer_endpoint),
                    hops,
                    verb,
                    reason.to_str(),
                    code_location,
                )
            }
            TraceEvent::OutgoingFrameDropped { code_location, network_id, source_mac, dest_mac, ethertype, frame_length, reason, .. } => {
                format!(
                    "VL2 OutgoingFrameDropped: network {:0>16x} {} -> {} ethertype {:0>4x} length {} dropped: {} ({:0>8x})",
                    network_id,
                    source_mac.to_string(),
                    dest_mac.to_string(),
                    ethertype,
                    frame_length,
                    reason.to_str(),
                    code_location,
                )
            }
            TraceEvent::IncomingFrameDropped { code_location, network_id, source_mac, dest_mac, ethertype, peer, peer_endpoint, frame_length, reason, .. } => {
                format!(
                    "VL2 IncomingFrameDropped: network {:0>16x} {} -> {} ethertype {:0>4x} length {} from {}@{} dropped: {} ({:0>8x})",
                    network_id,
                    source_mac.to_string(),
                    dest_mac.to_string(),
                    ethertype,
                    frame_length,
                    peer.address.to_string(),
                    trace_to_string_optional(peer_endpoint),
                    reason.to_str(),
                    code_location,
                )
            }
            TraceEvent::NetworkConfigRequested { code_location, network_id } => {
                format!(
                    "VL2 NetworkConfigRequested: {:0>16x} ({:0>8x})", network_id, code_location)
            }
            TraceEvent::NetworkFilter { code_location, network_id, source_address, dest_address, source_mac, dest_mac, frame_length, ethertype, inbound, result, .. } => {
                format!(
                    "VL2 NetworkFilter: network {:0>16x} {}: {} via {} -> {} via {} length {} ethertype {:0>4x} result {} ({:0>8x})",
                    network_id,
                    if *inbound { "IN" } else { "OUT" },
                    source_mac.to_string(),
                    source_address.to_string(),
                    dest_mac.to_string(),
                    dest_address.to_string(),
                    frame_length,
                    ethertype,
                    result.to_str(),
                    code_location,
                )
            }
            TraceEvent::NetworkCredentialRejected { code_location, network_id, from_peer, credential_id, credential_timestamp, credential_type, reason } => {
                format!(
                    "VL2 NetworkCredentialRejected: network {:0>16x} from {} id {:0>8x} timestamp {} type {}: {} ({:0>8x})",
                    network_id,
                    from_peer.address.to_string(),
                    credential_id,
                    credential_timestamp,
                    credential_type.to_str(),
                    reason.to_str(),
                    code_location,
                )
            }
        }
    }
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
    /// Decode a trace event packaged in a dictionary and return a TraceEvent if it is valid.
    pub fn parse_message(msg: &Dictionary) -> Option<TraceEvent> {
        msg.get_ui(ztcore::ZT_TRACE_FIELD_TYPE).map_or(None, |mt: u64| -> Option<TraceEvent> {
            let cl = msg.get_ui(ztcore::ZT_TRACE_FIELD_CODE_LOCATION).unwrap_or(0) as u32;
            match mt as u32 {
                _ => { // ztcore::ZT_TraceEventType_ZT_TRACE_UNEXPECTED_ERROR
                    Some(TraceEvent::UnexpectedError {
                        code_location: cl,
                        message: msg.get_str(ztcore::ZT_TRACE_FIELD_MESSAGE).map_or_else(|| {
                            format!("WARNING: unknown trace message type {}, this version may be too old!", mt)
                        }, |m| {
                            m.to_string()
                        }),
                    })
                }
                ztcore::ZT_TraceEventType_ZT_TRACE_VL1_RESETTING_PATHS_IN_SCOPE => {
                    Some(TraceEvent::ResetingPathsInScope {
                        code_location: cl,
                        reporter: trace_optional_fingerprint(msg.get(ztcore::ZT_TRACE_FIELD_IDENTITY_FINGERPRINT)),
                        reporter_endpoint: trace_optional_endpoint(msg.get(ztcore::ZT_TRACE_FIELD_TRIGGER_FROM_ENDPOINT)),
                        my_old_external: trace_optional_endpoint(msg.get(ztcore::ZT_TRACE_FIELD_OLD_ENDPOINT)),
                        my_new_external: trace_optional_endpoint(msg.get(ztcore::ZT_TRACE_FIELD_NEW_ENDPOINT)),
                        scope: IpScope::from_i32(msg.get_ui(ztcore::ZT_TRACE_FIELD_RESET_ADDRESS_SCOPE).unwrap_or(0) as i32).unwrap_or(IpScope::None),
                    })
                }
                ztcore::ZT_TraceEventType_ZT_TRACE_VL1_TRYING_NEW_PATH => {
                    let tf = msg.get(ztcore::ZT_TRACE_FIELD_IDENTITY_FINGERPRINT);
                    let ep = msg.get(ztcore::ZT_TRACE_FIELD_ENDPOINT);
                    if tf.is_some() && ep.is_some() {
                        let tf = Fingerprint::new_from_bytes(tf.unwrap().as_slice()).ok();
                        let ep = Endpoint::new_from_bytes(ep.unwrap().as_slice()).ok();
                        if tf.is_some() && ep.is_some() {
                            return Some(TraceEvent::TryingNewPath {
                                code_location: cl,
                                trying_peer: tf.unwrap(),
                                trying_endpoint: ep.unwrap(),
                                trigger_peer: trace_optional_fingerprint(msg.get(ztcore::ZT_TRACE_FIELD_TRIGGER_FROM_PEER_FINGERPRINT)),
                                trigger_packet_from: trace_optional_endpoint(msg.get(ztcore::ZT_TRACE_FIELD_TRIGGER_FROM_ENDPOINT)),
                                trigger_packet_id: msg.get_ui(ztcore::ZT_TRACE_FIELD_TRIGGER_FROM_PACKET_ID).unwrap_or(0),
                                trigger_packet_verb: msg.get_ui(ztcore::ZT_TRACE_FIELD_TRIGGER_FROM_PACKET_VERB).unwrap_or(0) as i32,
                            });
                        }
                    }
                    None
                }
                ztcore::ZT_TraceEventType_ZT_TRACE_VL1_LEARNED_NEW_PATH => {
                    let fp = msg.get(ztcore::ZT_TRACE_FIELD_IDENTITY_FINGERPRINT);
                    if fp.is_some() {
                        let fp = Fingerprint::new_from_bytes(fp.unwrap().as_slice()).ok();
                        if fp.is_some() {
                            return Some(TraceEvent::LearnedNewPath {
                                code_location: cl,
                                learned_from_packet_id: msg.get_ui(ztcore::ZT_TRACE_FIELD_PACKET_ID).unwrap_or(0),
                                peer: fp.unwrap(),
                                new_endpoint: trace_optional_endpoint(msg.get(ztcore::ZT_TRACE_FIELD_ENDPOINT)),
                                old_endpoint: trace_optional_endpoint(msg.get(ztcore::ZT_TRACE_FIELD_OLD_ENDPOINT)),
                            });
                        }
                    }
                    None
                }
                ztcore::ZT_TraceEventType_ZT_TRACE_VL1_INCOMING_PACKET_DROPPED => {
                    Some(TraceEvent::IncomingPacketDropped {
                        code_location: cl,
                        packet_id: msg.get_ui(ztcore::ZT_TRACE_FIELD_PACKET_ID).unwrap_or(0),
                        network_id: msg.get_ui(ztcore::ZT_TRACE_FIELD_NETWORK_ID).unwrap_or(0),
                        peer: trace_optional_fingerprint(msg.get(ztcore::ZT_TRACE_FIELD_IDENTITY_FINGERPRINT)),
                        peer_endpoint: trace_optional_endpoint(msg.get(ztcore::ZT_TRACE_FIELD_ENDPOINT)),
                        hops: msg.get_ui(ztcore::ZT_TRACE_FIELD_PACKET_HOPS).unwrap_or(0) as i32,
                        verb: msg.get_ui(ztcore::ZT_TRACE_FIELD_PACKET_VERB).unwrap_or(0) as i32,
                        reason: TracePacketDropReason::from_i32(msg.get_ui(ztcore::ZT_TRACE_FIELD_REASON).unwrap_or(0) as i32).unwrap_or(TracePacketDropReason::Unspecified),
                    })
                }
                ztcore::ZT_TraceEventType_ZT_TRACE_VL2_OUTGOING_FRAME_DROPPED => {
                    Some(TraceEvent::OutgoingFrameDropped {
                        code_location: cl,
                        network_id: msg.get_ui(ztcore::ZT_TRACE_FIELD_NETWORK_ID).unwrap_or(0),
                        source_mac: MAC(msg.get_ui(ztcore::ZT_TRACE_FIELD_SOURCE_MAC).unwrap_or(0)),
                        dest_mac: MAC(msg.get_ui(ztcore::ZT_TRACE_FIELD_DEST_MAC).unwrap_or(0)),
                        ethertype: msg.get_ui(ztcore::ZT_TRACE_FIELD_ETHERTYPE).unwrap_or(0) as u16,
                        frame_length: msg.get_ui(ztcore::ZT_TRACE_FIELD_FRAME_LENGTH).unwrap_or(0) as u32,
                        frame_data: msg.get(ztcore::ZT_TRACE_FIELD_FRAME_DATA).map_or_else(|| -> Vec<u8> {
                            Vec::new()
                        }, |d: &Vec<u8>| -> Vec<u8> {
                            d.clone()
                        }),
                        reason: TraceFrameDropReason::from_i32(msg.get_ui(ztcore::ZT_TRACE_FIELD_REASON).unwrap_or(0) as i32).unwrap_or(TraceFrameDropReason::Unspecified),
                    })
                }
                ztcore::ZT_TraceEventType_ZT_TRACE_VL2_INCOMING_FRAME_DROPPED => {
                    let fp = msg.get(ztcore::ZT_TRACE_FIELD_IDENTITY_FINGERPRINT);
                    if fp.is_some() {
                        let fp = Fingerprint::new_from_bytes(fp.unwrap().as_slice()).ok();
                        if fp.is_some() {
                            return Some(TraceEvent::IncomingFrameDropped {
                                code_location: cl,
                                network_id: msg.get_ui(ztcore::ZT_TRACE_FIELD_NETWORK_ID).unwrap_or(0),
                                source_mac: MAC(msg.get_ui(ztcore::ZT_TRACE_FIELD_SOURCE_MAC).unwrap_or(0)),
                                dest_mac: MAC(msg.get_ui(ztcore::ZT_TRACE_FIELD_DEST_MAC).unwrap_or(0)),
                                ethertype: msg.get_ui(ztcore::ZT_TRACE_FIELD_ETHERTYPE).unwrap_or(0) as u16,
                                peer: fp.unwrap(),
                                peer_endpoint: trace_optional_endpoint(msg.get(ztcore::ZT_TRACE_FIELD_ENDPOINT)),
                                hops: msg.get_ui(ztcore::ZT_TRACE_FIELD_PACKET_HOPS).unwrap_or(0) as i32,
                                verb: msg.get_ui(ztcore::ZT_TRACE_FIELD_PACKET_VERB).unwrap_or(0) as i32,
                                frame_length: msg.get_ui(ztcore::ZT_TRACE_FIELD_FRAME_LENGTH).unwrap_or(0) as u32,
                                frame_data: msg.get_or_empty(ztcore::ZT_TRACE_FIELD_FRAME_DATA),
                                credential_request_sent: msg.get_ui(ztcore::ZT_TRACE_FIELD_FLAG_CREDENTIAL_REQUEST_SENT).unwrap_or(0) != 0,
                                reason: TraceFrameDropReason::from_i32(msg.get_ui(ztcore::ZT_TRACE_FIELD_REASON).unwrap_or(0) as i32).unwrap_or(TraceFrameDropReason::Unspecified),
                            });
                        }
                    }
                    None
                }
                ztcore::ZT_TraceEventType_ZT_TRACE_VL2_NETWORK_CONFIG_REQUESTED => {
                    Some(TraceEvent::NetworkConfigRequested {
                        code_location: cl,
                        network_id: msg.get_ui(ztcore::ZT_TRACE_FIELD_NETWORK_ID).unwrap_or(0),
                    })
                }
                ztcore::ZT_TraceEventType_ZT_TRACE_VL2_NETWORK_FILTER => {
                    let verdict_int = msg.get(ztcore::ZT_TRACE_FIELD_RULE_FLAG_ACCEPT).map_or_else(|| -> i32 { 0 as i32 }, |a| -> i32 { i32::from_str_radix(str::from_utf8(a).unwrap_or("0"), 16).unwrap_or(0) });
                    let mut verdict = TraceFilterResult::Reject;
                    if verdict_int == 1 {
                        verdict = TraceFilterResult::Accept;
                    } else if verdict_int > 1 {
                        verdict = TraceFilterResult::SuperAccept;
                    }
                    Some(TraceEvent::NetworkFilter {
                        code_location: cl,
                        network_id: msg.get_ui(ztcore::ZT_TRACE_FIELD_NETWORK_ID).unwrap_or(0),
                        primary_rule_set_log: msg.get_or_empty(ztcore::ZT_TRACE_FIELD_PRIMARY_RULE_SET_LOG),
                        matching_capability_rule_set_log: msg.get_or_empty(ztcore::ZT_TRACE_FIELD_MATCHING_CAPABILITY_RULE_SET_LOG),
                        matching_capability_id: msg.get_ui(ztcore::ZT_TRACE_FIELD_MATCHING_CAPABILITY_ID).unwrap_or(0) as u32,
                        matching_capability_timestamp: msg.get_ui(ztcore::ZT_TRACE_FIELD_MATCHING_CAPABILITY_TIMESTAMP).unwrap_or(0) as i64,
                        source_address: Address(msg.get_ui(ztcore::ZT_TRACE_FIELD_SOURCE_ZT_ADDRESS).unwrap_or(0)),
                        dest_address: Address(msg.get_ui(ztcore::ZT_TRACE_FIELD_DEST_ZT_ADDRESS).unwrap_or(0)),
                        source_mac: MAC(msg.get_ui(ztcore::ZT_TRACE_FIELD_SOURCE_MAC).unwrap_or(0)),
                        dest_mac: MAC(msg.get_ui(ztcore::ZT_TRACE_FIELD_DEST_MAC).unwrap_or(0)),
                        frame_length: msg.get_ui(ztcore::ZT_TRACE_FIELD_FRAME_LENGTH).unwrap_or(0) as u32,
                        frame_data: msg.get_or_empty(ztcore::ZT_TRACE_FIELD_FRAME_DATA),
                        ethertype: msg.get_ui(ztcore::ZT_TRACE_FIELD_ETHERTYPE).unwrap_or(0) as u16,
                        vlan_id: msg.get_ui(ztcore::ZT_TRACE_FIELD_VLAN_ID).unwrap_or(0) as u16,
                        flag_notee: msg.get_ui(ztcore::ZT_TRACE_FIELD_RULE_FLAG_NOTEE).unwrap_or(0) != 0,
                        inbound: msg.get_ui(ztcore::ZT_TRACE_FIELD_RULE_FLAG_INBOUND).unwrap_or(0) != 0,
                        result: verdict,
                    })
                }
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
                }
            }
        })
    }
}
