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
	"strconv"
	"strings"

	"zerotier/pkg/zerotier"
)

func listNetworks(basePath, authToken string, jsonOutput bool) int {
	var networks []zerotier.APINetwork
	apiGet(basePath, authToken, "/network", &networks)

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

func showNetwork(nwids string, network *zerotier.APINetwork, jsonOutput bool) {
	if jsonOutput {
		fmt.Println(jsonDump(&network))
	} else {
		fmt.Printf("%s\t%s\n", nwids, network.Config.Name)
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
		managedIPs := "blocked"
		if network.Settings.AllowManagedIPs {
			managedIPs = "allowed"
		}
		managedIPsGlobal := "blocked"
		if network.Settings.AllowGlobalIPs {
			managedIPsGlobal = "allowed"
		}
		fmt.Printf("\n\tmanaged address local permissions:\t%s global %s\n", managedIPs, managedIPsGlobal)
		managedRoutes := "blocked"
		if network.Settings.AllowManagedRoutes {
			managedRoutes = "allowed"
		}
		managedGlobalRoutes := "blocked"
		if network.Settings.AllowGlobalRoutes {
			managedGlobalRoutes = "allowed"
		}
		managedDefaultRoute := "blocked"
		if network.Settings.AllowDefaultRouteOverride {
			managedDefaultRoute = "allowed"
		}
		fmt.Printf("\tmanaged route local permissions:\t%s global %s default %s\n", managedRoutes, managedGlobalRoutes, managedDefaultRoute)
	}
}

func Network(basePath string, authTokenGenerator func() string, args []string, jsonOutput bool) int {
	if len(args) < 1 {
		Help()
		return 1
	}

	authToken := authTokenGenerator()

	if len(args) == 1 && args[0] == "list" {
		return listNetworks(basePath, authToken, jsonOutput)
	}

	if len(args[0]) != zerotier.NetworkIDStringLength {
		fmt.Printf("ERROR: invalid network ID: %s\n", args[0])
		return 1
	}
	nwid, err := strconv.ParseUint(args[0], 16, 64)
	if err != nil {
		fmt.Printf("ERROR: invalid network ID: %s\n", args[0])
		return 1
	}
	nwids := fmt.Sprintf("%.16x", nwid)

	var network zerotier.APINetwork
	apiGet(basePath, authToken, "/network/"+nwids, &network)

	if len(args) == 1 {
		showNetwork(nwids, &network, jsonOutput)
	} else {
		switch args[1] {

		case "show", "info":
			showNetwork(nwids, &network, jsonOutput)

		case "set":
			if len(args) > 3 {
				Help()
				return 1
			} else if len(args) > 2 {
				fieldName := strings.ToLower(strings.TrimSpace(args[2]))
				var field *bool
				switch fieldName {
				case "managedips":
					field = &network.Settings.AllowManagedIPs
				case "managedroutes":
					field = &network.Settings.AllowGlobalRoutes
				case "globalips":
					field = &network.Settings.AllowGlobalIPs
				case "globalroutes":
					field = &network.Settings.AllowGlobalRoutes
				case "defaultroute":
					field = &network.Settings.AllowDefaultRouteOverride
				default:
					Help()
					return 1
				}

				if len(args) == 3 {
					*field = isTrue(args[2])
				}

				fmt.Printf("%s\t%t\n", fieldName, allowedBlocked(*field))
			} else {
				fmt.Printf("manageips\t%s\n", allowedBlocked(network.Settings.AllowManagedIPs))
				fmt.Printf("manageroutes\t%s\n", allowedBlocked(network.Settings.AllowManagedRoutes))
				fmt.Printf("globalips\t%s\n", allowedBlocked(network.Settings.AllowGlobalIPs))
				fmt.Printf("globalroutes\t%s\n", allowedBlocked(network.Settings.AllowGlobalRoutes))
				fmt.Printf("defaultroute\t%s\n", allowedBlocked(network.Settings.AllowDefaultRouteOverride))
			}

		}
	}

	return 0
}
