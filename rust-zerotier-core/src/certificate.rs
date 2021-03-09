/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

use std::ffi::CString;
use std::hash::{Hash, Hasher};
use std::mem::zeroed;
use std::os::raw::{c_char, c_uint, c_void};
use std::ptr::{copy_nonoverlapping, null, null_mut};

use num_derive::{FromPrimitive, ToPrimitive};
use num_traits::FromPrimitive;
use serde::{Deserialize, Serialize};

use crate::*;
use crate::capi as ztcore;

/// Maximum length of a string in a certificate (mostly for the certificate name fields).
pub const CERTIFICATE_MAX_STRING_LENGTH: isize = ztcore::ZT_CERTIFICATE_MAX_STRING_LENGTH as isize;

/// Certificate local trust bit field flag: this certificate self-signs a root CA.
pub const CERTIFICATE_LOCAL_TRUST_FLAG_ROOT_CA: u32 = ztcore::ZT_CERTIFICATE_LOCAL_TRUST_FLAG_ROOT_CA;

/// Certificate local trust bit field flag: this certificate specifies a set of ZeroTier roots.
pub const CERTIFICATE_LOCAL_TRUST_FLAG_ZEROTIER_ROOT_SET: u32 = ztcore::ZT_CERTIFICATE_LOCAL_TRUST_FLAG_ZEROTIER_ROOT_SET;

/// Length of a NIST P-384 unique ID (public key).
pub const CERTIFICATE_UNIQUE_ID_TYPE_NIST_P_384_SIZE: u32 = ztcore::ZT_CERTIFICATE_UNIQUE_ID_TYPE_NIST_P_384_SIZE;

/// Length of a private key corresponding to a NIST P-384 unique ID.
pub const CERTIFICATE_UNIQUE_ID_TYPE_NIST_P_384_PRIVATE_SIZE: u32 = ztcore::ZT_CERTIFICATE_UNIQUE_ID_TYPE_NIST_P_384_PRIVATE_SIZE;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#[derive(PartialEq, Eq, PartialOrd, Ord)]
pub struct CertificateSerialNo(pub [u8; 48]);

impl CertificateSerialNo {
    #[inline(always)]
    pub fn new() -> CertificateSerialNo { CertificateSerialNo([0; 48]) }
    pub fn new_from_string(s: &str) -> Result<CertificateSerialNo, ResultCode> { hex::decode(s).map_or_else(|_| { Err(ResultCode::ErrorBadParameter) }, |b| { Ok(CertificateSerialNo::from(b.unwrap().as_slice())) }) }
}

impl From<&[u8; 48]> for CertificateSerialNo {
    #[inline(always)]
    fn from(a: &[u8; 48]) -> CertificateSerialNo { CertificateSerialNo(*a) }
}

impl Hash for CertificateSerialNo {
    #[inline(always)]
    fn hash<H: Hasher>(&self, state: &mut H) { self.0.hash(state); }
}

impl ToString for CertificateSerialNo {
    #[inline(always)]
    fn to_string(&self) -> String { hex::encode(self.0) }
}

