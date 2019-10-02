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
	"encoding/json"
	"fmt"
	"io/ioutil"
	"os"
	"strings"

	"zerotier/pkg/zerotier"
)

func locatorNew(args []string) {
	if len(args) < 2 {
		Help()
		os.Exit(1)
	}

	identity := readIdentity(args[0])
	if !identity.HasPrivate() {
		fmt.Println("FATAL: identity does not contain a secret key (required to sign locator)")
		os.Exit(1)
	}

	var virt []*zerotier.Identity
	var phys []*zerotier.InetAddress
	for i := 1; i < len(args); i++ {
		if strings.Contains(args[i], "/") {
			a := zerotier.NewInetAddressFromString(args[i])
			if a == nil {
				fmt.Printf("FATAL: IP/port address '%s' is not valid\n", args[i])
				os.Exit(1)
			}
			phys = append(phys, a)
		} else {
			a, err := zerotier.NewIdentityFromString(args[i])
			if err != nil {
				fmt.Printf("FATAL: identity (virtual address) '%s' is not valid: %s\n", args[i], err.Error())
				os.Exit(1)
			}
			virt = append(virt, a)
		}
	}

	loc, err := zerotier.NewLocator(identity, virt, phys)
	if err != nil {
		fmt.Printf("FATAL: internal error creating locator: %s\n", err.Error())
		os.Exit(1)
	}
	fmt.Println(jsonDump(loc))
	os.Exit(0)
}

func locatorNewDNSKey(args []string) {
	if len(args) != 0 {
		Help()
		os.Exit(0)
	}

	sk, err := zerotier.NewLocatorDNSSigningKey()
	if err != nil {
		fmt.Printf("FATAL: error creating secure DNS signing key: %s", err.Error())
		os.Exit(1)
	}
	fmt.Println(jsonDump(sk))
	os.Exit(0)
}

func locatorGetDNS(args []string) {
	if len(args) < 2 {
		Help()
		os.Exit(1)
	}

	keyData, err := ioutil.ReadFile(args[0])
	if err != nil {
		fmt.Printf("FATAL: unable to read secure DNS key file: %s\n", err.Error())
		os.Exit(1)
	}
	var sk zerotier.LocatorDNSSigningKey
	err = json.Unmarshal(keyData, &sk)
	if err != nil {
		fmt.Printf("FATAL: DNS key file invalid: %s", err.Error())
		os.Exit(1)
	}

	locData, err := ioutil.ReadFile(args[1])
	if err != nil {
		fmt.Printf("FATAL: unable to read locator: %s\n", err.Error())
		os.Exit(1)
	}
	var loc zerotier.Locator
	err = json.Unmarshal(locData, &loc)
	if err != nil {
		fmt.Printf("FATAL: locator invalid: %s", err.Error())
		os.Exit(1)
	}

	txt, err := loc.MakeTXTRecords(&sk)
	if err != nil {
		fmt.Printf("FATAL: error creating TXT records: %s\n", err.Error())
		os.Exit(1)
	}
	for _, t := range txt {
		fmt.Println(t)
	}
	os.Exit(0)
}

// Locator CLI command
func Locator(args []string) {
	if len(args) > 0 {
		switch args[0] {
		case "new":
			locatorNew(args[1:])
		case "newdnskey":
			locatorNewDNSKey(args[1:])
		case "getdns":
			locatorGetDNS(args[1:])
		}
	}
	Help()
	os.Exit(1)
}
