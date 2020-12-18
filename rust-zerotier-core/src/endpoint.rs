use crate::*;
use crate::bindings::capi as ztcore;
use num_traits::FromPrimitive;
use std::os::raw::{c_char, c_int};
use std::ffi::CStr;
use std::mem::MaybeUninit;

pub struct Endpoint {
    pub type_: EndpointType,
    capi: ztcore::ZT_Endpoint
}

impl Endpoint {
    #[inline]
    pub(crate) fn new_from_capi(ep: &ztcore::ZT_Endpoint) -> Endpoint {
        return Endpoint{
            type_: EndpointType::from_u32(ep.type_ as u32).unwrap(),
            capi: *ep
        };
    }

    pub fn new_from_string(s: &str) -> Result<Endpoint, ResultCode> {
        unsafe {
            let mut cep: MaybeUninit<ztcore::ZT_Endpoint> = MaybeUninit::uninit();
            let ec = ztcore::ZT_Endpoint_fromString(cep.as_mut_ptr(), s.as_ptr() as *const c_char) as i32;
            if ec == 0 {
                let epi = cep.assume_init();
                return Ok(Endpoint{
                    type_: EndpointType::from_u32(epi.type_ as u32).unwrap(),
                    capi: epi
                });
            }
            return Err(ResultCode::from_i32(ec).unwrap());
        }
    }
}

impl ToString for Endpoint {
    fn to_string(&self) -> String {
        let mut buf: [u8; 1024] = [0; 1024];
        unsafe {
            if ztcore::ZT_Endpoint_toString(&(self.capi) as *const ztcore::ZT_Endpoint, buf.as_mut_ptr() as *mut c_char, buf.len() as c_int).is_null() {
                return String::from("(invalid)");
            }
            return String::from(CStr::from_bytes_with_nul(buf.as_ref()).unwrap().to_str().unwrap());
        }
    }
}

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