impl serde::Serialize for CertificateSerialNo {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: serde::Serializer { serializer.serialize_str(self.to_string().as_str()) }
}
struct CertificateSerialNoVisitor;
impl<'de> serde::de::Visitor<'de> for CertificateSerialNoVisitor {
    type Value = CertificateSerialNo;
    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result { formatter.write_str("object") }
    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E> where E: serde::de::Error { Self::Value::new_from_string(s).map_or_else(|_| { Err(serde::de::Error::invalid_value(serde::de::Unexpected::Str(s), &self)) },|id| { Ok(id as Self::Value) }) }
}
impl<'de> serde::Deserialize<'de> for CertificateSerialNo {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: serde::Deserializer<'de> { deserializer.deserialize_str(CertificateSerialNoVisitor) }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Type of certificate subject unique ID
#[derive(FromPrimitive, ToPrimitive, PartialEq, Eq)]
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
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: serde::Serializer { serializer.serialize_str(self.to_string().as_str()) }
}
struct CertificateUniqueIdTypeVisitor;
impl<'de> serde::de::Visitor<'de> for CertificateUniqueIdTypeVisitor {
    type Value = CertificateUniqueIdType;
    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result { formatter.write_str("object") }
    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E> where E: serde::de::Error { Self::Value::new_from_string(s).map_or_else(|_| { Err(serde::de::Error::invalid_value(serde::de::Unexpected::Str(s), &self)) },|id| { Ok(id as Self::Value) }) }
}
impl<'de> serde::Deserialize<'de> for CertificateUniqueIdType {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: serde::Deserializer<'de> { deserializer.deserialize_str(CertificateUniqueIdTypeVisitor) }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#[derive(Serialize, Deserialize, PartialEq, Eq)]
pub struct CertificateSubjectUniqueIdSecret {
    pub public: Vec<u8>,
    pub private: Vec<u8>,
    #[serde(rename = "type")]
    pub type_: CertificateUniqueIdType,
}

const CERTIFICATE_UNIQUE_ID_CREATE_BUF_SIZE: usize = 128;

impl CertificateSubjectUniqueIdSecret {
    pub fn new(t: CertificateUniqueIdType) -> Self {
        let mut unique_id: [u8; CERTIFICATE_UNIQUE_ID_CREATE_BUF_SIZE] = [0; CERTIFICATE_UNIQUE_ID_CREATE_BUF_SIZE];
        let mut unique_id_private: [u8; CERTIFICATE_UNIQUE_ID_CREATE_BUF_SIZE] = [0; CERTIFICATE_UNIQUE_ID_CREATE_BUF_SIZE];
        let mut unique_id_size = CERTIFICATE_UNIQUE_ID_CREATE_BUF_SIZE as c_int;
        let mut unique_id_private_size = CERTIFICATE_UNIQUE_ID_CREATE_BUF_SIZE as c_int;
        let ct: ztcore::ZT_CertificateUniqueIdType = num_traits::ToPrimitive::to_u32(&t).unwrap();
        unsafe {
            if ztcore::ZT_Certificate_newSubjectUniqueId(ct, unique_id.as_mut_ptr() as *mut c_void, &mut unique_id_size, unique_id_private.as_mut_ptr() as *mut c_void, &mut unique_id_private_size) != 0 {
                panic!("fatal internal error: ZT_Certificate_newSubjectUniqueId failed.");
            }
        }
        CertificateSubjectUniqueIdSecret {
            public: Vec::from(&unique_id[0..unique_id_size as usize]),
            private: Vec::from(&unique_id_private[0..unique_id_private_size as usize]),
            type_: num_traits::FromPrimitive::from_i32(ct as i32).unwrap(),
        }
    }
}

implement_to_from_json!(CertificateSubjectUniqueIdSecret);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#[derive(FromPrimitive, ToPrimitive, PartialEq, Eq)]
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
                CertificateError::InvalidIdentity => "InvalidIdentity",
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

impl<S: AsRef<str>> From<S> for CertificateError {
    fn from(s: S) -> CertificateError {
        match s.as_ref().to_ascii_lowercase().as_str() {
            "havenewercert" => CertificateError::HaveNewerCert,
            "invalidformat" => CertificateError::InvalidFormat,
            "invalididentity" => CertificateError::InvalidIdentity,
            "invalidprimarysignature" => CertificateError::InvalidPrimarySignature,
            "invalidchain" => CertificateError::InvalidChain,
            "invalidcomponentsignature" => CertificateError::InvalidComponentSignature,
            "invaliduniqueidproof" => CertificateError::InvalidUniqueIdProof,
            "missingrequiredfields" => CertificateError::MissingRequiredFields,
            "outofvalidtimewindow" => CertificateError::OutOfValidTimeWindow,
            _ => CertificateError::None // also "none"
        }
    }
}

impl serde::Serialize for CertificateError {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: serde::Serializer { serializer.serialize_str(self.to_string().as_str()) }
}
struct CertificateErrorVisitor;
impl<'de> serde::de::Visitor<'de> for CertificateErrorVisitor {
    type Value = CertificateError;
    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result { formatter.write_str("object") }
    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E> where E: serde::de::Error { return Ok(CertificateError::from(s)); }
}
impl<'de> serde::Deserialize<'de> for CertificateError {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: serde::Deserializer<'de> { deserializer.deserialize_str(CertificateErrorVisitor) }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#[derive(Serialize, Deserialize, PartialEq, Eq)]
pub struct CertificateName {
    #[serde(rename = "serialNo")]
    pub serial_no: String,
    #[serde(rename = "commonName")]
    pub common_name: String,
    pub country: String,
    pub organization: String,
    pub unit: String,
    pub locality: String,
    pub province: String,
    #[serde(rename = "streetAddress")]
    pub street_address: String,
    #[serde(rename = "postalCode")]
    pub postal_code: String,
    pub email: String,
    pub url: String,
    pub host: String,
}

impl CertificateName {
    pub fn new() -> CertificateName {
        CertificateName {
            serial_no: String::new(),
            common_name: String::new(),
            country: String::new(),
            organization: String::new(),
            unit: String::new(),
            locality: String::new(),
            province: String::new(),
            street_address: String::new(),
            postal_code: String::new(),
            email: String::new(),
            url: String::new(),
            host: String::new(),
        }
    }

