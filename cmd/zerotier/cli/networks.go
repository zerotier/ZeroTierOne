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

func Networks(basePath string, authTokenGenerator func() string, args []string, jsonOutput bool) int {
	var networks []zerotier.APINetwork
	apiGet(basePath, authTokenGenerator(), "/network", &networks)

	if jsonOutput {
		fmt.Println(jsonDump(networks))
	} else {
		fmt.Printf("%-16s %-24s %-17s %-8s <type>  <device>         <managed IP(s)>\n", "<id>", "<name>", "<mac>", "<status>")
		for _, nw := range networks {
			t := "PRIVATE"
			if nw.Config.Type == zerotier.NetworkTypePublic {
				t = "PUBLIC"
			}
			fmt.Printf("%.16x %-24s %-17s %-16s %-7s %-16s ", uint64(nw.ID), nw.Config.Name, nw.Config.MAC.String(), networkStatusStr(nw.Config.Status), t, nw.PortName)
			for i, ip := range nw.Config.AssignedAddresses {
				if i > 0 {
					fmt.Print(",")
				}
				fmt.Print(ip.String())
			}
			fmt.Print("\n")
		}
	}

	return 0
}
