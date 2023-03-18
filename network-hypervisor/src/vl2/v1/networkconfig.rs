// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::collections::{HashMap, HashSet};
use std::io::Write;
use std::str::FromStr;

use serde::{Deserialize, Serialize};

use crate::vl1::identity::Identity;
use crate::vl1::{Address, InetAddress};
use crate::vl2::iproute::IpRoute;
use crate::vl2::rule::Rule;
use crate::vl2::v1::{CertificateOfMembership, CertificateOfOwnership, Tag};
use crate::vl2::NetworkId;

use zerotier_utils::buffer::{Buffer, OutOfBoundsError};
use zerotier_utils::dictionary::Dictionary;
use zerotier_utils::error::InvalidParameterError;
use zerotier_utils::marshalable::{Marshalable, UnmarshalError};

/// Network configuration object sent to nodes by network controllers.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct NetworkConfig {
    /// Network ID
    pub network_id: NetworkId,

    /// Short address of node to which this config was issued
    pub issued_to: Address,

    /// Human-readable network name
    #[serde(skip_serializing_if = "String::is_empty")]
    #[serde(default)]
    pub name: String,

    /// True if network has access control (the default)
    pub private: bool,

    /// Network configuration timestamp
    pub timestamp: i64,

    /// Suggested horizon limit for multicast (not a hard limit, but 0 disables multicast)
    pub multicast_limit: u32,

    /// Multicast "like" expire time in milliseconds (default if omitted).
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(default)]
    pub multicast_like_expire: Option<u32>,

    /// L2 Ethernet MTU for this network.
    pub mtu: u16,

    /// ZeroTier-assigned L3 routes for this node.
    #[serde(skip_serializing_if = "HashSet::is_empty")]
    #[serde(default)]
    pub routes: HashSet<IpRoute>,

    /// ZeroTier-assigned static IP addresses for this node.
    #[serde(skip_serializing_if = "HashSet::is_empty")]
    #[serde(default)]
    pub static_ips: HashSet<InetAddress>,

    /// Network flow rules (low level).
    #[serde(skip_serializing_if = "Vec::is_empty")]
    #[serde(default)]
    pub rules: Vec<Rule>,

    /// DNS resolvers available to be auto-configured on the host.
    #[serde(skip_serializing_if = "HashMap::is_empty")]
    #[serde(default)]
    pub dns: HashMap<String, HashSet<InetAddress>>,

    /// V1 certificate of membership and other exchange-able credentials, may be absent on V2-only networks.
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(default)]
    pub v1_credentials: Option<V1Credentials>,

    /// URL to ZeroTier Central instance that is controlling the controller that issued this (if any).
    #[serde(skip_serializing_if = "String::is_empty")]
    #[serde(default)]
    pub central_url: String,

    /// SSO / third party auth information (if enabled).
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(default)]
    pub sso: Option<SSOAuthConfiguration>,
}

impl NetworkConfig {
    pub fn new(network_id: NetworkId, issued_to: Address) -> Self {
        Self {
            network_id,
            issued_to,
            name: String::new(),
            private: true,
            timestamp: 0,
            mtu: 0,
            multicast_limit: 0,
            multicast_like_expire: None,
            routes: HashSet::new(),
            static_ips: HashSet::new(),
            rules: Vec::new(),
            dns: HashMap::new(),
            v1_credentials: None,
            central_url: String::new(),
            sso: None,
        }
    }

