// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::fmt::Debug;
use std::str::FromStr;

use crate::vl1::{Address, PartialAddress};

use serde::{Deserialize, Deserializer, Serialize, Serializer};

use zerotier_utils::error::InvalidParameterError;
use zerotier_utils::hex;

#[derive(Clone, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub enum NetworkId {
    // Legacy network ID consisting of 40-bit partial address and 24-bit network number.
    Legacy(u64),
    // Full length network ID consisting of 384-bit address and 24-bit network number.
    Full(Address, u32),
}

impl NetworkId {
    pub fn to_bytes(&self) -> Vec<u8> {
        match self {
            Self::Legacy(nwid) => nwid.to_be_bytes().to_vec(),
            Self::Full(controller, nw) => {
                let mut tmp = [0u8; Address::SIZE_BYTES + 4];
                tmp[..Address::SIZE_BYTES].copy_from_slice(controller.as_bytes());
                tmp[Address::SIZE_BYTES..].copy_from_slice(&nw.to_be_bytes());
                tmp.to_vec()
            }
        }
    }

    pub fn from_bytes(b: &[u8]) -> Result<Self, InvalidParameterError> {
        if b.len() == 8 {
            Self::from_legacy_u64(u64::from_be_bytes(b.try_into().unwrap()))
        } else if b.len() == Address::SIZE_BYTES + 4 {
            Ok(Self::Full(
                Address::from_bytes(&b[..Address::SIZE_BYTES])?,
                u32::from_be_bytes(b[Address::SIZE_BYTES..].try_into().unwrap()),
            ))
        } else {
            Err(InvalidParameterError("invalid network ID"))
        }
    }

    pub fn from_legacy_u64(nwid: u64) -> Result<Self, InvalidParameterError> {
        let _ = PartialAddress::from_legacy_address_u64(nwid)?; // check validity of address portion
        Ok(Self::Legacy(nwid))
    }

    /// Get the legacy 40-bit partial controller address from this network ID.
    pub(crate) fn legacy_controller_address(&self) -> PartialAddress {
        match self {
            Self::Legacy(nwid) => PartialAddress::from_legacy_address_u64(nwid.wrapping_shr(24)).unwrap(),
            Self::Full(controller, _) => PartialAddress::from_bytes(&controller.as_bytes()[..PartialAddress::LEGACY_SIZE_BYTES]).unwrap(),
        }
    }

    /// Convert this into a legacy network ID in u64 form, or return itself if already a legacy ID.
    pub(crate) fn to_legacy_u64(&self) -> u64 {
        match self {
            Self::Legacy(nwid) => *nwid,
            Self::Full(controller, nw) => controller.legacy_u64().wrapping_shl(24) | ((*nw & 0xffffff) as u64),
        }
    }
}

impl ToString for NetworkId {
    fn to_string(&self) -> String {
        match self {
            Self::Legacy(nwid) => hex::to_string_u64(*nwid, false),
            Self::Full(controller, nw) => format!("{:08x}@{}", *nw, controller.to_string()),
        }
    }
}

impl FromStr for NetworkId {
    type Err = InvalidParameterError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        if s.len() == 16 {
            Self::from_legacy_u64(hex::from_string_u64(s))
        } else {
            let mut fno = 0;
            let mut net_no = 0;
            let mut controller = None;
            for ss in s.split('@') {
                if fno == 0 {
                    net_no = hex::from_string_u64(ss);
                } else if fno == 1 {
                    controller = Some(Address::from_str(ss)?);
                } else {
                    return Err(InvalidParameterError("invalid network ID"));
                }
                fno += 1;
            }
            if let Some(controller) = controller {
                return Ok(Self::Full(controller, net_no as u32));
            } else {
                return Err(InvalidParameterError("invalid network ID"));
            }
        }
    }
}

impl Debug for NetworkId {
    #[inline(always)]
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(self.to_string().as_str())
    }
}

impl Serialize for NetworkId {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        if serializer.is_human_readable() {
            serializer.serialize_str(self.to_string().as_str())
        } else {
            serializer.serialize_bytes(self.to_bytes().as_slice())
        }
    }
}

struct NetworkIdVisitor;

impl<'de> serde::de::Visitor<'de> for NetworkIdVisitor {
    type Value = NetworkId;

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("network ID")
    }

    fn visit_bytes<E>(self, v: &[u8]) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        NetworkId::from_bytes(v).map_err(|_| E::custom("invalid network ID"))
    }

    fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        NetworkId::from_str(v).map_err(|_| E::custom("invalid network ID"))
    }
}

impl<'de> Deserialize<'de> for NetworkId {
    fn deserialize<D>(deserializer: D) -> Result<NetworkId, D::Error>
    where
        D: Deserializer<'de>,
    {
        if deserializer.is_human_readable() {
            deserializer.deserialize_str(NetworkIdVisitor)
        } else {
            deserializer.deserialize_bytes(NetworkIdVisitor)
        }
    }
}
