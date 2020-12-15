use crate::*;
use crate::bindings::capi as ztcore;
use std::ffi::CStr;
use std::ptr::copy_nonoverlapping;
use std::os::raw::c_char;

pub struct CertificateName {
    pub serial_no: String,
    pub common_name: String,
    pub country: String,
    pub organization: String,
    pub unit: String,
    pub locality: String,
    pub province: String,
    pub street_address: String,
    pub postal_code: String,
    pub email: String,
    pub url: String,
    pub host: String
}

pub struct CertificateNetwork {
    pub id: NetworkId,
    pub controller: Fingerprint
}

pub struct CertificateIdentity {
    pub identity: Identity,
    pub locator: Locator
}

pub struct CertificateSubject {
    pub timestamp: i64,
    pub identities: Box<[CertificateIdentity]>,
    pub networks: Box<[CertificateNetwork]>,
    pub certificates: Box<[[u8; 48]]>,
    pub update_urls: Box<[String]>,
    pub name: CertificateName,
    pub unique_id: Box<[u8]>,
    pub unique_id_proof_signature: Box<[u8]>
}

pub struct Certificate {
    pub serial_no: [u8; 48],
    pub flags: u64,
    pub timestamp: i64,
    pub validity: [i64; 2],
    pub subject: CertificateSubject,
    pub issuer: Identity,
    pub issuer_name: CertificateName,
    pub extended_attributes: Box<[u8]>,
    pub max_path_length: u32,
    pub crl: Box<[u8; 48]>,
    pub signature: Box<[u8]>
}

impl CertificateName {
    pub(crate) fn new_from_capi(cn: &ztcore::ZT_Certificate_Name) -> CertificateName {
        unsafe {
            return CertificateName {
                serial_no: String::from(CStr::from_ptr(cn.serialNo.as_ptr()).to_str().unwrap()),
                common_name: String::from(CStr::from_ptr(cn.commonName.as_ptr()).to_str().unwrap()),
                country: String::from(CStr::from_ptr(cn.country.as_ptr()).to_str().unwrap()),
                organization: String::from(CStr::from_ptr(cn.organization.as_ptr()).to_str().unwrap()),
                unit: String::from(CStr::from_ptr(cn.unit.as_ptr()).to_str().unwrap()),
                locality: String::from(CStr::from_ptr(cn.locality.as_ptr()).to_str().unwrap()),
                province: String::from(CStr::from_ptr(cn.province.as_ptr()).to_str().unwrap()),
                street_address: String::from(CStr::from_ptr(cn.streetAddress.as_ptr()).to_str().unwrap()),
                postal_code: String::from(CStr::from_ptr(cn.postalCode.as_ptr()).to_str().unwrap()),
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
            id: cn.id as NetworkId,
            controller: Fingerprint{
                address: cn.controller.address as Address,
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
            let mut certificates: Vec<[u8; 48]> = Vec::new();
            let mut ctmp: [u8; 48] = [0; 48];
            for i in ccertificates.iter() {
                copy_nonoverlapping(*i, ctmp.as_mut_ptr(), 48);
                certificates.push(ctmp.clone());
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
                update_urls: update_urls.into_boxed_slice(),
                name: CertificateName::new_from_capi(&cs.name),
                unique_id: Box::from(std::slice::from_raw_parts(cs.uniqueId, cs.uniqueIdSize as usize).clone()),
                unique_id_proof_signature: Box::from(std::slice::from_raw_parts(cs.uniqueIdProofSignature, cs.uniqueIdProofSignatureSize as usize).clone())
            }
        }
    }
}

impl Certificate {
}
