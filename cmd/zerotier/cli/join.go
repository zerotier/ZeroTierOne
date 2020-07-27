/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
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

package cli

import (
	"flag"
	"fmt"
	"os"
	"strconv"
	"strings"

	"zerotier/pkg/zerotier"
)

func Join(basePath, authToken string, args []string) int {
	joinOpts := flag.NewFlagSet("join", flag.ContinueOnError)
	controllerAuthToken := joinOpts.String("a", "", "")
	controllerFingerprint := joinOpts.String("c", "", "")
	err := joinOpts.Parse(os.Args[1:])
	if err != nil {
		Help()
		return 1
	}
	args = joinOpts.Args()
	if len(args) < 1 {
		Help()
		return 1
	}
	if len(args[0]) != zerotier.NetworkIDStringLength {
		fmt.Printf("ERROR: invalid network ID: %s\n", args[0])
		return 1
	}

	_ = *controllerAuthToken // TODO: not implemented yet

	var fp *zerotier.Fingerprint
	if len(*controllerFingerprint) > 0 {
		if strings.ContainsRune(*controllerFingerprint, '-') {
			fp, err = zerotier.NewFingerprintFromString(*controllerFingerprint)
			if err != nil {
				fmt.Printf("ERROR: invalid network controller fingerprint: %s\n", *controllerFingerprint)
				return 1
			}
		} else {
			id, err := zerotier.NewIdentityFromString(*controllerFingerprint)
			if err != nil {
				fmt.Printf("ERROR: invalid network controller identity: %s\n", *controllerFingerprint)
				return 1
			}
			fp = id.Fingerprint()
		}
	}

	nwid, err := strconv.ParseUint(args[0], 16, 64)
	if err != nil {
		fmt.Printf("ERROR: invalid network ID: %s\n", args[0])
		return 1
	}
	nwids := fmt.Sprintf("%.16x", nwid)

	var network zerotier.APINetwork
	network.ID = zerotier.NetworkID(nwid)
	network.ControllerFingerprint = fp

	if apiPost(basePath, authToken, "/network/"+nwids, &network, nil) <= 0 {
		fmt.Printf("FAILED\n")
	} else {
		if fp == nil {
			fmt.Printf("OK %s\n", nwids)
		} else {
			fmt.Printf("OK %s %s\n", nwids, fp.String())
		}
	}

	return 0
}
