use std::mem::{size_of, transmute, zeroed};

use serde::{Deserialize, Serialize};
use num_derive::{FromPrimitive, ToPrimitive};
use num_traits::FromPrimitive;

use crate::*;
use crate::bindings::capi as ztcore;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#[derive(FromPrimitive,ToPrimitive)]
pub enum VirtualNetworkType {
    Private = ztcore::ZT_VirtualNetworkType_ZT_NETWORK_TYPE_PRIVATE as isize,
    Public = ztcore::ZT_VirtualNetworkType_ZT_NETWORK_TYPE_PUBLIC as isize
}

impl VirtualNetworkType {
    pub fn to_str(&self) -> &str {
        match *self {
            //VirtualNetworkType::Private => "PRIVATE",
            VirtualNetworkType::Public => "PUBLIC",
            _ => "PRIVATE"
        }
    }
}

impl From<&str> for VirtualNetworkType {
    fn from(s: &str) -> VirtualNetworkType {
        match s.to_ascii_lowercase().as_str() {
            //"requesting_configuration" | "requestingconfiguration" => VirtualNetworkStatus::RequestingConfiguration,
            "public" => VirtualNetworkType::Public,
            _ => VirtualNetworkType::Private
        }
    }
}

impl ToString for VirtualNetworkType {
    #[inline(always)]
    fn to_string(&self) -> String {
        String::from(self.to_str())
    }
}

impl serde::Serialize for VirtualNetworkType {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: serde::Serializer {
        serializer.serialize_str(self.to_str())
    }
}

struct VirtualNetworkTypeVisitor;

impl<'de> serde::de::Visitor<'de> for VirtualNetworkTypeVisitor {
    type Value = VirtualNetworkType;

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("VirtualNetworkType value in string form")
    }

    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E> where E: serde::de::Error {
        Ok(VirtualNetworkType::from(s))
    }
}

impl<'de> serde::Deserialize<'de> for VirtualNetworkType {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: serde::Deserializer<'de> {
        deserializer.deserialize_str(VirtualNetworkTypeVisitor)
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#[derive(FromPrimitive,ToPrimitive)]
pub enum VirtualNetworkConfigOperation {
    Up = ztcore::ZT_VirtualNetworkConfigOperation_ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_UP as isize,
    ConfigUpdate = ztcore::ZT_VirtualNetworkConfigOperation_ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE as isize,
    Down = ztcore::ZT_VirtualNetworkConfigOperation_ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN as isize,
    Destroy = ztcore::ZT_VirtualNetworkConfigOperation_ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY as isize
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#[derive(FromPrimitive,ToPrimitive)]
pub enum VirtualNetworkStatus {
    RequestingConfiguration = ztcore::ZT_VirtualNetworkStatus_ZT_NETWORK_STATUS_REQUESTING_CONFIGURATION as isize,
    Ok = ztcore::ZT_VirtualNetworkStatus_ZT_NETWORK_STATUS_OK as isize,
    AccessDenied = ztcore::ZT_VirtualNetworkStatus_ZT_NETWORK_STATUS_ACCESS_DENIED as isize,
    NotFound = ztcore::ZT_VirtualNetworkStatus_ZT_NETWORK_STATUS_NOT_FOUND as isize
}

impl VirtualNetworkStatus {
    pub fn to_str(&self) -> &str {
        match *self {
            VirtualNetworkStatus::RequestingConfiguration => "REQUESTING_CONFIGURATION",
            VirtualNetworkStatus::Ok => "OK",
            VirtualNetworkStatus::AccessDenied => "ACCESS_DENIED",
            VirtualNetworkStatus::NotFound => "NOT_FOUND"
        }
    }
}

impl From<&str> for VirtualNetworkStatus {
    fn from(s: &str) -> VirtualNetworkStatus {
        match s.to_ascii_lowercase().as_str() {
            //"requesting_configuration" | "requestingconfiguration" => VirtualNetworkStatus::RequestingConfiguration,
            "ok" => VirtualNetworkStatus::Ok,
            "access_denied" | "accessdenied" => VirtualNetworkStatus::AccessDenied,
            "not_found" | "notfound" => VirtualNetworkStatus::NotFound,
            _ => VirtualNetworkStatus::RequestingConfiguration
        }
    }
}

impl ToString for VirtualNetworkStatus {
    #[inline(always)]
    fn to_string(&self) -> String {
        String::from(self.to_str())
    }
}

impl serde::Serialize for VirtualNetworkStatus {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: serde::Serializer {
        serializer.serialize_str(self.to_str())
    }
}

struct VirtualNetworkStatusVisitor;

impl<'de> serde::de::Visitor<'de> for VirtualNetworkStatusVisitor {
    type Value = VirtualNetworkStatus;

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("VirtualNetworkStatus value in string form")
    }

    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E> where E: serde::de::Error {
        Ok(VirtualNetworkStatus::from(s))
    }
}

impl<'de> serde::Deserialize<'de> for VirtualNetworkStatus {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: serde::Deserializer<'de> {
        deserializer.deserialize_str(VirtualNetworkStatusVisitor)
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#[derive(Serialize, Deserialize)]
pub struct VirtualNetworkRoute {
    pub target: Option<InetAddress>,
    pub via: Option<InetAddress>,
    pub flags: u16,
    pub metric: u16
}

#[derive(Serialize, Deserialize)]
pub struct VirtualNetworkConfig {
    pub nwid: NetworkId,
    pub mac: MAC,
    pub name: String,
    pub status: VirtualNetworkStatus,
    #[serde(rename = "type")]
    pub type_: VirtualNetworkType,
    pub mtu: u32,
    pub bridge: bool,
    #[serde(rename = "broadcastEnabled")]
    pub broadcast_enabled: bool,
    #[serde(rename = "netconfRevision")]
    pub netconf_revision: u64,
    #[serde(rename = "assignedAddresses")]
    pub assigned_addresses: Vec<InetAddress>,
    pub routes: Vec<VirtualNetworkRoute>
}

impl VirtualNetworkConfig {
    pub(crate) fn new_from_capi(vnc: &ztcore::ZT_VirtualNetworkConfig) -> VirtualNetworkConfig {
        let mut aa: Vec<InetAddress> = Vec::new();
        let saptr = vnc.assignedAddresses.as_ptr();
        for i in 0..vnc.assignedAddressCount as isize {
            let a = InetAddress::new_from_capi(unsafe { *saptr.offset(i) });
            if a.is_some() {
                aa.push(a.unwrap());
            }
        }

        let mut rts: Vec<VirtualNetworkRoute> = Vec::new();
        let rtptr = vnc.routes.as_ptr();
        for i in 0..vnc.routeCount as isize {
            let r = unsafe { *rtptr.offset(i) };
            rts.push(VirtualNetworkRoute{
                target: InetAddress::new_from_capi(r.target),
                via: InetAddress::new_from_capi(r.via),
                flags: r.flags,
                metric: r.metric
            })
        }

        return VirtualNetworkConfig{
            nwid: NetworkId(vnc.nwid),
            mac: MAC(vnc.mac),
            name: unsafe { cstr_to_string(vnc.name.as_ptr(), vnc.name.len() as isize) },
            status: FromPrimitive::from_u32(vnc.status as u32).unwrap(),
            type_: FromPrimitive::from_u32(vnc.type_ as u32).unwrap(),
            mtu: vnc.mtu as u32,
            bridge: vnc.bridge != 0,
            broadcast_enabled: vnc.broadcastEnabled != 0,
            netconf_revision: vnc.netconfRevision as u64,
            assigned_addresses: aa,
            routes: rts
        }
    }
}
