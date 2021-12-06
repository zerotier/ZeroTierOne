/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::cmp::Ordering;
use std::hash::{Hash, Hasher};

use crate::vl1::{Address, MAC};
use crate::vl1::inetaddress::InetAddress;
use crate::util::buffer::Buffer;

const TYPE_NIL: u8 = 0;
const TYPE_ZEROTIER: u8 = 1;
const TYPE_ETHERNET: u8 = 2;
const TYPE_WIFIDIRECT: u8 = 3;
const TYPE_BLUETOOTH: u8 = 4;
const TYPE_IP: u8 = 5;
const TYPE_IPUDP: u8 = 6;
const TYPE_IPTCP: u8 = 7;
const TYPE_HTTP: u8 = 8;
const TYPE_WEBRTC: u8 = 9;

#[derive(Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum Type {
    Nil = TYPE_NIL,
    ZeroTier = TYPE_ZEROTIER,
    Ethernet = TYPE_ETHERNET,
    WifiDirect = TYPE_WIFIDIRECT,
    Bluetooth = TYPE_BLUETOOTH,
    Ip = TYPE_IP,
    IpUdp = TYPE_IPUDP,
    IpTcp = TYPE_IPTCP,
    Http = TYPE_HTTP,
    WebRTC = TYPE_WEBRTC,
}

#[derive(Clone, PartialEq, Eq)]
pub enum Endpoint {
    Nil,
    ZeroTier(Address),
    Ethernet(MAC),
    WifiDirect(MAC),
    Bluetooth(MAC),
    Ip(InetAddress),
    IpUdp(InetAddress),
    IpTcp(InetAddress),
    Http(String),
    WebRTC(Vec<u8>),
}

impl Default for Endpoint {
    #[inline(always)]
    fn default() -> Endpoint { Endpoint::Nil }
}

impl Endpoint {
    #[inline(always)]
    pub fn ep_type(&self) -> Type {
        match self {
            Endpoint::Nil => Type::Nil,
            Endpoint::ZeroTier(_) => Type::ZeroTier,
            Endpoint::Ethernet(_) => Type::Ethernet,
            Endpoint::WifiDirect(_) => Type::WifiDirect,
            Endpoint::Bluetooth(_) => Type::Bluetooth,
            Endpoint::Ip(_) => Type::Ip,
            Endpoint::IpUdp(_) => Type::IpUdp,
            Endpoint::IpTcp(_) => Type::IpTcp,
            Endpoint::Http(_) => Type::Http,
            Endpoint::WebRTC(_) => Type::WebRTC,
        }
    }

    #[inline(always)]
    pub fn ip(&self) -> Option<(&InetAddress, Type)> {
        match self {
            Endpoint::Ip(ip) => Some((&ip, Type::Ip)),
            Endpoint::IpUdp(ip) => Some((&ip, Type::IpUdp)),
            Endpoint::IpTcp(ip) => Some((&ip, Type::IpTcp)),
            _ => None
        }
    }

