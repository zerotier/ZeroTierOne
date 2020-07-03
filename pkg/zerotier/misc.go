/*
 * Copyright (C)2013-2020 ZeroTier, Inc.
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

import (
	"encoding/base32"
	"encoding/binary"
	"math/rand"
	"net"
	"sync"
	"time"
	"unsafe"
)

// LogoChar is the unicode character that is ZeroTier's logo
const LogoChar = "⏁"

// pointerSize is the size of a pointer on this system
const pointerSize = unsafe.Sizeof(uintptr(0))

// Base32StdLowerCase is a base32 encoder/decoder using a lower-case standard alphabet and no padding.
var Base32StdLowerCase = base32.NewEncoding("abcdefghijklmnopqrstuvwxyz234567").WithPadding(base32.NoPadding)

// unassignedPrivilegedPorts are ports below 1024 that do not appear to be assigned by IANA.
// The new 2.0+ ZeroTier default is 793, which we will eventually seek to have assigned. These
// are searched as backups if this port is already in use on a system.
var unassignedPrivilegedPorts = []int{
	4,
	6,
	8,
	10,
	12,
	14,
	15,
	16,
	26,
	28,
	30,
	32,
	34,
	36,
	40,
	60,
	269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279,
	285,
	288, 289, 290, 291, 292, 293, 294, 295, 296, 297, 298, 299, 300, 301, 302, 303, 304, 305, 306, 307,
	323, 324, 325, 326, 327, 328, 329, 330, 331, 332,
	334, 335, 336, 337, 338, 339, 340, 341, 342, 343,
	703,
	708,
	713, 714, 715, 716, 717, 718, 719, 720, 721, 722, 723, 724, 725, 726, 727, 728,
	732, 733, 734, 735, 736, 737, 738, 739, 740,
	743,
	745, 746,
	755, 756,
	766,
	768,
	778, 779,
	781, 782, 783, 784, 785, 786, 787, 788, 789, 790, 791, 792, 793, 794, 795, 796, 797, 798, 799,
	802, 803, 804, 805, 806, 807, 808, 809,
	811, 812, 813, 814, 815, 816, 817, 818, 819, 820, 821, 822, 823, 824, 825, 826, 827,
	834, 835, 836, 837, 838, 839, 840, 841, 842, 843, 844, 845, 846,
	849, 850, 851, 852, 853, 854, 855, 856, 857, 858, 859,
	862, 863, 864, 865, 866, 867, 868, 869, 870, 871, 872,
	874, 875, 876, 877, 878, 879, 880, 881, 882, 883, 884, 885,
	889, 890, 891, 892, 893, 894, 895, 896, 897, 898, 899,
	904, 905, 906, 907, 908, 909, 910, 911,
	914, 915, 916, 917, 918, 919, 920, 921, 922, 923, 924, 925, 926, 927, 928, 929, 930, 931, 932, 933, 934, 935, 936, 937, 938, 939, 940, 941, 942, 943, 944, 945, 946, 947, 948, 949, 950, 951, 952, 953, 954, 955, 956, 957, 958, 959, 960, 961, 962, 963, 964, 965, 966, 967, 968, 969, 970, 971, 972, 973, 974, 975, 976, 977, 978, 979, 980, 981, 982, 983, 984, 985, 986, 987, 988,
	1001, 1002, 1003, 1004, 1005, 1006, 1007, 1008, 1009,
	1023,
}

var prng = rand.NewSource(time.Now().UnixNano())
var prngLock sync.Mutex

func randomUInt() uint {
	prngLock.Lock()
	i := prng.Int63()
	prngLock.Unlock()
	return uint(i)
}

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

// stringAsZeroTerminatedBytes creates a C string but as a Go []byte
func stringAsZeroTerminatedBytes(s string) (b []byte) {
	if len(s) == 0 {
		b = []byte{0} // single zero
		return
	}
	sb := []byte(s)
	b = make([]byte, len(sb) + 1)
	copy(b, sb)
	// make() will zero memory, so b[len(sb)+1] will be 0
	return
}

// cStrCopy copies src into dest as a zero-terminated C string
func cStrCopy(dest unsafe.Pointer, destSize int, src string) {
	sb := []byte(src)
	b := C.GoBytes(dest, C.int(destSize))
	if len(sb) > (destSize - 1) {
		sb = sb[0:destSize - 1]
	}
	copy(b[:], sb[:])
	b[len(sb)] = 0
}