    /// Encode a network configuration for sending to V1 nodes.
    pub fn v1_proto_to_dictionary(&self, controller_identity: &Identity) -> Option<Dictionary> {
        let mut d = Dictionary::new();

        d.set_u64(proto_v1_field_name::network_config::VERSION, 6);

        d.set_str(proto_v1_field_name::network_config::NETWORK_ID, self.network_id.to_string().as_str());
        if !self.name.is_empty() {
            d.set_str(proto_v1_field_name::network_config::NAME, self.name.as_str());
        }
        d.set_str(proto_v1_field_name::network_config::ISSUED_TO, self.issued_to.to_string().as_str());
        d.set_str(
            proto_v1_field_name::network_config::TYPE,
            if self.private {
                "0"
            } else {
                "1"
            },
        );
        d.set_u64(proto_v1_field_name::network_config::TIMESTAMP, self.timestamp as u64);
        d.set_u64(proto_v1_field_name::network_config::MTU, self.mtu as u64);
        d.set_u64(proto_v1_field_name::network_config::MULTICAST_LIMIT, self.multicast_limit as u64);

        if !self.routes.is_empty() {
            let r: Vec<IpRoute> = self.routes.iter().cloned().collect();
            d.set_bytes(proto_v1_field_name::network_config::ROUTES, marshal_multiple_to_bytes(r.as_slice()));
        }

        if !self.static_ips.is_empty() {
            let ips: Vec<InetAddress> = self.static_ips.iter().cloned().collect();
            d.set_bytes(proto_v1_field_name::network_config::STATIC_IPS, marshal_multiple_to_bytes(ips.as_slice()));
        }

        if !self.rules.is_empty() {
            d.set_bytes(
                proto_v1_field_name::network_config::RULES,
                marshal_multiple_to_bytes(self.rules.as_slice()),
            );
        }

        if !self.dns.is_empty() {
            // NOTE: v1 nodes only support one DNS server per network! If there is more than
            // one the first will be picked, whichever that is (it's a set). The UI should not
            // allow a user to add more than one unless this is a v2-only network.
            let mut dns_bin: Vec<u8> = Vec::with_capacity(256);
            if let Some((name, servers)) = self.dns.iter().next() {
                let mut name_bytes = name.as_bytes();
                name_bytes = &name_bytes[..name_bytes.len().min(127)];
                let _ = dns_bin.write_all(name_bytes);
                for _ in 0..(128 - name_bytes.len()) {
                    dns_bin.push(0);
                }
                for s in servers.iter() {
                    if let Ok(s) = s.to_buffer::<64>() {
                        let _ = dns_bin.write_all(s.as_bytes());
                    }
                }
            }
            d.set_bytes(proto_v1_field_name::network_config::DNS, dns_bin);
        }

        if let Some(v1cred) = self.v1_credentials.as_ref() {
            d.set_u64(proto_v1_field_name::network_config::REVISION, v1cred.revision);
            d.set_u64(proto_v1_field_name::network_config::MAX_DELTA, v1cred.max_delta);

            d.set_bytes(
                proto_v1_field_name::network_config::CERTIFICATE_OF_MEMBERSHIP,
                v1cred
                    .certificate_of_membership
                    .to_bytes(self.network_id.network_controller())
                    .as_bytes()
                    .to_vec(),
            );

            if !v1cred.certificates_of_ownership.is_empty() {
                let mut certs = Vec::with_capacity(v1cred.certificates_of_ownership.len() * 256);
                for c in v1cred.certificates_of_ownership.iter() {
                    let _ = certs.write_all(c.to_bytes(&controller_identity.address)?.as_slice());
                }
                d.set_bytes(proto_v1_field_name::network_config::CERTIFICATES_OF_OWNERSHIP, certs);
            }

            if !v1cred.tags.is_empty() {
                let mut tags = Vec::with_capacity(v1cred.tags.len() * 256);
                for (_, t) in v1cred.tags.iter() {
                    let _ = tags.write_all(t.to_bytes(&controller_identity.address).as_ref());
                }
                d.set_bytes(proto_v1_field_name::network_config::TAGS, tags);
            }
        }

        // node_info is not supported by V1 nodes

        if !self.central_url.is_empty() {
            d.set_str(proto_v1_field_name::network_config::CENTRAL_URL, self.central_url.as_str());
        }

        if let Some(sso) = self.sso.as_ref() {
            d.set_bool(proto_v1_field_name::network_config::SSO_ENABLED, true);
            d.set_u64(proto_v1_field_name::network_config::SSO_VERSION, sso.version as u64);
            d.set_str(
                proto_v1_field_name::network_config::SSO_AUTHENTICATION_URL,
                sso.authentication_url.as_str(),
            );
            d.set_u64(
                proto_v1_field_name::network_config::SSO_AUTHENTICATION_EXPIRY_TIME,
                sso.authentication_expiry_time as u64,
            );
            d.set_str(proto_v1_field_name::network_config::SSO_ISSUER_URL, sso.issuer_url.as_str());
            d.set_str(proto_v1_field_name::network_config::SSO_NONCE, sso.nonce.as_str());
            d.set_str(proto_v1_field_name::network_config::SSO_STATE, sso.state.as_str());
            d.set_str(proto_v1_field_name::network_config::SSO_CLIENT_ID, sso.client_id.as_str());
        } else {
            d.set_bool(proto_v1_field_name::network_config::SSO_ENABLED, false);
        }

        Some(d)
    }