    pub(crate) unsafe fn new_from_capi(cn: &ztcore::ZT_Certificate_Name) -> CertificateName {
        return CertificateName {
            serial_no: cstr_to_string(cn.serialNo.as_ptr(), CERTIFICATE_MAX_STRING_LENGTH - 1),
            common_name: cstr_to_string(cn.commonName.as_ptr(), CERTIFICATE_MAX_STRING_LENGTH - 1),
            country: cstr_to_string(cn.country.as_ptr(), CERTIFICATE_MAX_STRING_LENGTH - 1),
            organization: cstr_to_string(cn.organization.as_ptr(), CERTIFICATE_MAX_STRING_LENGTH - 1),
            unit: cstr_to_string(cn.unit.as_ptr(), CERTIFICATE_MAX_STRING_LENGTH - 1),
            locality: cstr_to_string(cn.locality.as_ptr(), CERTIFICATE_MAX_STRING_LENGTH - 1),
            province: cstr_to_string(cn.province.as_ptr(), CERTIFICATE_MAX_STRING_LENGTH - 1),
            street_address: cstr_to_string(cn.streetAddress.as_ptr(), CERTIFICATE_MAX_STRING_LENGTH - 1),
            postal_code: cstr_to_string(cn.postalCode.as_ptr(), CERTIFICATE_MAX_STRING_LENGTH - 1),
            email: cstr_to_string(cn.email.as_ptr(), CERTIFICATE_MAX_STRING_LENGTH - 1),
            url: cstr_to_string(cn.url.as_ptr(), CERTIFICATE_MAX_STRING_LENGTH - 1),
            host: cstr_to_string(cn.host.as_ptr(), CERTIFICATE_MAX_STRING_LENGTH - 1),
        };
    }

    fn str_to_cert_cstr(s: &String, cs: &mut [c_char; 128]) {
        let mut l = s.len();
        if l == 0 {
            cs[0] = 0;
            return;
        }
        if l > 126 {
            l = 126;
        }
        unsafe {
            copy_nonoverlapping(s.as_ptr(), cs.as_mut_ptr() as *mut u8, l);
        }
        cs[l + 1] = 0;
    }

