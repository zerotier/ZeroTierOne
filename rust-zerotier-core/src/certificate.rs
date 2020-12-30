use std::ffi::CStr;
use std::intrinsics::write_bytes;
use std::mem::{MaybeUninit, zeroed};
use std::os::raw::{c_char, c_void};
use std::ptr::copy_nonoverlapping;

use serde::{Deserialize, Serialize};

use crate::*;
use crate::bindings::capi as ztcore;

/// Maximum length of a string in a certificate (mostly for the certificate name fields).
pub const CERTIFICATE_MAX_STRING_LENGTH: u32 = ztcore::ZT_CERTIFICATE_MAX_STRING_LENGTH;

/// Certificate local trust bit field flag: this certificate self-signs a root CA.
pub const CERTIFICATE_LOCAL_TRUST_FLAG_ROOT_CA: u32 = ztcore::ZT_CERTIFICATE_LOCAL_TRUST_FLAG_ROOT_CA;

/// Certificate local trust bit field flag: this certificate specifies a set of ZeroTier roots.
pub const CERTIFICATE_LOCAL_TRUST_FLAG_ZEROTIER_ROOT_SET: u32 = ztcore::ZT_CERTIFICATE_LOCAL_TRUST_FLAG_ZEROTIER_ROOT_SET;

/// Length of a NIST P-384 unique ID (public key).
pub const CERTIFICATE_UNIQUE_ID_TYPE_NIST_P_384_SIZE: u32 = ztcore::ZT_CERTIFICATE_UNIQUE_ID_TYPE_NIST_P_384_SIZE;

/// Length of a private key corresponding to a NIST P-384 unique ID.
pub const CERTIFICATE_UNIQUE_ID_TYPE_NIST_P_384_PRIVATE_SIZE: u32 = ztcore::ZT_CERTIFICATE_UNIQUE_ID_TYPE_NIST_P_384_PRIVATE_SIZE;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

pub struct CertificateSerialNo(pub [u8; 48]);

impl CertificateSerialNo {
    pub fn new_from_string(s: &str) -> Result<CertificateSerialNo, ResultCode> {
        let b = hex::decode(s);
        if b.is_err() {
            return Err(ResultCode::ErrorBadParameter);
        }
        return Ok(CertificateSerialNo::from(b.unwrap()));
    }
}

impl From<Vec<u8>> for CertificateSerialNo {
    fn from(v: Vec<u8>) -> CertificateSerialNo {
        let mut l = v.len();
        if l > 48 {
            l = 48;
        }
        unsafe {
            let mut r: [u8; 48] = MaybeUninit::uninit().assume_init();
            copy_nonoverlapping(v.as_ptr(), r.as_mut_ptr(), l);
            while l < 48 {
                r[l] = 0;
                l += 1;
            }
            return CertificateSerialNo(r);
        }
    }
}

impl ToString for CertificateSerialNo {
    fn to_string(&self) -> String {
        hex::encode(self.0)
    }
}

impl serde::Serialize for CertificateSerialNo {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: serde::Serializer {
        serializer.serialize_str(self.to_string().as_str())
    }
}

struct CertificateSerialNoVisitor;

impl<'de> serde::de::Visitor<'de> for CertificateSerialNoVisitor {
    type Value = CertificateSerialNo;

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("CertificateSerialNoVisitor value in string form")
    }

    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E> where E: serde::de::Error {
        let id = CertificateSerialNo::new_from_string(s);
        if id.is_err() {
            return Err(serde::de::Error::invalid_value(serde::de::Unexpected::Str(s), &self));
        }
        return Ok(id.ok().unwrap() as Self::Value);
    }
}

