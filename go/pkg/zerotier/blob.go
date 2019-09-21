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

// This is copied from the LF code base to make JSON blob encoding uniform

import (
	"encoding/json"
	"unicode/utf8"
)

// Blob is a byte array that serializes to a string or a base62 string prefixed by \b (binary)
type Blob []byte

// MarshalJSON returns this blob marshaled as a string using \b<base62> for non-UTF8 binary data.
func (b Blob) MarshalJSON() ([]byte, error) {
	if utf8.Valid(b) {
		return json.Marshal(string(b))
	}
	return []byte("\"\\b" + Base62Encode(b) + "\""), nil
}

// UnmarshalJSON unmarshals this blob from a string or byte array.
func (b *Blob) UnmarshalJSON(j []byte) error {
	var s string
	err := json.Unmarshal(j, &s)
	if err == nil {
		if len(s) == 0 {
			*b = nil
		} else if s[0] == '\b' {
			*b = Base62Decode(s[1:])
			return nil
		}
		*b = []byte(s)
		return nil
	}

	// Byte arrays are also accepted
	var bb []byte
	if json.Unmarshal(j, &bb) != nil {
		return err
	}
	*b = bb
	return nil
}