    /// Decode a V1 format network configuration.
    pub fn v1_proto_from_dictionary(d: &Dictionary) -> Result<NetworkConfig, InvalidParameterError> {
        let nwid = NetworkId::from_str(
            d.get_str(proto_v1_field_name::network_config::NETWORK_ID)
                .ok_or(InvalidParameterError("missing network ID"))?,
        )
        .map_err(|_| InvalidParameterError("invalid network ID"))?;
        let issued_to_address = Address::from_str(
            d.get_str(proto_v1_field_name::network_config::ISSUED_TO)
                .ok_or(InvalidParameterError("missing address"))?,
        )
        .map_err(|_| InvalidParameterError("invalid address"))?;

        let mut nc = Self::new(nwid, issued_to_address);

        d.get_str(proto_v1_field_name::network_config::NAME).map(|x| nc.name = x.to_string());
        nc.private = d.get_str(proto_v1_field_name::network_config::TYPE).map_or(true, |x| x == "1");
        nc.timestamp = d
            .get_i64(proto_v1_field_name::network_config::TIMESTAMP)
            .ok_or(InvalidParameterError("missing timestamp"))?;
        nc.mtu = d
            .get_u64(proto_v1_field_name::network_config::MTU)
            .unwrap_or(crate::protocol::ZEROTIER_VIRTUAL_NETWORK_DEFAULT_MTU as u64) as u16;
        nc.multicast_limit = d.get_u64(proto_v1_field_name::network_config::MULTICAST_LIMIT).unwrap_or(0) as u32;

        if let Some(routes_bin) = d.get_bytes(proto_v1_field_name::network_config::ROUTES) {
            for r in unmarshal_multiple_from_bytes(routes_bin)
                .map_err(|_| InvalidParameterError("invalid route object(s)"))?
                .drain(..)
            {
                let _ = nc.routes.insert(r);
            }
        }

        if let Some(static_ips_bin) = d.get_bytes(proto_v1_field_name::network_config::STATIC_IPS) {
            for ip in unmarshal_multiple_from_bytes(static_ips_bin)
                .map_err(|_| InvalidParameterError("invalid route object(s)"))?
                .drain(..)
            {
                let _ = nc.static_ips.insert(ip);
            }
        }

        if let Some(rules_bin) = d.get_bytes(proto_v1_field_name::network_config::RULES) {
            nc.rules = unmarshal_multiple_from_bytes(rules_bin).map_err(|_| InvalidParameterError("invalid route object(s)"))?;
        }

        if let Some(dns_bin) = d.get_bytes(proto_v1_field_name::network_config::DNS) {
            if dns_bin.len() > 128 && dns_bin.len() < 1024 {
                let mut name = String::with_capacity(64);
                for i in 0..128 {
                    if dns_bin[i] == 0 {
                        break;
                    } else {
                        name.push(dns_bin[i] as char);
                    }
                }
                if !name.is_empty() {
                    let mut tmp: Buffer<1024> = Buffer::new();
                    let _ = tmp.append_bytes(&dns_bin[128..]);
                    let mut servers = HashSet::new();
                    let mut cursor = 0;
                    while cursor < tmp.len() {
                        if let Ok(s) = InetAddress::unmarshal(&tmp, &mut cursor) {
                            let _ = servers.insert(s);
                        } else {
                            break;
                        }
                    }
                    if !servers.is_empty() {
                        let _ = nc.dns.insert(name, servers);
                    }
                }
            }
        }

        let mut v1cred = V1Credentials {
            revision: d.get_u64(proto_v1_field_name::network_config::REVISION).unwrap_or(0),
            max_delta: d.get_u64(proto_v1_field_name::network_config::MAX_DELTA).unwrap_or(0),
            certificate_of_membership: CertificateOfMembership::from_bytes(
                d.get_bytes(proto_v1_field_name::network_config::CERTIFICATE_OF_MEMBERSHIP)
                    .ok_or(InvalidParameterError("missing certificate of membership"))?,
            )?,
            certificates_of_ownership: Vec::new(),
            tags: HashMap::new(),
        };

        if let Some(mut coo_bin) = d.get_bytes(proto_v1_field_name::network_config::CERTIFICATES_OF_OWNERSHIP) {
            while !coo_bin.is_empty() {
                let c = CertificateOfOwnership::from_bytes(coo_bin)?;
                v1cred.certificates_of_ownership.push(c.0);
                coo_bin = c.1;
            }
        }

        if let Some(mut tag_bin) = d.get_bytes(proto_v1_field_name::network_config::TAGS) {
            while !tag_bin.is_empty() {
                let t = Tag::from_bytes(tag_bin)?;
                let _ = v1cred.tags.insert(t.0.id, t.0);
                tag_bin = t.1;
            }
        }

        nc.v1_credentials = Some(v1cred);

        if let Some(central_url) = d.get_str(proto_v1_field_name::network_config::CENTRAL_URL) {
            nc.central_url = central_url.to_string();
        }

        if d.get_bool(proto_v1_field_name::network_config::SSO_ENABLED).unwrap_or(false) {
            nc.sso = Some(SSOAuthConfiguration {
                version: d.get_u64(proto_v1_field_name::network_config::SSO_VERSION).unwrap_or(0) as u32,
                authentication_url: d
                    .get_str(proto_v1_field_name::network_config::SSO_AUTHENTICATION_URL)
                    .unwrap_or("")
                    .to_string(),
                authentication_expiry_time: d
                    .get_i64(proto_v1_field_name::network_config::SSO_AUTHENTICATION_EXPIRY_TIME)
                    .unwrap_or(0),
                issuer_url: d.get_str(proto_v1_field_name::network_config::SSO_ISSUER_URL).unwrap_or("").to_string(),
                nonce: d.get_str(proto_v1_field_name::network_config::SSO_NONCE).unwrap_or("").to_string(),
                state: d.get_str(proto_v1_field_name::network_config::SSO_STATE).unwrap_or("").to_string(),
                client_id: d.get_str(proto_v1_field_name::network_config::SSO_CLIENT_ID).unwrap_or("").to_string(),
            })
        }

        Ok(nc)
    }
}

