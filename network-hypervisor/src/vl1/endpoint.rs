use crate::vl1::{Address, MAC};
use crate::vl1::inetaddress::InetAddress;
use crate::vl1::buffer::{RawObject, Buffer};

#[repr(u8)]
pub enum Type {
    Nil = 0,
    ZeroTier = 1,
    Ethernet = 2,
    WifiDirect = 3,
    Bluetooth = 4,
    Ip = 5,
    IpUdp = 6,
    IpTcp = 7,
    Http = 8,
    WebRTC = 9,
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
    WebRTC(String),
}

impl Default for Endpoint {
    #[inline(always)]
    fn default() -> Endpoint {
        Endpoint::Nil
    }
}

impl Endpoint {
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

    pub fn marshal<BH: RawObject, const BL: usize>(&self, buf: &mut Buffer<BH, BL>) -> std::io::Result<()> {
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
                let b = offer.as_bytes();
                buf.append_u16(b.len() as u16)?;
                buf.append_bytes(b)
            }
        }
    }

    pub fn unmarshal<BH: RawObject, const BL: usize>(buf: &Buffer<BH, BL>, cursor: &mut usize) -> std::io::Result<Endpoint> {
        let type_byte = buf.get_u8(cursor)?;
        if type_byte < 16 {
            let ip = InetAddress::unmarshal(buf, cursor)?;
            if ip.is_nil() {
                Ok(Endpoint::Nil)
            } else {
                Ok(Endpoint::IpUdp(ip))
            }
        } else {
            match (type_byte - 16) as Type {
                Type::Nil => Ok(Endpoint::Nil),
                Type::ZeroTier => Ok(Endpoint::ZeroTier(Address::from(buf.get_bytes_fixed(cursor)?))),
                Type::Ethernet => Ok(Endpoint::Ethernet(MAC::from(buf.get_bytes_fixed(cursor)?))),
                Type::WifiDirect => Ok(Endpoint::WifiDirect(MAC::from(buf.get_bytes_fixed(cursor)?))),
                Type::Bluetooth => Ok(Endpoint::Bluetooth(MAC::from(buf.get_bytes_fixed(cursor)?))),
                Type::Ip => Ok(Endpoint::Ip(InetAddress::unmarshal(buf, cursor)?)),
                Type::IpUdp => Ok(Endpoint::IpUdp(InetAddress::unmarshal(buf, cursor)?)),
                Type::IpTcp => Ok(Endpoint::IpTcp(InetAddress::unmarshal(buf, cursor)?)),
                Type::Http => {
                    let l = buf.get_u16(cursor)?;
                    Ok(Endpoint::Http(String::from_utf8_lossy(buf.get_bytes(l as usize, cursor)?).to_string()))
                }
                Type::WebRTC => {
                    let l = buf.get_u16(cursor)?;
                    Ok(Endpoint::WebRTC(String::from_utf8_lossy(buf.get_bytes(l as usize, cursor)?).to_string()))
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
            Endpoint::WifiDirect(m) => format!("wifid:{}", m.to_string()),
            Endpoint::Bluetooth(m) => format!("bt:{}", m.to_string()),
            Endpoint::Ip(ip) => format!("ip:{}", ip.to_ip_string()),
            Endpoint::IpUdp(ip) => format!("udp:{}", ip.to_string()),
            Endpoint::IpTcp(ip) => format!("tcp:{}", ip.to_string()),
            Endpoint::Http(url) => url,
            Endpoint::WebRTC(offer) => format!("webrtc:offer:{}", urlencoding::encode(offer.as_str())),
        }
    }
}
