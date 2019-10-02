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
	"net/http"
	"net/url"
	"os"
	"strings"

	"zerotier/pkg/zerotier"
)

// RemoveRoot CLI command
func RemoveRoot(basePath, authToken string, args []string) {
	if len(args) != 1 {
		Help()
		os.Exit(1)
	}
	result, _ := zerotier.APIDelete(basePath, zerotier.APISocketName, authToken, "/root/"+url.PathEscape(strings.TrimSpace(args[0])), nil)
	if result == http.StatusOK {
		fmt.Printf("%s removed\n", args[0])
		os.Exit(0)
	}
	fmt.Printf("ERROR: root %s not found or another error occurred: status code %d\n", args[0], result)
	os.Exit(1)
}
