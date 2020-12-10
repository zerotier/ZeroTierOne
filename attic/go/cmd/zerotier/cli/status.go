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

func Status(basePath string, authTokenGenerator func() string, args []string, jsonOutput bool) int {
	var status zerotier.APIStatus
	apiGet(basePath, authTokenGenerator(), "/status", &status)

	if jsonOutput {
		fmt.Println(jsonDump(&status))
	} else {
		online := "ONLINE"
		if !status.Online {
			online = "OFFLINE"
		}
		fmt.Printf("%.10x: %s %s\n", uint64(status.Address), online, status.Version)
		fmt.Printf("\tidentity:\t%s\n", status.Identity.String())
		if status.Config.Settings.SecondaryPort > 0 && status.Config.Settings.SecondaryPort < 65536 {
			fmt.Printf("\tports:\t%d %d\n", status.Config.Settings.PrimaryPort, status.Config.Settings.SecondaryPort)
		} else {
			fmt.Printf("\tports:\t%d\n", status.Config.Settings.PrimaryPort)
		}
		fmt.Printf("\tport mapping (uPnP/NAT-PMP):\t%s\n", enabledDisabled(status.Config.Settings.PortMapping))
		fmt.Printf("\tblacklisted interface prefixes:\t")
		for i, bl := range status.Config.Settings.InterfacePrefixBlacklist {
			if i > 0 {
				fmt.Print(" ")
			}
			fmt.Print(bl)
		}
		fmt.Printf("\n\texplicit external addresses: ")
		for i, ea := range status.Config.Settings.ExplicitAddresses {
			if i > 0 {
				fmt.Print(" ")
			}
			fmt.Print(ea.String())
		}
		fmt.Printf("\n\tsystem interface addresses: ")
		for i, a := range status.InterfaceAddresses {
			if i > 0 {
				fmt.Print(" ")
			}
			fmt.Print(a.String())
		}
		fmt.Printf("\n\tmapped external addresses: ")
		for i, a := range status.MappedExternalAddresses {
			if i > 0 {
				fmt.Print(" ")
			}
			fmt.Print(a.String())
		}
		fmt.Printf("\n")
	}

	return 0
}
