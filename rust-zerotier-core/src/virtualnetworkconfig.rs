use std::ffi::CStr;
use std::mem::{size_of, transmute, zeroed};
use std::os::raw::{c_void, c_char};

use num_derive::{FromPrimitive, ToPrimitive};
use num_traits::FromPrimitive;
use socket2::SockAddr;

use crate::*;
use crate::bindings::capi as ztcore;

#[derive(FromPrimitive,ToPrimitive)]
pub enum VirtualNetworkType {
    Private = ztcore::ZT_VirtualNetworkType_ZT_NETWORK_TYPE_PRIVATE as isize,
    Public = ztcore::ZT_VirtualNetworkType_ZT_NETWORK_TYPE_PUBLIC as isize
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
    MatchIntegerRange = ztcore::ZT_VirtualNetworkRuleType_ZT_NETWORK_RULE_MATCH_INTEGER_RANGE as isize
}

#[derive(FromPrimitive,ToPrimitive)]
pub enum VirtualNetworkConfigOperation {
    Up = ztcore::ZT_VirtualNetworkConfigOperation_ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_UP as isize,
    ConfigUpdate = ztcore::ZT_VirtualNetworkConfigOperation_ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE as isize,
    Down = ztcore::ZT_VirtualNetworkConfigOperation_ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN as isize,
    Destroy = ztcore::ZT_VirtualNetworkConfigOperation_ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY as isize
}

#[derive(FromPrimitive,ToPrimitive)]
pub enum VirtualNetworkStatus {
    RequestingConfiguration = ztcore::ZT_VirtualNetworkStatus_ZT_NETWORK_STATUS_REQUESTING_CONFIGURATION as isize,
    Ok = ztcore::ZT_VirtualNetworkStatus_ZT_NETWORK_STATUS_OK as isize,
    AccessDenied = ztcore::ZT_VirtualNetworkStatus_ZT_NETWORK_STATUS_ACCESS_DENIED as isize,
    NotFound = ztcore::ZT_VirtualNetworkStatus_ZT_NETWORK_STATUS_NOT_FOUND as isize
}

pub struct VirtualNetworkRoute {
    pub target: Option<SockAddr>,
    pub via: Option<SockAddr>,
    pub flags: u16,
    pub metric: u16
}

#[allow(non_snake_case)]
pub struct VirtualNetworkConfig {
    pub nwid: NetworkId,
    pub mac: MAC,
    pub name: String,
    pub status: VirtualNetworkStatus,
    pub type_: VirtualNetworkType,
    pub mtu: u32,
    pub bridge: bool,
    pub broadcastEnabled: bool,
    pub netconfRevision: u64,
    pub assignedAddresses: Vec<SockAddr>,
    pub routes: Vec<VirtualNetworkRoute>
}

const SIZEOF_SOCKADDR_IN: ztcore::socklen_t = size_of::<ztcore::sockaddr_in>() as ztcore::socklen_t;
const SIZEOF_SOCKADDR_IN6: ztcore::socklen_t = size_of::<ztcore::sockaddr_in6>() as ztcore::socklen_t;

/// Obtain a socket2::SockAddr from a C struct sockaddr_storage as used in the ZeroTier core.
pub(crate) fn sockaddr_from_capi(ss: &ztcore::sockaddr_storage) -> Option<SockAddr> {
    match ss.ss_family as u32 {
        ztcore::AF_INET => { unsafe { Some(SockAddr::from_raw_parts(transmute(ss as *const ztcore::sockaddr_storage), transmute(SIZEOF_SOCKADDR_IN))) } },
        ztcore::AF_INET6 => { unsafe { Some(SockAddr::from_raw_parts(transmute(ss as *const ztcore::sockaddr_storage), transmute(SIZEOF_SOCKADDR_IN6))) } },
        _ => None
    }
}

impl VirtualNetworkConfig {
    pub(crate) fn new_from_capi(vnc: &ztcore::ZT_VirtualNetworkConfig) -> VirtualNetworkConfig {
        unsafe {
            let mut aa: Vec<SockAddr> = Vec::new();
            let saptr = vnc.assignedAddresses.as_ptr();
            for i in 0..vnc.assignedAddressCount as isize {
                let sa = sockaddr_from_capi(&*saptr.offset(i));
                if sa.is_some() {
                    aa.push(sa.unwrap());
                }
            }

            let mut rts: Vec<VirtualNetworkRoute> = Vec::new();
            let rtptr = vnc.routes.as_ptr();
            for i in 0..vnc.routeCount as isize {
                let r = *rtptr.offset(i);
                rts.push(VirtualNetworkRoute{
                    target: sockaddr_from_capi(&r.target),
                    via: sockaddr_from_capi(&r.via),
                    flags: r.flags,
                    metric: r.metric
                })
            }

            return VirtualNetworkConfig{
                nwid: NetworkId(vnc.nwid),
                mac: MAC(vnc.mac),
                name: String::from(CStr::from_ptr(vnc.name.as_ptr() as *const c_char).to_str().unwrap_or("")),
                status: FromPrimitive::from_u32(vnc.status as u32).unwrap(),
                type_: FromPrimitive::from_u32(vnc.type_ as u32).unwrap(),
                mtu: vnc.mtu as u32,
                bridge: vnc.bridge != 0,
                broadcastEnabled: vnc.broadcastEnabled != 0,
                netconfRevision: vnc.netconfRevision as u64,
                assignedAddresses: aa,
                routes: rts
            }
        }
    }
}
