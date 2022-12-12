// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::cmp::Ordering;
use std::hash::{Hash, Hasher};
use std::str::FromStr;

use serde::{Deserialize, Deserializer, Serialize, Serializer};

use crate::vl1::identity::IDENTITY_FINGERPRINT_SIZE;
use crate::vl1::inetaddress::InetAddress;
use crate::vl1::{Address, MAC};

use zerotier_utils::buffer::Buffer;
use zerotier_utils::error::InvalidFormatError;
use zerotier_utils::marshalable::{Marshalable, UnmarshalError};
use zerotier_utils::{base64_decode_url_nopad, base64_encode_url_nopad};

pub const TYPE_NIL: u8 = 0;
pub const TYPE_ZEROTIER: u8 = 1;
pub const TYPE_ETHERNET: u8 = 2;
pub const TYPE_WIFIDIRECT: u8 = 3;
pub const TYPE_BLUETOOTH: u8 = 4;
pub const TYPE_ICMP: u8 = 5;
pub const TYPE_IPUDP: u8 = 6;
pub const TYPE_IPTCP: u8 = 7;
pub const TYPE_HTTP: u8 = 8;
pub const TYPE_WEBRTC: u8 = 9;
pub const TYPE_ZEROTIER_ENCAP: u8 = 10;

pub(crate) const MAX_MARSHAL_SIZE: usize = 1024;

/// A communication endpoint on the network where a ZeroTier node can be reached.
///
/// Currently only a few of these are supported. The rest are reserved for future use.
#[derive(Clone, PartialEq, Eq, Debug)]
pub enum Endpoint {
    /// A null endpoint.
    Nil,

    /// Via another node using unencapsulated relaying (e.g. via a root)
    /// This is the address and the full identity fingerprint.
    ZeroTier(Address, [u8; IDENTITY_FINGERPRINT_SIZE]),

    /// Direct L2 Ethernet
    Ethernet(MAC),

    /// Direct L2 Ethernet over WiFi-Direct (P2P WiFi)
    WifiDirect(MAC),

    /// Local bluetooth
    Bluetooth(MAC),

    /// ICMP ECHO, which can be used to traverse some NATs
    Icmp(InetAddress),

    /// Raw UDP, the default and usually preferred transport mode
    IpUdp(InetAddress),

    /// Raw TCP with each packet prefixed by a varint size
    IpTcp(InetAddress),

    /// HTTP streaming
    Http(String),

    /// WebRTC data channel
    WebRTC(Vec<u8>),

    /// Via another node using inner encapsulation via VERB_ENCAP.
    /// This is the address and the full identity fingerprint.
    ZeroTierEncap(Address, [u8; IDENTITY_FINGERPRINT_SIZE]),
}

impl Default for Endpoint {
    #[inline(always)]
    fn default() -> Endpoint {
        Endpoint::Nil
    }
}

impl Endpoint {
    /// Get the IP address (and port if applicable) if this is an IP-based transport.
    #[inline(always)]
    pub fn ip(&self) -> Option<(&InetAddress, u8)> {
        match self {
            Endpoint::Icmp(ip) => Some((&ip, TYPE_ICMP)),
            Endpoint::IpUdp(ip) => Some((&ip, TYPE_IPUDP)),
            Endpoint::IpTcp(ip) => Some((&ip, TYPE_IPTCP)),
            _ => None,
        }
    }

    pub fn type_id(&self) -> u8 {
        match self {
            Endpoint::Nil => TYPE_NIL,
            Endpoint::ZeroTier(_, _) => TYPE_ZEROTIER,
            Endpoint::Ethernet(_) => TYPE_ETHERNET,
            Endpoint::WifiDirect(_) => TYPE_WIFIDIRECT,
            Endpoint::Bluetooth(_) => TYPE_BLUETOOTH,
            Endpoint::Icmp(_) => TYPE_ICMP,
            Endpoint::IpUdp(_) => TYPE_IPUDP,
            Endpoint::IpTcp(_) => TYPE_IPTCP,
            Endpoint::Http(_) => TYPE_HTTP,
            Endpoint::WebRTC(_) => TYPE_WEBRTC,
            Endpoint::ZeroTierEncap(_, _) => TYPE_ZEROTIER_ENCAP,
        }
    }

    #[inline(always)]
    pub fn is_nil(&self) -> bool {
        matches!(self, Endpoint::Nil)
    }

