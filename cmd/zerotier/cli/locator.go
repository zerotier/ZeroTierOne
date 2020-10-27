/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
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

package cli

import (
	"fmt"
	"zerotier/pkg/zerotier"
)

func Locator(args []string) int {
	if len(args) < 1 {
		Help()
		return 1
	}

	switch args[0] {

	case "new":
		if len(args) < 3 {
			Help()
			return 1
		}
		id := cliGetIdentityOrFatal(args[1])
		if !id.HasPrivate() {
			pErr("identity must include secret key to sign locator")
			return 1
		}
		var eps []*zerotier.Endpoint
		for i:=2;i<len(args);i++ {
			ep, err := zerotier.NewEndpointFromString(args[i])
			if err != nil {
				pErr("invalid endpoint: %s (%s)", args[i], err.Error())
				return 1
			}
			eps = append(eps, ep)
		}
		loc, err := zerotier.NewLocator(zerotier.TimeMs(), eps, id)
		if err != nil {
			pErr("error creating or signing locator: %s", err.Error())
			return 1
		}
		fmt.Println(loc.String())

	case "verify":
		if len(args) != 3 {
			Help()
			return 1
		}
		id := cliGetIdentityOrFatal(args[1])
		loc := cliGetLocatorOrFatal(args[2])
		if !loc.Validate(id) {
			fmt.Println("FAILED")
			return 1
		}
		fmt.Println("OK")

	case "show":
		if len(args) != 2 {
			Help()
			return 1
		}
		loc := cliGetLocatorOrFatal(args[1])
		fmt.Printf("%s %s\n",loc.Fingerprint.Address.String(),loc.Fingerprint.String())
		for _, e := range loc.Endpoints {
			fmt.Printf("\t%s\n",e.String())
		}

	}

	return 0
}
