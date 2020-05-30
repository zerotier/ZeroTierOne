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
	"time"
	"zerotier/pkg/zerotier"
)

func Locator(args []string) {
	if len(args) > 0 {
		switch args[0] {

		case "new":
			if len(args) >= 3 {
				id := readIdentity(args[1])
				if !id.HasPrivate() {
					fmt.Println("ERROR: identity is missing private key and can't be used to sign a locator.")
					os.Exit(1)
				}
				var eps []zerotier.Endpoint
				for i:=2;i<len(args);i++ {
					ep, _ := zerotier.NewEndpointFromString(args[i])
					if ep != nil {
						eps = append(eps, *ep)
					}
				}
				loc, err := zerotier.NewLocator(zerotier.TimeMs(),eps,id)
				if err != nil {
					fmt.Printf("ERROR: unable to create or sign locator: %s\n",err.Error())
					os.Exit(1)
				}
				fmt.Println(loc.String())
				os.Exit(0)
			}

		case "show":
			if len(args) > 1 && len(args) < 4 {
				loc := readLocator(args[1])
				var id *zerotier.Identity
				if len(args) == 3 {
					id = readIdentity(args[2])
				}
				ts, fp, eps, valid, _ := loc.GetInfo(id)
				fmt.Printf("%s\n  Timestamp: %s (%d)\n  Validity: ",fp.String(),time.Unix(ts / 1000,ts * 1000).String(),ts)
				if id == nil {
					fmt.Printf("(no identity provided)\n")
				} else {
					if valid {
						fmt.Printf("SIGNATURE VERIFIED\n")
					} else {
						fmt.Printf("! INVALID SIGNATURE\n")
					}
				}
				fmt.Print("  Endpoints: ")
				for i := range eps {
					if i > 0 {
						fmt.Print(" ")
					}
					fmt.Print(eps[i].String())
				}
				fmt.Printf("\n")
			}

		}

	}
	Help()
	os.Exit(1)
}
