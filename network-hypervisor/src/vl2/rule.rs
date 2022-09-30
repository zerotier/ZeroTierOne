use std::mem::{size_of, zeroed};

use serde::{Deserialize, Deserializer, Serialize, Serializer};

use phf::phf_map;

use zerotier_utils::buffer::Buffer;
use zerotier_utils::marshalable::{Marshalable, UnmarshalError};

use crate::protocol;
use crate::vl1::{Address, InetAddress, MAC};

#[allow(unused)]
pub const RULES_ENGINE_REVISION: u8 = 1;

#[allow(unused)]
pub mod characteristic {
    pub const INBOUND: u64 = 0x8000000000000000;
    pub const MULTICAST: u64 = 0x4000000000000000;
    pub const BROADCAST: u64 = 0x2000000000000000;
    pub const SENDER_IP_AUTHENTICATED: u64 = 0x1000000000000000;
    pub const SENDER_MAC_AUTHENTICATED: u64 = 0x0800000000000000;
    pub const TCP_RESERVED_0: u64 = 0x0000000000000800;
    pub const TCP_RESERVED_1: u64 = 0x0000000000000400;
    pub const TCP_RESERVED_2: u64 = 0x0000000000000200;
    pub const TCP_NS: u64 = 0x0000000000000100;
    pub const TCP_CWR: u64 = 0x0000000000000080;
    pub const TCP_ECE: u64 = 0x0000000000000040;
    pub const TCP_URG: u64 = 0x0000000000000020;
    pub const TCP_ACK: u64 = 0x0000000000000010;
    pub const TCP_PSH: u64 = 0x0000000000000008;
    pub const TCP_RST: u64 = 0x0000000000000004;
    pub const TCP_SYN: u64 = 0x0000000000000002;
    pub const TCP_FIN: u64 = 0x0000000000000001;
}

#[allow(unused)]
pub mod action {
    pub const DROP: u8 = 0;
    pub const ACCEPT: u8 = 1;
    pub const TEE: u8 = 2;
    pub const WATCH: u8 = 3;
    pub const REDIRECT: u8 = 4;
    pub const BREAK: u8 = 5;
    pub const PRIORITY: u8 = 6;
}

#[allow(unused)]
pub mod match_cond {
    pub const SOURCE_ZEROTIER_ADDRESS: u8 = 24;
    pub const DEST_ZEROTIER_ADDRESS: u8 = 25;
    pub const VLAN_ID: u8 = 26;
    pub const VLAN_PCP: u8 = 27;
    pub const VLAN_DEI: u8 = 28;
    pub const MAC_SOURCE: u8 = 29;
    pub const MAC_DEST: u8 = 30;
    pub const IPV4_SOURCE: u8 = 31;
    pub const IPV4_DEST: u8 = 32;
    pub const IPV6_SOURCE: u8 = 33;
    pub const IPV6_DEST: u8 = 34;
    pub const IP_TOS: u8 = 35;
    pub const IP_PROTOCOL: u8 = 36;
    pub const ETHERTYPE: u8 = 37;
    pub const ICMP: u8 = 38;
    pub const IP_SOURCE_PORT_RANGE: u8 = 39;
    pub const IP_DEST_PORT_RANGE: u8 = 40;
    pub const CHARACTERISTICS: u8 = 41;
    pub const FRAME_SIZE_RANGE: u8 = 42;
    pub const RANDOM: u8 = 43;
    pub const TAGS_DIFFERENCE: u8 = 44;
    pub const TAGS_BITWISE_AND: u8 = 45;
    pub const TAGS_BITWISE_OR: u8 = 46;
    pub const TAGS_BITWISE_XOR: u8 = 47;
    pub const TAGS_EQUAL: u8 = 48;
    pub const TAG_SENDER: u8 = 49;
    pub const TAG_RECEIVER: u8 = 50;
    pub const INTEGER_RANGE: u8 = 51;
}

#[allow(unused)]
mod rule_value {
    #[repr(C, packed)]
    #[derive(Clone, Copy)]
    pub struct Ipv6 {
        pub ip: [u8; 16],
        pub mask: u8,
    }

    #[repr(C, packed)]
    #[derive(Clone, Copy)]
    pub struct Ipv4 {
        pub ip: [u8; 4],
        pub mask: u8,
    }

    #[repr(C, packed)]
    #[derive(Clone, Copy)]
    pub struct IntRange {
        pub start: u64,
        pub end: u64,
        pub idx: u16,
        pub format: u8,
    }

    #[repr(C, packed)]
    #[derive(Clone, Copy)]
    pub struct IpTos {
        pub mask: u8,
        pub value: [u8; 2],
    }

    #[repr(C, packed)]
    #[derive(Clone, Copy)]
    pub struct Icmp {
        pub _type: u8,
        pub code: u8,
        pub flags: u8,
    }

    #[repr(C, packed)]
    #[derive(Clone, Copy)]
    pub struct Tag {
        pub id: u32,
        pub value: u32,
    }

    #[repr(C, packed)]
    #[derive(Clone, Copy)]
    pub struct Forward {
        pub address: u64,
        pub flags: u32,
        pub length: u16,
    }
}

#[repr(C, packed)]
#[derive(Clone, Copy)]
union RuleValue {
    pub ipv6: rule_value::Ipv6,
    pub ipv4: rule_value::Ipv4,
    pub int_range: rule_value::IntRange,
    pub characteristics: u64,
    pub port_range: [u16; 2],
    pub zt: u64,
    pub random_probability: u32,
    pub mac: [u8; 6],
    pub vlan_id: u16,
    pub vlan_pcp: u8,
    pub vlan_dei: u8,
    pub ethertype: u16,
    pub ip_protocol: u8,
    pub ip_tos: rule_value::IpTos,
    pub frame_size_range: [u16; 2],
    pub icmp: rule_value::Icmp,
    pub tag: rule_value::Tag,
    pub forward: rule_value::Forward,
    pub qos_bucket: u8,
}

