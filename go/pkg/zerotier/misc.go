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

var base32StdLowerCase = base32.NewEncoding("abcdefghijklmnopqrstuvwxyz234567")

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

// The ipClassify code below is based on and should produce identical results to
// InetAddress::ipScope() in the C++ code.
/*
InetAddress::IpScope InetAddress::ipScope() const
{
	switch(ss_family) {

		case AF_INET: {
			const uint32_t ip = Utils::ntoh((uint32_t)reinterpret_cast<const struct sockaddr_in *>(this)->sin_addr.s_addr);
			switch(ip >> 24) {
				case 0x00: return IP_SCOPE_NONE;                                   // 0.0.0.0/8 (reserved, never used)
				case 0x06: return IP_SCOPE_PSEUDOPRIVATE;                          // 6.0.0.0/8 (US Army)
				case 0x0a: return IP_SCOPE_PRIVATE;                                // 10.0.0.0/8
				case 0x0b: return IP_SCOPE_PSEUDOPRIVATE;                          // 11.0.0.0/8 (US DoD)
				case 0x15: return IP_SCOPE_PSEUDOPRIVATE;                          // 21.0.0.0/8 (US DDN-RVN)
				case 0x16: return IP_SCOPE_PSEUDOPRIVATE;                          // 22.0.0.0/8 (US DISA)
				case 0x19: return IP_SCOPE_PSEUDOPRIVATE;                          // 25.0.0.0/8 (UK Ministry of Defense)
				case 0x1a: return IP_SCOPE_PSEUDOPRIVATE;                          // 26.0.0.0/8 (US DISA)
				case 0x1c: return IP_SCOPE_PSEUDOPRIVATE;                          // 28.0.0.0/8 (US DSI-North)
				case 0x1d: return IP_SCOPE_PSEUDOPRIVATE;                          // 29.0.0.0/8 (US DISA)
				case 0x1e: return IP_SCOPE_PSEUDOPRIVATE;                          // 30.0.0.0/8 (US DISA)
				case 0x33: return IP_SCOPE_PSEUDOPRIVATE;                          // 51.0.0.0/8 (UK Department of Social Security)
				case 0x37: return IP_SCOPE_PSEUDOPRIVATE;                          // 55.0.0.0/8 (US DoD)
				case 0x38: return IP_SCOPE_PSEUDOPRIVATE;                          // 56.0.0.0/8 (US Postal Service)
				case 0x64:
					if ((ip & 0xffc00000) == 0x64400000) return IP_SCOPE_PRIVATE;    // 100.64.0.0/10
					break;
				case 0x7f: return IP_SCOPE_LOOPBACK;                               // 127.0.0.0/8
				case 0xa9:
					if ((ip & 0xffff0000) == 0xa9fe0000) return IP_SCOPE_LINK_LOCAL; // 169.254.0.0/16
					break;
				case 0xac:
					if ((ip & 0xfff00000) == 0xac100000) return IP_SCOPE_PRIVATE;    // 172.16.0.0/12
					break;
				case 0xc0:
					if ((ip & 0xffff0000) == 0xc0a80000) return IP_SCOPE_PRIVATE;    // 192.168.0.0/16
					break;
				case 0xff: return IP_SCOPE_NONE;                                   // 255.0.0.0/8 (broadcast, or unused/unusable)
			}
			switch(ip >> 28) {
				case 0xe: return IP_SCOPE_MULTICAST;                               // 224.0.0.0/4
				case 0xf: return IP_SCOPE_PSEUDOPRIVATE;                           // 240.0.0.0/4 ("reserved," usually unusable)
			}
			return IP_SCOPE_GLOBAL;
		}	break;

		case AF_INET6: {
			const unsigned char *ip = reinterpret_cast<const unsigned char *>(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr);
			if ((ip[0] & 0xf0) == 0xf0) {
				if (ip[0] == 0xff) return IP_SCOPE_MULTICAST;                      // ff00::/8
				if ((ip[0] == 0xfe)&&((ip[1] & 0xc0) == 0x80)) {
					unsigned int k = 2;
					while ((!ip[k])&&(k < 15)) ++k;
					if ((k == 15)&&(ip[15] == 0x01))
						return IP_SCOPE_LOOPBACK;                                      // fe80::1/128
					else return IP_SCOPE_LINK_LOCAL;                                 // fe80::/10
				}
				if ((ip[0] & 0xfe) == 0xfc) return IP_SCOPE_PRIVATE;               // fc00::/7
			}
			unsigned int k = 0;
			while ((!ip[k])&&(k < 15)) ++k;
			if (k == 15) { // all 0's except last byte
				if (ip[15] == 0x01) return IP_SCOPE_LOOPBACK;                      // ::1/128
				if (ip[15] == 0x00) return IP_SCOPE_NONE;                          // ::/128
			}
			return IP_SCOPE_GLOBAL;
		}	break;

	}

	return IP_SCOPE_NONE;
}
*/

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
