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
	"io/ioutil"
	"net/url"
	"os"
	"strings"

	"zerotier/pkg/zerotier"
)

// AddRoot CLI command
func AddRoot(basePath, authToken string, args []string) {
	if len(args) == 0 {
		Help()
		os.Exit(0)
	}

	locData, err := ioutil.ReadFile(args[0])
	if err != nil {
		fmt.Printf("ERROR: unable to read locator: %s\n", err.Error())
		os.Exit(1)
	}
	loc, err := zerotier.NewLocatorFromBytes(locData)
	if err != nil {
		fmt.Printf("ERROR: invalid locator in file '%s': %s\n", args[0], err.Error())
		os.Exit(1)
	}

	var name string
	if len(args) > 1 {
		if len(args) > 2 {
			Help()
			os.Exit(1)
		}
		name = strings.TrimSpace(args[1])
	}

	var result zerotier.Root
	apiPost(basePath, authToken, "/root/"+url.PathEscape(name), &zerotier.Root{
		Name:    name,
		Locator: loc,
	}, &result)

	fmt.Println(jsonDump(&result))
	os.Exit(0)
}
