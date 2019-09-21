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

// Err is a basic string error type for ZeroTier
type Err string

func (e Err) Error() string { return (string)(e) }

// Simple ZeroTier Errors
const (
	ErrInvalidMACAddress Err = "invalid MAC address"
	ErrInvalidParameter  Err = "invalid parameter"
)
