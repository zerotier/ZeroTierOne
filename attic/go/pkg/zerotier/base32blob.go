/*
 * Copyright (C)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

package zerotier

import (
	"encoding/json"
	"strings"
)

// Base32Blob is a byte array that JSON serializes to a Base32 string.
type Base32Blob []byte

// MarshalJSON returns this blob marshaled as a byte array or a string.
func (b *Base32Blob) MarshalJSON() ([]byte, error) {
	return []byte("\""+Base32.EncodeToString(*b)+"\""), nil
}

// UnmarshalJSON unmarshals this blob from a JSON array or string.
func (b *Base32Blob) UnmarshalJSON(j []byte) error {
	var b32 string
	err := json.Unmarshal(j, &b32)
	if err != nil {
		return err
	}
	*b, err = Base32.DecodeString(strings.TrimSpace(b32))
	return err
}
