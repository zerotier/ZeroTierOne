/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
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

// Roots CLI command
func Roots(basePath, authToken string, args []string, jsonOutput bool) {
	var roots []zerotier.Root
	apiGet(basePath, authToken, "/root", &roots)

	if jsonOutput {
		fmt.Println(jsonDump(roots))
	} else {
		fmt.Printf("%32s <address>  <physical/virtual>\n", "<name>")
		for _, r := range roots {
			rn := r.Name
			if len(rn) > 32 {
				rn = rn[len(rn)-32:]
			}
			if r.Locator != nil {
				if len(r.Locator.Physical) == 0 && len(r.Locator.Virtual) == 0 {
					fmt.Printf("%32s %.10x -\n", rn, uint64(r.Locator.Identity.Address()))
				} else {
					fmt.Printf("%32s %.10x ", rn, uint64(r.Locator.Identity.Address()))
					for i, a := range r.Locator.Physical {
						if i > 0 {
							fmt.Print(",")
						}
						fmt.Print(a.String())
					}
					for i, a := range r.Locator.Virtual {
						if i > 0 || len(r.Locator.Physical) > 0 {
							fmt.Print(",")
						}
						fmt.Print(a.String())
					}
					fmt.Printf("\n")
				}
			} else {
				fmt.Printf("%32s -          -\n", rn)
			}
		}
	}

	os.Exit(0)
}