    pub fn from_bytes(bytes: &[u8]) -> Option<Endpoint> {
        if bytes.len() < MAX_MARSHAL_SIZE {
            let mut cursor = 0;
            Self::unmarshal(&Buffer::<MAX_MARSHAL_SIZE>::from_bytes(bytes).unwrap(), &mut cursor).map_or(None, |e| Some(e))
        } else {
            None
        }
    }

    /// Get the maximum fragment size for this endpoint or usize::MAX if there is no hard limit.
    #[inline(always)]
    pub fn max_fragment_size(&self) -> usize {
        match self {
            Endpoint::Icmp(_) | Endpoint::IpUdp(_) | Endpoint::Ethernet(_) | Endpoint::Bluetooth(_) | Endpoint::WifiDirect(_) => {
                crate::protocol::UDP_DEFAULT_MTU
            }
            _ => usize::MAX,
        }
    }
}

impl Marshalable for Endpoint {
    const MAX_MARSHAL_SIZE: usize = MAX_MARSHAL_SIZE;

    fn marshal<const BL: usize>(&self, buf: &mut Buffer<BL>) -> Result<(), UnmarshalError> {
        match self {
            Endpoint::Nil => {
                buf.append_u8(16 + TYPE_NIL)?;
            }
            Endpoint::ZeroTier(a, h) => {
                buf.append_u8(16 + TYPE_ZEROTIER)?;
                buf.append_bytes_fixed(&a.to_bytes())?;
                buf.append_bytes_fixed(h)?;
            }
            Endpoint::Ethernet(m) => {
                buf.append_u8(16 + TYPE_ETHERNET)?;
                buf.append_bytes_fixed(&m.to_bytes())?;
            }
            Endpoint::WifiDirect(m) => {
                buf.append_u8(16 + TYPE_WIFIDIRECT)?;
                buf.append_bytes_fixed(&m.to_bytes())?;
            }
            Endpoint::Bluetooth(m) => {
                buf.append_u8(16 + TYPE_BLUETOOTH)?;
                buf.append_bytes_fixed(&m.to_bytes())?;
            }
            Endpoint::Icmp(ip) => {
                buf.append_u8(16 + TYPE_ICMP)?;
                ip.marshal(buf)?;
            }
            Endpoint::IpUdp(ip) => {
                // Wire encoding of IP/UDP type endpoints is the same as naked InetAddress
                // objects for backward compatibility. That way a naked InetAddress unmarshals
                // here as an IP/UDP Endpoint and vice versa. Supporting this is why 16 is added
                // to all Endpoint type IDs for wire encoding so that values of 4 or 6 can be
                // interpreted as IP/UDP InetAddress.
                ip.marshal(buf)?;
            }
            Endpoint::IpTcp(ip) => {
                buf.append_u8(16 + TYPE_IPTCP)?;
                ip.marshal(buf)?;
            }
            Endpoint::Http(url) => {
                buf.append_u8(16 + TYPE_HTTP)?;
                let b = url.as_bytes();
                buf.append_varint(b.len() as u64)?;
                buf.append_bytes(b)?;
            }
            Endpoint::WebRTC(offer) => {
                buf.append_u8(16 + TYPE_WEBRTC)?;
                let b = offer.as_slice();
                buf.append_varint(b.len() as u64)?;
                buf.append_bytes(b)?;
            }
            Endpoint::ZeroTierEncap(a, h) => {
                buf.append_u8(16 + TYPE_ZEROTIER_ENCAP)?;
                buf.append_bytes_fixed(&a.to_bytes())?;
                buf.append_bytes_fixed(h)?;
            }
        }
        Ok(())
    }

