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
	"fmt"
	"strings"
	"zerotier/pkg/zerotier"
)

func listPeers(basePath, authToken string, jsonOutput bool, rootsOnly bool) int {
	var peers []zerotier.Peer
	apiGet(basePath, authToken, "/peer", &peers)

	if rootsOnly {
		roots := make([]zerotier.Peer, 0, len(peers))
		for i := range peers {
			if peers[i].Root {
				roots = append(roots, peers[i])
			}
		}
		peers = roots
	}

	if jsonOutput {
		fmt.Println(jsonDump(&peers))
	} else {
		fmt.Printf("<address>  <ver>   <root> <lat(ms)> <path(s)>\n")
		for _, peer := range peers {
			root := ""
			if peer.Root {
				root = " *"
			}

			var paths strings.Builder
			if len(peer.Paths) > 0 {
				if paths.Len() > 0 {
					paths.WriteRune(' ')
				}
				paths.WriteString(peer.Paths[0].Endpoint.String())
			} else {
				paths.WriteString("(relayed)")
			}

			fmt.Printf("%.10x %-7s %-6s %-9d %s\n",
				uint64(peer.Address),
				fmt.Sprintf("%d.%d.%d", peer.Version[0], peer.Version[1], peer.Version[2]),
				root,
				peer.Latency,
				paths.String())
		}
	}

	return 0
}

func Peer(basePath string, authTokenGenerator func() string, args []string, jsonOutput bool) int {
	if len(args) < 1 {
		Help()
		return 1
	}

	authToken := authTokenGenerator()

	//var addr zerotier.Address
	if isValidAddress(args[0]) {
		//addr, _ = zerotier.NewAddressFromString(args[0])
		args = args[1:]
		if len(args) < 1 {
			Help()
			return 1
		}
	}

	switch args[0] {

	case "list":
		return listPeers(basePath, authToken, jsonOutput, false)
	case "listroots":
		return listPeers(basePath, authToken, jsonOutput, true)

	case "show":

	case "try":

	}

	return 0
}
