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

//#cgo CFLAGS: -O3
//#include "../../native/GoGlue.h"
import "C"

import "unsafe"

// LocatorDNSSigningKey is the public (as a secure DNS name) and private keys for entering locators into DNS
type LocatorDNSSigningKey struct {
	SecureDNSName string
	PrivateKey    []byte
}

// Locator is a binary serialized record containing information about where a ZeroTier node is located on the network
type Locator struct {
	// Identity is the full identity of the node being located
	Identity *Identity

	// Physical is a list of static physical network addresses for this node
	Physical []*InetAddress

	// Virtual is a list of ZeroTier nodes that can relay to this node
	Virtual []*Identity

	bytes []byte
}

// NewLocator creates a new locator with the given identity and addresses and the current time as timestamp.
// The identity must include its secret key so that it can sign the final locator.
func NewLocator(id *Identity, virtualAddresses []*Identity, physicalAddresses []*InetAddress) (*Locator, error) {
	if !id.HasPrivate() {
		return nil, ErrSecretKeyRequired
	}

	idstr := id.PrivateKeyString()
	phy := make([]C.struct_sockaddr_storage, len(physicalAddresses))
	virt := make([]*C.char, len(virtualAddresses))
	idCstr := C.CString(idstr)

	defer func() {
		C.free(unsafe.Pointer(idCstr))
		for _, v := range virt {
			if uintptr(unsafe.Pointer(v)) != 0 {
				C.free(unsafe.Pointer(v))
			}
		}
	}()

	for i := 0; i < len(physicalAddresses); i++ {
		if !makeSockaddrStorage(physicalAddresses[i].IP, physicalAddresses[i].Port, &phy[i]) {
			return nil, ErrInvalidParameter
		}
	}

	for i := 0; i < len(virtualAddresses); i++ {
		idstr := virtualAddresses[i].String()
		virt[i] = C.CString(idstr)
	}

	var buf [65536]byte
	var pPhy *C.struct_sockaddr_storage
	var pVirt *C.char
	if len(phy) > 0 {
		pPhy = &phy[0]
	}
	if len(virt) > 0 {
		pVirt = &virt[0]
	}
	locSize := C.ZT_GoLocator_makeLocator((*C.uint8_t)(unsafe.Pointer(&buf[0])), 65536, idCstr, pPhy, C.uint(len(phy)), pVirt, C.uint(len(virt)))
	if locSize <= 0 {
		return nil, ErrInvalidParameter
	}

	r := make([]byte, int(locSize))
	copy(r[:], buf[0:int(locSize)])
	return &Locator{
		Identity: id,
		Physical: physicalAddresses,
		Virtual:  virtualAddresses,
		bytes:    r,
	}, nil
}

// Bytes returns this locator in byte serialized format
func (l *Locator) Bytes() []byte { return l.bytes }
