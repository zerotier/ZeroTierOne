/*
 * Copyright (C)2013-2020 ZeroTier, Inc.
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

package main

import (
	"bytes"
	"fmt"
	"zerotier/pkg/zerotier"
)

func TestCertificate() bool {
	id, err := zerotier.NewIdentityFromString("8e4df28b72:0:ac3d46abe0c21f3cfe7a6c8d6a85cfcffcb82fbd55af6a4d6350657c68200843fa2e16f9418bbd9702cae365f2af5fb4c420908b803a681d4daef6114d78a2d7:bd8dd6e4ce7022d2f812797a80c6ee8ad180dc4ebf301dec8b06d1be08832bddd63a2f1cfa7b2c504474c75bdc8898ba476ef92e8e2d0509f8441985171ff16e")
	if err != nil {
		fmt.Printf("FATAL: error deserializing test identity: %s\n", err.Error())
		return false
	}

	uniqueId, uniqueIdPrivate, err := zerotier.NewCertificateSubjectUniqueId(zerotier.CertificateUniqueIdTypeNistP384)
	if err != nil {
		fmt.Printf("FATAL: error generating unique ID: %s", err.Error())
		return false
	}

	var c zerotier.Certificate

	c.SerialNo = make([]byte, 48)
	for i := 0; i < 48; i++ {
		c.SerialNo[i] = byte(i)
	}
	c.Flags = 1234
	c.Timestamp = 5678
	c.Validity[0] = 1010
	c.Validity[1] = 2020

	c.Subject.Timestamp = 31337
	c.Subject.Identities = append(c.Subject.Identities, zerotier.CertificateIdentity{
		Identity: id,
		Locator:  nil,
	})
	c.Subject.Networks = append(c.Subject.Networks, zerotier.CertificateNetwork{
		ID: 1111,
		Controller: zerotier.Fingerprint{
			Address: zerotier.Address(2222),
			Hash:    c.SerialNo,
		},
	})
	c.Subject.Certificates = append(c.Subject.Certificates, c.SerialNo)
	c.Subject.UpdateURLs = append(c.Subject.UpdateURLs, "https://www.zerotier.com/asdfasdf")
	c.Subject.Name.SerialNo = "a"
	c.Subject.Name.CommonName = "b"
	c.Subject.Name.StreetAddress = "c"
	c.Subject.Name.Locality = "d"
	c.Subject.Name.Province = "e"
	c.Subject.Name.PostalCode = "f"
	c.Subject.Name.Country = "g"
	c.Subject.Name.Organization = "h"
	c.Subject.Name.Unit = "i"
	c.Subject.Name.Email = "j"
	c.Subject.Name.URL = "k"
	c.Subject.Name.Host = "l"
	c.Subject.UniqueID = uniqueId

	c.Issuer = id
	c.IssuerName.SerialNo = "m"
	c.IssuerName.CommonName = "n"
	c.IssuerName.StreetAddress = "o"
	c.IssuerName.Locality = "p"
	c.IssuerName.Province = "q"
	c.IssuerName.PostalCode = "r"
	c.IssuerName.Country = "s"
	c.IssuerName.Organization = "t"
	c.IssuerName.Unit = "u"
	c.IssuerName.Email = "v"
	c.IssuerName.URL = "w"
	c.IssuerName.Host = "x"

	c.ExtendedAttributes = c.SerialNo
	c.MaxPathLength = 9999
	c.Signature = []byte("qwerty")

	fmt.Printf("Checking certificate marshal/unmarshal (10000 tests)... ")
	for k := 0; k < 10000; k++ {
		cb, err := c.Marshal()
		if err != nil {
			fmt.Printf("marshal FAILED (%s)\n", err.Error())
			return false
		}
		c2, err := zerotier.NewCertificateFromBytes(cb, false)
		if err != nil {
			fmt.Printf("unmarshal FAILED (%s)\n", err.Error())
			return false
		}
		cb2, err := c2.Marshal()
		if err != nil {
			fmt.Printf("second marshal FAILED (%s)\n", err.Error())
			return false
		}
		if !bytes.Equal(cb, cb2) {
			fmt.Printf("FAILED (results not equal)\n")
			return false
		}
	}
	fmt.Println("OK")

	fmt.Printf("Checking certificate CSR sign/verify (100 tests)... ")
	for k := 0; k < 100; k++ {
		csr, err := zerotier.NewCertificateCSR(&c.Subject, uniqueId, uniqueIdPrivate)
		if err != nil {
			fmt.Printf("CSR generate FAILED (%s)\n", err.Error())
			return false
		}
		//fmt.Printf("CSR size: %d ", len(csr))
		csr2, err := zerotier.NewCertificateFromBytes(csr, false)
		if err != nil {
			fmt.Printf("CSR decode FAILED (%s)\n", err.Error())
			return false
		}
		signedCert, err := csr2.Sign(id)
		if err != nil {
			fmt.Printf("CSR sign FAILED (%s)\n", err.Error())
			return false
		}
		if len(signedCert.Signature) == 0 {
			fmt.Println("CSR sign FAILED (no signature found)", err.Error())
			return false
		}
	}
	fmt.Println("OK")

	return true
}
