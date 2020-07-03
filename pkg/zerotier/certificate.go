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
	"unsafe"
)

const (
	CertificateSerialNoSize    = 48
	CertificateMaxStringLength = int(C.ZT_CERTIFICATE_MAX_STRING_LENGTH)
)

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

type CertificateIdentity struct {
	Identity *Identity `json:"identity"`
	Locator  *Locator  `json:"locator,omitempty"`
}

type CertificateNetwork struct {
	ID         uint64       `json:"id"`
	Controller *Fingerprint `json:"controller"`
}

type CertificateSubject struct {
	Timestamp              int64                           `json:"timestamp"`
	Identities             []CertificateIdentity           `json:"identities,omitempty"`
	Networks               []CertificateNetwork            `json:"networks,omitempty"`
	Certificates           [][CertificateSerialNoSize]byte `json:"certificates,omitempty"`
	UpdateURLs             []string                        `json:"updateURLs,omitempty"`
	Name                   CertificateName                 `json:"name"`
	UniqueID               []byte                          `json:"uniqueId,omitempty"`
	UniqueIDProofSignature []byte                          `json:"uniqueIdProofSignature,omitempty"`
}

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

type cCertificate struct {
	C                             C.ZT_Certificate
	internalSubjectIdentities     []C.ZT_Certificate_Identity
	internalSubjectNetworks       []C.ZT_Certificate_Network
	internalSubjectUpdateURLs     []*C.char
	internalSubjectUpdateURLsData [][]byte
}