/// Trait to implement in order to evaluate rules.
pub trait RuleVisitor {
    fn action_drop(self) -> bool;
    fn action_accept(self) -> bool;
    fn action_tee(self, address: Address, flags: u32, length: u16) -> bool;
    fn action_watch(self, address: Address, flags: u32, length: u16) -> bool;
    fn action_redirect(self, address: Address, flags: u32, length: u16) -> bool;
    fn action_break(self) -> bool;
    fn action_priority(self, qos_bucket: u8) -> bool;

    fn invalid_rule(self) -> bool;

    fn match_source_zerotier_address(self, not: bool, or: bool, address: Address);
    fn match_dest_zerotier_address(self, not: bool, or: bool, address: Address);
    fn match_vlan_id(self, not: bool, or: bool, id: u16);
    fn match_vlan_pcp(self, not: bool, or: bool, pcp: u8);
    fn match_vlan_dei(self, not: bool, or: bool, dei: u8);
    fn match_mac_source(self, not: bool, or: bool, mac: MAC);
    fn match_mac_dest(self, not: bool, or: bool, mac: MAC);
    fn match_ipv4_source(self, not: bool, or: bool, ip: &[u8; 4], mask: u8);
    fn match_ipv4_dest(self, not: bool, or: bool, ip: &[u8; 4], mask: u8);
    fn match_ipv6_source(self, not: bool, or: bool, ip: &[u8; 16], mask: u8);
    fn match_ipv6_dest(self, not: bool, or: bool, ip: &[u8; 16], mask: u8);
    fn match_ip_tos(self, not: bool, or: bool, mask: u8, start: u8, end: u8);
    fn match_ip_protocol(self, not: bool, or: bool, protocol: u8);
    fn match_ethertype(self, not: bool, or: bool, ethertype: u16);
    fn match_icmp(self, not: bool, or: bool, _type: u8, code: u8, flags: u8);
    fn match_ip_source_port_range(self, not: bool, or: bool, start: u16, end: u16);
    fn match_ip_dest_port_range(self, not: bool, or: bool, start: u16, end: u16);
    fn match_characteristics(self, not: bool, or: bool, characteristics: u64);
    fn match_frame_size_range(self, not: bool, or: bool, start: u16, end: u16);
    fn match_random(self, not: bool, or: bool, probability: u32);
    fn match_tags_difference(self, not: bool, or: bool, id: u32, value: u32);
    fn match_tags_bitwise_and(self, not: bool, or: bool, id: u32, value: u32);
    fn match_tags_bitwise_or(self, not: bool, or: bool, id: u32, value: u32);
    fn match_tags_bitwise_xor(self, not: bool, or: bool, id: u32, value: u32);
    fn match_tags_equal(self, not: bool, or: bool, id: u32, value: u32);
    fn match_tag_sender(self, not: bool, or: bool, id: u32, value: u32);
    fn match_tag_receiver(self, not: bool, or: bool, id: u32, value: u32);
    fn match_integer_range(self, not: bool, or: bool, start: u64, end: u64, idx: u16, format: u8);
}

#[repr(C, packed)]
#[derive(Clone, Copy)]
pub struct Rule {
    t: u8,
    v: RuleValue,
}

impl Default for Rule {
    #[inline(always)]
    fn default() -> Self {
        unsafe { zeroed() }
    }
}

impl Rule {
    #[inline(always)]
    pub fn action_or_condition(&self) -> u8 {
        self.t & 0x3f
    }

