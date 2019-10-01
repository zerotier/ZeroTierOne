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
	"encoding/hex"
	"fmt"
	"io/ioutil"
	"os"
	"strings"

	"zerotier/pkg/zerotier"
)

// Identity command
func Identity(args []string) {
	if len(args) > 0 {
		switch args[0] {

		case "new":
			idType := zerotier.IdentityTypeC25519
			if len(args) > 1 {
				if len(args) > 2 {
					Help()
					os.Exit(1)
				}
				switch args[1] {
				case "c25519":
				case "p384":
					idType = zerotier.IdentityTypeP384
				default:
					Help()
					os.Exit(1)
				}
			}
			id, err := zerotier.NewIdentity(idType)
			if err != nil {
				fmt.Printf("ERROR: internal error generating identity: %s\n", err.Error())
				os.Exit(1)
			}
			fmt.Println(id.PrivateKeyString())
			os.Exit(0)

		case "getpublic":
			if len(args) == 2 {
				idData, err := ioutil.ReadFile(args[1])
				if err != nil {
					fmt.Printf("ERROR: unable to read identity: %s\n", err.Error())
					os.Exit(1)
				}
				id, err := zerotier.NewIdentityFromString(string(idData))
				if err != nil {
					fmt.Printf("ERROR: identity in file '%s' invalid: %s\n", args[1], err.Error())
					os.Exit(1)
				}
				fmt.Println(id.String())
				os.Exit(0)
			}

		case "validate":
			if len(args) == 2 {
				idData, err := ioutil.ReadFile(args[1])
				if err != nil {
					fmt.Printf("ERROR: unable to read identity: %s\n", err.Error())
					os.Exit(1)
				}
				id, err := zerotier.NewIdentityFromString(string(idData))
				if err != nil {
					fmt.Printf("ERROR: identity in file '%s' invalid: %s\n", args[1], err.Error())
					os.Exit(1)
				}
				if id.LocallyValidate() {
					fmt.Println("OK")
					os.Exit(0)
				}
				fmt.Println("FAILED")
				os.Exit(1)
			}

		case "sign", "verify":
			if len(args) > 2 {
				idData, err := ioutil.ReadFile(args[1])
				if err != nil {
					fmt.Printf("ERROR: unable to read identity: %s\n", err.Error())
					os.Exit(1)
				}
				id, err := zerotier.NewIdentityFromString(string(idData))
				if err != nil {
					fmt.Printf("ERROR: identity in file '%s' invalid: %s\n", args[1], err.Error())
					os.Exit(1)
				}
				msg, err := ioutil.ReadFile(args[2])
				if err != nil {
					fmt.Printf("ERROR: unable to read input file: %s\n", err.Error())
					os.Exit(1)
				}

				if args[0] == "verify" {
					if len(args) == 4 {
						sig, err := hex.DecodeString(strings.TrimSpace(args[3]))
						if err != nil {
							fmt.Println("FAILED")
							os.Exit(1)
						}
						if id.Verify(msg, sig) {
							fmt.Println("OK")
							os.Exit(0)
						}
					}
					fmt.Println("FAILED")
					os.Exit(1)
				} else {
					sig, err := id.Sign(msg)
					if err != nil {
						fmt.Printf("ERROR: internal error signing message: %s\n", err.Error())
						os.Exit(1)
					}
					fmt.Println(hex.EncodeToString(sig))
					os.Exit(0)
				}
			}

		}
	}
	Help()
	os.Exit(1)
}
