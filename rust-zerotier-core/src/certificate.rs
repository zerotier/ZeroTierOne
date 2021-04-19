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
use std::pin::Pin;
use std::ptr::{copy_nonoverlapping, null, null_mut};

use num_derive::{FromPrimitive, ToPrimitive};
use num_traits::{FromPrimitive, ToPrimitive};
use serde::{Deserialize, Serialize};

use crate::*;
use crate::capi as ztcore;

/// Maximum length of a string in a certificate (mostly for the certificate name fields).
pub const CERTIFICATE_MAX_STRING_LENGTH: isize = ztcore::ZT_CERTIFICATE_MAX_STRING_LENGTH as isize;

/// Certificate local trust bit field flag: this certificate self-signs a root CA.
pub const CERTIFICATE_LOCAL_TRUST_FLAG_ROOT_CA: u32 = ztcore::ZT_CERTIFICATE_LOCAL_TRUST_FLAG_ROOT_CA;

/// Certificate local trust bit field flag: this certificate specifies a set of ZeroTier roots.
pub const CERTIFICATE_LOCAL_TRUST_FLAG_ZEROTIER_ROOT_SET: u32 = ztcore::ZT_CERTIFICATE_LOCAL_TRUST_FLAG_ZEROTIER_ROOT_SET;