    /// Execute the visitor, returning the result of action methods and true for condition methods.
    #[inline(always)]
    pub fn visit<V: RuleVisitor>(&self, v: V) -> bool {
        unsafe {
            let t = self.t;
            let not = (t & 0x80) != 0;
            let or = (t & 0x40) != 0;
            match t & 0x3f {
                action::ACCEPT => {
                    return v.action_accept();
                }
                action::TEE => {
                    if let Some(a) = Address::from_u64(self.v.forward.address) {
                        return v.action_tee(a, self.v.forward.flags, self.v.forward.length);
                    } else {
                        return v.invalid_rule();
                    }
                }
                action::WATCH => {
                    if let Some(a) = Address::from_u64(self.v.forward.address) {
                        return v.action_watch(a, self.v.forward.flags, self.v.forward.length);
                    } else {
                        return v.invalid_rule();
                    }
                }
                action::REDIRECT => {
                    if let Some(a) = Address::from_u64(self.v.forward.address) {
                        return v.action_redirect(a, self.v.forward.flags, self.v.forward.length);
                    } else {
                        return v.invalid_rule();
                    }
                }
                action::BREAK => {
                    return v.action_break();
                }
                action::PRIORITY => {
                    return v.action_priority(self.v.qos_bucket);
                }
                match_cond::SOURCE_ZEROTIER_ADDRESS => {
                    if let Some(a) = Address::from_u64(self.v.zt) {
                        v.match_source_zerotier_address(not, or, a);
                    } else {
                        return v.invalid_rule();
                    }
                }
                match_cond::DEST_ZEROTIER_ADDRESS => {
                    if let Some(a) = Address::from_u64(self.v.zt) {
                        v.match_dest_zerotier_address(not, or, a);
                    } else {
                        return v.invalid_rule();
                    }
                }
                match_cond::VLAN_ID => {
                    v.match_vlan_id(not, or, self.v.vlan_id);
                }
                match_cond::VLAN_PCP => {
                    v.match_vlan_pcp(not, or, self.v.vlan_pcp);
                }
                match_cond::VLAN_DEI => {
                    v.match_vlan_dei(not, or, self.v.vlan_dei);
                }
                match_cond::MAC_SOURCE => {
                    if let Some(m) = MAC::from_bytes_fixed(&self.v.mac) {
                        v.match_mac_source(not, or, m);
                    } else {
                        return v.invalid_rule();
                    }
                }
                match_cond::MAC_DEST => {
                    if let Some(m) = MAC::from_bytes_fixed(&self.v.mac) {
                        v.match_mac_dest(not, or, m);
                    } else {
                        return v.invalid_rule();
                    }
                }
                match_cond::IPV4_SOURCE => {
                    v.match_ipv4_source(not, or, &self.v.ipv4.ip, self.v.ipv4.mask);
                }
                match_cond::IPV4_DEST => {
                    v.match_ipv4_dest(not, or, &self.v.ipv4.ip, self.v.ipv4.mask);
                }
                match_cond::IPV6_SOURCE => {
                    v.match_ipv6_source(not, or, &self.v.ipv6.ip, self.v.ipv6.mask);
                }
                match_cond::IPV6_DEST => {
                    v.match_ipv6_dest(not, or, &self.v.ipv6.ip, self.v.ipv6.mask);
                }
                match_cond::IP_TOS => {
                    v.match_ip_tos(not, or, self.v.ip_tos.mask, self.v.ip_tos.value[0], self.v.ip_tos.value[1]);
                }
                match_cond::IP_PROTOCOL => {
                    v.match_ip_protocol(not, or, self.v.ip_protocol);
                }
                match_cond::ETHERTYPE => {
                    v.match_ethertype(not, or, self.v.ethertype);
                }
                match_cond::ICMP => {
                    v.match_icmp(not, or, self.v.icmp._type, self.v.icmp.code, self.v.icmp.flags);
                }
                match_cond::IP_SOURCE_PORT_RANGE => {
                    v.match_ip_source_port_range(not, or, self.v.port_range[0], self.v.port_range[1]);
                }
                match_cond::IP_DEST_PORT_RANGE => {
                    v.match_ip_dest_port_range(not, or, self.v.port_range[0], self.v.port_range[1]);
                }
                match_cond::CHARACTERISTICS => {
                    v.match_characteristics(not, or, self.v.characteristics);
                }
                match_cond::FRAME_SIZE_RANGE => {
                    v.match_frame_size_range(not, or, self.v.frame_size_range[0], self.v.frame_size_range[1]);
                }
                match_cond::RANDOM => {
                    v.match_random(not, or, self.v.random_probability);
                }
                match_cond::TAGS_DIFFERENCE => {
                    v.match_tags_difference(not, or, self.v.tag.id, self.v.tag.value);
                }
                match_cond::TAGS_BITWISE_AND => {
                    v.match_tags_bitwise_and(not, or, self.v.tag.id, self.v.tag.value);
                }
                match_cond::TAGS_BITWISE_OR => {
                    v.match_tags_bitwise_or(not, or, self.v.tag.id, self.v.tag.value);
                }
                match_cond::TAGS_BITWISE_XOR => {
                    v.match_tags_bitwise_xor(not, or, self.v.tag.id, self.v.tag.value);
                }
                match_cond::TAGS_EQUAL => {
                    v.match_tags_equal(not, or, self.v.tag.id, self.v.tag.value);
                }
                match_cond::TAG_SENDER => {
                    v.match_tag_sender(not, or, self.v.tag.id, self.v.tag.value);
                }
                match_cond::TAG_RECEIVER => {
                    v.match_tag_receiver(not, or, self.v.tag.id, self.v.tag.value);
                }
                match_cond::INTEGER_RANGE => v.match_integer_range(
                    not,
                    or,
                    self.v.int_range.start,
                    self.v.int_range.end,
                    self.v.int_range.idx,
                    self.v.int_range.format,
                ),
                _ => return v.invalid_rule(),
            }
        }
        return true;
    }
}

impl Marshalable for Rule {
    const MAX_MARSHAL_SIZE: usize = 21;

