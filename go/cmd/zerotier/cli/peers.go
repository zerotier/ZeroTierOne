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
	"strings"

	"zerotier/pkg/zerotier"
)

// Peers CLI command (also used for 'roots' command with rootsOnly set to true)
func Peers(basePath, authToken string, args []string, jsonOutput bool, rootsOnly bool) {
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
				paths.WriteString(fmt.Sprintf("%s/%d", peer.Paths[0].IP.String(), peer.Paths[0].Port))
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

	os.Exit(0)
}
