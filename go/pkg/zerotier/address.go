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

import (
	"encoding/json"
	"fmt"
	"strconv"
)

// Address represents a 40-bit short ZeroTier address
type Address uint64

// NewAddressFromString parses a 10-digit ZeroTier address
func NewAddressFromString(s string) (Address, error) {
	if len(s) != 10 {
		return Address(0), ErrInvalidZeroTierAddress
	}
	a, err := strconv.ParseUint(s, 16, 64)
	return Address(a & 0xffffffffff), err
}

// String returns this address's 10-digit hex identifier
func (a Address) String() string {
	return fmt.Sprintf("%.10x", uint64(a))
}

// MarshalJSON marshals this Address as a string
func (a Address) MarshalJSON() ([]byte, error) {
	return []byte("\"" + a.String() + "\""), nil
}

// UnmarshalJSON unmarshals this Address from a string
func (a *Address) UnmarshalJSON(j []byte) error {
	var s string
	err := json.Unmarshal(j, &s)
	if err != nil {
		return err
	}
	tmp, err := NewAddressFromString(s)
	*a = tmp
	return err
}