impl<'de> serde::Deserialize<'de> for CertificateSerialNo {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: serde::Deserializer<'de> {
        deserializer.deserialize_str(CertificateSerialNoVisitor)
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Type of certificate subject unique ID
#[derive(FromPrimitive,ToPrimitive)]
pub enum CertificateUniqueIdType {
    NistP384 = ztcore::ZT_CertificateUniqueIdType_ZT_CERTIFICATE_UNIQUE_ID_TYPE_NIST_P_384 as isize
}

impl CertificateUniqueIdType {
    pub fn new_from_string(s: &str) -> Result<CertificateUniqueIdType, ResultCode> {
        if s.to_ascii_lowercase() == "nistp384" {
            return Ok(CertificateUniqueIdType::NistP384);
        }
        return Err(ResultCode::ErrorBadParameter);
    }
}

impl ToString for CertificateUniqueIdType {
    fn to_string(&self) -> String {
        match *self {
            CertificateUniqueIdType::NistP384 => String::from("NistP384")
        }
    }
}

impl serde::Serialize for CertificateUniqueIdType {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: serde::Serializer {
        serializer.serialize_str(self.to_string().as_str())
    }
}

struct CertificateUniqueIdTypeVisitor;

impl<'de> serde::de::Visitor<'de> for CertificateUniqueIdTypeVisitor {
    type Value = CertificateUniqueIdType;

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("CertificateUniqueIdType value in string form")
    }

    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E> where E: serde::de::Error {
        let id = CertificateUniqueIdType::new_from_string(s);
        if id.is_err() {
            return Err(serde::de::Error::invalid_value(serde::de::Unexpected::Str(s), &self));
        }
        return Ok(id.ok().unwrap() as Self::Value);
    }
}

