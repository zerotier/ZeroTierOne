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

func Leave(basePath, authToken string, args []string) {
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

	apiDelete(basePath, authToken, "/network/"+nwids, nil)
	fmt.Printf("OK %s", nwids)
	os.Exit(0)
}
