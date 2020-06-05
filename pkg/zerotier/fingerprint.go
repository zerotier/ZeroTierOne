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

// #include "../../native/GoGlue.h"
import "C"

import (
	"bytes"
	"encoding/json"
	"fmt"
	"strings"
	"unsafe"
)

// FingerprintHashSize is the length of a fingerprint hash in bytes.
const FingerprintHashSize = 48

// Fingerprint bundles an address with an optional SHA384 full hash of the identity's key(s).
type Fingerprint struct {
	Address Address
	Hash    []byte
}

// NewFingerprintFromString decodes a string-format fingerprint.
// A fingerprint has the format address-hash, where address is a 10-digit
// ZeroTier address and a hash is a base32-encoded SHA384 hash. Fingerprints
// can be missing the hash in which case they are represented the same as
// an Address and the hash field will be nil.
func NewFingerprintFromString(fps string) (*Fingerprint, error) {
	if len(fps) < AddressStringLength {
		return nil, ErrInvalidZeroTierAddress
	}
	ss := strings.Split(fps, "-")
	if len(ss) < 1 || len(ss) > 2 {
		return nil, ErrInvalidParameter
	}
	a, err := NewAddressFromString(ss[0])
	if err != nil {
		return nil, err
	}
	if len(ss) == 2 {
		h, err := Base32StdLowerCase.DecodeString(ss[1])
		if err != nil {
			return nil, err
		}
		if len(h) != 48 {
			return nil, ErrInvalidParameter
		}
		return &Fingerprint{Address: a, Hash: h}, nil
	}
	return &Fingerprint{Address: a, Hash: nil}, nil
}

func newFingerprintFromCFingerprint(cfp *C.ZT_Fingerprint) *Fingerprint {
	var fp Fingerprint
	if uintptr(unsafe.Pointer(cfp)) != 0 {
		fp.Address = Address(cfp.address)
		fp.Hash = C.GoBytes(unsafe.Pointer(&cfp.hash[0]), 48)
		if allZero(fp.Hash) {
			fp.Hash = nil
		}
	}
	return &fp
}

// String returns an address or a full address-hash depenting on whether a hash is present.
func (fp *Fingerprint) String() string {
	if len(fp.Hash) == FingerprintHashSize {
		return fmt.Sprintf("%.10x-%s", uint64(fp.Address), Base32StdLowerCase.EncodeToString(fp.Hash))
	}
	return fp.Address.String()
}

// Equals test for full equality with another fingerprint (including hash).
func (fp *Fingerprint) Equals(fp2 *Fingerprint) bool {
	return fp.Address == fp2.Address && bytes.Equal(fp.Hash[:], fp2.Hash[:])
}

// BestSpecificityEquals compares either just the addresses or also the hashes if both are present.
func (fp *Fingerprint) BestSpecificityEquals(fp2 *Fingerprint) bool {
	if fp2 == nil || fp.Address != fp2.Address {
		return false
	}
	if len(fp.Hash) == FingerprintHashSize && len(fp2.Hash) == FingerprintHashSize {
		return bytes.Equal(fp.Hash, fp2.Hash)
	}
	return true
}

func (fp *Fingerprint) MarshalJSON() ([]byte, error) {
	return []byte("\"" + fp.String() + "\""), nil
}

func (fp *Fingerprint) UnmarshalJSON(j []byte) error {
	var s string
	err := json.Unmarshal(j, &s)
	if err != nil {
		return err
	}
	fp2, err := NewFingerprintFromString(s)
	fp.Address = fp2.Address
	fp.Hash = fp2.Hash
	return err
}

func (fp *Fingerprint) cFingerprint() *C.ZT_Fingerprint {
	var apifp C.ZT_Fingerprint
	apifp.address = C.uint64_t(fp.Address)
	copy((*[48]byte)(unsafe.Pointer(&apifp.hash[0]))[:], fp.Hash[:])
	return &apifp
}
