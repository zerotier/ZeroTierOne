// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::collections::HashMap;
use std::io::Write;

use serde::{Deserialize, Serialize};

use crate::vl1::{Address, InetAddress};
use crate::vl2::certificateofmembership::CertificateOfMembership;
use crate::vl2::certificateofownership::CertificateOfOwnership;
use crate::vl2::rule::Rule;
use crate::vl2::tag::Tag;

use zerotier_utils::buffer::Buffer;
use zerotier_utils::dictionary::Dictionary;
use zerotier_utils::marshalable::{Marshalable, UnmarshalError};

#[allow(unused)]
pub mod field_name {
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
        pub const MOTD: &'static str = "motd";
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
#[derive(Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct SSOAuthConfiguration {
    pub version: u32,
    pub authentication_url: String,
    pub authentication_expiry_time: i64,
    pub issuer_url: String,
    pub nonce: String,
    pub state: String,
    pub client_id: String,
}

/// Network configuration object sent to nodes by network controllers.
#[derive(Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct NetworkConfig {
    pub id: u64,
    pub name: String,
    pub motd: String,
    pub issued_to: Address,
    pub private: bool,

    pub timestamp: i64,
    pub max_delta: i64,
    pub revision: u64,

    pub mtu: u16,
    pub multicast_limit: u32,
    pub routes: Vec<IpRoute>,
    pub static_ips: Vec<InetAddress>,
    pub rules: Vec<Rule>,
    pub dns: Vec<Nameserver>,

    pub certificate_of_membership: CertificateOfMembership,
    pub certificates_of_ownership: Vec<CertificateOfOwnership>,
    pub tags: Vec<Tag>,

    pub node_info: HashMap<Address, NodeInfo>,

    pub central_url: String,

    pub sso: Option<SSOAuthConfiguration>,
}

impl NetworkConfig {
    pub fn v1_proto_to_dictionary(&self) -> Option<Dictionary> {
        let mut d = Dictionary::new();
        d.set_u64(field_name::network_config::NETWORK_ID, self.id);
        if !self.name.is_empty() {
            d.set_str(field_name::network_config::NAME, self.name.as_str());
        }
        if !self.motd.is_empty() {
            d.set_str(field_name::network_config::MOTD, self.motd.as_str());
        }
        d.set_str(field_name::network_config::ISSUED_TO, self.issued_to.to_string().as_str());
        d.set_str(
            field_name::network_config::TYPE,
            if self.private {
                "0"
            } else {
                "1"
            },
        );
        d.set_u64(field_name::network_config::TIMESTAMP, self.timestamp as u64);
        d.set_u64(field_name::network_config::MAX_DELTA, self.max_delta as u64);
        d.set_u64(field_name::network_config::REVISION, self.revision);
        d.set_u64(field_name::network_config::MTU, self.mtu as u64);
        d.set_u64(field_name::network_config::MULTICAST_LIMIT, self.multicast_limit as u64);
        if !self.routes.is_empty() {
            d.set_bytes(
                field_name::network_config::ROUTES,
                IpRoute::marshal_multiple_to_bytes(self.routes.as_slice()).unwrap(),
            );
        }
        if !self.static_ips.is_empty() {
            d.set_bytes(
                field_name::network_config::STATIC_IPS,
                InetAddress::marshal_multiple_to_bytes(self.static_ips.as_slice()).unwrap(),
            );
        }
        if !self.rules.is_empty() {
            d.set_bytes(
                field_name::network_config::RULES,
                Rule::marshal_multiple_to_bytes(self.rules.as_slice()).unwrap(),
            );
        }
        if !self.dns.is_empty() {
            d.set_bytes(
                field_name::network_config::DNS,
                Nameserver::marshal_multiple_to_bytes(self.dns.as_slice()).unwrap(),
            );
        }
        d.set_bytes(
            field_name::network_config::CERTIFICATE_OF_MEMBERSHIP,
            self.certificate_of_membership.v1_proto_to_bytes()?,
        );
        if !self.certificates_of_ownership.is_empty() {
            let mut certs = Vec::with_capacity(self.certificates_of_ownership.len() * 256);
            for c in self.certificates_of_ownership.iter() {
                let _ = certs.write_all(c.v1_proto_to_bytes()?.as_slice());
            }
            d.set_bytes(field_name::network_config::CERTIFICATES_OF_OWNERSHIP, certs);
        }
        if !self.tags.is_empty() {
            let mut certs = Vec::with_capacity(self.certificates_of_ownership.len() * 256);
            for t in self.tags.iter() {
                let _ = certs.write_all(t.v1_proto_to_bytes()?.as_slice());
            }
            d.set_bytes(field_name::network_config::TAGS, certs);
        }
        // node_info is not supported by V1 nodes
        if !self.central_url.is_empty() {
            d.set_str(field_name::network_config::CENTRAL_URL, self.central_url.as_str());
        }
        if let Some(sso) = self.sso.as_ref() {
            d.set_bool(field_name::network_config::SSO_ENABLED, true);
            d.set_u64(field_name::network_config::SSO_VERSION, sso.version as u64);
            d.set_str(field_name::network_config::SSO_AUTHENTICATION_URL, sso.authentication_url.as_str());
            d.set_u64(
                field_name::network_config::SSO_AUTHENTICATION_EXPIRY_TIME,
                sso.authentication_expiry_time as u64,
            );
            d.set_str(field_name::network_config::SSO_ISSUER_URL, sso.issuer_url.as_str());
            d.set_str(field_name::network_config::SSO_NONCE, sso.nonce.as_str());
            d.set_str(field_name::network_config::SSO_STATE, sso.state.as_str());
            d.set_str(field_name::network_config::SSO_CLIENT_ID, sso.client_id.as_str());
        } else {
            d.set_bool(field_name::network_config::SSO_ENABLED, false);
        }
        Some(d)
    }
}

