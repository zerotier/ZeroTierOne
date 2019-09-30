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
	"encoding/binary"
	"net"
	"time"
	"unsafe"
)

// ZeroTierLogoChar is the unicode character that is ZeroTier's logo
const ZeroTierLogoChar = "⏁"

var base32StdLowerCase = base32.NewEncoding("abcdefghijklmnopqrstuvwxyz234567").WithPadding(base32.NoPadding)

// TimeMs returns the time in milliseconds since epoch.
func TimeMs() int64 { return int64(time.Now().UnixNano()) / int64(1000000) }

// ipNetToKey creates a key that can be used in a map[] from a net.IPNet
func ipNetToKey(ipn *net.IPNet) (k [3]uint64) {
	copy(((*[16]byte)(unsafe.Pointer(&k[0])))[:], ipn.IP)
	ones, bits := ipn.Mask.Size()
	k[2] = (uint64(ones) << 32) | uint64(bits)
	return
}

func allZero(b []byte) bool {
	for _, bb := range b {
		if bb != 0 {
			return false
		}
	}
	return true
}

// checkPort does trial binding to a port using both UDP and TCP and returns false if any bindings fail.
func checkPort(port int) bool {
	var ua net.UDPAddr
	ua.IP = net.IPv6zero
	ua.Port = port
	uc, err := net.ListenUDP("udp6", &ua)
	if uc != nil {
		_ = uc.Close()
	}
	if err != nil {
		return false
	}
	ua.IP = net.IPv4zero
	uc, err = net.ListenUDP("udp4", &ua)
	if uc != nil {
		_ = uc.Close()
	}
	if err != nil {
		return false
	}

	var ta net.TCPAddr
	ta.IP = net.IPv6zero
	ta.Port = port
	tc, err := net.ListenTCP("tcp6", &ta)
	if tc != nil {
		_ = tc.Close()
	}
	if err != nil {
		return false
	}
	ta.IP = net.IPv4zero
	tc, err = net.ListenTCP("tcp4", &ta)
	if tc != nil {
		_ = tc.Close()
	}
	if err != nil {
		return false
	}

	return true
}

// The ipClassify code below is based on and should produce identical results to
// InetAddress::ipScope() in the C++ code.

const (
	ipClassificationNone          = -1
	ipClassificationLoopback      = 0
	ipClassificationPseudoprivate = 1
	ipClassificationPrivate       = 2
	ipClassificationLinkLocal     = 3
	ipClassificationMulticast     = 4
	ipClassificationGlobal        = 5
)

var ipv4PseudoprivatePrefixes = []byte{
	0x06, // 6.0.0.0/8 (US Army)
	0x0b, // 11.0.0.0/8 (US DoD)
	0x15, // 21.0.0.0/8 (US DDN-RVN)
	0x16, // 22.0.0.0/8 (US DISA)
	0x19, // 25.0.0.0/8 (UK Ministry of Defense)
	0x1a, // 26.0.0.0/8 (US DISA)
	0x1c, // 28.0.0.0/8 (US DSI-North)
	0x1d, // 29.0.0.0/8 (US DISA)
	0x1e, // 30.0.0.0/8 (US DISA)
	0x33, // 51.0.0.0/8 (UK Department of Social Security)
	0x37, // 55.0.0.0/8 (US DoD)
	0x38, // 56.0.0.0/8 (US Postal Service)
}

// ipClassify determines the official or in a few cases unofficial role of an IP address
func ipClassify(ip net.IP) int {
	if len(ip) == 16 {
		ip4 := ip.To4()
		if len(ip4) == 4 {
			ip = ip4
		}
	}
	if len(ip) == 4 {
		ip4FirstByte := ip[0]
		for _, b := range ipv4PseudoprivatePrefixes {
			if ip4FirstByte == b {
				return ipClassificationPseudoprivate
			}
		}
		ip4 := binary.BigEndian.Uint32(ip)
		switch ip4FirstByte {
		case 0x0a: // 10.0.0.0/8
			return ipClassificationPrivate
		case 0x64: // 100.64.0.0/10
			if (ip4 & 0xffc00000) == 0x64400000 {
				return ipClassificationPrivate
			}
		case 0x7f: // 127.0.0.1/8
			return ipClassificationLoopback
		case 0xa9: // 169.254.0.0/16
			if (ip4 & 0xffff0000) == 0xa9fe0000 {
				return ipClassificationLinkLocal
			}
		case 0xac: // 172.16.0.0/12
			if (ip4 & 0xfff00000) == 0xac100000 {
				return ipClassificationPrivate
			}
		case 0xc0: // 192.168.0.0/16
			if (ip4 & 0xffff0000) == 0xc0a80000 {
				return ipClassificationPrivate
			}
		}
		switch ip4 >> 28 {
		case 0xe: // 224.0.0.0/4
			return ipClassificationMulticast
		case 0xf: // 240.0.0.0/4 ("reserved," usually unusable)
			return ipClassificationNone
		}
		return ipClassificationGlobal
	}

	if len(ip) == 16 {
		if (ip[0] & 0xf0) == 0xf0 {
			if ip[0] == 0xff { // ff00::/8
				return ipClassificationMulticast
			}
			if ip[0] == 0xfe && (ip[1]&0xc0) == 0x80 {
				if allZero(ip[2:15]) {
					if ip[15] == 0x01 { // fe80::1/128
						return ipClassificationLoopback
					}
					return ipClassificationLinkLocal
				}
			}
			if (ip[0] & 0xfe) == 0xfc { // fc00::/7
				return ipClassificationPrivate
			}
		}
		if allZero(ip[0:15]) {
			if ip[15] == 0x01 { // ::1/128
				return ipClassificationLoopback
			}
			if ip[15] == 0x00 { // ::/128
				return ipClassificationNone
			}
		}
		return ipClassificationGlobal
	}

	return ipClassificationNone
}
