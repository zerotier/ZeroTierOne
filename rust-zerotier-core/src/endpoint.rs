/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

use std::ffi::CString;
use std::mem::MaybeUninit;
use std::os::raw::{c_char, c_int, c_void};

use num_derive::{FromPrimitive, ToPrimitive};
use num_traits::FromPrimitive;

use crate::*;
use crate::bindings::capi as ztcore;

#[derive(FromPrimitive,ToPrimitive)]
pub enum EndpointType {
    Nil = ztcore::ZT_EndpointType_ZT_ENDPOINT_TYPE_NIL as isize,
    ZeroTier = ztcore::ZT_EndpointType_ZT_ENDPOINT_TYPE_ZEROTIER as isize,
    Ethernet = ztcore::ZT_EndpointType_ZT_ENDPOINT_TYPE_ETHERNET as isize,
    WifiDirect = ztcore::ZT_EndpointType_ZT_ENDPOINT_TYPE_WIFI_DIRECT as isize,
    Bluetooth = ztcore::ZT_EndpointType_ZT_ENDPOINT_TYPE_BLUETOOTH as isize,
    Ip = ztcore::ZT_EndpointType_ZT_ENDPOINT_TYPE_IP as isize,
    IpUdp = ztcore::ZT_EndpointType_ZT_ENDPOINT_TYPE_IP_UDP as isize,
    IpTcp = ztcore::ZT_EndpointType_ZT_ENDPOINT_TYPE_IP_TCP as isize,
    IpHttp = ztcore::ZT_EndpointType_ZT_ENDPOINT_TYPE_IP_HTTP as isize,
}

pub struct Endpoint {
    pub type_: EndpointType,
    capi: ztcore::ZT_Endpoint
}

impl Endpoint {
    pub(crate) fn new_from_capi(ep: &ztcore::ZT_Endpoint) -> Endpoint {
        return Endpoint{
            type_: EndpointType::from_i32(ep.type_ as i32).unwrap(),
            capi: *ep
        };
    }

    pub fn new_from_string(s: &str) -> Result<Endpoint, ResultCode> {
        let cs = CString::new(s);
        if cs.is_err() {
            return Err(ResultCode::ErrorBadParameter);
        }
        let cs = cs.unwrap();
        unsafe {
            let mut cep: MaybeUninit<ztcore::ZT_Endpoint> = MaybeUninit::uninit();
            let ec = ztcore::ZT_Endpoint_fromString(cep.as_mut_ptr(), cs.as_ptr()) as i32;
            if ec == 0 {
                let epi = cep.assume_init();
                return Ok(Endpoint{
                    type_: EndpointType::from_i32(epi.type_ as i32).unwrap(),
                    capi: epi
                });
            }
            return Err(ResultCode::from_i32(ec).unwrap());
        }
    }

    /// Get a reference to the InetAddress in this endpoint or None if this is not of a relevant type.
    pub fn as_inetaddress(&self) -> Option<&InetAddress> {
        match self.type_ {
            EndpointType::Ip | EndpointType::IpUdp | EndpointType::IpTcp | EndpointType::IpHttp => {
                unsafe {
                    Some(InetAddress::transmute_capi(&self.capi.value.ia))
                }
            },
            _ => None
        }
    }
}

impl ToString for Endpoint {
    fn to_string(&self) -> String {
        unsafe {
            let mut buf: MaybeUninit<[c_char; 1024]> = MaybeUninit::uninit();
            if ztcore::ZT_Endpoint_toString(&(self.capi) as *const ztcore::ZT_Endpoint, (*buf.as_mut_ptr()).as_mut_ptr(), 1024).is_null() {
                return String::from("(invalid)");
            }
            return cstr_to_string((*buf.as_ptr()).as_ptr(), 1024);
        }
    }
}

impl PartialEq for Endpoint {
    fn eq(&self, other: &Endpoint) -> bool {
        self.to_string() == other.to_string()
    }
}

impl Eq for Endpoint {}

impl serde::Serialize for Endpoint {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: serde::Serializer {
        serializer.serialize_str(self.to_string().as_str())
    }
}

struct EndpointVisitor;

impl<'de> serde::de::Visitor<'de> for EndpointVisitor {
    type Value = Endpoint;

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("Endpoint value in string form")
    }

    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E> where E: serde::de::Error {
        let id = Endpoint::new_from_string(s);
        if id.is_err() {
            return Err(serde::de::Error::invalid_value(serde::de::Unexpected::Str(s), &self));
        }
        return Ok(id.ok().unwrap() as Self::Value);
    }
}

impl<'de> serde::Deserialize<'de> for Endpoint {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: serde::Deserializer<'de> {
        deserializer.deserialize_str(EndpointVisitor)
    }
}