    fn marshal<const BL: usize>(&self, buf: &mut Buffer<BL>) -> Result<(), UnmarshalError> {
        buf.append_u8(self.t)?;
        unsafe {
            match self.t & 0x3f {
                action::TEE | action::WATCH | action::REDIRECT => {
                    buf.append_u8(14)?;
                    buf.append_u64(self.v.forward.address)?;
                    buf.append_u32(self.v.forward.flags)?;
                    buf.append_u16(self.v.forward.length)?;
                }
                action::PRIORITY => {
                    buf.append_bytes_fixed(&[1u8, self.v.qos_bucket])?;
                }
                match_cond::SOURCE_ZEROTIER_ADDRESS | match_cond::DEST_ZEROTIER_ADDRESS => {
                    buf.append_u8(5)?;
                    buf.append_bytes(&self.v.zt.to_be_bytes()[..protocol::ADDRESS_SIZE])?;
                }
                match_cond::VLAN_ID => {
                    buf.append_u8(2)?;
                    buf.append_u16(self.v.vlan_id)?;
                }
                match_cond::VLAN_PCP => {
                    buf.append_bytes_fixed(&[1u8, self.v.vlan_pcp])?;
                }
                match_cond::VLAN_DEI => {
                    buf.append_bytes_fixed(&[1u8, self.v.vlan_dei])?;
                }
                match_cond::MAC_SOURCE | match_cond::MAC_DEST => {
                    buf.append_u8(6)?;
                    buf.append_bytes_fixed(&self.v.mac)?;
                }
                match_cond::IPV4_SOURCE | match_cond::IPV4_DEST => {
                    buf.append_u8(5)?;
                    buf.append_bytes_fixed(&self.v.ipv4.ip)?;
                    buf.append_u8(self.v.ipv4.mask)?;
                }
                match_cond::IPV6_SOURCE | match_cond::IPV6_DEST => {
                    buf.append_u8(17)?;
                    buf.append_bytes_fixed(&self.v.ipv6.ip)?;
                    buf.append_u8(self.v.ipv6.mask)?;
                }
                match_cond::IP_TOS => {
                    buf.append_bytes_fixed(&[3u8, self.v.ip_tos.mask, self.v.ip_tos.value[0], self.v.ip_tos.value[1]])?;
                }
                match_cond::IP_PROTOCOL => {
                    buf.append_bytes_fixed(&[1u8, self.v.ip_protocol])?;
                }
                match_cond::ETHERTYPE => {
                    buf.append_u8(2)?;
                    buf.append_u16(self.v.ethertype)?;
                }
                match_cond::ICMP => {
                    buf.append_bytes_fixed(&[3u8, self.v.icmp._type, self.v.icmp.code, self.v.icmp.flags])?;
                }
                match_cond::IP_SOURCE_PORT_RANGE | match_cond::IP_DEST_PORT_RANGE => {
                    buf.append_u8(4)?;
                    buf.append_u16(self.v.port_range[0])?;
                    buf.append_u16(self.v.port_range[1])?;
                }
                match_cond::CHARACTERISTICS => {
                    buf.append_u8(8)?;
                    buf.append_u64(self.v.characteristics)?;
                }
                match_cond::FRAME_SIZE_RANGE => {
                    buf.append_u8(4)?;
                    buf.append_u16(self.v.frame_size_range[0])?;
                    buf.append_u16(self.v.frame_size_range[1])?;
                }
                match_cond::RANDOM => {
                    buf.append_u8(4)?;
                    buf.append_u32(self.v.random_probability)?;
                }
                match_cond::TAGS_DIFFERENCE
                | match_cond::TAGS_BITWISE_AND
                | match_cond::TAGS_BITWISE_OR
                | match_cond::TAGS_BITWISE_XOR
                | match_cond::TAGS_EQUAL
                | match_cond::TAG_SENDER
                | match_cond::TAG_RECEIVER => {
                    buf.append_u8(8)?;
                    buf.append_u32(self.v.tag.id)?;
                    buf.append_u32(self.v.tag.value)?;
                }
                match_cond::INTEGER_RANGE => {
                    buf.append_u8(19)?;
                    buf.append_u64(self.v.int_range.start)?;
                    buf.append_u64(self.v.int_range.end)?;
                    buf.append_u16(self.v.int_range.idx)?;
                    buf.append_u8(self.v.int_range.format)?;
                }
                _ => buf.append_u8(0)?,
            }
        }

        Ok(())
    }

    fn unmarshal<const BL: usize>(buf: &Buffer<BL>, cursor: &mut usize) -> Result<Self, UnmarshalError> {
        let mut r = Rule::default();

        let rt = buf.read_u8(cursor)?;
        r.t = rt;
        let cursor_after = *cursor + buf.read_u8(cursor)? as usize;

        unsafe {
            match rt & 0x3f {
                action::TEE | action::WATCH | action::REDIRECT => {
                    r.v.forward.address = buf.read_u64(cursor)?;
                    r.v.forward.flags = buf.read_u32(cursor)?;
                    r.v.forward.length = buf.read_u16(cursor)?;
                }
                action::PRIORITY => {
                    r.v.qos_bucket = buf.read_u8(cursor)?;
                }
                match_cond::SOURCE_ZEROTIER_ADDRESS | match_cond::DEST_ZEROTIER_ADDRESS => {
                    let a = buf.read_bytes_fixed::<{ protocol::ADDRESS_SIZE }>(cursor)?;
                    r.v.zt = a[0].wrapping_shl(32) as u64
                        | a[1].wrapping_shl(24) as u64
                        | a[2].wrapping_shl(16) as u64
                        | a[3].wrapping_shl(8) as u64
                        | a[4] as u64;
                }
                match_cond::VLAN_ID => {
                    r.v.vlan_id = buf.read_u16(cursor)?;
                }
                match_cond::VLAN_PCP => {
                    r.v.vlan_pcp = buf.read_u8(cursor)?;
                }
                match_cond::VLAN_DEI => {
                    r.v.vlan_dei = buf.read_u8(cursor)?;
                }
                match_cond::MAC_SOURCE | match_cond::MAC_DEST => {
                    r.v.mac = *buf.read_bytes_fixed(cursor)?;
                }
                match_cond::IPV4_SOURCE | match_cond::IPV4_DEST => {
                    r.v.ipv4.ip = *buf.read_bytes_fixed(cursor)?;
                    r.v.ipv4.mask = buf.read_u8(cursor)?;
                }
                match_cond::IPV6_SOURCE | match_cond::IPV6_DEST => {
                    r.v.ipv6.ip = *buf.read_bytes_fixed(cursor)?;
                    r.v.ipv6.mask = buf.read_u8(cursor)?;
                }
                match_cond::IP_TOS => {
                    r.v.ip_tos.mask = buf.read_u8(cursor)?;
                    r.v.ip_tos.value = *buf.read_bytes_fixed(cursor)?;
                }
                match_cond::IP_PROTOCOL => {
                    r.v.ip_protocol = buf.read_u8(cursor)?;
                }
                match_cond::ETHERTYPE => {
                    r.v.ethertype = buf.read_u16(cursor)?;
                }
                match_cond::ICMP => {
                    r.v.icmp._type = buf.read_u8(cursor)?;
                    r.v.icmp.code = buf.read_u8(cursor)?;
                    r.v.icmp.flags = buf.read_u8(cursor)?;
                }
                match_cond::IP_SOURCE_PORT_RANGE | match_cond::IP_DEST_PORT_RANGE => {
                    r.v.port_range[0] = buf.read_u16(cursor)?;
                    r.v.port_range[1] = buf.read_u16(cursor)?;
                }
                match_cond::CHARACTERISTICS => {
                    r.v.characteristics = buf.read_u64(cursor)?;
                }
                match_cond::FRAME_SIZE_RANGE => {
                    r.v.frame_size_range[0] = buf.read_u16(cursor)?;
                    r.v.frame_size_range[1] = buf.read_u16(cursor)?;
                }
                match_cond::RANDOM => {
                    r.v.random_probability = buf.read_u32(cursor)?;
                }
                match_cond::TAGS_DIFFERENCE
                | match_cond::TAGS_BITWISE_AND
                | match_cond::TAGS_BITWISE_OR
                | match_cond::TAGS_BITWISE_XOR
                | match_cond::TAGS_EQUAL
                | match_cond::TAG_SENDER
                | match_cond::TAG_RECEIVER => {
                    r.v.tag.id = buf.read_u32(cursor)?;
                    r.v.tag.value = buf.read_u32(cursor)?;
                }
                match_cond::INTEGER_RANGE => {
                    r.v.int_range.start = buf.read_u64(cursor)?;
                    r.v.int_range.end = buf.read_u64(cursor)?;
                    r.v.int_range.idx = buf.read_u16(cursor)?;
                    r.v.int_range.format = buf.read_u8(cursor)?;
                }
                _ => {
                    // Skip unrecognized types and return as DROP
                    r.t = action::DROP;
                    *cursor = cursor_after;
                }
            }
        }

        if *cursor != cursor_after {
            return Err(UnmarshalError::InvalidData);
        }

        return Ok(r);
    }
}

