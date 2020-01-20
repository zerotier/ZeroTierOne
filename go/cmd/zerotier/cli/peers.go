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

	"zerotier/pkg/zerotier"
)

// Peers CLI command
func Peers(basePath, authToken string, args []string, jsonOutput bool) {
	var peers []zerotier.Peer
	apiGet(basePath, authToken, "/peer", &peers)

	if jsonOutput {
		fmt.Println(jsonDump(&peers))
	} else {
		fmt.Printf("<address>  <ver>   <role> <lat> <link> <lastTX> <lastRX> <path(s)>\n")
		for _, peer := range peers {
			role := "LEAF"
			link := "RELAY"
			lastTX, lastRX := int64(0), int64(0)
			address := ""
			if len(peer.Paths) > 0 {
				link = "DIRECT"
				lastTX, lastRX = peer.Clock-peer.Paths[0].LastSend, peer.Clock-peer.Paths[0].LastReceive
				if lastTX < 0 {
					lastTX = 0
				}
				if lastRX < 0 {
					lastRX = 0
				}
				address = fmt.Sprintf("%s/%d", peer.Paths[0].IP.String(), peer.Paths[0].Port)
			}
			fmt.Printf("%.10x %-7s %-6s %-5d %-6s %-8d %-8d %s\n",
				uint64(peer.Address),
				fmt.Sprintf("%d.%d.%d", peer.Version[0], peer.Version[1], peer.Version[2]),
				role,
				peer.Latency,
				link,
				lastTX,
				lastRX,
				address,
			)
		}
	}

	os.Exit(0)
}