    pub fn marshal<const BL: usize>(&self, buf: &mut Buffer<BL>) -> std::io::Result<()> {
        match self {
            Endpoint::Nil => {
                buf.append_u8(TYPE_NIL)
            }
            Endpoint::ZeroTier(a) => {
                buf.append_u8(16 + TYPE_ZEROTIER)?;
                buf.append_bytes_fixed(&a.to_bytes())
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
        }
    }

    pub fn unmarshal<const BL: usize>(buf: &Buffer<BL>, cursor: &mut usize) -> std::io::Result<Endpoint> {
        let type_byte = buf.read_u8(cursor)?;
        if type_byte < 16 {
            if type_byte == 4 {
                let b: &[u8; 6] = buf.read_bytes_fixed(cursor)?;
                Ok(Endpoint::IpUdp(InetAddress::from_ip_port(&b[0..4], crate::util::load_u16_be(&b[4..6]))))
            } else if type_byte == 6 {
                let b: &[u8; 18] = buf.read_bytes_fixed(cursor)?;
                Ok(Endpoint::IpUdp(InetAddress::from_ip_port(&b[0..16], crate::util::load_u16_be(&b[16..18]))))
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
                        Ok(Endpoint::ZeroTier(zt.unwrap()))
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
            Endpoint::ZeroTier(a) => {
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
        }
    }
}

impl PartialOrd for Endpoint {
    #[inline(always)]
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> { Some(self.cmp(other)) }
}

// We manually implement Ord and PartialOrd to ensure that ordering is always the same, since I'm not
// sure if Rust's derivations for this are guaranteed to remain constant forever. Endpoint ordering
// is important in the reproducibility of digital signatures any time one is signing a vector of them.
impl Ord for Endpoint {
    fn cmp(&self, other: &Self) -> Ordering {
        match self {
            Endpoint::Nil => {
                match other {
                    Endpoint::Nil => Ordering::Equal,
                    _ => Ordering::Greater,
                }
            }
            Endpoint::ZeroTier(a) => {
                match other {
                    Endpoint::Nil => Ordering::Less,
                    Endpoint::ZeroTier(b) => a.cmp(b),
                    _ => Ordering::Greater,
                }
            }
            Endpoint::Ethernet(a) => {
                match other {
                    Endpoint::Nil => Ordering::Less,
                    Endpoint::ZeroTier(_) => Ordering::Less,
                    Endpoint::Ethernet(b) => a.cmp(b),
                    _ => Ordering::Greater,
                }
            }
            Endpoint::WifiDirect(a) => {
                match other {
                    Endpoint::Nil => Ordering::Less,
                    Endpoint::ZeroTier(_) => Ordering::Less,
                    Endpoint::Ethernet(_) => Ordering::Less,
                    Endpoint::WifiDirect(b) => a.cmp(b),
                    _ => Ordering::Greater,
                }
            }
            Endpoint::Bluetooth(a) => {
                match other {
                    Endpoint::Nil => Ordering::Less,
                    Endpoint::ZeroTier(_) => Ordering::Less,
                    Endpoint::Ethernet(_) => Ordering::Less,
                    Endpoint::WifiDirect(_) => Ordering::Less,
                    Endpoint::Bluetooth(b) => a.cmp(b),
                    _ => Ordering::Greater,
                }
            }
            Endpoint::Ip(a) => {
                match other {
                    Endpoint::Nil => Ordering::Less,
                    Endpoint::ZeroTier(_) => Ordering::Less,
                    Endpoint::Ethernet(_) => Ordering::Less,
                    Endpoint::WifiDirect(_) => Ordering::Less,
                    Endpoint::Bluetooth(_) => Ordering::Less,
                    Endpoint::Ip(b) => a.cmp(b),
                    _ => Ordering::Greater,
                }
            }
            Endpoint::IpUdp(a) => {
                match other {
                    Endpoint::Nil => Ordering::Less,
                    Endpoint::ZeroTier(_) => Ordering::Less,
                    Endpoint::Ethernet(_) => Ordering::Less,
                    Endpoint::WifiDirect(_) => Ordering::Less,
                    Endpoint::Bluetooth(_) => Ordering::Less,
                    Endpoint::Ip(_) => Ordering::Less,
                    Endpoint::IpUdp(b) => a.cmp(b),
                    _ => Ordering::Greater,
                }
            }
            Endpoint::IpTcp(a) => {
                match other {
                    Endpoint::Nil => Ordering::Less,
                    Endpoint::ZeroTier(_) => Ordering::Less,
                    Endpoint::Ethernet(_) => Ordering::Less,
                    Endpoint::WifiDirect(_) => Ordering::Less,
                    Endpoint::Bluetooth(_) => Ordering::Less,
                    Endpoint::Ip(_) => Ordering::Less,
                    Endpoint::IpUdp(_) => Ordering::Less,
                    Endpoint::IpTcp(b) => a.cmp(b),
                    _ => Ordering::Greater,
                }
            }
            Endpoint::Http(a) => {
                match other {
                    Endpoint::Nil => Ordering::Less,
                    Endpoint::ZeroTier(_) => Ordering::Less,
                    Endpoint::Ethernet(_) => Ordering::Less,
                    Endpoint::WifiDirect(_) => Ordering::Less,
                    Endpoint::Bluetooth(_) => Ordering::Less,
                    Endpoint::Ip(_) => Ordering::Less,
                    Endpoint::IpUdp(_) => Ordering::Less,
                    Endpoint::IpTcp(_) => Ordering::Less,
                    Endpoint::Http(b) => a.cmp(b),
                    _ => Ordering::Greater,
                }
            }
            Endpoint::WebRTC(a) => {
                match other {
                    Endpoint::WebRTC(b) => a.cmp(b),
                    _ => Ordering::Less,
                }
            }
        }
    }
}

impl ToString for Endpoint {
    fn to_string(&self) -> String {
        match self {
            Endpoint::Nil => format!("nil"),
            Endpoint::ZeroTier(a) => format!("zt:{}", a.to_string()),
            Endpoint::Ethernet(m) => format!("eth:{}", m.to_string()),
            Endpoint::WifiDirect(m) => format!("wifip2p:{}", m.to_string()),
            Endpoint::Bluetooth(m) => format!("bt:{}", m.to_string()),
            Endpoint::Ip(ip) => format!("ip:{}", ip.to_ip_string()),
            Endpoint::IpUdp(ip) => format!("udp:{}", ip.to_string()),
            Endpoint::IpTcp(ip) => format!("tcp:{}", ip.to_string()),
            Endpoint::Http(url) => url.clone(),
            Endpoint::WebRTC(offer) => format!("webrtc:{}", base64::encode(offer.as_slice())),
        }
    }
}