    pub(crate) unsafe fn to_capi(&self) -> ztcore::ZT_Certificate_Name {
        let mut cn: ztcore::ZT_Certificate_Name = zeroed();
        Self::str_to_cert_cstr(&self.serial_no, &mut cn.serialNo);
        Self::str_to_cert_cstr(&self.common_name, &mut cn.commonName);
        Self::str_to_cert_cstr(&self.country, &mut cn.country);
        Self::str_to_cert_cstr(&self.organization, &mut cn.organization);
        Self::str_to_cert_cstr(&self.unit, &mut cn.unit);
        Self::str_to_cert_cstr(&self.locality, &mut cn.locality);
        Self::str_to_cert_cstr(&self.province, &mut cn.province);
        Self::str_to_cert_cstr(&self.street_address, &mut cn.streetAddress);
        Self::str_to_cert_cstr(&self.postal_code, &mut cn.postalCode);
        Self::str_to_cert_cstr(&self.email, &mut cn.email);
        Self::str_to_cert_cstr(&self.url, &mut cn.url);
        Self::str_to_cert_cstr(&self.host, &mut cn.host);
        return cn;
    }
}

implement_to_from_json!(CertificateName);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#[derive(Serialize, Deserialize, PartialEq, Eq)]
pub struct CertificateNetwork {
    pub id: NetworkId,
    pub controller: Fingerprint,
}

impl CertificateNetwork {
    pub(crate) fn new_from_capi(cn: &ztcore::ZT_Certificate_Network) -> CertificateNetwork {
        CertificateNetwork {
            id: NetworkId(cn.id),
            controller: Fingerprint {
                address: Address(cn.controller.address),
                hash: cn.controller.hash,
            },
        }
    }

    pub(crate) fn to_capi(&self) -> ztcore::ZT_Certificate_Network {
        ztcore::ZT_Certificate_Network {
            id: self.id.0,
            controller: ztcore::ZT_Fingerprint {
                address: self.controller.address.0,
                hash: self.controller.hash,
            },
        }
    }
}

implement_to_from_json!(CertificateNetwork);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#[derive(Serialize, Deserialize, PartialEq, Eq)]
pub struct CertificateIdentity {
    pub identity: Identity,
    pub locator: Option<Locator>,
}

impl CertificateIdentity {
    pub(crate) unsafe fn new_from_capi(ci: &ztcore::ZT_Certificate_Identity) -> Option<CertificateIdentity> {
        if ci.identity.is_null() {
            return None;
        }
        Some(CertificateIdentity {
            identity: Identity::new_from_capi(ci.identity, false).clone(),
            locator: if ci.locator.is_null() { None } else { Some(Locator::new_from_capi(ci.locator, false).clone()) },
        })
    }

    pub(crate) unsafe fn to_capi(&self) -> ztcore::ZT_Certificate_Identity {
        ztcore::ZT_Certificate_Identity {
            identity: self.identity.capi,
            locator: if self.locator.is_some() { self.locator.as_ref().unwrap().capi } else { null() },
        }
    }
}

implement_to_from_json!(CertificateIdentity);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#[derive(Serialize, Deserialize, PartialEq, Eq)]
pub struct CertificateSubject {
    pub timestamp: i64,
    pub identities: Vec<CertificateIdentity>,
    pub networks: Vec<CertificateNetwork>,
    pub certificates: Vec<CertificateSerialNo>,
    #[serde(rename = "updateURLs")]
    pub update_urls: Vec<String>,
    pub name: CertificateName,
    #[serde(rename = "uniqueId")]
    pub unique_id: Vec<u8>,
    #[serde(rename = "uniqueIdProofSignature")]
    pub unique_id_proof_signature: Vec<u8>,
}

#[allow(unused)]
pub(crate) struct CertificateSubjectCAPIContainer {
    pub(crate) subject: ztcore::ZT_Certificate_Subject,
    subject_identities: Vec<ztcore::ZT_Certificate_Identity>,
    subject_networks: Vec<ztcore::ZT_Certificate_Network>,
    subject_certificates: Vec<*const u8>,
    subject_urls: Vec<*const c_char>,
    subject_urls_strs: Vec<CString>,
}

impl CertificateSubject {
    pub fn new() -> CertificateSubject {
        CertificateSubject {
            timestamp: 0,
            identities: Vec::new(),
            networks: Vec::new(),
            certificates: Vec::new(),
            update_urls: Vec::new(),
            name: CertificateName::new(),
            unique_id: Vec::new(),
            unique_id_proof_signature: Vec::new(),
        }
    }

