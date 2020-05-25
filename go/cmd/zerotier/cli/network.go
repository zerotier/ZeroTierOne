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
	"fmt"
	"os"
	"strconv"

	"zerotier/pkg/zerotier"
)

// Network CLI command
func Network(basePath, authToken string, args []string, jsonOutput bool) {
	if len(args) != 1 {
		Help()
		os.Exit(1)
	}

	if len(args[0]) != zerotier.NetworkIDStringLength {
		fmt.Printf("ERROR: invalid network ID: %s\n", args[0])
		os.Exit(1)
	}
	nwid, err := strconv.ParseUint(args[0], 16, 64)
	if err != nil {
		fmt.Printf("ERROR: invalid network ID: %s\n", args[0])
		os.Exit(1)
	}
	nwids := fmt.Sprintf("%.16x", nwid)

	var network zerotier.APINetwork
	apiGet(basePath, authToken, "/network/"+nwids, &network)

	if jsonOutput {
		fmt.Println(jsonDump(&network))
	} else {
		fmt.Printf("%s: %s\n", nwids, network.Config.Name)
		fmt.Printf("\tstatus:\t%s\n", networkStatusStr(network.Config.Status))
		enabled := "no"
		if network.PortEnabled {
			enabled = "yes"
		}
		bridge := "no"
		if network.Config.Bridge {
			bridge = "yes"
		}
		broadcast := "off"
		if network.Config.BroadcastEnabled {
			broadcast = "on"
		}
		fmt.Printf("\tport:\t%s dev %s type %s mtu %d enabled %s bridge %s broadcast %s\n", network.Config.MAC.String(), network.PortName, network.PortType, network.Config.MTU, enabled, bridge, broadcast)
		fmt.Printf("\tmanaged addresses:\t")
		for i, a := range network.Config.AssignedAddresses {
			if i > 0 {
				fmt.Print(" ")
			}
			fmt.Print(a.String())
		}
		fmt.Printf("\n\tmanaged routes:\t")
		for i, r := range network.Config.Routes {
			if i > 0 {
				fmt.Print(" ")
			}
			fmt.Print(r.Target.String())
			if r.Via == nil {
				fmt.Print("->LAN")
			} else {
				fmt.Printf("->%s", r.Via.String())
			}
		}
		managedIPs := "disabled"
		if network.Settings.AllowManagedIPs {
			managedIPs = "enabled"
		}
		managedIPsGlobal := "disabled"
		if network.Settings.AllowGlobalIPs {
			managedIPsGlobal = "enabled"
		}
		fmt.Printf("\n\tmanaged address local permissions:\t%s global %s\n", managedIPs, managedIPsGlobal)
		managedRoutes := "diabled"
		if network.Settings.AllowManagedRoutes {
			managedRoutes = "enabled"
		}
		managedGlobalRoutes := "disabled"
		if network.Settings.AllowGlobalRoutes {
			managedGlobalRoutes = "enabled"
		}
		managedDefaultRoute := "disabled"
		if network.Settings.AllowDefaultRouteOverride {
			managedDefaultRoute = "enabled"
		}
		fmt.Printf("\tmanaged route local permissions:\t%s global %s default %s\n", managedRoutes, managedGlobalRoutes, managedDefaultRoute)
	}
}
