/*
 * Copyright (C)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

package zerotier

// #include "../../serviceiocore/GoGlue.h"
import "C"

// Path is a path to another peer on the network
type Path struct {
	Endpoint    Endpoint `json:"endpoint"`
	LastSend    int64    `json:"lastSend"`
	LastReceive int64    `json:"lastReceive"`
}

func (p *Path) setFromCPath(cp *C.ZT_Path) {
	p.Endpoint.setFromCEndpoint(&(cp.endpoint))
	p.LastSend = int64(cp.lastSend)
	p.LastReceive = int64(cp.lastReceive)
}