fn vec_to_array<const L: usize>(v: &Vec<u8>) -> [u8; L] {
    let mut a = [0_u8; L];
    unsafe { copy_nonoverlapping(v.as_ptr(), a.as_mut_ptr(), v.len().min(L)) };
    a
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#[derive(FromPrimitive, ToPrimitive, PartialEq, Eq, Clone, Copy)]
pub enum CertificatePublicKeyAlgorithm {
    None = ztcore::ZT_CertificatePublicKeyAlgorithm_ZT_CERTIFICATE_PUBLIC_KEY_ALGORITHM_NONE as isize,
    ECDSANistP384 = ztcore::ZT_CertificatePublicKeyAlgorithm_ZT_CERTIFICATE_PUBLIC_KEY_ALGORITHM_ECDSA_NIST_P_384 as isize,
}

impl From<i32> for CertificatePublicKeyAlgorithm {
    #[inline(always)]
    fn from(n: i32) -> CertificatePublicKeyAlgorithm {
        CertificatePublicKeyAlgorithm::from_i32(n).unwrap_or(CertificatePublicKeyAlgorithm::None)
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#[derive(PartialEq, Eq, PartialOrd, Ord, Clone)]
pub struct CertificateSerialNo(pub [u8; 48]);

impl CertificateSerialNo {
    #[inline(always)]
    pub fn new() -> CertificateSerialNo { CertificateSerialNo([0; 48]) }
    pub fn new_from_string(s: &str) -> Result<CertificateSerialNo, ResultCode> { hex::decode(s).map_or_else(|_| { Err(ResultCode::ErrorBadParameter) }, |b| { Ok(CertificateSerialNo::from(b)) }) }
}

impl<A: AsRef<[u8]>> From<A> for CertificateSerialNo {
    fn from(a: A) -> CertificateSerialNo {
        let mut sn = CertificateSerialNo::new();
        let aa = a.as_ref();
        for i in 0..aa.len() {
            sn.0[i] = aa[i];
        }
        sn
    }
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

#[derive(FromPrimitive, ToPrimitive, PartialEq, Eq, Clone, Copy)]
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
            _ => CertificateError::None
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

#[derive(Serialize, Deserialize, PartialEq, Eq, Clone)]
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
        unsafe { copy_nonoverlapping(s.as_ptr(), cs.as_mut_ptr() as *mut u8, l); }
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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#[derive(Serialize, Deserialize, PartialEq, Eq, Clone)]
pub struct CertificateNetwork {
    pub id: NetworkId,
    pub controller: Option<Fingerprint>,
}

impl CertificateNetwork {
    pub(crate) fn new_from_capi(cn: &ztcore::ZT_Certificate_Network) -> CertificateNetwork {
        if is_all_zeroes(cn.controller.hash) {
            CertificateNetwork {
                id: NetworkId(cn.id),
                controller: None,
            }
        } else {
            CertificateNetwork {
                id: NetworkId(cn.id),
                controller: Some(Fingerprint {
                    address: Address(cn.controller.address),
                    hash: cn.controller.hash,
                }),
            }
        }
    }

    pub(crate) fn to_capi(&self) -> ztcore::ZT_Certificate_Network {
        self.controller.as_ref().map_or_else(|| {
            ztcore::ZT_Certificate_Network {
                id: self.id.0,
                controller: ztcore::ZT_Fingerprint {
                    address: 0,
                    hash: [0_u8; 48],
                }
            }
        }, |controller| {
            ztcore::ZT_Certificate_Network {
                id: self.id.0,
                controller: ztcore::ZT_Fingerprint {
                    address: controller.address.0,
                    hash: controller.hash,
                },
            }
        })
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#[derive(Serialize, Deserialize, PartialEq, Eq, Clone)]
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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#[derive(Serialize, Deserialize, PartialEq, Eq, Clone)]
pub struct CertificateSubject {
    pub timestamp: i64,
    pub identities: Vec<CertificateIdentity>,
    pub networks: Vec<CertificateNetwork>,
    #[serde(rename = "updateURLs")]
    pub update_urls: Vec<String>,
    pub name: CertificateName,
    #[serde(with = "Base64Standard")]
    #[serde(rename = "uniqueId")]
    pub unique_id: Vec<u8>,
    #[serde(with = "Base64Standard")]
    #[serde(rename = "uniqueIdSignature")]
    pub unique_id_signature: Vec<u8>,
}

#[allow(unused)]
pub(crate) struct CertificateSubjectCAPIContainer {
    pub(crate) subject: ztcore::ZT_Certificate_Subject,
    subject_identities: Pin<Box<[ztcore::ZT_Certificate_Identity]>>,
    subject_networks: Pin<Box<[ztcore::ZT_Certificate_Network]>>,
    subject_urls: Pin<Box<[*const c_char]>>,
    subject_urls_strs: Pin<Box<[CString]>>,
}

impl CertificateSubject {
    pub fn new() -> CertificateSubject {
        CertificateSubject {
            timestamp: 0,
            identities: Vec::new(),
            networks: Vec::new(),
            update_urls: Vec::new(),
            name: CertificateName::new(),
            unique_id: Vec::new(),
            unique_id_signature: Vec::new(),
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

        let mut update_urls: Vec<String> = Vec::new();
        if !cs.updateURLs.is_null() && cs.updateURLCount > 0 {
            let cupdate_urls: &[*const c_char] = std::slice::from_raw_parts(cs.updateURLs, cs.updateURLCount as usize);
            for i in cupdate_urls.iter() {
                update_urls.push(cstr_to_string(*i, CERTIFICATE_MAX_STRING_LENGTH - 1));
            }
        }

        return CertificateSubject {
            timestamp: cs.timestamp,
            identities,
            networks,
            update_urls,
            name: CertificateName::new_from_capi(&cs.name),
            unique_id: cs.uniqueId[0..(cs.uniqueIdSize as usize)].to_vec(),
            unique_id_signature: cs.uniqueIdSignature[0..cs.uniqueIdSignatureSize as usize].to_vec(),
        };
    }

    pub(crate) unsafe fn to_capi(&self) -> CertificateSubjectCAPIContainer {
        let mut capi_identities: Vec<ztcore::ZT_Certificate_Identity> = Vec::new();
        let mut capi_networks: Vec<ztcore::ZT_Certificate_Network> = Vec::new();
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

        let mut capi_identities = Pin::from(capi_identities.into_boxed_slice());
        let mut capi_networks = Pin::from(capi_networks.into_boxed_slice());
        let mut capi_urls = Pin::from(capi_urls.into_boxed_slice());
        let mut capi_urls_strs = Pin::from(capi_urls_strs.into_boxed_slice());

        CertificateSubjectCAPIContainer {
            subject: ztcore::ZT_Certificate_Subject {
                timestamp: self.timestamp,
                identities: capi_identities.as_mut_ptr(),
                networks: capi_networks.as_mut_ptr(),
                updateURLs: capi_urls.as_ptr(),
                identityCount: capi_identities.len() as c_uint,
                networkCount: capi_networks.len() as c_uint,
                updateURLCount: capi_urls.len() as c_uint,
                name: self.name.to_capi(),
                uniqueId: vec_to_array(&self.unique_id),
                uniqueIdSignature: vec_to_array(&self.unique_id_signature),
                uniqueIdSize: self.unique_id.len() as c_uint,
                uniqueIdSignatureSize: self.unique_id_signature.len() as c_uint,
            },
            subject_identities: capi_identities,
            subject_networks: capi_networks,
            subject_urls: capi_urls,
            subject_urls_strs: capi_urls_strs,
        }
    }

    /// Create a new certificate signing request.
    /// A CSR is a Certificate containing only the subject (with optional unique ID and signature)
    /// and its private key. Other fields must be filled in by the owner of the signing certificate.
    pub fn new_csr(&self, certificate_public_key: &[u8], subject_unique_id_private_key: Option<&[u8]>) -> Result<Vec<u8>, ResultCode> {
        let mut csr: Vec<u8> = Vec::new();
        csr.resize(65536, 0);
        let mut csr_size: c_int = 65536;

        let (uid, uid_size) = subject_unique_id_private_key.map_or((null::<u8>(), 0 as c_int), |b| (b.as_ptr(), b.len() as c_int));
        let r = unsafe {
            let s = self.to_capi();
            ztcore::ZT_Certificate_newCSR(&s.subject, certificate_public_key.as_ptr().cast(), certificate_public_key.len() as c_int, uid.cast(), uid_size, csr.as_mut_ptr().cast(), &mut csr_size)
        };

        if r == 0 {
            csr.resize(csr_size as usize, 0);
            csr.shrink_to_fit();
            Ok(csr)
        } else {
            Err(ResultCode::from_i32(r as i32).unwrap_or(ResultCode::ErrorInternalNonFatal))
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#[derive(Serialize, Deserialize, PartialEq, Eq, Clone)]
pub struct Certificate {
    #[serde(rename = "serialNo")]
    pub serial_no: CertificateSerialNo,
    pub flags: u64,
    pub timestamp: i64,
    pub validity: [i64; 2],
    pub subject: CertificateSubject,
    pub issuer: CertificateSerialNo,
    #[serde(rename = "issuerPublicKey")]
    pub issuer_public_key: Vec<u8>,
    #[serde(rename = "publicKey")]
    pub public_key: Vec<u8>,
    #[serde(rename = "extendedAttributes")]
    pub extended_attributes: Vec<u8>,
    #[serde(with = "Base64Standard")]
    pub signature: Vec<u8>,
    #[serde(rename = "maxPathLength")]
    pub max_path_length: u32,
}

#[allow(unused)]
pub(crate) struct CertificateCAPIContainer {
    pub(crate) certificate: ztcore::ZT_Certificate,
    subject_container: CertificateSubjectCAPIContainer,
}

impl Certificate {
    /// Create a new public/private key pair for use in certificate signing or subject unique IDs.
    /// This returns a pair of (public, private) or an error. The first byte of both the
    /// public and private are the type.
    pub fn new_key_pair(alg: CertificatePublicKeyAlgorithm) -> Result<(Vec<u8>, Vec<u8>), ResultCode> {
        let mut public_key = [0_u8; ztcore::ZT_CERTIFICATE_MAX_PUBLIC_KEY_SIZE as usize];
        let mut private_key = [0_u8; ztcore::ZT_CERTIFICATE_MAX_PRIVATE_KEY_SIZE as usize];
        let mut public_key_size: c_int = 0;
        let mut private_key_size: c_int = 0;
        let r = unsafe { ztcore::ZT_Certificate_newKeyPair(alg.to_i32().unwrap() as ztcore::ZT_CertificatePublicKeyAlgorithm, public_key.as_mut_ptr(), &mut public_key_size, private_key.as_mut_ptr(), &mut private_key_size) };
        if r == 0 {
            if public_key_size > 0 && private_key_size > 0 {
                Ok((public_key[0..public_key_size as usize].to_vec(), private_key[0..private_key_size as usize].to_vec()))
            } else {
                Err(ResultCode::ErrorInternalNonFatal)
            }
        } else {
            Err(ResultCode::from_i32(r as i32).unwrap_or(ResultCode::ErrorBadParameter))
        }
    }

    /// Create an empty certificate structure.
    pub fn new() -> Certificate {
        Certificate {
            serial_no: CertificateSerialNo::new(),
            flags: 0,
            timestamp: 0,
            validity: [0, i64::MAX],
            subject: CertificateSubject::new(),
            issuer: CertificateSerialNo::new(),
            issuer_public_key: Vec::new(),
            public_key: Vec::new(),
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
            issuer: CertificateSerialNo(c.issuer),
            issuer_public_key: c.issuerPublicKey[0..(c.issuerPublicKeySize as usize)].to_vec(),
            public_key: c.publicKey[0..(c.publicKeySize as usize)].to_vec(),
            extended_attributes: Vec::from(std::slice::from_raw_parts(c.extendedAttributes, c.extendedAttributesSize as usize)),
            max_path_length: c.maxPathLength as u32,
            signature: c.signature[0..(c.signatureSize as usize)].to_vec(),
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
                issuer: self.issuer.0,
                issuerPublicKey: vec_to_array(&self.issuer_public_key),
                publicKey: vec_to_array(&self.public_key),
                issuerPublicKeySize: self.issuer_public_key.len() as c_uint,
                publicKeySize: self.public_key.len() as c_uint,
                extendedAttributes: self.extended_attributes.as_ptr(),
                extendedAttributesSize: self.extended_attributes.len() as c_uint,
                maxPathLength: self.max_path_length as c_uint,
                signature: vec_to_array(&self.signature),
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

    /// Sign this certificate, returning new signed certificate.
    pub fn sign(&self, issuer: &CertificateSerialNo, issuer_private_key: &[u8]) -> Result<Certificate, ResultCode> {
        let signed_cert = unsafe {
            let c = self.to_capi();
            ztcore::ZT_Certificate_sign(&c.certificate, issuer.0.as_ptr(), issuer_private_key.as_ptr().cast(), issuer_private_key.len() as c_int)
        };
        if signed_cert.is_null() {
            Err(ResultCode::ErrorBadParameter)
        } else {
            let signed_cert2 = unsafe { Certificate::new_from_capi(&*signed_cert) };
            unsafe { ztcore::ZT_Certificate_delete(signed_cert) };
            Ok(signed_cert2)
        }
    }

    /// Verify certificate structure and signatures.
    /// This does not verify the full certificate chain, just what can be verified
    /// by looking at the certificate itself.
    pub fn verify(&self, clock: i64) -> CertificateError {
        unsafe {
            let capi = self.to_capi();
            return CertificateError::from_i32(ztcore::ZT_Certificate_verify(&capi.certificate as *const ztcore::ZT_Certificate, clock) as i32).unwrap_or(CertificateError::InvalidFormat);
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#[cfg(test)]
mod tests {
    use crate::*;
    use num_traits::FromPrimitive;

    #[test]
    fn generate_certificate_unique_id() {
        let (pubk, privk) = Certificate::new_key_pair(CertificatePublicKeyAlgorithm::ECDSANistP384).ok().unwrap();
        println!("certificate unique ID public: {}", hex::encode(pubk.as_ref()).as_str());
        println!("certificate unique ID private: {}", hex::encode(privk.as_ref()).as_str());
    }

    #[test]
    fn enum_from_primitive() {
        let ce = CertificateError::from_i32(-2 as i32);
        assert!(ce.is_some());
        let ce = ce.unwrap();
        assert!(ce == CertificateError::InvalidIdentity);
    }

    #[test]
    fn cert() {
        let (issuer_pubk, issuer_privk) = Certificate::new_key_pair(CertificatePublicKeyAlgorithm::ECDSANistP384).ok().unwrap();
        let (pubk, privk) = Certificate::new_key_pair(CertificatePublicKeyAlgorithm::ECDSANistP384).ok().unwrap();
        let (unique_id, unique_id_private) = Certificate::new_key_pair(CertificatePublicKeyAlgorithm::ECDSANistP384).ok().unwrap();
        let id0 = Identity::new_generate(IdentityType::Curve25519).ok().unwrap();

        let mut cert = Certificate{
            serial_no: CertificateSerialNo::new(),
            flags: 1,
            timestamp: 2,
            validity: [ 1,10 ],
            subject: CertificateSubject::new(),
            issuer: CertificateSerialNo::new(),
            issuer_public_key: issuer_pubk,
            public_key: pubk,
            extended_attributes: Vec::new(),
            max_path_length: 123,
            signature: Vec::new()
        };
        cert.serial_no.0[1] = 99;
        cert.issuer.0[1] = 199;
        cert.subject.timestamp = 5;
        cert.subject.identities.push(CertificateIdentity{
            identity: id0.clone(),
            locator: None
        });
        cert.subject.networks.push(CertificateNetwork{
            id: NetworkId(0xdeadbeef),
            controller: Some(id0.fingerprint())
        });
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

        println!("{}", cert.to_json().as_str());

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

        let csr = cert.subject.new_csr(pubk.as_ref(), Some(unique_id_private.as_ref()));
        assert!(csr.is_ok());
        let csr = csr.ok().unwrap();

        let mut csr_decoded = Certificate::new_from_bytes(csr.as_ref(), false);
        assert!(csr_decoded.is_ok());
        let mut csr_decoded = csr_decoded.ok().unwrap();

        let cert_signed = csr_decoded.sign(&cert.issuer, issuer_privk.as_ref());
        assert!(cert_signed.is_ok());
        let cert_signed = cert_signed.ok().unwrap();

        assert!(cert_signed.verify(-1) == CertificateError::None);
        assert!(cert_signed.verify(5) == CertificateError::None);
        assert!(cert_signed.verify(15) != CertificateError::None);
    }
}
