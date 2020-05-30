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
	"runtime"
	"unsafe"
)

type Locator struct {
	cl unsafe.Pointer
}

func locatorFinalizer(obj interface{}) {
	if obj != nil {
		C.ZT_Locator_delete(obj.(Locator).cl)
	}
}

func NewLocator(ts int64, endpoints []Endpoint, signer *Identity) (*Locator, error) {
	if ts <= 0 || len(endpoints) == 0 || signer == nil {
		return nil, ErrInvalidParameter
	}
	eps := make([]C.ZT_Endpoint, 0, len(endpoints))
	for _, e := range endpoints {
		eps = append(eps, e.cep)
	}
	signer.initCIdentityPtr()
	loc := C.ZT_Locator_create(C.int64_t(ts), &eps[0], C.uint(len(eps)), signer.cid)
	if uintptr(loc) == 0 {
		return nil, ErrInvalidParameter
	}
	goloc := new(Locator)
	goloc.cl = unsafe.Pointer(loc)
	runtime.SetFinalizer(goloc, locatorFinalizer)
	return goloc, nil
}

func NewLocatorFromBytes(lb []byte) (*Locator, error) {
	if len(lb) == 0 {
		return nil, ErrInvalidParameter
	}
	loc := C.ZT_Locator_unmarshal(unsafe.Pointer(&lb[0]), C.uint(len(lb)))
	if uintptr(loc) == 0 {
		return nil, ErrInvalidParameter
	}
	goloc := new(Locator)
	goloc.cl = unsafe.Pointer(loc)
	runtime.SetFinalizer(goloc, locatorFinalizer)
	return goloc, nil
}

func NewLocatorFromString(s string) (*Locator, error) {
	if len(s) == 0 {
		return nil, ErrInvalidParameter
	}
	sb := []byte(s)
	sb = append(sb,0)
	loc := C.ZT_Locator_fromString(unsafe.Pointer(&sb[0]))
	if uintptr(loc) == 0 {
		return nil, ErrInvalidParameter
	}
	goloc := new(Locator)
	goloc.cl = unsafe.Pointer(loc)
	runtime.SetFinalizer(goloc, locatorFinalizer)
	return goloc, nil
}

// GetInfo gets information about this locator, also validating its signature if 'id' is non-nil.
// If 'id' is nil the 'valid' return value is undefined.
func (loc *Locator) GetInfo(id *Identity) (ts int64, fp *Fingerprint, eps []Endpoint, valid bool, err error) {
	ts = int64(C.ZT_Locator_timestamp(loc.cl))
	cfp := C.ZT_Locator_fingerprint(loc.cl)
	if uintptr(unsafe.Pointer(cfp)) == 0 {
		err = ErrInternal
		return
	}
	fp = newFingerprintFromCFingerprint(cfp)
	epc := int(C.ZT_Locator_endpointCount(loc.cl))
	eps = make([]Endpoint, epc)
	for i:=0;i<epc;i++ {
		eps[i].cep = *C.ZT_Locator_endpoint(loc.cl, C.uint(i))
	}
	if id != nil {
		id.initCIdentityPtr()
		valid = C.ZT_Locator_verify(loc.cl, id.cid) != 0
	}
	return
}

func (loc *Locator) String() string {
	var buf [4096]byte
	C.ZT_Locator_toString(loc.cl,unsafe.Pointer(&buf[0]),4096)
	for i:=range buf {
		if buf[i] == 0 {
			return string(buf[0:i])
		}
	}
	return ""
}
