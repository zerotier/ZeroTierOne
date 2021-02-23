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

package cli

import (
	"encoding/json"
	"fmt"
	"io/ioutil"
	"zerotier/pkg/zerotier"
)

func interactiveMakeSubject() *zerotier.CertificateSubject {
	s := new(zerotier.CertificateSubject)


	return s
}

func Cert(basePath string, authTokenGenerator func() string, args []string, jsonOutput bool) int {
	if len(args) < 1 {
		Help()
		return 1
	}

	switch args[0] {

	case "list":

	case "show":
		if len(args) != 1 {
			Help()
			return 1
		}

	case "newsid":
		if len(args) > 2 {
			Help()
			return 1
		}

		uniqueId, uniqueIdPrivate, err := zerotier.NewCertificateSubjectUniqueId(zerotier.CertificateUniqueIdTypeNistP384)
		if err != nil {
			pErr("unable to create unique ID and private key: %s", err.Error())
			return 1
		}

		sec, err := json.MarshalIndent(&zerotier.CertificateSubjectUniqueIDSecret{
			UniqueID: uniqueId,
			UniqueIDSecret: uniqueIdPrivate,
		}, "", "  ")
		if err != nil {
			pErr("unable to create unique ID and private key: %s", err.Error())
			return 1
		}

		if len(args) == 1 {
			fmt.Println(string(sec))
		} else {
			_ = ioutil.WriteFile(args[1], sec, 0600)
			pResult("%s", args[1])
		}

	case "newcsr":
		if len(args) != 4 {
			Help()
			return 1
		}

		var subject zerotier.CertificateSubject
		err := readJSONFile(args[1], &subject)
		if err != nil {
			pErr("unable to read subject from %s: %s", args[1], err.Error())
			return 1
		}

		var uniqueIdSecret zerotier.CertificateSubjectUniqueIDSecret
		err = readJSONFile(args[2], &uniqueIdSecret)
		if err != nil {
			pErr("unable to read unique ID secret from %s: %s", args[2], err.Error())
			return 1
		}

		csr, err := zerotier.NewCertificateCSR(&subject, uniqueIdSecret.UniqueID, uniqueIdSecret.UniqueIDSecret)
		if err != nil {
			pErr("problem creating CSR: %s", err.Error())
			return 1
		}

		err = ioutil.WriteFile(args[3], csr, 0644)
		if err == nil {
			pResult("%s", args[3])
		} else {
			pErr("unable to write CSR to %s: %s", args[3], err.Error())
			return 1
		}

	case "sign":
		if len(args) != 4 {
			Help()
			return 1
		}

		csrBytes, err := ioutil.ReadFile(args[1])
		if err != nil {
			pErr("unable to read CSR from %s: %s", args[1], err.Error())
			return 1
		}
		csr, err := zerotier.NewCertificateFromBytes(csrBytes, false)
		if err != nil {
			pErr("CSR in %s is invalid: %s", args[1], err.Error())
			return 1
		}

		signingIdentity := cliGetIdentityOrFatal(args[2])
		if signingIdentity == nil {
			pErr("unable to read identity from %s", args[2])
			return 1
		}
		if !signingIdentity.HasPrivate() {
			pErr("signing identity in %s lacks private key", args[2])
			return 1
		}

		cert, err := csr.Sign(signingIdentity)
		if err != nil {
			pErr("error signing CSR or generating certificate: %s", err.Error())
			return 1
		}

		cb, err := cert.Marshal()
		if err != nil {
			pErr("error marshaling signed certificate: %s", err.Error())
			return 1
		}

		err = ioutil.WriteFile(args[3], cb, 0644)
		if err == nil {
			pResult("%s", args[3])
		} else {
			pErr("unable to write signed certificate to %s: %s", args[3], err.Error())
			return 1
		}

	case "verify", "dump":
		if len(args) != 2 {
			Help()
			return 1
		}

		certBytes, err := ioutil.ReadFile(args[1])
		if err != nil {
			pErr("unable to read certificate from %s: %s", args[1], err.Error())
			return 1
		}

		cert, err := zerotier.NewCertificateFromBytes(certBytes, true)
		if err != nil {
			pErr("certificate in %s invalid: %s", args[1], err.Error())
			return 1
		}

		if args[0] == "dump" {
			fmt.Println(cert.JSON())
		} else {
			fmt.Println("OK")
		}

	case "import":

	case "restore":

	case "export":

	case "delete":

	}

	return 0
}