// cCertificate creates a C ZT_Certificate structure
// The returned Go structure bundles this with some objects that have
// to be created to set their pointers in ZT_Certificate. It's easier to
// manage allocation of these in Go and bundle them so Go's GC will clean
// them up automatically when cCertificate is releaed. Only the 'C' field
// in cCertificate should be directly used.
func (c *Certificate) cCertificate() *cCertificate {
	var cc cCertificate

	if len(c.SerialNo) == 48 {
		copy((*[48]byte)(unsafe.Pointer(&cc.C.serialNo[0]))[:], c.SerialNo[:])
	}
	cc.C.flags = C.uint64_t(c.Flags)
	cc.C.timestamp = C.int64_t(c.Timestamp)
	cc.C.validity[0] = C.int64_t(c.Validity[0])
	cc.C.validity[1] = C.int64_t(c.Validity[1])

	cc.C.subject.timestamp = C.int64_t(c.Subject.Timestamp)

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
		cc.C.subject.identities = &cc.internalSubjectIdentities[0]
		cc.C.subject.identityCount = C.uint(len(c.Subject.Identities))
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
		cc.C.subject.networks = &cc.internalSubjectNetworks[0]
		cc.C.subject.networkCount = C.uint(len(c.Subject.Networks))
	}

	if len(c.Subject.Certificates) > 0 {
		cc.C.subject.certificates = (**C.uint8_t)(unsafe.Pointer(&c.Subject.Certificates[0]))
		cc.C.subject.certificateCount = C.uint(len(c.Subject.Certificates))
	}

	if len(c.Subject.UpdateURLs) > 0 {
		cc.internalSubjectUpdateURLs = make([]*C.char, len(c.Subject.UpdateURLs))
		cc.internalSubjectUpdateURLsData = make([][]byte, len(c.Subject.UpdateURLs))
		for i, u := range c.Subject.UpdateURLs {
			cc.internalSubjectUpdateURLsData[i] = stringAsZeroTerminatedBytes(u)
			cc.internalSubjectUpdateURLs[i] = (*C.char)(unsafe.Pointer(&cc.internalSubjectUpdateURLsData[0]))
		}
		cc.C.subject.updateURLs = (**C.char)(unsafe.Pointer(&cc.internalSubjectUpdateURLs[0]))
		cc.C.subject.updateURLCount = C.uint(len(c.Subject.UpdateURLs))
	}

	cStrCopy(unsafe.Pointer(&cc.C.subject.name.serialNo[0]), CertificateMaxStringLength+1, c.Subject.Name.SerialNo)
	cStrCopy(unsafe.Pointer(&cc.C.subject.name.commonName[0]), CertificateMaxStringLength+1, c.Subject.Name.CommonName)
	cStrCopy(unsafe.Pointer(&cc.C.subject.name.country[0]), CertificateMaxStringLength+1, c.Subject.Name.Country)
	cStrCopy(unsafe.Pointer(&cc.C.subject.name.organization[0]), CertificateMaxStringLength+1, c.Subject.Name.Organization)
	cStrCopy(unsafe.Pointer(&cc.C.subject.name.unit[0]), CertificateMaxStringLength+1, c.Subject.Name.Unit)
	cStrCopy(unsafe.Pointer(&cc.C.subject.name.locality[0]), CertificateMaxStringLength+1, c.Subject.Name.Locality)
	cStrCopy(unsafe.Pointer(&cc.C.subject.name.province[0]), CertificateMaxStringLength+1, c.Subject.Name.Province)
	cStrCopy(unsafe.Pointer(&cc.C.subject.name.streetAddress[0]), CertificateMaxStringLength+1, c.Subject.Name.StreetAddress)
	cStrCopy(unsafe.Pointer(&cc.C.subject.name.postalCode[0]), CertificateMaxStringLength+1, c.Subject.Name.PostalCode)
	cStrCopy(unsafe.Pointer(&cc.C.subject.name.email[0]), CertificateMaxStringLength+1, c.Subject.Name.Email)
	cStrCopy(unsafe.Pointer(&cc.C.subject.name.url[0]), CertificateMaxStringLength+1, c.Subject.Name.URL)
	cStrCopy(unsafe.Pointer(&cc.C.subject.name.host[0]), CertificateMaxStringLength+1, c.Subject.Name.Host)

	if len(c.Subject.UniqueID) > 0 {
		cc.C.subject.uniqueId = (*C.uint8_t)(unsafe.Pointer(&c.Subject.UniqueID[0]))
		cc.C.subject.uniqueIdSize = C.uint(len(c.Subject.UniqueID))
		if len(c.Subject.UniqueIDProofSignature) > 0 {
			cc.C.subject.uniqueIdProofSignature = (*C.uint8_t)(unsafe.Pointer(&c.Subject.UniqueIDProofSignature[0]))
			cc.C.subject.uniqueIdProofSignatureSize = C.uint(len(c.Subject.UniqueIDProofSignature))
		}
	}

	if c.Issuer != nil {
		if !c.Issuer.initCIdentityPtr() {
			return nil
		}
		cc.C.issuer = c.Issuer.cid
	}

	cStrCopy(unsafe.Pointer(&cc.C.issuerName.serialNo[0]), CertificateMaxStringLength+1, c.IssuerName.SerialNo)
	cStrCopy(unsafe.Pointer(&cc.C.issuerName.commonName[0]), CertificateMaxStringLength+1, c.IssuerName.CommonName)
	cStrCopy(unsafe.Pointer(&cc.C.issuerName.country[0]), CertificateMaxStringLength+1, c.IssuerName.Country)
	cStrCopy(unsafe.Pointer(&cc.C.issuerName.organization[0]), CertificateMaxStringLength+1, c.IssuerName.Organization)
	cStrCopy(unsafe.Pointer(&cc.C.issuerName.unit[0]), CertificateMaxStringLength+1, c.IssuerName.Unit)
	cStrCopy(unsafe.Pointer(&cc.C.issuerName.locality[0]), CertificateMaxStringLength+1, c.IssuerName.Locality)
	cStrCopy(unsafe.Pointer(&cc.C.issuerName.province[0]), CertificateMaxStringLength+1, c.IssuerName.Province)
	cStrCopy(unsafe.Pointer(&cc.C.issuerName.streetAddress[0]), CertificateMaxStringLength+1, c.IssuerName.StreetAddress)
	cStrCopy(unsafe.Pointer(&cc.C.issuerName.postalCode[0]), CertificateMaxStringLength+1, c.IssuerName.PostalCode)
	cStrCopy(unsafe.Pointer(&cc.C.issuerName.email[0]), CertificateMaxStringLength+1, c.IssuerName.Email)
	cStrCopy(unsafe.Pointer(&cc.C.issuerName.url[0]), CertificateMaxStringLength+1, c.IssuerName.URL)
	cStrCopy(unsafe.Pointer(&cc.C.issuerName.host[0]), CertificateMaxStringLength+1, c.IssuerName.Host)

	if len(c.ExtendedAttributes) > 0 {
		cc.C.extendedAttributes = (*C.uint8_t)(unsafe.Pointer(&c.ExtendedAttributes[0]))
		cc.C.extendedAttributesSize = C.uint(len(c.ExtendedAttributes))
	}

	cc.C.maxPathLength = C.uint(c.MaxPathLength)

	if len(c.Signature) > 0 {
		cc.C.signature = (*C.uint8_t)(unsafe.Pointer(&c.Signature[0]))
		cc.C.signatureSize = C.uint(len(c.Signature))
	}

	return &cc
}