impl PartialEq for Rule {
    fn eq(&self, other: &Self) -> bool {
        unsafe { (*(self as *const Self).cast::<[u8; size_of::<Self>()]>()).eq(&*(other as *const Self).cast::<[u8; size_of::<Self>()]>()) }
    }
}

impl Eq for Rule {}

impl Serialize for Rule {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        if serializer.is_human_readable() {
            HumanReadableRule::from_rule(self).serialize(serializer)
        } else {
            serializer.serialize_bytes(self.to_buffer::<{ Rule::MAX_MARSHAL_SIZE }>().unwrap().as_bytes())
        }
    }
}

struct SerdeRuleVisitor;

impl<'de> serde::de::Visitor<'de> for SerdeRuleVisitor {
    type Value = Rule;

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("network rule")
    }

    fn visit_bytes<E>(self, v: &[u8]) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        let mut tmp: Buffer<1024> = Buffer::new();
        if tmp.append_bytes(v).is_err() {
            return Err(E::custom("invalid rule"));
        }
        let mut cursor = 0;
        if let Ok(r) = Rule::unmarshal(&tmp, &mut cursor) {
            return Ok(r);
        } else {
            return Err(E::custom("invalid rule"));
        }
    }
}

impl<'de> Deserialize<'de> for Rule {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        if deserializer.is_human_readable() {
            if let Some(r) = HumanReadableRule::deserialize(deserializer)?.to_rule() {
                Ok(r)
            } else {
                Ok(Rule::default()) // interpret invalids as DROP
            }
        } else {
            deserializer.deserialize_bytes(SerdeRuleVisitor)
        }
    }
}

// Compile time generated perfect hash for O(1) lookup of types from human readable names.
static HR_NAME_TO_RULE_TYPE: phf::Map<&'static str, u8> = phf_map! {
    "ACTION_DROP" => action::DROP,
    "ACTION_ACCEPT" => action::ACCEPT,
    "ACTION_TEE" => action::TEE,
    "ACTION_WATCH" => action::WATCH,
    "ACTION_REDIRECT" => action::REDIRECT,
    "ACTION_BREAK" => action::BREAK,
    "ACTION_PRIORITY" => action::PRIORITY,
    "MATCH_SOURCE_ZEROTIER_ADDRESS" => match_cond::SOURCE_ZEROTIER_ADDRESS,
    "MATCH_DEST_ZEROTIER_ADDRESS" => match_cond::DEST_ZEROTIER_ADDRESS,
    "MATCH_VLAN_ID" => match_cond::VLAN_ID,
    "MATCH_VLAN_PCP" => match_cond::VLAN_PCP,
    "MATCH_VLAN_DEI" => match_cond::VLAN_DEI,
    "MATCH_MAC_SOURCE" => match_cond::MAC_SOURCE,
    "MATCH_MAC_DEST" => match_cond::MAC_DEST,
    "MATCH_IPV4_SOURCE" => match_cond::IPV4_SOURCE,
    "MATCH_IPV4_DEST" => match_cond::IPV4_DEST,
    "MATCH_IPV6_SOURCE" => match_cond::IPV6_SOURCE,
    "MATCH_IPV6_DEST" => match_cond::IPV6_DEST,
    "MATCH_IP_TOS" => match_cond::IP_TOS,
    "MATCH_IP_PROTOCOL" => match_cond::IP_PROTOCOL,
    "MATCH_ETHERTYPE" => match_cond::ETHERTYPE,
    "MATCH_ICMP" => match_cond::ICMP,
    "MATCH_IP_SOURCE_PORT_RANGE" => match_cond::IP_SOURCE_PORT_RANGE,
    "MATCH_IP_DEST_PORT_RANGE" => match_cond::IP_DEST_PORT_RANGE,
    "MATCH_CHARACTERISTICS" => match_cond::CHARACTERISTICS,
    "MATCH_FRAME_SIZE_RANGE" => match_cond::FRAME_SIZE_RANGE,
    "MATCH_RANDOM" => match_cond::RANDOM,
    "MATCH_TAGS_DIFFERENCE" => match_cond::TAGS_DIFFERENCE,
    "MATCH_TAGS_BITWISE_AND" => match_cond::TAGS_BITWISE_AND,
    "MATCH_TAGS_BITWISE_OR" => match_cond::TAGS_BITWISE_OR,
    "MATCH_TAGS_BITWISE_XOR" => match_cond::TAGS_BITWISE_XOR,
    "MATCH_TAGS_EQUAL" => match_cond::TAGS_EQUAL,
    "MATCH_TAG_SENDER" => match_cond::TAG_SENDER,
    "MATCH_TAG_RECEIVER" => match_cond::TAG_RECEIVER,
    "INTEGER_RANGE" => match_cond::INTEGER_RANGE,
};

