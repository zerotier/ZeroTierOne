/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::cmp::Ordering;
use std::hash::{Hash, Hasher};

use zerotier_core_crypto::hash::SHA384_HASH_SIZE;

use crate::vl1::{Address, MAC};
use crate::vl1::inetaddress::InetAddress;
use crate::util::buffer::Buffer;

pub const TYPE_NIL: u8 = 0;
pub const TYPE_ZEROTIER: u8 = 1;
pub const TYPE_ETHERNET: u8 = 2;
pub const TYPE_WIFIDIRECT: u8 = 3;
pub const TYPE_BLUETOOTH: u8 = 4;
pub const TYPE_IP: u8 = 5;
pub const TYPE_IPUDP: u8 = 6;
pub const TYPE_IPTCP: u8 = 7;
pub const TYPE_HTTP: u8 = 8;
pub const TYPE_WEBRTC: u8 = 9;
pub const TYPE_ZEROTIER_ENCAP: u8 = 10;

/// A communication endpoint on the network where a ZeroTier node can be reached.
///
/// Currently only a few of these are supported. The rest are reserved for future use.
#[derive(Clone, PartialEq, Eq)]
pub enum Endpoint {
    /// A null endpoint.
    Nil,

    /// Via another node using unencapsulated relaying (e.g. via a root)
    /// Hash is a full hash of the identity for strong verification.
    ZeroTier(Address, [u8; SHA384_HASH_SIZE]),

    /// Direct L2 Ethernet
    Ethernet(MAC),

    /// Direct L2 Ethernet over WiFi-Direct (P2P WiFi)
    WifiDirect(MAC),

    /// Local bluetooth
    Bluetooth(MAC),

    /// Raw IP without a UDP or other header
    Ip(InetAddress),

    /// Raw UDP, the default and usually preferred transport mode
    IpUdp(InetAddress),

    /// Raw TCP with each packet prefixed by a varint size
    IpTcp(InetAddress),

    /// HTTP streaming
    Http(String),

    /// WebRTC data channel
    WebRTC(Vec<u8>),

    /// Via another node using inner encapsulation via VERB_ENCAP.
    /// Hash is a full hash of the identity for strong verification.
    ZeroTierEncap(Address, [u8; SHA384_HASH_SIZE]),
}

impl Default for Endpoint {
    #[inline(always)]
    fn default() -> Endpoint { Endpoint::Nil }
}

impl Endpoint {
    /// Get the IP address (and port if applicable) if this is an IP-based transport.
    #[inline(always)]
    pub fn ip(&self) -> Option<(&InetAddress, u8)> {
        match self {
            Endpoint::Ip(ip) => Some((&ip, TYPE_IP)),
            Endpoint::IpUdp(ip) => Some((&ip, TYPE_IPUDP)),
            Endpoint::IpTcp(ip) => Some((&ip, TYPE_IPTCP)),
            _ => None
        }
    }

    pub fn type_id(&self) -> u8 {
        match self {
            Endpoint::Nil => TYPE_NIL,
            Endpoint::ZeroTier(_, _) => TYPE_ZEROTIER,
            Endpoint::Ethernet(_) => TYPE_ETHERNET,
            Endpoint::WifiDirect(_) => TYPE_WIFIDIRECT,
            Endpoint::Bluetooth(_) => TYPE_BLUETOOTH,
            Endpoint::Ip(_) => TYPE_IP,
            Endpoint::IpUdp(_) => TYPE_IPUDP,
            Endpoint::IpTcp(_) => TYPE_IPTCP,
            Endpoint::Http(_) => TYPE_HTTP,
            Endpoint::WebRTC(_) => TYPE_WEBRTC,
            Endpoint::ZeroTierEncap(_, _) => TYPE_ZEROTIER_ENCAP,
        }
    }

    #[inline(always)]
    pub fn is_nil(&self) -> bool { matches!(self, Endpoint::Nil) }

