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
	"bytes"
	"encoding/binary"
	"encoding/json"
	"errors"
	"net"
	"strconv"
	"strings"
	"syscall"
	"unsafe"
)

func sockaddrStorageToIPNet(ss *C.struct_sockaddr_storage) *net.IPNet {
	var a net.IPNet
	switch ss.ss_family {
	case syscall.AF_INET:
		sa4 := (*C.struct_sockaddr_in)(unsafe.Pointer(ss))
		var ip4 [4]byte
		copy(ip4[:], (*[4]byte)(unsafe.Pointer(&sa4.sin_addr))[:])
		a.IP = ip4[:]
		a.Mask = net.CIDRMask(int(binary.BigEndian.Uint16(((*[2]byte)(unsafe.Pointer(&sa4.sin_port)))[:])), 32)
		return &a
	case syscall.AF_INET6:
		sa6 := (*C.struct_sockaddr_in6)(unsafe.Pointer(ss))
		var ip6 [16]byte
		copy(ip6[:], (*[16]byte)(unsafe.Pointer(&sa6.sin6_addr))[:])
		a.IP = ip6[:]
		a.Mask = net.CIDRMask(int(binary.BigEndian.Uint16(((*[2]byte)(unsafe.Pointer(&sa6.sin6_port)))[:])), 128)
		return &a
	}
	return nil
}

func sockaddrStorageToUDPAddr(ss *C.struct_sockaddr_storage) *net.UDPAddr {
	var a net.UDPAddr
	switch ss.ss_family {
	case syscall.AF_INET:
		sa4 := (*C.struct_sockaddr_in)(unsafe.Pointer(ss))
		var ip4 [4]byte
		copy(ip4[:], (*[4]byte)(unsafe.Pointer(&sa4.sin_addr))[:])
		a.IP = ip4[:]
		a.Port = int(binary.BigEndian.Uint16(((*[2]byte)(unsafe.Pointer(&sa4.sin_port)))[:]))
		return &a
	case syscall.AF_INET6:
		sa6 := (*C.struct_sockaddr_in6)(unsafe.Pointer(ss))
		var ip6 [16]byte
		copy(ip6[:], (*[16]byte)(unsafe.Pointer(&sa6.sin6_addr))[:])
		a.IP = ip6[:]
		a.Port = int(binary.BigEndian.Uint16(((*[2]byte)(unsafe.Pointer(&sa6.sin6_port)))[:]))
		return &a
	}
	return nil
}

func sockaddrStorageToUDPAddr2(ss unsafe.Pointer) *net.UDPAddr {
	return sockaddrStorageToUDPAddr((*C.struct_sockaddr_storage)(ss))
}

func zeroSockaddrStorage(ss *C.struct_sockaddr_storage) {
	C.memset(unsafe.Pointer(ss), 0, C.sizeof_struct_sockaddr_storage)
}

func makeSockaddrStorage(ip net.IP, port int, ss *C.struct_sockaddr_storage) bool {
	zeroSockaddrStorage(ss)
	if len(ip) == 4 {
		sa4 := (*C.struct_sockaddr_in)(unsafe.Pointer(ss))
		sa4.sin_family = syscall.AF_INET
		copy(((*[4]byte)(unsafe.Pointer(&sa4.sin_addr)))[:], ip)
		binary.BigEndian.PutUint16(((*[2]byte)(unsafe.Pointer(&sa4.sin_port)))[:], uint16(port))
		return true
	}
	if len(ip) == 16 {
		sa6 := (*C.struct_sockaddr_in6)(unsafe.Pointer(ss))
		sa6.sin6_family = syscall.AF_INET6
		copy(((*[16]byte)(unsafe.Pointer(&sa6.sin6_addr)))[:], ip)
		binary.BigEndian.PutUint16(((*[2]byte)(unsafe.Pointer(&sa6.sin6_port)))[:], uint16(port))
		return true
	}
	return false
}

var ErrInvalidInetAddress = errors.New("invalid marshaled InetAddress object")

// InetAddress implements net.Addr but has a ZeroTier-like string representation
type InetAddress struct {
	IP   net.IP
	Port int
}

// Nil returns true if this InetAddress is empty.
func (ina *InetAddress) Nil() bool {
	return len(ina.IP) == 0
}

// Less returns true if this IP/port is lexicographically less than another
func (ina *InetAddress) Less(i2 *InetAddress) bool {
	c := bytes.Compare(ina.IP, i2.IP)
	if c < 0 {
		return true
	}
	if c == 0 {
		return ina.Port < i2.Port
	}
	return false
}

// NewInetAddressFromString parses an IP[/port] format address
func NewInetAddressFromString(s string) *InetAddress {
	i := new(InetAddress)
	ss := strings.Split(strings.TrimSpace(s), "/")
	if len(ss) > 0 {
		i.IP = net.ParseIP(ss[0])
		i4 := i.IP.To4()
		if len(i4) == 4 { // down-convert IPv4-in-6 IPs to native IPv4 as this is what all our code expects
			i.IP = i4
		}
		if len(ss) > 1 {
			p64, _ := strconv.ParseUint(ss[1], 10, 64)
			i.Port = int(p64 & 0xffff)
		}
	}
	return i
}

// NewInetAddressFromSockaddr parses a sockaddr_in or sockaddr_in6 C structure (may crash if given something other than these!)
// This is a convenience wrapper around the CGO functions in node.go.
func NewInetAddressFromSockaddr(sa unsafe.Pointer) *InetAddress {
	i := new(InetAddress)
	if uintptr(sa) != 0 {
		ua := sockaddrStorageToUDPAddr2(sa)
		if ua != nil {
			i.IP = ua.IP
			i.Port = ua.Port
		}
	}
	return i
}

// Network returns "udp" to implement net.Addr
func (ina *InetAddress) Network() string {
	return "udp"
}

// String returns this address in ZeroTier-canonical IP/port format
func (ina *InetAddress) String() string {
	return ina.IP.String() + "/" + strconv.FormatInt(int64(ina.Port), 10)
}

// Family returns the address family (AFInet etc.) or 0 if none
func (ina *InetAddress) Family() int {
	switch len(ina.IP) {
	case 4:
		return syscall.AF_INET
	case 16:
		return syscall.AF_INET6
	}
	return 0
}

// Valid returns true if both the IP and port have valid values
func (ina *InetAddress) Valid() bool {
	return (len(ina.IP) == 4 || len(ina.IP) == 16) && (ina.Port > 0 && ina.Port < 65536)
}

// MarshalJSON marshals this MAC as a string
func (ina *InetAddress) MarshalJSON() ([]byte, error) {
	s := ina.String()
	return json.Marshal(&s)
}

// UnmarshalJSON unmarshals this MAC from a string
func (ina *InetAddress) UnmarshalJSON(j []byte) error {
	var s string
	err := json.Unmarshal(j, &s)
	if err != nil {
		return err
	}
	*ina = *NewInetAddressFromString(s)
	return nil
}

// key returns a short array suitable for use as a map[] key for this IP
func (ina *InetAddress) key() (k [3]uint64) {
	copy(((*[16]byte)(unsafe.Pointer(&k[0])))[:], ina.IP)
	k[2] = uint64(ina.Port)
	return
}