/// A "bag of fields" used to serialize/deserialize rules in human readable form e.g. JSON.
/// Fields not populated should be omitted.
#[allow(non_snake_case)]
#[derive(Default, Serialize, Deserialize)]
struct HumanReadableRule<'a> {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub address: Option<Address>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub flags: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub length: Option<u16>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub zt: Option<Address>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub vlanId: Option<u16>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub vlanPcp: Option<u8>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub vlanDei: Option<u8>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub mac: Option<MAC>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub ip: Option<InetAddress>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub start: Option<u64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub end: Option<u64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub mask: Option<u64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub ipProtocol: Option<u8>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub etherType: Option<u16>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub icmpType: Option<u8>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub icmpCode: Option<u8>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub probability: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub id: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub value: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub idx: Option<u16>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub little: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub bits: Option<u8>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub qosBucket: Option<u8>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub not: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub or: Option<bool>,

    #[serde(rename = "type")]
    pub _type: &'a str,
}

impl<'a> HumanReadableRule<'a> {
    fn to_rule(&self) -> Option<Rule> {
        if let Some(t) = HR_NAME_TO_RULE_TYPE.get(self._type) {
            let mut r = Rule::default();
            r.t =
                *t | if self.not.unwrap_or(false) {
                    0x80
                } else {
                    0
                } | if self.or.unwrap_or(false) {
                    0x40
                } else {
                    0
                };
            unsafe {
                match *t {
                    action::TEE | action::WATCH | action::REDIRECT => {
                        r.v.forward.address = self.address?.into();
                        r.v.forward.flags = self.flags?;
                        r.v.forward.length = self.length?;
                    }
                    action::PRIORITY => {
                        r.v.qos_bucket = self.qosBucket?;
                    }
                    match_cond::SOURCE_ZEROTIER_ADDRESS | match_cond::DEST_ZEROTIER_ADDRESS => {
                        r.v.zt = self.address?.into();
                    }
                    match_cond::VLAN_ID => {
                        r.v.vlan_id = self.vlanId?;
                    }
                    match_cond::VLAN_PCP => {
                        r.v.vlan_pcp = self.vlanPcp?;
                    }
                    match_cond::VLAN_DEI => {
                        r.v.vlan_dei = self.vlanDei?;
                    }
                    match_cond::MAC_SOURCE | match_cond::MAC_DEST => {
                        r.v.mac = self.mac?.to_bytes();
                    }
                    match_cond::IPV4_SOURCE | match_cond::IPV4_DEST => {
                        if let Some(ip) = self.ip.as_ref() {
                            if ip.is_ipv4() {
                                r.v.ipv4.ip = ip.ip_bytes().try_into().unwrap();
                                r.v.ipv4.mask = self.mask? as u8;
                            } else {
                                return None;
                            }
                        } else {
                            return None;
                        }
                    }
                    match_cond::IPV6_SOURCE | match_cond::IPV6_DEST => {
                        if let Some(ip) = self.ip.as_ref() {
                            if ip.is_ipv6() {
                                r.v.ipv4.ip = ip.ip_bytes().try_into().unwrap();
                                r.v.ipv4.mask = self.mask? as u8;
                            } else {
                                return None;
                            }
                        } else {
                            return None;
                        }
                    }
                    match_cond::IP_TOS => {
                        r.v.ip_tos.mask = self.mask? as u8;
                        r.v.ip_tos.value[0] = self.start? as u8;
                        r.v.ip_tos.value[1] = self.end? as u8;
                    }
                    match_cond::IP_PROTOCOL => {
                        r.v.ip_protocol = self.ipProtocol?;
                    }
                    match_cond::ETHERTYPE => {
                        r.v.ethertype = self.etherType?;
                    }
                    match_cond::ICMP => {
                        r.v.icmp._type = self.icmpType?;
                        r.v.icmp.code = self.icmpCode.unwrap_or(0);
                    }
                    match_cond::IP_SOURCE_PORT_RANGE | match_cond::IP_DEST_PORT_RANGE => {
                        r.v.port_range[0] = self.start? as u16;
                        r.v.port_range[1] = self.end? as u16;
                    }
                    match_cond::CHARACTERISTICS => {
                        r.v.characteristics = self.mask?;
                    }
                    match_cond::FRAME_SIZE_RANGE => {
                        r.v.frame_size_range[0] = self.start? as u16;
                        r.v.frame_size_range[1] = self.end? as u16;
                    }
                    match_cond::RANDOM => {
                        r.v.random_probability = self.probability?;
                    }
                    match_cond::TAGS_DIFFERENCE
                    | match_cond::TAGS_BITWISE_AND
                    | match_cond::TAGS_BITWISE_OR
                    | match_cond::TAGS_BITWISE_XOR
                    | match_cond::TAGS_EQUAL
                    | match_cond::TAG_SENDER
                    | match_cond::TAG_RECEIVER => {
                        r.v.tag.id = self.id?;
                        r.v.tag.value = self.value?;
                    }
                    match_cond::INTEGER_RANGE => {
                        r.v.int_range.start = self.start?;
                        r.v.int_range.end = self.end?;
                        r.v.int_range.idx = self.idx?;
                        r.v.int_range.format = if self.little.unwrap_or(false) {
                            0x80
                        } else {
                            0
                        } | ((self.bits.unwrap_or(1) - 1) & 63);
                    }
                    _ => {}
                }
            }
            todo!()
        }
        return None;
    }