/// Information about nodes on the network that can be included in a network config.
#[derive(Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct NodeInfo {
    pub flags: u64,
    pub ip: Option<InetAddress>,
    pub name: Option<String>,
    pub services: HashMap<String, Option<String>>,
}

/// Statically pushed L3 IP routes included with a network configuration.
#[derive(Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct IpRoute {
    pub target: InetAddress,
    pub via: Option<InetAddress>,
    pub flags: u16,
    pub metric: u16,
}

impl Marshalable for IpRoute {
    const MAX_MARSHAL_SIZE: usize = (InetAddress::MAX_MARSHAL_SIZE * 2) + 2 + 2;

    fn marshal<const BL: usize>(
        &self,
        buf: &mut zerotier_utils::buffer::Buffer<BL>,
    ) -> Result<(), zerotier_utils::marshalable::UnmarshalError> {
        self.target.marshal(buf)?;
        if let Some(via) = self.via.as_ref() {
            via.marshal(buf)?;
        } else {
            buf.append_u8(0)?; // "nil" InetAddress
        }
        buf.append_u16(self.flags)?;
        buf.append_u16(self.metric)?;
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
            flags: buf.read_u16(cursor)?,
            metric: buf.read_u16(cursor)?,
        })
    }
}

/// ZeroTier-pushed DNS nameserver configuration.
#[derive(Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct Nameserver {
    pub ip: Vec<InetAddress>,
    pub domain: String,
}

impl Marshalable for Nameserver {
    const MAX_MARSHAL_SIZE: usize = 128 + InetAddress::MAX_MARSHAL_SIZE;

    fn marshal<const BL: usize>(&self, buf: &mut Buffer<BL>) -> Result<(), UnmarshalError> {
        let domain_bytes = self.domain.as_bytes();
        let domain_bytes_len = domain_bytes.len().min(127);
        let mut domain_bytes_pad128 = [0_u8; 128];
        domain_bytes_pad128[..domain_bytes_len].copy_from_slice(&domain_bytes[..domain_bytes_len]);
        buf.append_bytes_fixed(&domain_bytes_pad128)?;
        buf.append_bytes(InetAddress::marshal_multiple_to_bytes(self.ip.as_slice()).unwrap().as_slice())?;
        Ok(())
    }

    fn unmarshal<const BL: usize>(buf: &Buffer<BL>, cursor: &mut usize) -> Result<Self, UnmarshalError> {
        let domain_bytes_pad128: &[u8; 128] = buf.read_bytes_fixed(cursor)?;
        let mut domain_bytes_len = 0;
        for i in 0..128 {
            if domain_bytes_pad128[i] == 0 {
                domain_bytes_len = i;
                break;
            }
        }
        if domain_bytes_len == 0 {
            return Err(UnmarshalError::InvalidData);
        }
        Ok(Nameserver {
            ip: InetAddress::unmarshal_multiple(buf, cursor, buf.len())?,
            domain: String::from_utf8_lossy(&domain_bytes_pad128[..domain_bytes_len]).to_string(),
        })
    }
}
