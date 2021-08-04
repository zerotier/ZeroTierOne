use std::hash::{Hash, Hasher};

use crate::vl1::{Address, MAC};
use crate::vl1::inetaddress::InetAddress;
use crate::vl1::buffer::Buffer;
use std::cmp::Ordering;

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

#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
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
    fn default() -> Endpoint {
        Endpoint::Nil
    }
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
                buf.append_u8(Type::Nil as u8)
            }
            Endpoint::ZeroTier(a) => {
                buf.append_u8(16 + (Type::ZeroTier as u8))?;
                buf.append_bytes_fixed(&a.to_bytes())
            }
            Endpoint::Ethernet(m) => {
                buf.append_u8(16 + (Type::Ethernet as u8))?;
                buf.append_bytes_fixed(&m.to_bytes())
            }
            Endpoint::WifiDirect(m) => {
                buf.append_u8(16 + (Type::WifiDirect as u8))?;
                buf.append_bytes_fixed(&m.to_bytes())
            }
            Endpoint::Bluetooth(m) => {
                buf.append_u8(16 + (Type::Bluetooth as u8))?;
                buf.append_bytes_fixed(&m.to_bytes())
            }
            Endpoint::Ip(ip) => {
                buf.append_u8(16 + (Type::Ip as u8))?;
                ip.marshal(buf)
            }
            Endpoint::IpUdp(ip) => {
                // IP/UDP endpoints are marshaled as naked InetAddress objects for backward
                // compatibility. This is why 16 is added to all the other type IDs. Naked
                // InetAddress objects always start with either 4 or 6.
                ip.marshal(buf)
            }
            Endpoint::IpTcp(ip) => {
                buf.append_u8(16 + (Type::IpTcp as u8))?;
                ip.marshal(buf)
            }
            Endpoint::Http(url) => {
                buf.append_u8(16 + (Type::Http as u8))?;
                let b = url.as_bytes();
                buf.append_u16(b.len() as u16)?;
                buf.append_bytes(b)
            }
            Endpoint::WebRTC(offer) => {
                buf.append_u8(16 + (Type::WebRTC as u8))?;
                let b = offer.as_slice();
                buf.append_u16(b.len() as u16)?;
                buf.append_bytes(b)
            }
        }
    }

    pub fn unmarshal<const BL: usize>(buf: &Buffer<BL>, cursor: &mut usize) -> std::io::Result<Endpoint> {
        let type_byte = buf.read_u8(cursor)?;
        if type_byte < 16 {
            let ip = InetAddress::unmarshal(buf, cursor)?;
            if ip.is_nil() {
                Ok(Endpoint::Nil)
            } else {
                Ok(Endpoint::IpUdp(ip))
            }
        } else {
            match type_byte - 16 {
                TYPE_NIL => Ok(Endpoint::Nil),
                TYPE_ZEROTIER => Ok(Endpoint::ZeroTier(Address::from(buf.read_bytes_fixed(cursor)?))),
                TYPE_ETHERNET => Ok(Endpoint::Ethernet(MAC::from(buf.read_bytes_fixed(cursor)?))),
                TYPE_WIFIDIRECT => Ok(Endpoint::WifiDirect(MAC::from(buf.read_bytes_fixed(cursor)?))),
                TYPE_BLUETOOTH => Ok(Endpoint::Bluetooth(MAC::from(buf.read_bytes_fixed(cursor)?))),
                TYPE_IP => Ok(Endpoint::Ip(InetAddress::unmarshal(buf, cursor)?)),
                TYPE_IPUDP => Ok(Endpoint::IpUdp(InetAddress::unmarshal(buf, cursor)?)),
                TYPE_IPTCP => Ok(Endpoint::IpTcp(InetAddress::unmarshal(buf, cursor)?)),
                TYPE_HTTP => {
                    let l = buf.read_u16(cursor)?;
                    Ok(Endpoint::Http(String::from_utf8_lossy(buf.read_bytes(l as usize, cursor)?).to_string()))
                }
                TYPE_WEBRTC => {
                    let l = buf.read_u16(cursor)?;
                    Ok(Endpoint::WebRTC(buf.read_bytes(l as usize, cursor)?.to_vec()))
                }
                _ => std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "unrecognized endpoint type in stream"))
            }
        }
    }
}

impl Hash for Endpoint {
    fn hash<H: Hasher>(&self, state: &mut H) {
        match self {
            Endpoint::Nil => {
                state.write_u8(Type::Nil as u8);
            }
            Endpoint::ZeroTier(a) => {
                state.write_u8(Type::ZeroTier as u8);
                state.write_u64(a.to_u64())
            }
            Endpoint::Ethernet(m) => {
                state.write_u8(Type::Ethernet as u8);
                state.write_u64(m.to_u64())
            }
            Endpoint::WifiDirect(m) => {
                state.write_u8(Type::WifiDirect as u8);
                state.write_u64(m.to_u64())
            }
            Endpoint::Bluetooth(m) => {
                state.write_u8(Type::Bluetooth as u8);
                state.write_u64(m.to_u64())
            }
            Endpoint::Ip(ip) => {
                state.write_u8(Type::Ip as u8);
                ip.hash(state);
            }
            Endpoint::IpUdp(ip) => {
                state.write_u8(Type::IpUdp as u8);
                ip.hash(state);
            }
            Endpoint::IpTcp(ip) => {
                state.write_u8(Type::IpTcp as u8);
                ip.hash(state);
            }
            Endpoint::Http(url) => {
                state.write_u8(Type::Http as u8);
                url.hash(state);
            }
            Endpoint::WebRTC(offer) => {
                state.write_u8(Type::WebRTC as u8);
                offer.hash(state);
            }
        }
    }
}

impl PartialOrd for Endpoint {
    #[inline(always)]
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for Endpoint {
    fn cmp(&self, other: &Self) -> Ordering {
        // This ordering is done explicitly instead of using derive(Ord) so it will be certain
        // to be consistent with the integer order in the Type enum. Make sure it stays this
        // way if new types are added in future revisions.
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
            Endpoint::WebRTC(offer) => format!("webrtc:{}", urlencoding::encode(String::from_utf8_lossy(offer.as_slice()).as_ref())),
        }
    }
}