    fn from_rule(r: &Rule) -> Self {
        let mut hr = HumanReadableRule::default();
        r.visit(MakeHumanReadable(&mut hr));
        hr
    }
}

struct MakeHumanReadable<'a>(&'a mut HumanReadableRule<'static>);

impl<'a> MakeHumanReadable<'a> {
    fn do_tag(self, t: &'static str, not: bool, or: bool, id: u32, value: u32) {
        let _ = self.0.id.insert(id);
        let _ = self.0.value.insert(value);
        self.do_cond(t, not, or);
    }

    fn do_cond(self, t: &'static str, not: bool, or: bool) {
        self.0._type = t;
        if not {
            let _ = self.0.not.insert(not);
        }
        if or {
            let _ = self.0.or.insert(or);
        }
    }
}

impl<'a> RuleVisitor for MakeHumanReadable<'a> {
    #[inline(always)]
    fn action_drop(self) -> bool {
        self.0._type = "ACTION_DROP";
        true
    }

    #[inline(always)]
    fn action_accept(self) -> bool {
        self.0._type = "ACTION_ACCEPT";
        true
    }

    #[inline(always)]
    fn action_tee(self, address: Address, flags: u32, length: u16) -> bool {
        self.0._type = "ACTION_TEE";
        let _ = self.0.address.insert(address);
        let _ = self.0.flags.insert(flags);
        let _ = self.0.length.insert(length);
        true
    }

    #[inline(always)]
    fn action_watch(self, address: Address, flags: u32, length: u16) -> bool {
        self.0._type = "ACTION_WATCH";
        let _ = self.0.address.insert(address);
        let _ = self.0.flags.insert(flags);
        let _ = self.0.length.insert(length);
        true
    }

    #[inline(always)]
    fn action_redirect(self, address: Address, flags: u32, length: u16) -> bool {
        self.0._type = "ACTION_REDIRECT";
        let _ = self.0.address.insert(address);
        let _ = self.0.flags.insert(flags);
        let _ = self.0.length.insert(length);
        true
    }

    #[inline(always)]
    fn action_break(self) -> bool {
        self.0._type = "ACTION_BREAK";
        true
    }

    #[inline(always)]
    fn action_priority(self, qos_bucket: u8) -> bool {
        self.0._type = "ACTION_PRIORITY";
        let _ = self.0.qosBucket.insert(qos_bucket);
        true
    }

    #[inline(always)]
    fn invalid_rule(self) -> bool {
        false
    }

    #[inline(always)]
    fn match_source_zerotier_address(self, not: bool, or: bool, address: Address) {
        let _ = self.0.zt.insert(address);
        self.do_cond("MATCH_SOURCE_ZEROTIER_ADDRESS", not, or);
    }

    #[inline(always)]
    fn match_dest_zerotier_address(self, not: bool, or: bool, address: Address) {
        let _ = self.0.zt.insert(address);
        self.do_cond("MATCH_DEST_ZEROTIER_ADDRESS", not, or);
    }

    #[inline(always)]
    fn match_vlan_id(self, not: bool, or: bool, id: u16) {
        let _ = self.0.vlanId.insert(id);
        self.do_cond("MATCH_VLAN_ID", not, or);
    }

    #[inline(always)]
    fn match_vlan_pcp(self, not: bool, or: bool, pcp: u8) {
        let _ = self.0.vlanPcp.insert(pcp);
        self.do_cond("MATCH_VLAN_PCP", not, or);
    }

    #[inline(always)]
    fn match_vlan_dei(self, not: bool, or: bool, dei: u8) {
        let _ = self.0.vlanDei.insert(dei);
        self.do_cond("MATCH_VLAN_DEI", not, or);
    }

    #[inline(always)]
    fn match_mac_source(self, not: bool, or: bool, mac: MAC) {
        let _ = self.0.mac.insert(mac);
        self.do_cond("MATCH_MAC_SOURCE", not, or);
    }

    #[inline(always)]
    fn match_mac_dest(self, not: bool, or: bool, mac: MAC) {
        let _ = self.0.mac.insert(mac);
        self.do_cond("MATCH_MAC_DEST", not, or);
    }

