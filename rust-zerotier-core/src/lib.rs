mod bindings;
mod identity;
mod address;
mod fingerprint;
mod endpoint;
mod certificate;
mod networkid;
mod locator;

pub use identity::Identity;
pub use address::*;
pub use fingerprint::Fingerprint;
pub use endpoint::Endpoint;
pub use networkid::*;
pub use locator::Locator;

use bindings::capi as ztcore;
use num_derive::FromPrimitive;
use num_derive::ToPrimitive;

pub const DEFAULT_PORT: u16 = ztcore::ZT_DEFAULT_PORT as u16;

pub const BUF_SIZE: u32 = ztcore::ZT_BUF_SIZE;

pub const MIN_MTU: u32 = ztcore::ZT_MIN_MTU;
pub const MAX_MTU: u32 = ztcore::ZT_MAX_MTU;
pub const DEFAULT_UDP_MTU: u32 = ztcore::ZT_DEFAULT_UDP_MTU;
pub const MAX_UDP_MTU: u32 = ztcore::ZT_MAX_UDP_MTU;

#[allow(non_snake_case,non_upper_case_globals)]
pub mod RulePacketCharacteristics {
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

#[derive(FromPrimitive,ToPrimitive)]
pub enum IdentityType {
    Curve25519 = ztcore::ZT_IdentityType_ZT_IDENTITY_TYPE_C25519 as isize,
    NistP384 = ztcore::ZT_IdentityType_ZT_IDENTITY_TYPE_P384 as isize,
}

pub const CERTIFICATE_MAX_STRING_LENGTH: u32 = ztcore::ZT_CERTIFICATE_MAX_STRING_LENGTH;
pub const CERTIFICATE_LOCAL_TRUST_FLAG_ROOT_CA: u32 = ztcore::ZT_CERTIFICATE_LOCAL_TRUST_FLAG_ROOT_CA;
pub const CERTIFICATE_LOCAL_TRUST_FLAG_ZEROTIER_ROOT_SET: u32 = ztcore::ZT_CERTIFICATE_LOCAL_TRUST_FLAG_ZEROTIER_ROOT_SET;

pub const CERTIFICATE_UNIQUE_ID_TYPE_NIST_P_384_SIZE: u32 = ztcore::ZT_CERTIFICATE_UNIQUE_ID_TYPE_NIST_P_384_SIZE;
pub const CERTIFICATE_UNIQUE_ID_TYPE_NIST_P_384_PRIVATE_SIZE: u32 = ztcore::ZT_CERTIFICATE_UNIQUE_ID_TYPE_NIST_P_384_PRIVATE_SIZE;

#[derive(FromPrimitive,ToPrimitive)]
pub enum CertificateUniqueIdType {
    NistP384 = ztcore::ZT_CertificateUniqueIdType_ZT_CERTIFICATE_UNIQUE_ID_TYPE_NIST_P_384 as isize
}

#[derive(FromPrimitive,ToPrimitive)]
pub enum CertificateError {
    None = ztcore::ZT_CertificateError_ZT_CERTIFICATE_ERROR_NONE as isize,
    HaveNewerCert = ztcore::ZT_CertificateError_ZT_CERTIFICATE_ERROR_HAVE_NEWER_CERT as isize,
    InvalidFormat = ztcore::ZT_CertificateError_ZT_CERTIFICATE_ERROR_INVALID_FORMAT as isize,
    InvalidIdentity = ztcore::ZT_CertificateError_ZT_CERTIFICATE_ERROR_INVALID_IDENTITY as isize,
    InvalidPrimarySignature = ztcore::ZT_CertificateError_ZT_CERTIFICATE_ERROR_INVALID_PRIMARY_SIGNATURE as isize,
    InvalidChain = ztcore::ZT_CertificateError_ZT_CERTIFICATE_ERROR_INVALID_CHAIN as isize,
    InvalidComponentSignature = ztcore::ZT_CertificateError_ZT_CERTIFICATE_ERROR_INVALID_COMPONENT_SIGNATURE as isize,
    InvalidUniqueIdProof = ztcore::ZT_CertificateError_ZT_CERTIFICATE_ERROR_INVALID_UNIQUE_ID_PROOF as isize,
    MissingRequiredFields = ztcore::ZT_CertificateError_ZT_CERTIFICATE_ERROR_MISSING_REQUIRED_FIELDS as isize,
    OutOfValidTimeWindow = ztcore::ZT_CertificateError_ZT_CERTIFICATE_ERROR_OUT_OF_VALID_TIME_WINDOW as isize,
}

#[derive(FromPrimitive,ToPrimitive)]
pub enum CredentialType {
    Null = ztcore::ZT_CredentialType_ZT_CREDENTIAL_TYPE_NULL as isize,
    CertificateOfMembership = ztcore::ZT_CredentialType_ZT_CREDENTIAL_TYPE_COM as isize,
    Capability = ztcore::ZT_CredentialType_ZT_CREDENTIAL_TYPE_CAPABILITY as isize,
    Tag = ztcore::ZT_CredentialType_ZT_CREDENTIAL_TYPE_TAG as isize,
    CertificateOfOwnership = ztcore::ZT_CredentialType_ZT_CREDENTIAL_TYPE_COO as isize,
    Revocation = ztcore::ZT_CredentialType_ZT_CREDENTIAL_TYPE_REVOCATION as isize,
}

#[derive(FromPrimitive,ToPrimitive)]
pub enum EndpointType {
    Nil = ztcore::ZT_EndpointType_ZT_ENDPOINT_TYPE_NIL as isize,
    ZeroTier = ztcore::ZT_EndpointType_ZT_ENDPOINT_TYPE_ZEROTIER as isize,
    Ethernet = ztcore::ZT_EndpointType_ZT_ENDPOINT_TYPE_ETHERNET as isize,
    WifiDirect = ztcore::ZT_EndpointType_ZT_ENDPOINT_TYPE_WIFI_DIRECT as isize,
    Bluetooth = ztcore::ZT_EndpointType_ZT_ENDPOINT_TYPE_BLUETOOTH as isize,
    Ip = ztcore::ZT_EndpointType_ZT_ENDPOINT_TYPE_IP as isize,
    IpUdp = ztcore::ZT_EndpointType_ZT_ENDPOINT_TYPE_IP_UDP as isize,
    IpTcp = ztcore::ZT_EndpointType_ZT_ENDPOINT_TYPE_IP_TCP as isize,
    IpHttp = ztcore::ZT_EndpointType_ZT_ENDPOINT_TYPE_IP_HTTP as isize,
}

#[derive(FromPrimitive,ToPrimitive)]
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

#[derive(FromPrimitive,ToPrimitive)]
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

#[derive(FromPrimitive,ToPrimitive)]
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

#[derive(FromPrimitive,ToPrimitive)]
pub enum TraceCredentialRejectionReason {
    SignatureVerificationFailed = ztcore::ZT_TraceCredentialRejectionReason_ZT_TRACE_CREDENTIAL_REJECTION_REASON_SIGNATURE_VERIFICATION_FAILED as isize,
    Revoked = ztcore::ZT_TraceCredentialRejectionReason_ZT_TRACE_CREDENTIAL_REJECTION_REASON_REVOKED as isize,
    OlderThanLatest = ztcore::ZT_TraceCredentialRejectionReason_ZT_TRACE_CREDENTIAL_REJECTION_REASON_OLDER_THAN_LATEST as isize,
    Invalid = ztcore::ZT_TraceCredentialRejectionReason_ZT_TRACE_CREDENTIAL_REJECTION_REASON_INVALID as isize,
}

#[derive(FromPrimitive,ToPrimitive)]
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

#[derive(FromPrimitive,ToPrimitive)]
pub enum Event {
    Up = ztcore::ZT_Event_ZT_EVENT_UP as isize,
    Offline = ztcore::ZT_Event_ZT_EVENT_OFFLINE as isize,
    Online = ztcore::ZT_Event_ZT_EVENT_ONLINE as isize,
    Down = ztcore::ZT_Event_ZT_EVENT_DOWN as isize,
    Trace = ztcore::ZT_Event_ZT_EVENT_TRACE as isize,
    UserMessage = ztcore::ZT_Event_ZT_EVENT_USER_MESSAGE as isize,
}

#[derive(FromPrimitive,ToPrimitive)]
pub enum VirtualNetworkStatus {
    RequestingConfiguration = ztcore::ZT_VirtualNetworkStatus_ZT_NETWORK_STATUS_REQUESTING_CONFIGURATION as isize,
    Ok = ztcore::ZT_VirtualNetworkStatus_ZT_NETWORK_STATUS_OK as isize,
    AccessDenied = ztcore::ZT_VirtualNetworkStatus_ZT_NETWORK_STATUS_ACCESS_DENIED as isize,
    NotFound = ztcore::ZT_VirtualNetworkStatus_ZT_NETWORK_STATUS_NOT_FOUND as isize,
}

#[derive(FromPrimitive,ToPrimitive)]
pub enum VirtualNetworkType {
    Private = ztcore::ZT_VirtualNetworkType_ZT_NETWORK_TYPE_PRIVATE as isize,
    Public = ztcore::ZT_VirtualNetworkType_ZT_NETWORK_TYPE_PUBLIC as isize,
}

#[derive(FromPrimitive,ToPrimitive)]
pub enum VirtualNetworkRuleType {
    ActionDrop = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_ACTION_DROP as isize,
    ActionAccept = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_ACTION_ACCEPT as isize,
    ActionTee = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_ACTION_TEE as isize,
    ActionWatch = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_ACTION_WATCH as isize,
    ActionRedirect = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_ACTION_REDIRECT as isize,
    ActionBreak = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_ACTION_BREAK as isize,
    ActionPriority = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_ACTION_PRIORITY as isize,
    MatchSourceZeroTierAddress = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_SOURCE_ZEROTIER_ADDRESS as isize,
    MatchDestinationZeroTierAddress = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_DEST_ZEROTIER_ADDRESS as isize,
    MatchVlanId = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_VLAN_ID as isize,
    MatchVlanPcp = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_VLAN_PCP as isize,
    MatchVlanDei = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_VLAN_DEI as isize,
    MatchMacSource = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_MAC_SOURCE as isize,
    MatchMacDestination = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_MAC_DEST as isize,
    MatchIpv4Source = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_IPV4_SOURCE as isize,
    MatchIpv4Destination = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_IPV4_DEST as isize,
    MatchIpv6Source = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_IPV6_SOURCE as isize,
    MatchIpv6Destination = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_IPV6_DEST as isize,
    MatchIpTos = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_IP_TOS as isize,
    MatchIpProtocol = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_IP_PROTOCOL as isize,
    MatchEtherType = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_ETHERTYPE as isize,
    MatchIcmp = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_ICMP as isize,
    MatchIpSourcePortRange = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_IP_SOURCE_PORT_RANGE as isize,
    MatchIpDestinationSourceRange = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE as isize,
    MatchCharacteristics = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_CHARACTERISTICS as isize,
    MatchFrameSizeRange = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_FRAME_SIZE_RANGE as isize,
    MatchRandom = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_RANDOM as isize,
    MatchTagsDifference = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_TAGS_DIFFERENCE as isize,
    MatchTagsBitwiseAnd = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_AND as isize,
    MatchTagsBitwiseOr = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_OR as isize,
    MatchTagsBitwiseXor = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_XOR as isize,
    MatchTagsEqual = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_TAGS_EQUAL as isize,
    MatchTagSender = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_TAG_SENDER as isize,
    MatchTagReceiver = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_TAG_RECEIVER as isize,
    MatchIntegerRange = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_INTEGER_RANGE as isize,
}

#[derive(FromPrimitive,ToPrimitive)]
pub enum VirtualNetworkConfigOperation {
    Up = ztcore::ZT_VirtualNetworkConfigOperation_ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_UP as isize,
    ConfigUpdate = ztcore::ZT_VirtualNetworkConfigOperation_ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE as isize,
    Down = ztcore::ZT_VirtualNetworkConfigOperation_ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN as isize,
    Destroy = ztcore::ZT_VirtualNetworkConfigOperation_ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY as isize
}

#[derive(FromPrimitive,ToPrimitive)]
pub enum StateObjectType {
    IdentityPublic = ztcore::ZT_StateObjectType_ZT_STATE_OBJECT_IDENTITY_PUBLIC as isize,
    IdentitySecret = ztcore::ZT_StateObjectType_ZT_STATE_OBJECT_IDENTITY_SECRET as isize,
    Locator = ztcore::ZT_StateObjectType_ZT_STATE_OBJECT_LOCATOR as isize,
    Peer = ztcore::ZT_StateObjectType_ZT_STATE_OBJECT_PEER as isize,
    NetworkConfig = ztcore::ZT_StateObjectType_ZT_STATE_OBJECT_NETWORK_CONFIG as isize,
    TrustStore = ztcore::ZT_StateObjectType_ZT_STATE_OBJECT_TRUST_STORE as isize,
    Certificate = ztcore::ZT_StateObjectType_ZT_STATE_OBJECT_CERT as isize
}

#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        assert_eq!(2 + 2, 4);
    }
}
