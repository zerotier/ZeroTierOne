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

// NewAddressFromBytes reads a 5-byte 40-bit address.
func NewAddressFromBytes(b []byte) (Address, error) {
	if len(b) < 5 {
		return Address(0), ErrInvalidZeroTierAddress
	}
	return Address((uint64(b[0]) << 32) | (uint64(b[1]) << 24) | (uint64(b[2]) << 16) | (uint64(b[3]) << 8) | uint64(b[4])), nil
}

// Copy this address to a byte array, which must be 5 bytes in length or this will panic.
func (a Address) CopyTo(b []byte) {
	b[0] = byte(a >> 32)
	b[1] = byte(a >> 24)
	b[2] = byte(a >> 16)
	b[3] = byte(a >> 8)
	b[4] = byte(a)
}

// IsReserved returns true if this address is reserved and therefore is not valid for a real node.
func (a Address) IsReserved() bool { return a == 0 || (a>>32) == 0xff }

// String returns this address's 10-digit hex identifier
func (a Address) String() string {
	return fmt.Sprintf("%.10x", uint64(a))
}

// MarshalJSON marshals this Address as a string
func (a Address) MarshalJSON() ([]byte, error) {
	return []byte(fmt.Sprintf("\"%.10x\"", uint64(a))), nil
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