#[allow(unused)]
mod proto_v1_field_name {
    pub mod network_config {
        pub const VERSION: &'static str = "v";
        pub const NETWORK_ID: &'static str = "nwid";
        pub const TIMESTAMP: &'static str = "ts";
        pub const REVISION: &'static str = "r";
        pub const ISSUED_TO: &'static str = "id";
        pub const FLAGS: &'static str = "f";
        pub const MULTICAST_LIMIT: &'static str = "ml";
        pub const TYPE: &'static str = "t";
        pub const NAME: &'static str = "n";
        pub const MTU: &'static str = "mtu";
        pub const MAX_DELTA: &'static str = "ctmd";
        pub const CERTIFICATE_OF_MEMBERSHIP: &'static str = "C";
        pub const ROUTES: &'static str = "RT";
        pub const STATIC_IPS: &'static str = "I";
        pub const RULES: &'static str = "R";
        pub const TAGS: &'static str = "TAG";
        pub const CERTIFICATES_OF_OWNERSHIP: &'static str = "COO";
        pub const DNS: &'static str = "DNS";
        pub const NODE_INFO: &'static str = "NI";
        pub const CENTRAL_URL: &'static str = "ssoce";
        pub const SSO_ENABLED: &'static str = "ssoe";
        pub const SSO_VERSION: &'static str = "ssov";
        pub const SSO_AUTHENTICATION_URL: &'static str = "aurl";
        pub const SSO_AUTHENTICATION_EXPIRY_TIME: &'static str = "aexpt";
        pub const SSO_ISSUER_URL: &'static str = "iurl";
        pub const SSO_NONCE: &'static str = "sson";
        pub const SSO_STATE: &'static str = "ssos";
        pub const SSO_CLIENT_ID: &'static str = "ssocid";
    }