    #[inline(always)]
    fn match_ipv4_source(self, not: bool, or: bool, ip: &[u8; 4], mask: u8) {
        let _ = self.0.ip.insert(InetAddress::from_ip_port(ip, mask as u16));
        self.do_cond("MATCH_IPV4_SOURCE", not, or);
    }

    #[inline(always)]
    fn match_ipv4_dest(self, not: bool, or: bool, ip: &[u8; 4], mask: u8) {
        let _ = self.0.ip.insert(InetAddress::from_ip_port(ip, mask as u16));
        self.do_cond("MATCH_IPV4_DEST", not, or);
    }

    #[inline(always)]
    fn match_ipv6_source(self, not: bool, or: bool, ip: &[u8; 16], mask: u8) {
        let _ = self.0.ip.insert(InetAddress::from_ip_port(ip, mask as u16));
        self.do_cond("MATCH_IPV6_SOURCE", not, or);
    }

    #[inline(always)]
    fn match_ipv6_dest(self, not: bool, or: bool, ip: &[u8; 16], mask: u8) {
        let _ = self.0.ip.insert(InetAddress::from_ip_port(ip, mask as u16));
        self.do_cond("MATCH_IPV6_DEST", not, or);
    }

    #[inline(always)]
    fn match_ip_tos(self, not: bool, or: bool, mask: u8, start: u8, end: u8) {
        let _ = self.0.mask.insert(mask as u64);
        let _ = self.0.start.insert(start as u64);
        let _ = self.0.end.insert(end as u64);
        self.do_cond("MATCH_IP_TOS", not, or);
    }

    #[inline(always)]
    fn match_ip_protocol(self, not: bool, or: bool, protocol: u8) {
        let _ = self.0.ipProtocol.insert(protocol);
        self.do_cond("MATCH_IP_PROTOCOL", not, or);
    }

    #[inline(always)]
    fn match_ethertype(self, not: bool, or: bool, ethertype: u16) {
        let _ = self.0.etherType.insert(ethertype);
        self.do_cond("MATCH_ETHERTYPE", not, or);
    }

    #[inline(always)]
    fn match_icmp(self, not: bool, or: bool, _type: u8, code: u8, flags: u8) {
        let _ = self.0.icmpType.insert(_type);
        if (flags & 0x01) != 0 {
            let _ = self.0.icmpCode.insert(code);
        }
        self.do_cond("MATCH_ICMP", not, or);
    }

    #[inline(always)]
    fn match_ip_source_port_range(self, not: bool, or: bool, start: u16, end: u16) {
        let _ = self.0.start.insert(start as u64);
        let _ = self.0.end.insert(end as u64);
        self.do_cond("MATCH_IP_SOURCE_PORT_RANGE", not, or);
    }

    #[inline(always)]
    fn match_ip_dest_port_range(self, not: bool, or: bool, start: u16, end: u16) {
        let _ = self.0.start.insert(start as u64);
        let _ = self.0.end.insert(end as u64);
        self.do_cond("MATCH_IP_DEST_PORT_RANGE", not, or);
    }

    #[inline(always)]
    fn match_characteristics(self, not: bool, or: bool, characteristics: u64) {
        let _ = self.0.mask.insert(characteristics);
        self.do_cond("MATCH_CHARACTERISTICS", not, or);
    }

    #[inline(always)]
    fn match_frame_size_range(self, not: bool, or: bool, start: u16, end: u16) {
        let _ = self.0.start.insert(start as u64);
        let _ = self.0.end.insert(end as u64);
        self.do_cond("MATCH_FRAME_SIZE_RANGE", not, or);
    }

    #[inline(always)]
    fn match_random(self, not: bool, or: bool, probability: u32) {
        let _ = self.0.probability.insert(probability);
        self.do_cond("MATCH_RANDOM", not, or);
    }

    #[inline(always)]
    fn match_tags_difference(self, not: bool, or: bool, id: u32, value: u32) {
        self.do_tag("MATCH_TAGS_DIFFERENCE", not, or, id, value);
    }

    #[inline(always)]
    fn match_tags_bitwise_and(self, not: bool, or: bool, id: u32, value: u32) {
        self.do_tag("MATCH_TAGS_BITWISE_AND", not, or, id, value);
    }

    #[inline(always)]
    fn match_tags_bitwise_or(self, not: bool, or: bool, id: u32, value: u32) {
        self.do_tag("MATCH_TAGS_BITWISE_OR", not, or, id, value);
    }

    #[inline(always)]
    fn match_tags_bitwise_xor(self, not: bool, or: bool, id: u32, value: u32) {
        self.do_tag("MATCH_TAGS_BITWISE_XOR", not, or, id, value);
    }

    #[inline(always)]
    fn match_tags_equal(self, not: bool, or: bool, id: u32, value: u32) {
        self.do_tag("MATCH_TAGS_EQUAL", not, or, id, value);
    }

    #[inline(always)]
    fn match_tag_sender(self, not: bool, or: bool, id: u32, value: u32) {
        self.do_tag("MATCH_TAG_SENDER", not, or, id, value);
    }

    #[inline(always)]
    fn match_tag_receiver(self, not: bool, or: bool, id: u32, value: u32) {
        self.do_tag("MATCH_TAG_RECEIVER", not, or, id, value);
    }

    #[inline(always)]
    fn match_integer_range(self, not: bool, or: bool, start: u64, end: u64, idx: u16, format: u8) {
        let _ = self.0.start.insert(start);
        let _ = self.0.end.insert(end);
        let _ = self.0.idx.insert(idx);
        let _ = self.0.little.insert((format & 0x80) != 0);
        let _ = self.0.bits.insert((format & 63) + 1);
        self.do_cond("INTEGER_RANGE", not, or);
    }
}