    pub(crate) unsafe fn new_from_capi(cs: &ztcore::ZT_Certificate_Subject) -> CertificateSubject {
        let mut identities: Vec<CertificateIdentity> = Vec::new();
        if !cs.identities.is_null() && cs.identityCount > 0 {
            let cidentities: &[ztcore::ZT_Certificate_Identity] = std::slice::from_raw_parts(cs.identities, cs.identityCount as usize);
            for i in cidentities.iter() {
                let ci = CertificateIdentity::new_from_capi(i);
                if ci.is_some() {
                    identities.push(ci.unwrap());
                }
            }
        }

        let mut networks: Vec<CertificateNetwork> = Vec::new();
        if !cs.networks.is_null() && cs.networkCount > 0 {
            let cnetworks: &[ztcore::ZT_Certificate_Network] = std::slice::from_raw_parts(cs.networks, cs.networkCount as usize);
            for i in cnetworks.iter() {
                networks.push(CertificateNetwork::new_from_capi(i));
            }
        }

        let mut certificates: Vec<CertificateSerialNo> = Vec::new();
        if !cs.certificates.is_null() && cs.certificateCount > 0 {
            let ccertificates: &[*const u8] = std::slice::from_raw_parts(cs.certificates, cs.certificateCount as usize);
            let mut ctmp: [u8; 48] = [0; 48];
            for i in ccertificates.iter() {
                copy_nonoverlapping(*i, ctmp.as_mut_ptr(), 48);
                certificates.push(CertificateSerialNo(ctmp));
            }
        }

        let mut update_urls: Vec<String> = Vec::new();
        if !cs.updateURLs.is_null() && cs.updateURLCount > 0 {
            let cupdate_urls: &[*const c_char] = std::slice::from_raw_parts(cs.updateURLs, cs.updateURLCount as usize);
            for i in cupdate_urls.iter() {
                update_urls.push(cstr_to_string(*i, CERTIFICATE_MAX_STRING_LENGTH - 1));
            }
        }

        return CertificateSubject {
            timestamp: cs.timestamp,
            identities: identities,
            networks: networks,
            certificates: certificates,
            update_urls: update_urls,
            name: CertificateName::new_from_capi(&cs.name),
            unique_id: Vec::from(std::slice::from_raw_parts(cs.uniqueId, cs.uniqueIdSize as usize)),
            unique_id_proof_signature: Vec::from(std::slice::from_raw_parts(cs.uniqueIdProofSignature, cs.uniqueIdProofSignatureSize as usize)),
        };
    }

    pub(crate) unsafe fn to_capi(&self) -> CertificateSubjectCAPIContainer {
        let mut capi_identities: Vec<ztcore::ZT_Certificate_Identity> = Vec::new();
        let mut capi_networks: Vec<ztcore::ZT_Certificate_Network> = Vec::new();
        let mut capi_certificates: Vec<*const u8> = Vec::new();
        let mut capi_urls: Vec<*const c_char> = Vec::new();
        let mut capi_urls_strs: Vec<CString> = Vec::new();

        if !self.identities.is_empty() {
            capi_identities.reserve(self.identities.len());
            for i in self.identities.iter() {
                capi_identities.push((*i).to_capi());
            }
        }
        if !self.networks.is_empty() {
            capi_networks.reserve(self.networks.len());
            for i in self.networks.iter() {
                capi_networks.push((*i).to_capi());
            }
        }
        if !self.certificates.is_empty() {
            capi_certificates.reserve(self.certificates.len());
            for i in self.certificates.iter() {
                capi_certificates.push((*i).0.as_ptr());
            }
        }
        if !self.update_urls.is_empty() {
            capi_urls.reserve(self.update_urls.len());
            capi_urls_strs.reserve(self.update_urls.len());
            for i in self.update_urls.iter() {
                let cs = CString::new((*i).as_str());
                if cs.is_ok() {
                    capi_urls_strs.push(cs.unwrap());
                }
            }
            for i in capi_urls_strs.iter() {
                capi_urls.push((*i).as_ptr());
            }
        }

        CertificateSubjectCAPIContainer {
            subject: ztcore::ZT_Certificate_Subject {
                timestamp: self.timestamp,
                identities: capi_identities.as_mut_ptr(),
                networks: capi_networks.as_mut_ptr(),
                certificates: capi_certificates.as_ptr(),
                updateURLs: capi_urls.as_ptr(),
                identityCount: capi_identities.len() as c_uint,
                networkCount: capi_networks.len() as c_uint,
                certificateCount: capi_certificates.len() as c_uint,
                updateURLCount: capi_urls.len() as c_uint,
                name: self.name.to_capi(),
                uniqueId: self.unique_id.as_ptr(),
                uniqueIdProofSignature: self.unique_id_proof_signature.as_ptr(),
                uniqueIdSize: self.unique_id.len() as c_uint,
                uniqueIdProofSignatureSize: self.unique_id_proof_signature.len() as c_uint,
            },
            subject_identities: capi_identities,
            subject_networks: capi_networks,
            subject_certificates: capi_certificates,
            subject_urls: capi_urls,
            subject_urls_strs: capi_urls_strs,
        }
    }