impl<'de> serde::Deserialize<'de> for CertificateUniqueIdType {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: serde::Deserializer<'de> {
        deserializer.deserialize_str(CertificateUniqueIdTypeVisitor)
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Reasons a certificate may be rejected.
#[derive(FromPrimitive,ToPrimitive)]
pub enum CertificateError {
    None = ztcore::ZT_CertificateError_ZT_CERTIFICATE_ERROR_NONE as isize,
    HaveNewerCert = ztcore::ZT_CertificateError_ZT_CERTIFICATE_ERROR_HAVE_NEWER_CERT as isize,
    InvalidFormat = ztcore::ZT_CertificateError_ZT_CERTIFICATE_ERROR_INVALID_FORMAT as isize,
    InvalidIdentity = ztcore::ZT_CertificateError_ZT_CERTIFICATE_ERROR_INVALID_IDENTITY as isize,
    InvalidPrimarySignature = ztcore::ZT_CertificateError_ZT_CERTIFICATE_ERROR_INVALID_PRIMARY_SIGNATURE as isize,
    InvalidChain = ztcore::ZT_CertificateError_ZT_CERTIFICATE_ERROR_INVALID_CHAIN as isize,
    InvalidComponentSignature = ztcore::ZT_CertificateError_ZT_CERTIFICATE_ERROR_INVALID_COMPONENT_SIGNATURE as isize,
    InvalidUniqueIdProof = ztcore::ZT_CertificateError_ZT_CERTIFICATE_ERROR_INVALID_UNIQUE_ID_PROOF as isize,
    MissingRequiredFields = ztcore::ZT_CertificateError_ZT_CERTIFICATE_ERROR_MISSING_REQUIRED_FIELDS as isize,
    OutOfValidTimeWindow = ztcore::ZT_CertificateError_ZT_CERTIFICATE_ERROR_OUT_OF_VALID_TIME_WINDOW as isize,
}

impl ToString for CertificateError {
    fn to_string(&self) -> String {
        String::from(
            match self {
                CertificateError::None => "None",
                CertificateError::HaveNewerCert => "HaveNewerCert",
                CertificateError::InvalidFormat => "InvalidFormat",
                CertificateError::InvalidIdentity => "InavlidIdentity",
                CertificateError::InvalidPrimarySignature => "InvalidPrimarySignature",
                CertificateError::InvalidChain => "InvalidChain",
                CertificateError::InvalidComponentSignature => "InvalidComponentSignature",
                CertificateError::InvalidUniqueIdProof => "InvalidUniqueIdProof",
                CertificateError::MissingRequiredFields => "MissingRequiredFields",
                CertificateError::OutOfValidTimeWindow => "OutOfValidTimeWindow"
            }
        )
    }
}

#[allow(non_snake_case)]
#[derive(Serialize, Deserialize)]
pub struct CertificateName {
    pub serialNo: String,
    pub commonName: String,
    pub country: String,
    pub organization: String,
    pub unit: String,
    pub locality: String,
    pub province: String,
    pub streetAddress: String,
    pub postalCode: String,
    pub email: String,
    pub url: String,
    pub host: String
}

#[derive(Serialize, Deserialize)]
pub struct CertificateNetwork {
    pub id: NetworkId,
    pub controller: Fingerprint
}

#[derive(Serialize, Deserialize)]
pub struct CertificateIdentity {
    pub identity: Identity,
    pub locator: Locator
}

#[derive(Serialize, Deserialize)]
pub struct CertificateSubjectUniqueIdSecret {
    pub public: Vec<u8>,
    pub private: Vec<u8>,
    pub type_: CertificateUniqueIdType
}

#[allow(non_snake_case)]
#[derive(Serialize, Deserialize)]
pub struct CertificateSubject {
    pub timestamp: i64,
    pub identities: Vec<CertificateIdentity>,
    pub networks: Vec<CertificateNetwork>,
    pub certificates: Vec<CertificateSerialNo>,
    pub updateURLs: Vec<String>,
    pub name: CertificateName,
    pub uniqueId: Vec<u8>,
    pub uniqueIdProofSignature: Vec<u8>
}

#[allow(non_snake_case)]
#[derive(Serialize, Deserialize)]
pub struct Certificate {
    pub serialNo: CertificateSerialNo,
    pub flags: u64,
    pub timestamp: i64,
    pub validity: [i64; 2],
    pub subject: CertificateSubject,
    pub issuer: Identity,
    pub issuerName: CertificateName,
    pub extendedAttributes: Vec<u8>,
    pub maxPathLength: u32,
    pub crl: Vec<CertificateSerialNo>,
    pub signature: Vec<u8>
}

impl CertificateName {
    pub(crate) fn new_from_capi(cn: &ztcore::ZT_Certificate_Name) -> CertificateName {
        unsafe {
            return CertificateName {
                serialNo: String::from(CStr::from_ptr(cn.serialNo.as_ptr()).to_str().unwrap()),
                commonName: String::from(CStr::from_ptr(cn.commonName.as_ptr()).to_str().unwrap()),
                country: String::from(CStr::from_ptr(cn.country.as_ptr()).to_str().unwrap()),
                organization: String::from(CStr::from_ptr(cn.organization.as_ptr()).to_str().unwrap()),
                unit: String::from(CStr::from_ptr(cn.unit.as_ptr()).to_str().unwrap()),
                locality: String::from(CStr::from_ptr(cn.locality.as_ptr()).to_str().unwrap()),
                province: String::from(CStr::from_ptr(cn.province.as_ptr()).to_str().unwrap()),
                streetAddress: String::from(CStr::from_ptr(cn.streetAddress.as_ptr()).to_str().unwrap()),
                postalCode: String::from(CStr::from_ptr(cn.postalCode.as_ptr()).to_str().unwrap()),
                email: String::from(CStr::from_ptr(cn.email.as_ptr()).to_str().unwrap()),
                url: String::from(CStr::from_ptr(cn.url.as_ptr()).to_str().unwrap()),
                host: String::from(CStr::from_ptr(cn.host.as_ptr()).to_str().unwrap())
            };
        }
    }
}

impl CertificateNetwork {
    pub(crate) fn new_from_capi(cn: &ztcore::ZT_Certificate_Network) -> CertificateNetwork {
        CertificateNetwork{
            id: NetworkId(cn.id),
            controller: Fingerprint{
                address: Address(cn.controller.address),
                hash: cn.controller.hash
            }
        }
    }
}

impl CertificateIdentity {
    pub(crate) fn new_from_capi(ci: &ztcore::ZT_Certificate_Identity) -> CertificateIdentity {
        CertificateIdentity{
            identity: Identity::new_from_capi(ci.identity, false).clone(),
            locator: Locator::new_from_capi(ci.locator, false).clone()
        }
    }
}

impl CertificateSubject {
    pub(crate) fn new_from_capi(cs: &ztcore::ZT_Certificate_Subject) -> CertificateSubject {
        unsafe {
            let cidentities: &[ztcore::ZT_Certificate_Identity] = std::slice::from_raw_parts(cs.identities, cs.identityCount as usize);
            let mut identities: Vec<CertificateIdentity> = Vec::new();
            for i in cidentities.iter() {
                identities.push(CertificateIdentity::new_from_capi(i));
            }

            let cnetworks: &[ztcore::ZT_Certificate_Network] = std::slice::from_raw_parts(cs.networks, cs.networkCount as usize);
            let mut networks: Vec<CertificateNetwork> = Vec::new();
            for i in cnetworks.iter() {
                networks.push(CertificateNetwork::new_from_capi(i));
            }

            let ccertificates: &[*const u8] = std::slice::from_raw_parts(cs.certificates, cs.certificateCount as usize);
            let mut certificates: Vec<CertificateSerialNo> = Vec::new();
            let mut ctmp: [u8; 48] = [0; 48];
            for i in ccertificates.iter() {
                copy_nonoverlapping(*i, ctmp.as_mut_ptr(), 48);
                certificates.push(CertificateSerialNo(ctmp));
            }

            let cupdate_urls: &[*const c_char] = std::slice::from_raw_parts(cs.updateURLs, cs.updateURLCount as usize);
            let mut update_urls: Vec<String> = Vec::new();
            for i in cupdate_urls.iter() {
                update_urls.push(CStr::from_ptr(*i).to_str().unwrap().to_string());
            }

            return CertificateSubject{
                timestamp: cs.timestamp,
                identities: identities,
                networks: networks,
                certificates: certificates,
                updateURLs: update_urls,
                name: CertificateName::new_from_capi(&cs.name),
                uniqueId: Vec::from(std::slice::from_raw_parts(cs.uniqueId, cs.uniqueIdSize as usize)),
                uniqueIdProofSignature: Vec::from(std::slice::from_raw_parts(cs.uniqueIdProofSignature, cs.uniqueIdProofSignatureSize as usize))
            }
        }
    }
}

impl Certificate {
    pub(crate) fn new_from_capi(c: &ztcore::ZT_Certificate) -> Certificate {
        unsafe {
            let ccrl: &[*const u8] = std::slice::from_raw_parts(c.crl, c.crlCount as usize);
            let mut crl: Vec<CertificateSerialNo> = Vec::new();
            let mut ctmp: [u8; 48] = [0; 48];
            for i in ccrl.iter() {
                copy_nonoverlapping(*i, ctmp.as_mut_ptr(), 48);
                crl.push(CertificateSerialNo(ctmp));
            }

            return Certificate{
                serialNo: CertificateSerialNo(c.serialNo),
                flags: c.flags,
                timestamp: c.timestamp,
                validity: c.validity,
                subject: CertificateSubject::new_from_capi(&c.subject),
                issuer: Identity::new_from_capi(c.issuer, false),
                issuerName: CertificateName::new_from_capi(&c.issuerName),
                extendedAttributes: Vec::from(std::slice::from_raw_parts(c.extendedAttributes, c.extendedAttributesSize as usize)),
                maxPathLength: c.maxPathLength as u32,
                crl: crl,
                signature: Vec::from(std::slice::from_raw_parts(c.signature, c.signatureSize as usize))
            }
        }
    }
}

impl CertificateSubjectUniqueIdSecret {
    pub fn new(t: CertificateUniqueIdType) -> Self {
        unsafe {
            let mut unique_id: [u8; 128] = zeroed();
            let mut unique_id_private: [u8; 128] = zeroed();
            let mut unique_id_size: c_int = unique_id.len() as c_int;
            let mut unique_id_private_size: c_int = unique_id_private.len() as c_int;
            let ct: ztcore::ZT_CertificateUniqueIdType = num_traits::ToPrimitive::to_u32(&t).unwrap();
            if ztcore::ZT_Certificate_newSubjectUniqueId(ct, unique_id.as_mut_ptr() as *mut c_void, &mut unique_id_size as *mut c_int, unique_id_private.as_mut_ptr() as *mut c_void, &mut unique_id_private_size as *mut c_int) != 0 {
                panic!("fatal internal error: ZT_Certificate_newSubjectUniqueId failed.");
            }
            return CertificateSubjectUniqueIdSecret{
                public: Vec::from(&unique_id[0..unique_id_size as usize]),
                private: Vec::from(&unique_id_private[0..unique_id_private_size as usize]),
                type_: num_traits::FromPrimitive::from_u32(ct as u32).unwrap()
            };
        }
    }
}

implement_to_from_json!(CertificateName);
implement_to_from_json!(CertificateNetwork);
implement_to_from_json!(CertificateIdentity);
implement_to_from_json!(CertificateSubject);
implement_to_from_json!(Certificate);
implement_to_from_json!(CertificateSubjectUniqueIdSecret);
