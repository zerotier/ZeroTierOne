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

package zerotier

//#include "../../native/GoGlue.h"
import "C"

import "fmt"

// SelfTest runs a series of tests on the ZeroTier core and the Go service code, returning true on success.
// Results are sent to stdout.
func SelfTest() bool {
	fmt.Print("Running ZeroTier core tests...\n\n")

	if C.ZT_TestOther() != 0 {
		return false
	}
	fmt.Println()
	if C.ZT_TestCrypto() != 0 {
		return false
	}
	fmt.Println()
	if C.ZT_TestIdentity() != 0 {
		return false
	}

	return true
}