    pub fn new_csr(&self, uid: Option<&CertificateSubjectUniqueIdSecret>) -> Result<Box<[u8]>, ResultCode> {
        let mut csr: Vec<u8> = Vec::new();
        csr.resize(16384, 0);
        let mut csr_size: c_int = 16384;

        unsafe {
            let capi = self.to_capi();
            if uid.is_some() {
                let uid2 = uid.unwrap();
                if ztcore::ZT_Certificate_newCSR(&capi.subject as *const ztcore::ZT_Certificate_Subject, uid2.public.as_ptr() as *const c_void, uid2.public.len() as c_int, uid2.private.as_ptr() as *const c_void, uid2.private.len() as c_int, csr.as_mut_ptr() as *mut c_void, &mut csr_size) != 0 {
                    return Err(ResultCode::ErrorBadParameter);
                }
            } else {
                if ztcore::ZT_Certificate_newCSR(&capi.subject, null(), -1, null(), -1, csr.as_mut_ptr() as *mut c_void, &mut csr_size) != 0 {
                    return Err(ResultCode::ErrorBadParameter);
                }
            }
        }

        return Ok(csr.into_boxed_slice());
    }
}

implement_to_from_json!(CertificateSubject);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#[derive(Serialize, Deserialize, PartialEq, Eq)]
pub struct Certificate {
    #[serde(rename = "serialNo")]
    pub serial_no: CertificateSerialNo,
    pub flags: u64,
    pub timestamp: i64,
    pub validity: [i64; 2],
    pub subject: CertificateSubject,
    pub issuer: Option<Identity>,
    #[serde(rename = "issuerName")]
    pub issuer_name: CertificateName,
    #[serde(rename = "extendedAttributes")]
    pub extended_attributes: Vec<u8>,
    #[serde(rename = "maxPathLength")]
    pub max_path_length: u32,
    pub signature: Vec<u8>,
}

#[allow(unused)]
pub(crate) struct CertificateCAPIContainer {
    pub(crate) certificate: ztcore::ZT_Certificate,
    subject_container: CertificateSubjectCAPIContainer,
}

impl Certificate {
    pub fn new() -> Certificate {
        Certificate {
            serial_no: CertificateSerialNo::new(),
            flags: 0,
            timestamp: 0,
            validity: [0, i64::max_value()],
            subject: CertificateSubject::new(),
            issuer: None,
            issuer_name: CertificateName::new(),
            extended_attributes: Vec::new(),
            max_path_length: 0,
            signature: Vec::new(),
        }
    }

