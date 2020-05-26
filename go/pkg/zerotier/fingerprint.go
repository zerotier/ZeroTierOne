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

//#cgo CFLAGS: -O3
//#include "../../native/GoGlue.h"
import "C"

import (
	"bytes"
	"errors"
	"strings"
	"unsafe"
)

type Fingerprint struct {
	Address Address  `json:"address"`
	Hash    [48]byte `json:"hash"`
}

func NewFingerprintFromString(fps string) (*Fingerprint, error) {
	fpb, err := Base32StdLowerCase.DecodeString(strings.TrimSpace(strings.ToLower(fps)))
	if err != nil {
		return nil, err
	}
	if len(fpb) != 53 {
		return nil, errors.New("invalid fingerprint length")
	}
	var fp Fingerprint
	fp.Address, _ = NewAddressFromBytes(fpb[0:5])
	copy(fp.Hash[:],fpb[5:])
	return &fp, nil
}

func (fp *Fingerprint) String() string {
	var tmp [53]byte
	fp.Address.CopyTo(tmp[0:5])
	copy(tmp[5:],fp.Hash[:])
	return Base32StdLowerCase.EncodeToString(tmp[:])
}

func (fp *Fingerprint) Equals(fp2 *Fingerprint) bool {
	return fp.Address == fp2.Address && bytes.Equal(fp.Hash[:], fp2.Hash[:])
}

func (fp *Fingerprint) apiFingerprint() *C.ZT_Fingerprint {
	var apifp C.ZT_Fingerprint
	apifp.address = C.uint64_t(fp.Address)
	copy((*[48]byte)(unsafe.Pointer(&apifp.hash[0]))[:], fp.Hash[:])
	return &apifp
}
