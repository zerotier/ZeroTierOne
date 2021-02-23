/*
 * Copyright (C)2013-2020 ZeroTier, Inc.
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

package main

import (
	"bytes"
	"fmt"
	"zerotier/pkg/zerotier"
)

func TestLocator() bool {
	fmt.Printf("Creating Endpoint instances... ")
	ep0, err := zerotier.NewEndpointFromString("1.1.1.1/1")
	if err != nil {
		fmt.Printf("IPv4 FAILED (%s)\n",err.Error())
		return false
	}
	ep1, err := zerotier.NewEndpointFromString("2600:1901:0:4006::1234/2")
	if err != nil {
		fmt.Printf("IPv6 FAILED (%s)\n",err.Error())
		return false
	}
	eps := []*zerotier.Endpoint{ep0, ep1}
	fmt.Printf("OK\n")

	fmt.Printf("Creating signing Identity... ")
	signer, err := zerotier.NewIdentity(zerotier.IdentityTypeP384)
	if err != nil {
		fmt.Printf("FAILED (%s)\n", err.Error())
		return false
	}
	fmt.Printf("OK %s\n",signer.String())

	fmt.Printf("Creating Locator instance... ")
	loc, err := zerotier.NewLocator(zerotier.TimeMs(), eps, signer)
	if err != nil {
		fmt.Printf("FAILED (%s)\n",err.Error())
		return false
	}
	locStr := loc.String()
	locBytes := loc.Bytes()
	fmt.Printf("OK (%d bytes)\n",len(locBytes))

	fmt.Printf("Testing Locator Validate()... ")
	if !loc.Validate(signer) {
		fmt.Printf("FAILED (should have validated)\n")
		return false
	}
	fmt.Printf("OK\n")

	fmt.Printf("Testing Locator marshal/unmarshal... ")
	loc2, err := zerotier.NewLocatorFromString(locStr)
	if err != nil {
		fmt.Printf("FAILED (%s)\n",err.Error())
		return false
	}
	if !bytes.Equal(locBytes, loc2.Bytes()) {
		fmt.Printf("FAILED (not equal)\n")
		return false
	}
	loc2, err = zerotier.NewLocatorFromBytes(locBytes)
	if err != nil {
		fmt.Printf("FAILED (%s)\n",err.Error())
		return false
	}
	if !bytes.Equal(locBytes, loc2.Bytes()) {
		fmt.Printf("FAILED (not equal)\n")
		return false
	}
	fmt.Printf("OK\n")

	return true
}