    fn unmarshal<const BL: usize>(buf: &Buffer<BL>, cursor: &mut usize) -> Result<Endpoint, UnmarshalError> {
        let type_byte = buf.read_u8(cursor)?;
        if type_byte < 16 {
            if type_byte == 4 {
                let b: &[u8; 6] = buf.read_bytes_fixed(cursor)?;
                Ok(Endpoint::IpUdp(InetAddress::from_ip_port(
                    &b[0..4],
                    u16::from_be_bytes(b[4..6].try_into().unwrap()),
                )))
            } else if type_byte == 6 {
                let b: &[u8; 18] = buf.read_bytes_fixed(cursor)?;
                Ok(Endpoint::IpUdp(InetAddress::from_ip_port(
                    &b[0..16],
                    u16::from_be_bytes(b[16..18].try_into().unwrap()),
                )))
            } else {
                Err(UnmarshalError::InvalidData)
            }
        } else {
            match type_byte - 16 {
                TYPE_NIL => Ok(Endpoint::Nil),
                TYPE_ZEROTIER => {
                    let zt = Address::from_bytes_fixed(buf.read_bytes_fixed(cursor)?).ok_or(UnmarshalError::InvalidData)?;
                    Ok(Endpoint::ZeroTier(
                        zt,
                        buf.read_bytes_fixed::<IDENTITY_FINGERPRINT_SIZE>(cursor)?.clone(),
                    ))
                }
                TYPE_ETHERNET => Ok(Endpoint::Ethernet(MAC::unmarshal(buf, cursor)?)),
                TYPE_WIFIDIRECT => Ok(Endpoint::WifiDirect(MAC::unmarshal(buf, cursor)?)),
                TYPE_BLUETOOTH => Ok(Endpoint::Bluetooth(MAC::unmarshal(buf, cursor)?)),
                TYPE_ICMP => Ok(Endpoint::Icmp(InetAddress::unmarshal(buf, cursor)?)),
                TYPE_IPUDP => Ok(Endpoint::IpUdp(InetAddress::unmarshal(buf, cursor)?)),
                TYPE_IPTCP => Ok(Endpoint::IpTcp(InetAddress::unmarshal(buf, cursor)?)),
                TYPE_HTTP => Ok(Endpoint::Http(
                    String::from_utf8_lossy(buf.read_bytes(buf.read_varint(cursor)? as usize, cursor)?).to_string(),
                )),
                TYPE_WEBRTC => Ok(Endpoint::WebRTC(
                    buf.read_bytes(buf.read_varint(cursor)? as usize, cursor)?.to_vec(),
                )),
                TYPE_ZEROTIER_ENCAP => {
                    let zt = Address::from_bytes_fixed(buf.read_bytes_fixed(cursor)?).ok_or(UnmarshalError::InvalidData)?;
                    Ok(Endpoint::ZeroTierEncap(zt, buf.read_bytes_fixed(cursor)?.clone()))
                }
                _ => Err(UnmarshalError::InvalidData),
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
                state.write_u64(a.into())
            }
            Endpoint::Ethernet(m) => {
                state.write_u8(TYPE_ETHERNET);
                state.write_u64(m.into())
            }
            Endpoint::WifiDirect(m) => {
                state.write_u8(TYPE_WIFIDIRECT);
                state.write_u64(m.into())
            }
            Endpoint::Bluetooth(m) => {
                state.write_u8(TYPE_BLUETOOTH);
                state.write_u64(m.into())
            }
            Endpoint::Icmp(ip) => {
                state.write_u8(TYPE_ICMP);
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
                state.write_u64(a.into())
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
            (Endpoint::Icmp(a), Endpoint::Icmp(b)) => a.cmp(b),
            (Endpoint::IpUdp(a), Endpoint::IpUdp(b)) => a.cmp(b),
            (Endpoint::IpTcp(a), Endpoint::IpTcp(b)) => a.cmp(b),
            (Endpoint::Http(a), Endpoint::Http(b)) => a.cmp(b),
            (Endpoint::WebRTC(a), Endpoint::WebRTC(b)) => a.cmp(b),
            (Endpoint::ZeroTierEncap(a, ah), Endpoint::ZeroTierEncap(b, bh)) => a.cmp(b).then_with(|| ah.cmp(bh)),
            _ => self.type_id().cmp(&other.type_id()),
        }
    }
}

impl PartialOrd for Endpoint {
    #[inline(always)]
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl ToString for Endpoint {
    fn to_string(&self) -> String {
        match self {
            Endpoint::Nil => format!("nil"),
            Endpoint::ZeroTier(a, ah) => format!("zt:{}-{}", a.to_string(), base64_encode_url_nopad(ah)),
            Endpoint::Ethernet(m) => format!("eth:{}", m.to_string()),
            Endpoint::WifiDirect(m) => format!("wifip2p:{}", m.to_string()),
            Endpoint::Bluetooth(m) => format!("bt:{}", m.to_string()),
            Endpoint::Icmp(ip) => format!("icmp:{}", ip.to_string()),
            Endpoint::IpUdp(ip) => format!("udp:{}", ip.to_string()),
            Endpoint::IpTcp(ip) => format!("tcp:{}", ip.to_string()),
            Endpoint::Http(url) => format!("url:{}", url.clone()), // http or https
            Endpoint::WebRTC(offer) => format!("webrtc:{}", base64_encode_url_nopad(offer.as_slice())),
            Endpoint::ZeroTierEncap(a, ah) => format!("zte:{}-{}", a.to_string(), base64_encode_url_nopad(ah)),
        }
    }
}

impl FromStr for Endpoint {
    type Err = InvalidFormatError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let ss = s.trim();
        if ss.is_empty() || ss == "nil" {
            return Ok(Endpoint::Nil);
        }
        let ss = ss.split_once(":");
        if ss.is_none() {
            return Err(InvalidFormatError);
        }
        let (endpoint_type, endpoint_data) = ss.unwrap();
        match endpoint_type {
            "zt" | "zte" => {
                let address_and_hash = endpoint_data.split_once("-");
                if address_and_hash.is_some() {
                    let (address, hash) = address_and_hash.unwrap();
                    if let Some(hash) = base64_decode_url_nopad(hash) {
                        if hash.len() == IDENTITY_FINGERPRINT_SIZE {
                            if endpoint_type == "zt" {
                                return Ok(Endpoint::ZeroTier(Address::from_str(address)?, hash.as_slice().try_into().unwrap()));
                            } else {
                                return Ok(Endpoint::ZeroTierEncap(
                                    Address::from_str(address)?,
                                    hash.as_slice().try_into().unwrap(),
                                ));
                            }
                        }
                    }
                }
            }
            "eth" => return Ok(Endpoint::Ethernet(MAC::from_str(endpoint_data)?)),
            "wifip2p" => return Ok(Endpoint::WifiDirect(MAC::from_str(endpoint_data)?)),
            "bt" => return Ok(Endpoint::Bluetooth(MAC::from_str(endpoint_data)?)),
            "icmp" => return Ok(Endpoint::Icmp(InetAddress::from_str(endpoint_data)?)),
            "udp" => return Ok(Endpoint::IpUdp(InetAddress::from_str(endpoint_data)?)),
            "tcp" => return Ok(Endpoint::IpTcp(InetAddress::from_str(endpoint_data)?)),
            "url" => return Ok(Endpoint::Http(endpoint_data.into())),
            "webrtc" => {
                if let Some(offer) = base64_decode_url_nopad(endpoint_data) {
                    return Ok(Endpoint::WebRTC(offer));
                }
            }
            _ => {}
        }
        return Err(InvalidFormatError);
    }
}

impl Serialize for Endpoint {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        if serializer.is_human_readable() {
            serializer.serialize_str(self.to_string().as_str())
        } else {
            let mut tmp: Buffer<MAX_MARSHAL_SIZE> = Buffer::new();
            assert!(self.marshal(&mut tmp).is_ok());
            serializer.serialize_bytes(tmp.as_bytes())
        }
    }
}