    pub fn marshal<const BL: usize>(&self, buf: &mut Buffer<BL>) -> std::io::Result<()> {
        match self {
            Endpoint::Nil => {
                buf.append_u8(TYPE_NIL)
            }
            Endpoint::ZeroTier(a, h) => {
                buf.append_u8(16 + TYPE_ZEROTIER)?;
                buf.append_bytes_fixed(&a.to_bytes())?;
                buf.append_bytes_fixed(h)
            }
            Endpoint::Ethernet(m) => {
                buf.append_u8(16 + TYPE_ETHERNET)?;
                buf.append_bytes_fixed(&m.to_bytes())
            }
            Endpoint::WifiDirect(m) => {
                buf.append_u8(16 + TYPE_WIFIDIRECT)?;
                buf.append_bytes_fixed(&m.to_bytes())
            }
            Endpoint::Bluetooth(m) => {
                buf.append_u8(16 + TYPE_BLUETOOTH)?;
                buf.append_bytes_fixed(&m.to_bytes())
            }
            Endpoint::Ip(ip) => {
                buf.append_u8(16 + TYPE_IP)?;
                ip.marshal(buf)
            }
            Endpoint::IpUdp(ip) => {
                // Wire encoding of IP/UDP type endpoints is the same as naked InetAddress
                // objects for backward compatibility. That way a naked InetAddress unmarshals
                // here as an IP/UDP Endpoint and vice versa. Supporting this is why 16 is added
                // to all Endpoint type IDs for wire encoding so that values of 4 or 6 can be
                // interpreted as IP/UDP InetAddress.
                ip.marshal(buf)
            }
            Endpoint::IpTcp(ip) => {
                buf.append_u8(16 + TYPE_IPTCP)?;
                ip.marshal(buf)
            }
            Endpoint::Http(url) => {
                buf.append_u8(16 + TYPE_HTTP)?;
                let b = url.as_bytes();
                buf.append_varint(b.len() as u64)?;
                buf.append_bytes(b)
            }
            Endpoint::WebRTC(offer) => {
                buf.append_u8(16 + TYPE_WEBRTC)?;
                let b = offer.as_slice();
                buf.append_varint(b.len() as u64)?;
                buf.append_bytes(b)
            }
            Endpoint::ZeroTierEncap(a, h) => {
                buf.append_u8(16 + TYPE_ZEROTIER_ENCAP)?;
                buf.append_bytes_fixed(&a.to_bytes())?;
                buf.append_bytes_fixed(h)
            }
        }
    }

    pub fn unmarshal<const BL: usize>(buf: &Buffer<BL>, cursor: &mut usize) -> std::io::Result<Endpoint> {
        let type_byte = buf.read_u8(cursor)?;
        if type_byte < 16 {
            if type_byte == 4 {
                let b: &[u8; 6] = buf.read_bytes_fixed(cursor)?;
                Ok(Endpoint::IpUdp(InetAddress::from_ip_port(&b[0..4], u16::from_be_bytes(b[4..6].try_into().unwrap()))))
            } else if type_byte == 6 {
                let b: &[u8; 18] = buf.read_bytes_fixed(cursor)?;
                Ok(Endpoint::IpUdp(InetAddress::from_ip_port(&b[0..16], u16::from_be_bytes(b[16..18].try_into().unwrap()))))
            } else {
                Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "unrecognized endpoint type in stream"))
            }
        } else {
            let read_mac = |buf: &Buffer<BL>, cursor: &mut usize| {
                let m = MAC::unmarshal(buf, cursor)?;
                if m.is_some() {
                    Ok(m.unwrap())
                } else {
                    Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid MAC address"))
                }
            };

            match type_byte - 16 {
                TYPE_NIL => Ok(Endpoint::Nil),
                TYPE_ZEROTIER => {
                    let zt = Address::unmarshal(buf, cursor)?;
                    if zt.is_some() {
                        let h = buf.read_bytes_fixed::<SHA384_HASH_SIZE>(cursor)?;
                        Ok(Endpoint::ZeroTier(zt.unwrap(), h.clone()))
                    } else {
                        Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid ZeroTier address"))
                    }
                },
                TYPE_ETHERNET => Ok(Endpoint::Ethernet(read_mac(buf, cursor)?)),
                TYPE_WIFIDIRECT => Ok(Endpoint::WifiDirect(read_mac(buf, cursor)?)),
                TYPE_BLUETOOTH => Ok(Endpoint::Bluetooth(read_mac(buf, cursor)?)),
                TYPE_IP => Ok(Endpoint::Ip(InetAddress::unmarshal(buf, cursor)?)),
                TYPE_IPUDP => Ok(Endpoint::IpUdp(InetAddress::unmarshal(buf, cursor)?)),
                TYPE_IPTCP => Ok(Endpoint::IpTcp(InetAddress::unmarshal(buf, cursor)?)),
                TYPE_HTTP => Ok(Endpoint::Http(String::from_utf8_lossy(buf.read_bytes(buf.read_varint(cursor)? as usize, cursor)?).to_string())),
                TYPE_WEBRTC => Ok(Endpoint::WebRTC(buf.read_bytes(buf.read_varint(cursor)? as usize, cursor)?.to_vec())),
                TYPE_ZEROTIER_ENCAP => {
                    let zt = Address::unmarshal(buf, cursor)?;
                    if zt.is_some() {
                        let h = buf.read_bytes_fixed::<SHA384_HASH_SIZE>(cursor)?;
                        Ok(Endpoint::ZeroTierEncap(zt.unwrap(), h.clone()))
                    } else {
                        Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid ZeroTier address"))
                    }
                },
                _ => Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "unrecognized endpoint type in stream"))
            }
        }
    }
}

