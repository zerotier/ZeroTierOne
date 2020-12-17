use crate::*;
use crate::bindings::capi as ztcore;
use std::ffi::CStr;
use std::ptr::copy_nonoverlapping;
use std::os::raw::{c_char, c_void};
use serde::{Deserialize, Serialize};

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
    pub public: Box<[u8]>,
    pub private: Box<[u8]>,
    pub type_: CertificateUniqueIdType
}

#[allow(non_snake_case)]
#[derive(Serialize, Deserialize)]
pub struct CertificateSubject {
    pub timestamp: i64,
    pub identities: Box<[CertificateIdentity]>,
    pub networks: Box<[CertificateNetwork]>,
    pub certificates: Box<[Box<[u8]>]>,
    pub updateURLs: Box<[String]>,
    pub name: CertificateName,
    pub uniqueId: Box<[u8]>,
    pub uniqueIdProofSignature: Box<[u8]>
}

#[allow(non_snake_case)]
#[derive(Serialize, Deserialize)]
pub struct Certificate {
    pub serialNo: Box<[u8]>,
    pub flags: u64,
    pub timestamp: i64,
    pub validity: [i64; 2],
    pub subject: CertificateSubject,
    pub issuer: Identity,
    pub issuerName: CertificateName,
    pub extendedAttributes: Box<[u8]>,
    pub maxPathLength: u32,
    pub crl: Box<[Box<[u8]>]>,
    pub signature: Box<[u8]>
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
            identity: Identity::new_from_capi(ci.identity, false),
            locator: Locator::new_from_capi(ci.locator, false)
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
            let mut certificates: Vec<Box<[u8]>> = Vec::new();
            let mut ctmp: [u8; 48] = [0; 48];
            for i in ccertificates.iter() {
                copy_nonoverlapping(*i, ctmp.as_mut_ptr(), 48);
                certificates.push(Box::from(ctmp));
            }

            let cupdate_urls: &[*const c_char] = std::slice::from_raw_parts(cs.updateURLs, cs.updateURLCount as usize);
            let mut update_urls: Vec<String> = Vec::new();
            for i in cupdate_urls.iter() {
                update_urls.push(CStr::from_ptr(*i).to_str().unwrap().to_string());
            }

            return CertificateSubject{
                timestamp: cs.timestamp,
                identities: identities.into_boxed_slice(),
                networks: networks.into_boxed_slice(),
                certificates: certificates.into_boxed_slice(),
                updateURLs: update_urls.into_boxed_slice(),
                name: CertificateName::new_from_capi(&cs.name),
                uniqueId: Box::from(std::slice::from_raw_parts(cs.uniqueId, cs.uniqueIdSize as usize).clone()),
                uniqueIdProofSignature: Box::from(std::slice::from_raw_parts(cs.uniqueIdProofSignature, cs.uniqueIdProofSignatureSize as usize).clone())
            }
        }
    }
}

impl Certificate {
    pub(crate) fn new_from_capi(c: &ztcore::ZT_Certificate) -> Certificate {
        unsafe {
            let cextended_attributes: &[u8] = std::slice::from_raw_parts(c.extendedAttributes, c.extendedAttributesSize as usize);
            let mut extended_attributes: Vec<u8> = Vec::new();
            extended_attributes.extend(cextended_attributes.iter());

            let ccrl: &[*const u8] = std::slice::from_raw_parts(c.crl, c.crlCount as usize);
            let mut crl: Vec<Box<[u8]>> = Vec::new();
            let mut ctmp: [u8; 48] = [0; 48];
            for i in ccrl.iter() {
                copy_nonoverlapping(*i, ctmp.as_mut_ptr(), 48);
                crl.push(Box::from(ctmp));
            }

            let csignature: &[u8] = std::slice::from_raw_parts(c.signature, c.signatureSize as usize);
            let mut signature: Vec<u8> = Vec::new();
            signature.extend(csignature.iter());

            return Certificate{
                serialNo: Box::from(c.serialNo),
                flags: c.flags,
                timestamp: c.timestamp,
                validity: c.validity,
                subject: CertificateSubject::new_from_capi(&c.subject),
                issuer: Identity::new_from_capi(c.issuer, false),
                issuerName: CertificateName::new_from_capi(&c.issuerName),
                extendedAttributes: extended_attributes.into_boxed_slice(),
                maxPathLength: c.maxPathLength as u32,
                crl: crl.into_boxed_slice(),
                signature: signature.into_boxed_slice()
            }
        }
    }
}

impl CertificateSubjectUniqueIdSecret {
    pub fn new(t: CertificateUniqueIdType) -> Self {
        unsafe {
            let mut unique_id: Vec<u8> = Vec::new();
            let mut unique_id_private: Vec<u8> = Vec::new();
            unique_id.resize(128, 0);
            unique_id_private.resize(128, 0);
            let mut unique_id_size: c_int = 128;
            let mut unique_id_private_size: c_int = 128;
            let ct: ztcore::ZT_CertificateUniqueIdType = num_traits::ToPrimitive::to_u32(&t).unwrap();
            if ztcore::ZT_Certificate_newSubjectUniqueId(ct, unique_id.as_mut_ptr() as *mut c_void, &mut unique_id_size as *mut c_int, unique_id_private.as_mut_ptr() as *mut c_void, &mut unique_id_private_size as *mut c_int) != 0 {
                panic!("fatal internal error: ZT_Certificate_newSubjectUniqueId failed.");
            }
            unique_id.resize(unique_id_size as usize, 0);
            unique_id_private.resize(unique_id_private_size as usize, 0);
            return CertificateSubjectUniqueIdSecret{
                public: unique_id.into_boxed_slice(),
                private: unique_id_private.into_boxed_slice(),
                type_: num_traits::FromPrimitive::from_u32(ct as u32).unwrap()
            };
        }
    }
}


implement_json_serializable!(CertificateName);
implement_json_serializable!(CertificateNetwork);
implement_json_serializable!(CertificateIdentity);
implement_json_serializable!(CertificateSubject);
implement_json_serializable!(Certificate);
implement_json_serializable!(CertificateSubjectUniqueIdSecret);
