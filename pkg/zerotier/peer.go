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

// #include "../../serviceiocore/GoGlue.h"
import "C"

import "unsafe"

// Peer is another ZeroTier node
type Peer struct {
	Address     Address      `json:"address"`
	Identity    *Identity    `json:"identity"`
	Fingerprint *Fingerprint `json:"fingerprint"`
	Version     [3]int       `json:"version"`
	Latency     int          `json:"latency"`
	Root        bool         `json:"root"`
	Paths       []Path       `json:"paths,omitempty"`
	Locator     *Locator     `json:"locator,omitempty"`
}

func newPeerFromCPeer(cp *C.ZT_Peer) (p *Peer, err error) {
	p = new(Peer)
	p.Address = Address(cp.address)
	p.Identity, err = newIdentityFromCIdentity(cp.identity)
	if err != nil {
		return
	}
	p.Fingerprint = newFingerprintFromCFingerprint(&(cp.fingerprint))
	p.Version[0] = int(cp.versionMajor)
	p.Version[1] = int(cp.versionMinor)
	p.Version[2] = int(cp.versionRev)
	p.Latency = int(cp.latency)
	p.Root = cp.root != 0
	p.Paths = make([]Path, int(cp.pathCount))
	for i := range p.Paths {
		p.Paths[i].setFromCPath((*C.ZT_Path)(unsafe.Pointer(uintptr(unsafe.Pointer(cp.paths)) + (uintptr(C.sizeof_ZT_Path) * uintptr(i)))))
	}
	p.Locator, err = NewLocatorFromBytes(C.GoBytes(unsafe.Pointer(cp.locator), C.int(cp.locatorSize)))
	return
}
