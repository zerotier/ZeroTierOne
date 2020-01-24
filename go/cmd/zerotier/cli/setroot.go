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

// SetRoot CLI command, used for addroot and removeroot.
func SetRoot(basePath, authToken string, args []string, root bool) {
	if len(args) < 1 || len(args) > 2 {
		Help()
		os.Exit(1)
	}

	id := readIdentity(args[0])
	if id == nil {
		fmt.Printf("ERROR: invalid identity '%s' (tried literal or reading as file)\n",args[0])
		os.Exit(1)
	}

	var bootstrap *zerotier.InetAddress
	if len(args) == 2 {
		bootstrap = zerotier.NewInetAddressFromString(args[1])
		if bootstrap == nil || bootstrap.Nil() {
			fmt.Printf("ERROR: invalid bootstrap address '%s'\n",args[1])
			os.Exit(1)
		}
	}

	var peer zerotier.PeerMutableFields
	peer.Identity = id
	peer.Bootstrap = bootstrap
	peer.Root = &root
	apiPost(basePath, authToken, "/peer/"+id.Address().String(), &peer, nil)
	fmt.Printf("OK %s", id.String())
	os.Exit(0)
}