    pub(crate) unsafe fn new_from_capi(c: &ztcore::ZT_Certificate) -> Certificate {
        return Certificate {
            serial_no: CertificateSerialNo(c.serialNo),
            flags: c.flags,
            timestamp: c.timestamp,
            validity: c.validity,
            subject: CertificateSubject::new_from_capi(&c.subject),
            issuer: if c.issuer.is_null() { None } else { Some(Identity::new_from_capi(c.issuer, false).clone()) },
            issuer_name: CertificateName::new_from_capi(&c.issuerName),
            extended_attributes: Vec::from(std::slice::from_raw_parts(c.extendedAttributes, c.extendedAttributesSize as usize)),
            max_path_length: c.maxPathLength as u32,
            signature: Vec::from(std::slice::from_raw_parts(c.signature, c.signatureSize as usize)),
        };
    }

    pub(crate) unsafe fn to_capi(&self) -> CertificateCAPIContainer {
        let subject = self.subject.to_capi();
        CertificateCAPIContainer {
            certificate: ztcore::ZT_Certificate {
                serialNo: self.serial_no.0,
                flags: self.flags,
                timestamp: self.timestamp,
                validity: self.validity,
                subject: subject.subject,
                issuer: if self.issuer.is_some() { self.issuer.as_ref().unwrap().capi } else { null() },
                issuerName: self.issuer_name.to_capi(),
                extendedAttributes: self.extended_attributes.as_ptr(),
                extendedAttributesSize: self.extended_attributes.len() as c_uint,
                maxPathLength: self.max_path_length as c_uint,
                signature: self.signature.as_ptr(),
                signatureSize: self.signature.len() as c_uint,
            },
            subject_container: subject,
        }
    }

    pub fn new_from_bytes(b: &[u8], verify: bool) -> Result<Certificate, CertificateError> {
        let mut capi_cert: *const ztcore::ZT_Certificate = null_mut();
        let capi_verify: c_int = if verify { 1 } else { 0 };
        let result = unsafe { ztcore::ZT_Certificate_decode(&mut capi_cert as *mut *const ztcore::ZT_Certificate, b.as_ptr() as *const c_void, b.len() as c_int, capi_verify) };
        if result != ztcore::ZT_CertificateError_ZT_CERTIFICATE_ERROR_NONE {
            return Err(CertificateError::from_i32(result as i32).unwrap_or(CertificateError::InvalidFormat));
        }
        if capi_cert.is_null() {
            return Err(CertificateError::InvalidFormat);
        }
        unsafe {
            let cert = Certificate::new_from_capi(&*capi_cert);
            ztcore::ZT_Certificate_delete(capi_cert);
            return Ok(cert);
        }
    }

    pub fn to_bytes(&self) -> Result<Box<[u8]>, ResultCode> {
        let mut cert: Vec<u8> = Vec::new();
        cert.resize(16384, 0);
        let mut cert_size: c_int = 16384;
        unsafe {
            let capi = self.to_capi();
            if ztcore::ZT_Certificate_encode(&capi.certificate as *const ztcore::ZT_Certificate, cert.as_mut_ptr() as *mut c_void, &mut cert_size) != 0 {
                return Err(ResultCode::ErrorInternalNonFatal);
            }
        }
        cert.resize(cert_size as usize, 0);
        return Ok(cert.into_boxed_slice());
    }

    pub fn sign(&self, id: &Identity) -> Result<Vec<u8>, ResultCode> {
        if !id.has_private() {
            return Err(ResultCode::ErrorBadParameter);
        }
        let mut signed_cert: Vec<u8> = Vec::new();
        signed_cert.resize(16384, 0);
        let mut signed_cert_size: c_int = 16384;
        unsafe {
            let capi = self.to_capi();
            if ztcore::ZT_Certificate_sign(&capi.certificate as *const ztcore::ZT_Certificate, id.capi, signed_cert.as_mut_ptr() as *mut c_void, &mut signed_cert_size) != 0 {
                return Err(ResultCode::ErrorBadParameter);
            }
        }
        signed_cert.resize(signed_cert_size as usize, 0);
        return Ok(signed_cert);
    }

