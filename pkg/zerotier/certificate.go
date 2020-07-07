/*
 * Copyright (C)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

package zerotier

// #include "../../serviceiocore/GoGlue.h"
import "C"

import (
	"fmt"
	"unsafe"
)

const (
	CertificateSerialNoSize    = 48
	CertificateMaxStringLength = int(C.ZT_CERTIFICATE_MAX_STRING_LENGTH)

	CertificateUniqueIdTypeNistP384 = int(C.ZT_CERTIFICATE_UNIQUE_ID_TYPE_NIST_P_384)
)

// CertificateName identifies a real-world entity that owns a subject or has signed a certificate.
type CertificateName struct {
	SerialNo      string `json:"serialNo,omitempty"`
	CommonName    string `json:"commonName,omitempty"`
	StreetAddress string `json:"streetAddress,omitempty"`
	Locality      string `json:"locality,omitempty"`
	Province      string `json:"province,omitempty"`
	PostalCode    string `json:"postalCode,omitempty"`
	Country       string `json:"country,omitempty"`
	Organization  string `json:"organization,omitempty"`
	Unit          string `json:"unit,omitempty"`
	Email         string `json:"email,omitempty"`
	URL           string `json:"url,omitempty"`
	Host          string `json:"host,omitempty"`
}

// CertificateIdentity bundles an identity with an optional locator.
type CertificateIdentity struct {
	Identity *Identity `json:"identity"`
	Locator  *Locator  `json:"locator,omitempty"`
}

// CertificateNetwork bundles a network ID with the fingerprint of its primary controller.
type CertificateNetwork struct {
	ID         uint64      `json:"id"`
	Controller Fingerprint `json:"controller"`
}

// CertificateSubject contains information about the subject of a certificate.
type CertificateSubject struct {
	Timestamp              int64                 `json:"timestamp"`
	Identities             []CertificateIdentity `json:"identities,omitempty"`
	Networks               []CertificateNetwork  `json:"networks,omitempty"`
	Certificates           [][]byte              `json:"certificates,omitempty"`
	UpdateURLs             []string              `json:"updateURLs,omitempty"`
	Name                   CertificateName       `json:"name"`
	UniqueID               []byte                `json:"uniqueId,omitempty"`
	UniqueIDProofSignature []byte                `json:"uniqueIdProofSignature,omitempty"`
}

// Certificate is a Go reflection of the C ZT_Certificate struct.
type Certificate struct {
	SerialNo           []byte             `json:"serialNo,omitempty"`
	Flags              uint64             `json:"flags"`
	Timestamp          int64              `json:"timestamp"`
	Validity           [2]int64           `json:"validity"`
	Subject            CertificateSubject `json:"subject"`
	Issuer             *Identity          `json:"issuer,omitempty"`
	IssuerName         CertificateName    `json:"issuerName"`
	ExtendedAttributes []byte             `json:"extendedAttributes,omitempty"`
	MaxPathLength      uint               `json:"maxPathLength,omitempty"`
	Signature          []byte             `json:"signature,omitempty"`
}

// CCertificate wraps a pointer to a C ZT_Certificate with any related allocated memory.
// Only the 'C' field should be used directly, and only this field is exported.
type CCertificate struct {
	C                             unsafe.Pointer
	internalCertificate           C.ZT_Certificate
	internalSubjectIdentities     []C.ZT_Certificate_Identity
	internalSubjectNetworks       []C.ZT_Certificate_Network
	internalSubjectCertificates   []uintptr
	internalSubjectUpdateURLs     []uintptr
	internalSubjectUpdateURLsData [][]byte
}

func certificateErrorToError(cerr int) error {
	switch cerr {
	case C.ZT_CERTIFICATE_ERROR_NONE:
		return nil
	case C.ZT_CERTIFICATE_ERROR_HAVE_NEWER_CERT:
		return ErrCertificateHaveNewerCert
	case C.ZT_CERTIFICATE_ERROR_INVALID_FORMAT:
		return ErrCertificateInvalidFormat
	case C.ZT_CERTIFICATE_ERROR_INVALID_IDENTITY:
		return ErrCertificateInvalidIdentity
	case C.ZT_CERTIFICATE_ERROR_INVALID_PRIMARY_SIGNATURE:
		return ErrCertificateInvalidPrimarySignature
	case C.ZT_CERTIFICATE_ERROR_INVALID_CHAIN:
		return ErrCertificateInvalidChain
	case C.ZT_CERTIFICATE_ERROR_INVALID_COMPONENT_SIGNATURE:
		return ErrCertificateInvalidComponentSignature
	case C.ZT_CERTIFICATE_ERROR_INVALID_UNIQUE_ID_PROOF:
		return ErrCertificateInvalidUniqueIDProof
	case C.ZT_CERTIFICATE_ERROR_MISSING_REQUIRED_FIELDS:
		return ErrCertificateMissingRequiredFields
	case C.ZT_CERTIFICATE_ERROR_OUT_OF_VALID_TIME_WINDOW:
		return ErrCertificateOutOfValidTimeWindow
	}
	return ErrInternal
}

// NewCertificateFromBytes decodes a certificate from an encoded byte string.
// Note that this is also used to decode a CSR. When used for a CSR only the
// Subject part of the certificate will contain anything and the rest will be
// blank. If 'verify' is true the certificate will also be verified. If using
// to decode a CSR this should be false as a CSR will not contain a full set
// of fields or a certificate signature.
func NewCertificateFromBytes(cert []byte, verify bool) (*Certificate, error) {
	if len(cert) == 0 {
		return nil, ErrInvalidParameter
	}
	var dec unsafe.Pointer
	ver := C.int(0)
	if verify {
		ver = 1
	}
	cerr := C.ZT_Certificate_decode((**C.ZT_Certificate)(unsafe.Pointer(&dec)), unsafe.Pointer(&cert[0]), C.int(len(cert)), ver)
	if dec != unsafe.Pointer(nil) {
		defer C.ZT_Certificate_delete((*C.ZT_Certificate)(dec))
	}
	if cerr != 0 {
		return nil, certificateErrorToError(int(cerr))
	}
	if dec == unsafe.Pointer(nil) {
		return nil, ErrInternal
	}

	goCert := NewCertificateFromCCertificate(dec)
	if goCert == nil {
		return nil, ErrInternal
	}
	return goCert, nil
}

// NewCertificateFromCCertificate translates a C ZT_Certificate into a Go Certificate.
func NewCertificateFromCCertificate(ccptr unsafe.Pointer) *Certificate {
	cc := (*C.ZT_Certificate)(ccptr)
	c := new(Certificate)

	if cc == nil {
		return c
	}

	sn := (*[48]byte)(unsafe.Pointer(&cc.serialNo[0]))[:]
	if !allZero(sn) {
		var tmp [48]byte
		copy(tmp[:], sn)
		c.SerialNo = tmp[:]
	}
	c.Flags = uint64(cc.flags)
	c.Timestamp = int64(cc.timestamp)
	c.Validity[0] = int64(cc.validity[0])
	c.Validity[1] = int64(cc.validity[1])

	c.Subject.Timestamp = int64(cc.subject.timestamp)

	for i := 0; i < int(cc.subject.identityCount); i++ {
		cid := (*C.ZT_Certificate_Identity)(unsafe.Pointer(uintptr(unsafe.Pointer(cc.subject.identities)) + (uintptr(C.sizeof_ZT_Certificate_Identity) * uintptr(i))))
		if cid.identity == nil {
			return nil
		}
		id, err := newIdentityFromCIdentity(cid.identity)
		if err != nil {
			return nil
		}
		var loc *Locator
		if cid.locator != nil {
			loc, err = newLocatorFromCLocator(cid.locator, false)
			if err != nil {
				return nil
			}
		}
		c.Subject.Identities = append(c.Subject.Identities, CertificateIdentity{
			Identity: id,
			Locator:  loc,
		})
	}

	for i := 0; i < int(cc.subject.networkCount); i++ {
		cn := (*C.ZT_Certificate_Network)(unsafe.Pointer(uintptr(unsafe.Pointer(cc.subject.networks)) + (uintptr(C.sizeof_ZT_Certificate_Network) * uintptr(i))))
		fp := newFingerprintFromCFingerprint(&cn.controller)
		if fp == nil {
			return nil
		}
		c.Subject.Networks = append(c.Subject.Networks, CertificateNetwork{
			ID:         uint64(cn.id),
			Controller: *fp,
		})
	}

	for i := 0; i < int(cc.subject.certificateCount); i++ {
		csn := *((**[48]byte)(unsafe.Pointer(uintptr(unsafe.Pointer(cc.subject.certificates)) + (uintptr(i) * pointerSize))))
		var tmp [48]byte
		copy(tmp[:], csn[:])
		c.Subject.Certificates = append(c.Subject.Certificates, tmp[:])
	}

	for i := 0; i < int(cc.subject.updateURLCount); i++ {
		curl := *((**C.char)(unsafe.Pointer(uintptr(unsafe.Pointer(cc.subject.updateURLs)) + (uintptr(i) * pointerSize))))
		c.Subject.UpdateURLs = append(c.Subject.UpdateURLs, C.GoString(curl))
	}

	c.Subject.Name.SerialNo = C.GoString(&cc.subject.name.serialNo[0])
	c.Subject.Name.CommonName = C.GoString(&cc.subject.name.commonName[0])
	c.Subject.Name.Country = C.GoString(&cc.subject.name.country[0])
	c.Subject.Name.Organization = C.GoString(&cc.subject.name.organization[0])
	c.Subject.Name.Unit = C.GoString(&cc.subject.name.unit[0])
	c.Subject.Name.Locality = C.GoString(&cc.subject.name.locality[0])
	c.Subject.Name.Province = C.GoString(&cc.subject.name.province[0])
	c.Subject.Name.StreetAddress = C.GoString(&cc.subject.name.streetAddress[0])
	c.Subject.Name.PostalCode = C.GoString(&cc.subject.name.postalCode[0])
	c.Subject.Name.Email = C.GoString(&cc.subject.name.email[0])
	c.Subject.Name.URL = C.GoString(&cc.subject.name.url[0])
	c.Subject.Name.Host = C.GoString(&cc.subject.name.host[0])

	if cc.subject.uniqueIdSize > 0 {
		c.Subject.UniqueID = C.GoBytes(unsafe.Pointer(cc.subject.uniqueId), C.int(cc.subject.uniqueIdSize))
		if cc.subject.uniqueIdProofSignatureSize > 0 {
			c.Subject.UniqueIDProofSignature = C.GoBytes(unsafe.Pointer(cc.subject.uniqueIdProofSignature), C.int(cc.subject.uniqueIdProofSignatureSize))
		}
	}

	if cc.issuer != nil {
		id, err := newIdentityFromCIdentity(cc.issuer)
		if err != nil {
			return nil
		}
		c.Issuer = id
	}

	c.IssuerName.SerialNo = C.GoString(&cc.issuerName.serialNo[0])
	c.IssuerName.CommonName = C.GoString(&cc.issuerName.commonName[0])
	c.IssuerName.Country = C.GoString(&cc.issuerName.country[0])
	c.IssuerName.Organization = C.GoString(&cc.issuerName.organization[0])
	c.IssuerName.Unit = C.GoString(&cc.issuerName.unit[0])
	c.IssuerName.Locality = C.GoString(&cc.issuerName.locality[0])
	c.IssuerName.Province = C.GoString(&cc.issuerName.province[0])
	c.IssuerName.StreetAddress = C.GoString(&cc.issuerName.streetAddress[0])
	c.IssuerName.PostalCode = C.GoString(&cc.issuerName.postalCode[0])
	c.IssuerName.Email = C.GoString(&cc.issuerName.email[0])
	c.IssuerName.URL = C.GoString(&cc.issuerName.url[0])
	c.IssuerName.Host = C.GoString(&cc.issuerName.host[0])

	if cc.extendedAttributesSize > 0 {
		c.ExtendedAttributes = C.GoBytes(unsafe.Pointer(cc.extendedAttributes), C.int(cc.extendedAttributesSize))
	}

	c.MaxPathLength = uint(cc.maxPathLength)

	if cc.signatureSize > 0 {
		c.Signature = C.GoBytes(unsafe.Pointer(cc.signature), C.int(cc.signatureSize))
	}

	return c
}

// CCertificate creates a C ZT_Certificate structure from the content of a Certificate.
//
// This will return nil if an error occurs, which would indicate an invalid C
// structure or one with invalid values.
//
// The returned Go structure bundles this with some objects that have
// to be created to set their pointers in ZT_Certificate. It's easier to
// manage allocation of these in Go and bundle them so Go's GC will clean
// them up automatically when CCertificate is released. Only the 'C' field
// in CCertificate should be directly used. The rest are internal and are
// hidden outside the package.
//
// Ensure that Certificate is not modified until the generated C certificate
// is no longer in use.
func (c *Certificate) CCertificate() *CCertificate {
	var cc CCertificate
	var ccC *C.ZT_Certificate

	cc.C = unsafe.Pointer(&cc.internalCertificate)
	ccC = &cc.internalCertificate

	if len(c.SerialNo) == 48 {
		copy((*[48]byte)(unsafe.Pointer(&ccC.serialNo[0]))[:], c.SerialNo)
	}
	ccC.flags = C.uint64_t(c.Flags)
	ccC.timestamp = C.int64_t(c.Timestamp)
	ccC.validity[0] = C.int64_t(c.Validity[0])
	ccC.validity[1] = C.int64_t(c.Validity[1])

	ccC.subject.timestamp = C.int64_t(c.Subject.Timestamp)

	if len(c.Subject.Identities) > 0 {
		cc.internalSubjectIdentities = make([]C.ZT_Certificate_Identity, len(c.Subject.Identities))
		for i, id := range c.Subject.Identities {
			if id.Identity == nil || !id.Identity.initCIdentityPtr() {
				return nil
			}
			cc.internalSubjectIdentities[i].identity = id.Identity.cid
			if id.Locator != nil {
				cc.internalSubjectIdentities[i].locator = id.Locator.cl
			}
		}
		ccC.subject.identities = &cc.internalSubjectIdentities[0]
		ccC.subject.identityCount = C.uint(len(c.Subject.Identities))
	}

	if len(c.Subject.Networks) > 0 {
		cc.internalSubjectNetworks = make([]C.ZT_Certificate_Network, len(c.Subject.Networks))
		for i, n := range c.Subject.Networks {
			cc.internalSubjectNetworks[i].id = C.uint64_t(n.ID)
			cc.internalSubjectNetworks[i].controller.address = C.uint64_t(n.Controller.Address)
			if len(n.Controller.Hash) == 48 {
				copy((*[48]byte)(unsafe.Pointer(&cc.internalSubjectNetworks[i].controller.hash[0]))[:], n.Controller.Hash)
			}
		}
		ccC.subject.networks = &cc.internalSubjectNetworks[0]
		ccC.subject.networkCount = C.uint(len(c.Subject.Networks))
	}

	if len(c.Subject.Certificates) > 0 {
		cc.internalSubjectCertificates = make([]uintptr, len(c.Subject.Certificates))
		for i, cert := range c.Subject.Certificates {
			if len(cert) != 48 {
				return nil
			}
			cc.internalSubjectCertificates[i] = uintptr(unsafe.Pointer(&cert[0]))
		}
		ccC.subject.certificates = (**C.uint8_t)(unsafe.Pointer(&cc.internalSubjectCertificates[0]))
		ccC.subject.certificateCount = C.uint(len(c.Subject.Certificates))
	}

	if len(c.Subject.UpdateURLs) > 0 {
		cc.internalSubjectUpdateURLs = make([]uintptr, len(c.Subject.UpdateURLs))
		cc.internalSubjectUpdateURLsData = make([][]byte, len(c.Subject.UpdateURLs))
		for i, u := range c.Subject.UpdateURLs {
			cc.internalSubjectUpdateURLsData[i] = stringAsZeroTerminatedBytes(u)
			cc.internalSubjectUpdateURLs[i] = uintptr(unsafe.Pointer(&cc.internalSubjectUpdateURLsData[0][0]))
		}
		ccC.subject.updateURLs = (**C.char)(unsafe.Pointer(&cc.internalSubjectUpdateURLs[0]))
		ccC.subject.updateURLCount = C.uint(len(c.Subject.UpdateURLs))
	}

	cStrCopy(unsafe.Pointer(&ccC.subject.name.serialNo[0]), CertificateMaxStringLength+1, c.Subject.Name.SerialNo)
	cStrCopy(unsafe.Pointer(&ccC.subject.name.commonName[0]), CertificateMaxStringLength+1, c.Subject.Name.CommonName)
	cStrCopy(unsafe.Pointer(&ccC.subject.name.country[0]), CertificateMaxStringLength+1, c.Subject.Name.Country)
	cStrCopy(unsafe.Pointer(&ccC.subject.name.organization[0]), CertificateMaxStringLength+1, c.Subject.Name.Organization)
	cStrCopy(unsafe.Pointer(&ccC.subject.name.unit[0]), CertificateMaxStringLength+1, c.Subject.Name.Unit)
	cStrCopy(unsafe.Pointer(&ccC.subject.name.locality[0]), CertificateMaxStringLength+1, c.Subject.Name.Locality)
	cStrCopy(unsafe.Pointer(&ccC.subject.name.province[0]), CertificateMaxStringLength+1, c.Subject.Name.Province)
	cStrCopy(unsafe.Pointer(&ccC.subject.name.streetAddress[0]), CertificateMaxStringLength+1, c.Subject.Name.StreetAddress)
	cStrCopy(unsafe.Pointer(&ccC.subject.name.postalCode[0]), CertificateMaxStringLength+1, c.Subject.Name.PostalCode)
	cStrCopy(unsafe.Pointer(&ccC.subject.name.email[0]), CertificateMaxStringLength+1, c.Subject.Name.Email)
	cStrCopy(unsafe.Pointer(&ccC.subject.name.url[0]), CertificateMaxStringLength+1, c.Subject.Name.URL)
	cStrCopy(unsafe.Pointer(&ccC.subject.name.host[0]), CertificateMaxStringLength+1, c.Subject.Name.Host)

	if len(c.Subject.UniqueID) > 0 {
		ccC.subject.uniqueId = (*C.uint8_t)(unsafe.Pointer(&c.Subject.UniqueID[0]))
		ccC.subject.uniqueIdSize = C.uint(len(c.Subject.UniqueID))
		if len(c.Subject.UniqueIDProofSignature) > 0 {
			ccC.subject.uniqueIdProofSignature = (*C.uint8_t)(unsafe.Pointer(&c.Subject.UniqueIDProofSignature[0]))
			ccC.subject.uniqueIdProofSignatureSize = C.uint(len(c.Subject.UniqueIDProofSignature))
		}
	}

	if c.Issuer != nil {
		if !c.Issuer.initCIdentityPtr() {
			return nil
		}
		ccC.issuer = c.Issuer.cid
	}

	cStrCopy(unsafe.Pointer(&ccC.issuerName.serialNo[0]), CertificateMaxStringLength+1, c.IssuerName.SerialNo)
	cStrCopy(unsafe.Pointer(&ccC.issuerName.commonName[0]), CertificateMaxStringLength+1, c.IssuerName.CommonName)
	cStrCopy(unsafe.Pointer(&ccC.issuerName.country[0]), CertificateMaxStringLength+1, c.IssuerName.Country)
	cStrCopy(unsafe.Pointer(&ccC.issuerName.organization[0]), CertificateMaxStringLength+1, c.IssuerName.Organization)
	cStrCopy(unsafe.Pointer(&ccC.issuerName.unit[0]), CertificateMaxStringLength+1, c.IssuerName.Unit)
	cStrCopy(unsafe.Pointer(&ccC.issuerName.locality[0]), CertificateMaxStringLength+1, c.IssuerName.Locality)
	cStrCopy(unsafe.Pointer(&ccC.issuerName.province[0]), CertificateMaxStringLength+1, c.IssuerName.Province)
	cStrCopy(unsafe.Pointer(&ccC.issuerName.streetAddress[0]), CertificateMaxStringLength+1, c.IssuerName.StreetAddress)
	cStrCopy(unsafe.Pointer(&ccC.issuerName.postalCode[0]), CertificateMaxStringLength+1, c.IssuerName.PostalCode)
	cStrCopy(unsafe.Pointer(&ccC.issuerName.email[0]), CertificateMaxStringLength+1, c.IssuerName.Email)
	cStrCopy(unsafe.Pointer(&ccC.issuerName.url[0]), CertificateMaxStringLength+1, c.IssuerName.URL)
	cStrCopy(unsafe.Pointer(&ccC.issuerName.host[0]), CertificateMaxStringLength+1, c.IssuerName.Host)

	if len(c.ExtendedAttributes) > 0 {
		ccC.extendedAttributes = (*C.uint8_t)(unsafe.Pointer(&c.ExtendedAttributes[0]))
		ccC.extendedAttributesSize = C.uint(len(c.ExtendedAttributes))
	}

	ccC.maxPathLength = C.uint(c.MaxPathLength)

	if len(c.Signature) > 0 {
		ccC.signature = (*C.uint8_t)(unsafe.Pointer(&c.Signature[0]))
		ccC.signatureSize = C.uint(len(c.Signature))
	}

	return &cc
}

// Marshal encodes this certificae as a byte array.
func (c *Certificate) Marshal() ([]byte, error) {
	cc := c.CCertificate()
	if cc == nil {
		return nil, ErrInternal
	}
	var encoded [16384]byte
	encodedSize := C.int(16384)
	rv := int(C.ZT_Certificate_encode((*C.ZT_Certificate)(cc.C), unsafe.Pointer(&encoded[0]), &encodedSize))
	if rv != 0 {
		return nil, fmt.Errorf("Certificate encode error %d", rv)
	}
	return append(make([]byte, 0, int(encodedSize)), encoded[0:int(encodedSize)]...), nil
}

// Verify returns nil on success or a certificate error if there is a problem with this certificate.
func (c *Certificate) Verify() error {
	cc := c.CCertificate()
	if cc == nil {
		return ErrInternal
	}
	return certificateErrorToError(int(C.ZT_Certificate_verify((*C.ZT_Certificate)(cc.C))))
}

// NewCertificateSubjectUniqueId creates a new certificate subject unique ID and corresponding private key.
// Right now only one type is supported: CertificateUniqueIdTypeNistP384
func NewCertificateSubjectUniqueId(uniqueIdType int) (id []byte, priv []byte, err error) {
	if uniqueIdType != CertificateUniqueIdTypeNistP384 {
		err = ErrInvalidParameter
		return
	}
	id = make([]byte, int(C.ZT_CERTIFICATE_UNIQUE_ID_TYPE_NIST_P_384_SIZE))
	priv = make([]byte, int(C.ZT_CERTIFICATE_UNIQUE_ID_TYPE_NIST_P_384_SIZE))
	idSize := C.int(len(id))
	idPrivateSize := C.int(len(priv))
	if C.ZT_Certificate_newSubjectUniqueId((C.enum_ZT_CertificateUniqueIdType)(uniqueIdType), unsafe.Pointer(&id[0]), &idSize, unsafe.Pointer(&priv[0]), &idPrivateSize) != 0 {
		id = nil
		priv = nil
		err = ErrInvalidParameter
		return
	}
	if int(idSize) != len(id) || int(idPrivateSize) != len(priv) {
		id = nil
		priv = nil
		err = ErrInvalidParameter
		return
	}
	return
}

// NewCertificateCSR creates a new certificate signing request (CSR) from a certificate subject and optional unique ID.
func NewCertificateCSR(subject *CertificateSubject, uniqueId []byte, uniqueIdPrivate []byte) ([]byte, error) {
	var tmp Certificate
	tmp.Subject = *subject
	ctmp := tmp.CCertificate()
	if ctmp == nil {
		return nil, ErrInternal
	}
	ccert := (*C.ZT_Certificate)(ctmp.C)
	var uid unsafe.Pointer
	var uidp unsafe.Pointer
	if len(uniqueId) > 0 && len(uniqueIdPrivate) > 0 {
		uid = unsafe.Pointer(&uniqueId[0])
		uidp = unsafe.Pointer(&uniqueIdPrivate[0])
	}
	var csr [16384]byte
	csrSize := C.int(16384)
	rv := int(C.ZT_Certificate_newCSR(&(ccert.subject), uid, C.int(len(uniqueId)), uidp, C.int(len(uniqueIdPrivate)), unsafe.Pointer(&csr[0]), &csrSize))
	if rv != 0 {
		return nil, fmt.Errorf("newCSR error %d", rv)
	}
	return append(make([]byte, 0, int(csrSize)), csr[0:int(csrSize)]...), nil
}
