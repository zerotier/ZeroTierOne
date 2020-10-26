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

// #include "../../serviceiocore/GoGlue.h"
import "C"

import (
	"encoding/json"
	"runtime"
	"unsafe"
)

type Locator struct {
	Timestamp   int64        `json:"timestamp"`
	Fingerprint *Fingerprint `json:"fingerprint"`
	Endpoints   []Endpoint   `json:"endpoints"`
	cl          unsafe.Pointer
}

func newLocatorFromCLocator(cl unsafe.Pointer, needFinalizer bool) (*Locator, error) {
	loc := new(Locator)
	loc.cl = cl
	err := loc.init(needFinalizer)
	if err != nil {
		return nil, err
	}
	return loc, nil
}

func NewLocator(ts int64, endpoints []*Endpoint, signer *Identity) (*Locator, error) {
	if ts <= 0 || len(endpoints) == 0 || signer == nil {
		return nil, ErrInvalidParameter
	}
	eps := make([]C.ZT_Endpoint, 0, len(endpoints))
	for _, e := range endpoints {
		eps = append(eps, e.cep)
	}
	loc := C.ZT_Locator_create(C.int64_t(ts), &eps[0], nil, C.uint(len(eps)), signer.cIdentity())
	if uintptr(loc) == 0 {
		return nil, ErrInvalidParameter
	}

	goloc := new(Locator)
	goloc.cl = unsafe.Pointer(loc)
	return goloc, goloc.init(true)
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
	return goloc, goloc.init(true)
}

func NewLocatorFromString(s string) (*Locator, error) {
	if len(s) == 0 {
		return nil, ErrInvalidParameter
	}
	sb := []byte(s)
	sb = append(sb, 0)
	loc := C.ZT_Locator_fromString((*C.char)(unsafe.Pointer(&sb[0])))
	if loc == nil {
		return nil, ErrInvalidParameter
	}

	goloc := new(Locator)
	goloc.cl = unsafe.Pointer(loc)
	return goloc, goloc.init(true)
}

func (loc *Locator) Validate(id *Identity) bool {
	if id == nil {
		return false
	}
	return C.ZT_Locator_verify(loc.cl, id.cIdentity()) != 0
}

func (loc *Locator) Bytes() []byte {
	if loc.cl == nil {
		return nil
	}
	var buf [16384]byte // larger than ZT_LOCATOR_MARSHAL_SIZE_MAX
	bl := C.ZT_Locator_marshal(loc.cl, unsafe.Pointer(&buf[0]), 16384)
	if bl <= 0 {
		return nil
	}
	return buf[0:int(bl)]
}

func (loc *Locator) String() string {
	if loc.cl == nil {
		return ""
	}
	var buf [16384]C.char // 16384 == ZT_LOCATOR_STRING_SIZE_MAX
	return C.GoString(C.ZT_Locator_toString(loc.cl, &buf[0], 16384))
}

func (loc *Locator) MarshalJSON() ([]byte, error) {
	return json.Marshal(loc)
}

func (loc *Locator) UnmarshalJSON(j []byte) error {
	if loc.cl != nil {
		C.ZT_Locator_delete(loc.cl)
		loc.cl = unsafe.Pointer(nil)
	}

	err := json.Unmarshal(j, loc)
	if err != nil {
		return err
	}
	return loc.init(true)
}

func locatorFinalizer(obj interface{}) {
	if obj != nil {
		cl := obj.(*Locator).cl
		if cl != nil {
			C.ZT_Locator_delete(cl)
		}
	}
}

func (loc *Locator) init(needFinalizer bool) error {
	loc.Timestamp = int64(C.ZT_Locator_timestamp(loc.cl))
	cfp := C.ZT_Locator_fingerprint(loc.cl)
	if uintptr(unsafe.Pointer(cfp)) == 0 {
		return ErrInternal
	}
	loc.Fingerprint = newFingerprintFromCFingerprint(cfp)
	epc := int(C.ZT_Locator_endpointCount(loc.cl))
	loc.Endpoints = make([]Endpoint, epc)
	for i := 0; i < epc; i++ {
		loc.Endpoints[i].cep = *C.ZT_Locator_endpoint(loc.cl, C.uint(i))
	}
	if needFinalizer {
		runtime.SetFinalizer(loc, locatorFinalizer)
	}
	return nil
}