impl Hash for Endpoint {
    fn hash<H: Hasher>(&self, state: &mut H) {
        match self {
            Endpoint::Nil => {
                state.write_u8(TYPE_NIL);
            }
            Endpoint::ZeroTier(a, _) => {
                state.write_u8(TYPE_ZEROTIER);
                state.write_u64(a.to_u64())
            }
            Endpoint::Ethernet(m) => {
                state.write_u8(TYPE_ETHERNET);
                state.write_u64(m.to_u64())
            }
            Endpoint::WifiDirect(m) => {
                state.write_u8(TYPE_WIFIDIRECT);
                state.write_u64(m.to_u64())
            }
            Endpoint::Bluetooth(m) => {
                state.write_u8(TYPE_BLUETOOTH);
                state.write_u64(m.to_u64())
            }
            Endpoint::Ip(ip) => {
                state.write_u8(TYPE_IP);
                ip.hash(state);
            }
            Endpoint::IpUdp(ip) => {
                state.write_u8(TYPE_IPUDP);
                ip.hash(state);
            }
            Endpoint::IpTcp(ip) => {
                state.write_u8(TYPE_IPTCP);
                ip.hash(state);
            }
            Endpoint::Http(url) => {
                state.write_u8(TYPE_HTTP);
                url.hash(state);
            }
            Endpoint::WebRTC(offer) => {
                state.write_u8(TYPE_WEBRTC);
                offer.hash(state);
            }
            Endpoint::ZeroTierEncap(a, _) => {
                state.write_u8(TYPE_ZEROTIER_ENCAP);
                state.write_u64(a.to_u64())
            }
        }
    }
}

impl Ord for Endpoint {
    fn cmp(&self, other: &Self) -> Ordering {
        // Manually implement Ord to ensure that sort order is known and consistent.
        match (self, other) {
            (Endpoint::Nil, Endpoint::Nil) => Ordering::Equal,
            (Endpoint::ZeroTier(a, ah), Endpoint::ZeroTier(b, bh)) => a.cmp(b).then_with(|| ah.cmp(bh)),
            (Endpoint::Ethernet(a), Endpoint::Ethernet(b)) => a.cmp(b),
            (Endpoint::WifiDirect(a), Endpoint::WifiDirect(b)) => a.cmp(b),
            (Endpoint::Bluetooth(a), Endpoint::Bluetooth(b)) => a.cmp(b),
            (Endpoint::Ip(a), Endpoint::Ip(b)) => a.cmp(b),
            (Endpoint::IpUdp(a), Endpoint::IpUdp(b)) => a.cmp(b),
            (Endpoint::IpTcp(a), Endpoint::IpTcp(b)) => a.cmp(b),
            (Endpoint::Http(a), Endpoint::Http(b)) => a.cmp(b),
            (Endpoint::WebRTC(a), Endpoint::WebRTC(b)) => a.cmp(b),
            (Endpoint::ZeroTierEncap(a, ah), Endpoint::ZeroTierEncap(b, bh)) => a.cmp(b).then_with(|| ah.cmp(bh)),
            _ => self.type_id().cmp(&other.type_id())
        }
    }
}

impl PartialOrd for Endpoint {
    #[inline(always)]
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> { Some(self.cmp(other)) }
}

impl ToString for Endpoint {
    fn to_string(&self) -> String {
        match self {
            Endpoint::Nil => format!("nil"),
            Endpoint::ZeroTier(a, ah) => format!("zt:{}-{}", a.to_string(), base64::encode_config(ah, base64::URL_SAFE_NO_PAD)),
            Endpoint::Ethernet(m) => format!("eth:{}", m.to_string()),
            Endpoint::WifiDirect(m) => format!("wifip2p:{}", m.to_string()),
            Endpoint::Bluetooth(m) => format!("bt:{}", m.to_string()),
            Endpoint::Ip(ip) => format!("ip:{}", ip.to_ip_string()),
            Endpoint::IpUdp(ip) => format!("udp:{}", ip.to_string()),
            Endpoint::IpTcp(ip) => format!("tcp:{}", ip.to_string()),
            Endpoint::Http(url) => url.clone(),
            Endpoint::WebRTC(offer) => format!("webrtc:{}", base64::encode_config(offer.as_slice(), base64::URL_SAFE_NO_PAD)),
            Endpoint::ZeroTierEncap(a, ah) => format!("ztzt:{}-{}", a.to_string(), base64::encode_config(ah, base64::URL_SAFE_NO_PAD)),
        }
    }
}
