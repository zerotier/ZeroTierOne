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

type Fingerprint struct {
	Address Address
	Hash    []byte
}

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

func (fp *Fingerprint) String() string {
	if len(fp.Hash) == 48 {
		return fmt.Sprintf("%.10x-%s", uint64(fp.Address), Base32StdLowerCase.EncodeToString(fp.Hash))
	}
	return fp.Address.String()
}

func (fp *Fingerprint) Equals(fp2 *Fingerprint) bool {
	return fp.Address == fp2.Address && bytes.Equal(fp.Hash[:], fp2.Hash[:])
}

func (fp *Fingerprint) cFingerprint() *C.ZT_Fingerprint {
	var apifp C.ZT_Fingerprint
	apifp.address = C.uint64_t(fp.Address)
	copy((*[48]byte)(unsafe.Pointer(&apifp.hash[0]))[:], fp.Hash[:])
	return &apifp
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
