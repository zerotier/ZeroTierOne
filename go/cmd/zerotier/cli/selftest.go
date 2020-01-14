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

func SelfTest() {
	if !zerotier.SelfTest() {
		fmt.Println("FAILED: at least one ZeroTier self-test reported failure.")
		os.Exit(1)
	}
	os.Exit(0)
}
