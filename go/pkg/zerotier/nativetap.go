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
	"fmt"
	"net"
	"sync"
	"sync/atomic"
	"unsafe"
)

//#cgo CFLAGS: -O3
//#define ZT_CGO 1
//#include <stdint.h>
//#include <stdlib.h>
//#include <string.h>
//#include "../../native/GoGlue.h"
import "C"

// nativeTap is a Tap implementation that wraps a native C++ interface to a system tun/tap device
type nativeTap struct {
	tap                        unsafe.Pointer
	networkStatus              uint32
	enabled                    uint32
	multicastGroupHandlers     []func(bool, *MulticastGroup)
	multicastGroupHandlersLock sync.Mutex
}

var _ Tap = &nativeTap{}

// Type returns a human-readable description of this tap implementation
func (t *nativeTap) Type() string {
	return "native"
}

// Error gets this tap device's error status
func (t *nativeTap) Error() (int, string) {
	return 0, ""
}

// SetEnabled sets this tap's enabled state
func (t *nativeTap) SetEnabled(enabled bool) {
	if enabled && atomic.SwapUint32(&t.enabled, 1) == 0 {
		C.ZT_GoTap_setEnabled(t.tap, 1)
	} else if !enabled && atomic.SwapUint32(&t.enabled, 0) == 1 {
		C.ZT_GoTap_setEnabled(t.tap, 0)
	}
}

// Enabled returns true if this tap is currently processing packets
func (t *nativeTap) Enabled() bool {
	return atomic.LoadUint32(&t.enabled) != 0
}

// AddIP adds an IP address (with netmask) to this tap
func (t *nativeTap) AddIP(ip net.IPNet) error {
	bits, _ := ip.Mask.Size()
	if len(ip.IP) == 16 {
		if bits > 128 || bits < 0 {
			return ErrInvalidParameter
		}
		C.ZT_GoTap_addIp(t.tap, C.int(afInet6), unsafe.Pointer(&ip.IP[0]), C.int(bits))
	} else if len(ip.IP) == 4 {
		if bits > 32 || bits < 0 {
			return ErrInvalidParameter
		}
		C.ZT_GoTap_addIp(t.tap, C.int(afInet), unsafe.Pointer(&ip.IP[0]), C.int(bits))
	}
	return ErrInvalidParameter
}

// RemoveIP removes this IP address (with netmask) from this tap
func (t *nativeTap) RemoveIP(ip net.IPNet) error {
	bits, _ := ip.Mask.Size()
	if len(ip.IP) == 16 {
		if bits > 128 || bits < 0 {
			return ErrInvalidParameter
		}
		C.ZT_GoTap_removeIp(t.tap, C.int(afInet6), unsafe.Pointer(&ip.IP[0]), C.int(bits))
		return nil
	}
	if len(ip.IP) == 4 {
		if bits > 32 || bits < 0 {
			return ErrInvalidParameter
		}
		C.ZT_GoTap_removeIp(t.tap, C.int(afInet), unsafe.Pointer(&ip.IP[0]), C.int(bits))
		return nil
	}
	return ErrInvalidParameter
}

// IPs returns IPs currently assigned to this tap (including externally or system-assigned IPs)
func (t *nativeTap) IPs() (ips []net.IPNet, err error) {
	defer func() {
		e := recover()
		if e != nil {
			err = fmt.Errorf("%v", e)
		}
	}()
	var ipbuf [16384]byte
	count := int(C.ZT_GoTap_ips(t.tap, unsafe.Pointer(&ipbuf[0]), 16384))
	ipptr := 0
	for i := 0; i < count; i++ {
		af := int(ipbuf[ipptr])
		ipptr++
		switch af {
		case afInet:
			var ip [4]byte
			for j := 0; j < 4; j++ {
				ip[j] = ipbuf[ipptr]
				ipptr++
			}
			bits := ipbuf[ipptr]
			ipptr++
			ips = append(ips, net.IPNet{IP: net.IP(ip[:]), Mask: net.CIDRMask(int(bits), 32)})
		case afInet6:
			var ip [16]byte
			for j := 0; j < 16; j++ {
				ip[j] = ipbuf[ipptr]
				ipptr++
			}
			bits := ipbuf[ipptr]
			ipptr++
			ips = append(ips, net.IPNet{IP: net.IP(ip[:]), Mask: net.CIDRMask(int(bits), 128)})
		}
	}
	return
}

// DeviceName gets this tap's OS-specific device name
func (t *nativeTap) DeviceName() string {
	var dn [256]byte
	C.ZT_GoTap_deviceName(t.tap, (*C.char)(unsafe.Pointer(&dn[0])))
	for i, b := range dn {
		if b == 0 {
			return string(dn[0:i])
		}
	}
	return ""
}

// AddMulticastGroupChangeHandler adds a function to be called when the tap subscribes or unsubscribes to a multicast group.
func (t *nativeTap) AddMulticastGroupChangeHandler(handler func(bool, *MulticastGroup)) {
	t.multicastGroupHandlersLock.Lock()
	t.multicastGroupHandlers = append(t.multicastGroupHandlers, handler)
	t.multicastGroupHandlersLock.Unlock()
}
