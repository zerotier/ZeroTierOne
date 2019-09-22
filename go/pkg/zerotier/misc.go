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

import (
	"encoding/base32"
	"net"
	"time"
	"unsafe"
)

var base32StdLowerCase = base32.NewEncoding("abcdefghijklmnopqrstuvwxyz234567")

// TimeMs returns the time in milliseconds since epoch.
func TimeMs() int64 { return int64(time.Now().UnixNano()) / int64(1000000) }

// ipNetToKey creates a key that can be used in a map[] from a net.IPNet
func ipNetToKey(ipn *net.IPNet) (k [3]uint64) {
	if len(ipn.IP) > 0 {
		copy(((*[16]byte)(unsafe.Pointer(&k[0])))[:], ipn.IP)
	}
	ones, bits := ipn.Mask.Size()
	k[2] = (uint64(ones) << 32) | uint64(bits)
	return
}