struct EndpointVisitor;

impl<'de> serde::de::Visitor<'de> for EndpointVisitor {
    type Value = Endpoint;

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("an Endpoint")
    }

    fn visit_bytes<E>(self, v: &[u8]) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        if v.len() <= MAX_MARSHAL_SIZE {
            let mut tmp: Buffer<MAX_MARSHAL_SIZE> = Buffer::new();
            let _ = tmp.append_bytes(v);
            let mut cursor = 0;
            Endpoint::unmarshal(&tmp, &mut cursor).map_err(|e| E::custom(e.to_string()))
        } else {
            Err(E::custom("object too large"))
        }
    }

    fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        Endpoint::from_str(v).map_err(|e| E::custom(e.to_string()))
    }
}

impl<'de> Deserialize<'de> for Endpoint {
    fn deserialize<D>(deserializer: D) -> Result<Endpoint, D::Error>
    where
        D: Deserializer<'de>,
    {
        if deserializer.is_human_readable() {
            deserializer.deserialize_str(EndpointVisitor)
        } else {
            deserializer.deserialize_bytes(EndpointVisitor)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::protocol::*;

    fn randstring(len: u8) -> String {
        (0..len)
            .map(|_| (rand::random::<u8>() % 26) + 'a' as u8)
            .map(|c| {
                if rand::random::<bool>() {
                    (c as char).to_ascii_uppercase()
                } else {
                    c as char
                }
            })
            .map(|c| c.to_string())
            .collect::<Vec<String>>()
            .join("")
    }

    #[test]
    fn endpoint_default() {
        let e: Endpoint = Default::default();
        assert!(matches!(e, Endpoint::Nil))
    }

    #[test]
    fn endpoint_from_bytes() {
        let v = [0u8; MAX_MARSHAL_SIZE];
        assert!(Endpoint::from_bytes(&v).is_none());
    }

    #[test]
    fn endpoint_marshal_nil() {
        let n = Endpoint::Nil;

        let mut buf = Buffer::<1>::new();

        let res = n.marshal(&mut buf);
        assert!(res.is_ok());

        let res = Endpoint::unmarshal(&buf, &mut 0);
        assert!(res.is_ok());

        let n2 = res.unwrap();
        assert_eq!(n, n2);
    }

    #[test]
    fn endpoint_marshal_zerotier() {
        for _ in 0..1000 {
            let mut hash = [0u8; IDENTITY_FINGERPRINT_SIZE];
            hash.fill_with(|| rand::random());

            let mut v = [0u8; ADDRESS_SIZE];
            v.fill_with(|| rand::random());

            // correct for situations where RNG generates a prefix which generates a None value.
            while v[0] == ADDRESS_RESERVED_PREFIX {
                v[0] = rand::random()
            }

            let zte = Endpoint::ZeroTier(Address::from_bytes(&v).unwrap(), hash);

            const TMP: usize = IDENTITY_FINGERPRINT_SIZE + 8;
            let mut buf = Buffer::<TMP>::new();

            let res = zte.marshal(&mut buf);
            assert!(res.is_ok());

            let res = Endpoint::unmarshal(&buf, &mut 0);
            assert!(res.is_ok());

            let zte2 = res.unwrap();
            assert_eq!(zte, zte2);
        }
    }

    #[test]
    fn endpoint_marshal_zerotier_encap() {
        for _ in 0..1000 {
            let mut hash = [0u8; IDENTITY_FINGERPRINT_SIZE];
            hash.fill_with(|| rand::random());

            let mut v = [0u8; ADDRESS_SIZE];
            v.fill_with(|| rand::random());

            // correct for situations where RNG generates a prefix which generates a None value.
            while v[0] == ADDRESS_RESERVED_PREFIX {
                v[0] = rand::random()
            }

            let zte = Endpoint::ZeroTierEncap(Address::from_bytes(&v).unwrap(), hash);

            const TMP: usize = IDENTITY_FINGERPRINT_SIZE + 8;
            let mut buf = Buffer::<TMP>::new();

            let res = zte.marshal(&mut buf);
            assert!(res.is_ok());

            let res = Endpoint::unmarshal(&buf, &mut 0);
            assert!(res.is_ok());

            let zte2 = res.unwrap();
            assert_eq!(zte, zte2);
        }
    }

    #[test]
    fn endpoint_marshal_mac() {
        for _ in 0..1000 {
            let mac = crate::vl1::MAC::from_u64(rand::random()).unwrap();

            for e in [
                Endpoint::Ethernet(mac.clone()),
                Endpoint::WifiDirect(mac.clone()),
                Endpoint::Bluetooth(mac.clone()),
            ] {
                let mut buf = Buffer::<7>::new();

                let res = e.marshal(&mut buf);
                assert!(res.is_ok());

                let res = Endpoint::unmarshal(&buf, &mut 0);
                assert!(res.is_ok());

                let e2 = res.unwrap();
                assert_eq!(e, e2);
            }
        }
    }

    #[test]
    fn endpoint_marshal_inetaddress() {
        for _ in 0..1000 {
            let mut v = [0u8; 16];
            v.fill_with(|| rand::random());

            let inet = crate::vl1::InetAddress::from_ip_port(&v, 1234);

            for e in [
                Endpoint::Icmp(inet.clone()),
                Endpoint::IpTcp(inet.clone()),
                Endpoint::IpUdp(inet.clone()),
            ] {
                let mut buf = Buffer::<20>::new();

                let res = e.marshal(&mut buf);
                assert!(res.is_ok());

                let res = Endpoint::unmarshal(&buf, &mut 0);
                assert!(res.is_ok());

                let e2 = res.unwrap();
                assert_eq!(e, e2);
            }
        }
    }

    #[test]
    fn endpoint_marshal_http() {
        for _ in 0..1000 {
            let http = Endpoint::Http(randstring(30));
            let mut buf = Buffer::<33>::new();

            assert!(http.marshal(&mut buf).is_ok());

            let res = Endpoint::unmarshal(&buf, &mut 0);
            assert!(res.is_ok());

            let http2 = res.unwrap();
            assert_eq!(http, http2);
        }
    }

    #[test]
    fn endpoint_marshal_webrtc() {
        for _ in 0..1000 {
            let mut v = Vec::with_capacity(100);
            v.fill_with(|| rand::random());

            let rtc = Endpoint::WebRTC(v);
            let mut buf = Buffer::<102>::new();

            assert!(rtc.marshal(&mut buf).is_ok());

            let res = Endpoint::unmarshal(&buf, &mut 0);
            assert!(res.is_ok());

            let rtc2 = res.unwrap();
            assert_eq!(rtc, rtc2);
        }
    }

    #[test]
    fn endpoint_to_from_string() {
        use std::str::FromStr;

        for _ in 0..1000 {
            let mut v = Vec::with_capacity(100);
            v.fill_with(|| rand::random());
            let rtc = Endpoint::WebRTC(v);

            assert_ne!(rtc.to_string().len(), 0);
            assert!(rtc.to_string().starts_with("webrtc"));

            let rtc2 = Endpoint::from_str(&rtc.to_string()).unwrap();
            assert_eq!(rtc, rtc2);

            let http = Endpoint::Http(randstring(30));
            assert_ne!(http.to_string().len(), 0);
            assert!(http.to_string().starts_with("url"));

            let http2 = Endpoint::from_str(&http.to_string()).unwrap();
            assert_eq!(http, http2);

            let mut v = [0u8; 16];
            v.fill_with(|| rand::random());

            let inet = crate::vl1::InetAddress::from_ip_port(&v, 0);

            let ip = Endpoint::Icmp(inet.clone());
            assert_ne!(ip.to_string().len(), 0);
            assert!(ip.to_string().starts_with("icmp"));

            let ip2 = Endpoint::from_str(&ip.to_string()).unwrap();
            assert_eq!(ip, ip2);

            let inet = crate::vl1::InetAddress::from_ip_port(&v, 1234);

            for e in [(Endpoint::IpTcp(inet.clone()), "tcp"), (Endpoint::IpUdp(inet.clone()), "udp")] {
                assert_ne!(e.0.to_string().len(), 0);
                assert!(e.0.to_string().starts_with(e.1));

                let e2 = Endpoint::from_str(&e.0.to_string()).unwrap();
                assert_eq!(e.0, e2);
            }

            let mac = crate::vl1::MAC::from_u64(rand::random()).unwrap();

            for e in [
                (Endpoint::Ethernet(mac.clone()), "eth"),
                (Endpoint::WifiDirect(mac.clone()), "wifip2p"),
                (Endpoint::Bluetooth(mac.clone()), "bt"),
            ] {
                assert_ne!(e.0.to_string().len(), 0);
                assert!(e.0.to_string().starts_with(e.1));

                let e2 = Endpoint::from_str(&e.0.to_string()).unwrap();
                assert_eq!(e.0, e2);
            }

            let mut hash = [0u8; IDENTITY_FINGERPRINT_SIZE];
            hash.fill_with(|| rand::random());

            let mut v = [0u8; ADDRESS_SIZE];
            v.fill_with(|| rand::random());

            // correct for situations where RNG generates a prefix which generates a None value.
            while v[0] == ADDRESS_RESERVED_PREFIX {
                v[0] = rand::random()
            }

            for e in [
                (Endpoint::ZeroTier(Address::from_bytes(&v).unwrap(), hash), "zt"),
                (Endpoint::ZeroTierEncap(Address::from_bytes(&v).unwrap(), hash), "zte"),
            ] {
                assert_ne!(e.0.to_string().len(), 0);
                assert!(e.0.to_string().starts_with(e.1));

                let e2 = Endpoint::from_str(&e.0.to_string()).unwrap();
                assert_eq!(e.0, e2);
            }

            assert_eq!(Endpoint::Nil.to_string(), "nil");
        }
    }
}