    pub fn verify(&self) -> CertificateError {
        unsafe {
            let capi = self.to_capi();
            return CertificateError::from_i32(ztcore::ZT_Certificate_verify(&capi.certificate as *const ztcore::ZT_Certificate) as i32).unwrap_or(CertificateError::InvalidFormat);
        }
    }
}

implement_to_from_json!(Certificate);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#[cfg(test)]
mod tests {
    use crate::*;
    use num_traits::FromPrimitive;

    #[test]
    fn certificate_serial_no() {
        let test: [u8; 48] = [1; 48];
        let sn = CertificateSerialNo::from(&test[0..48]);
        assert!(test.eq(&sn.0));
    }

    #[test]
    fn generate_certificate_unique_id() {
        let uid = CertificateSubjectUniqueIdSecret::new(CertificateUniqueIdType::NistP384);
        println!("certificate unique ID public: {}", hex::encode(uid.public).as_str());
        println!("certificate unique ID private: {}", hex::encode(uid.private).as_str());
    }

    #[test]
    fn enum_from_primitive() {
        let ce = CertificateError::from_i32(-2 as i32);
        assert!(ce.is_some());
        let ce = ce.unwrap();
        assert!(ce == CertificateError::InvalidIdentity);
    }

    #[test]
    fn cert_encode_decode() {
        let id0 = Identity::new_generate(IdentityType::NistP384).ok().unwrap();

        let mut cert = Certificate{
            serial_no: CertificateSerialNo::new(),
            flags: 1,
            timestamp: 2,
            validity: [ 3,4 ],
            subject: CertificateSubject::new(),
            issuer: None,
            issuer_name: CertificateName::new(),
            extended_attributes: Vec::new(),
            max_path_length: 123,
            signature: Vec::new()
        };
        cert.serial_no.0[1] = 99;
        cert.subject.timestamp = 5;
        cert.subject.identities.push(CertificateIdentity{
            identity: id0.clone(),
            locator: None
        });
        cert.subject.networks.push(CertificateNetwork{
            id: NetworkId(0xdeadbeef),
            controller: id0.fingerprint()
        });
        cert.subject.certificates.push(CertificateSerialNo::new());
        cert.subject.update_urls.push(String::from("http://foo.bar"));
        cert.subject.name = CertificateName{
            serial_no: String::from("12345"),
            common_name: String::from("foo"),
            country: String::from("bar"),
            organization: String::from("baz"),
            unit: String::from("asdf"),
            locality: String::from("qwerty"),
            province: String::from("province"),
            street_address: String::from("street address"),
            postal_code: String::from("postal code"),
            email: String::from("nobody@nowhere.org"),
            url: String::from("https://www.zerotier.com/"),
            host: String::from("zerotier.com")
        };

        //println!("{}", cert.to_json().as_str());

        unsafe {
            let cert_capi = cert.to_capi();
            let cert2 = Certificate::new_from_capi(&cert_capi.certificate);
            assert!(cert == cert2);
            //println!("{}", cert2.to_json().as_str());
        }

        {
            let cert2 = Certificate::new_from_json(cert.to_json().as_str());
            assert!(cert2.is_ok());
            assert!(cert2.ok().unwrap() == cert);
        }

        let uid = CertificateSubjectUniqueIdSecret::new(CertificateUniqueIdType::NistP384);
        let csr = cert.subject.new_csr(Some(&uid));
        assert!(csr.is_ok());
        let csr = csr.ok().unwrap();

        let mut csr_decoded = Certificate::new_from_bytes(csr.as_ref(), false);
        assert!(csr_decoded.is_ok());
        let mut csr_decoded = csr_decoded.ok().unwrap();

        let cert_signed = csr_decoded.sign(&id0);
        assert!(cert_signed.is_ok());
        let cert_signed = cert_signed.ok().unwrap();

        let cert_signed_decoded = Certificate::new_from_bytes(cert_signed.as_slice(), false);
        assert!(cert_signed_decoded.is_ok());
        let cert_signed_decoded = cert_signed_decoded.ok().unwrap();
        assert!(cert_signed_decoded.signature.len() > 0);

        assert!(cert_signed_decoded.verify() == CertificateError::None);
    }
}