    pub mod sso_auth_info {
        pub const VERSION: &'static str = "aV";
        pub const AUTHENTICATION_URL: &'static str = "aU";
        pub const ISSUER_URL: &'static str = "iU";
        pub const CENTRAL_URL: &'static str = "aCU";
        pub const NONCE: &'static str = "aN";
        pub const STATE: &'static str = "aS";
        pub const CLIENT_ID: &'static str = "aCID";
    }
}

/// SSO authentication configuration object.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct SSOAuthConfiguration {
    pub version: u32,
    pub authentication_url: String,
    pub authentication_expiry_time: i64,
    pub issuer_url: String,
    pub nonce: String,
    pub state: String,
    pub client_id: String,
}

/// Credentials that must be sent to V1 nodes to allow access.
///
/// These are also handed out to V2 nodes to use when communicating with V1 nodes on
/// networks that support older protocol versions.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct V1Credentials {
    pub revision: u64,
    pub max_delta: u64,
    pub certificate_of_membership: CertificateOfMembership,
    pub certificates_of_ownership: Vec<CertificateOfOwnership>,
    #[serde(skip_serializing_if = "HashMap::is_empty")]
    #[serde(default)]
    pub tags: HashMap<u32, Tag>,
}

impl Marshalable for IpRoute {
    const MAX_MARSHAL_SIZE: usize = (InetAddress::MAX_MARSHAL_SIZE * 2) + 2 + 2;

    fn marshal<const BL: usize>(&self, buf: &mut zerotier_utils::buffer::Buffer<BL>) -> Result<(), OutOfBoundsError> {
        self.target.marshal(buf)?;
        if let Some(via) = self.via.as_ref() {
            via.marshal(buf)?;
        } else {
            buf.append_u8(0)?; // "nil" InetAddress
        }
        buf.append_u16(self.flags.unwrap_or(0))?;
        buf.append_u16(self.metric.unwrap_or(0))?;
        Ok(())
    }

    fn unmarshal<const BL: usize>(
        buf: &zerotier_utils::buffer::Buffer<BL>,
        cursor: &mut usize,
    ) -> Result<Self, zerotier_utils::marshalable::UnmarshalError> {
        Ok(IpRoute {
            target: InetAddress::unmarshal(buf, cursor)?,
            via: {
                let via = InetAddress::unmarshal(buf, cursor)?;
                if via.is_nil() {
                    None
                } else {
                    Some(via)
                }
            },
            flags: buf.read_u16(cursor).map(|f| {
                if f == 0 {
                    None
                } else {
                    Some(f)
                }
            })?,
            metric: buf.read_u16(cursor).map(|f| {
                if f == 0 {
                    None
                } else {
                    Some(f)
                }
            })?,
        })
    }
}

const TEMP_BUF_SIZE: usize = 1024;

fn marshal_multiple_to_bytes<M: Marshalable>(multiple: &[M]) -> Vec<u8> {
    debug_assert!(M::MAX_MARSHAL_SIZE <= TEMP_BUF_SIZE);
    let mut tmp = Vec::with_capacity(M::MAX_MARSHAL_SIZE * multiple.len());
    for m in multiple.iter() {
        let _ = tmp.write_all(m.to_buffer::<TEMP_BUF_SIZE>().unwrap().as_bytes());
    }
    tmp
}

fn unmarshal_multiple_from_bytes<M: Marshalable>(mut bytes: &[u8]) -> Result<Vec<M>, UnmarshalError> {
    debug_assert!(M::MAX_MARSHAL_SIZE <= TEMP_BUF_SIZE);
    let mut tmp: Buffer<TEMP_BUF_SIZE> = Buffer::new();
    let mut v: Vec<M> = Vec::new();
    while bytes.len() > 0 {
        let chunk_size = bytes.len().min(M::MAX_MARSHAL_SIZE);
        if tmp.append_bytes(&bytes[..chunk_size]).is_err() {
            return Err(UnmarshalError::OutOfBounds);
        }
        let mut cursor = 0;
        v.push(M::unmarshal(&mut tmp, &mut cursor)?);
        if cursor == 0 {
            return Err(UnmarshalError::InvalidData);
        }
        let _ = tmp.erase_first_n(cursor);
        bytes = &bytes[chunk_size..];
    }
    Ok(v)
}
