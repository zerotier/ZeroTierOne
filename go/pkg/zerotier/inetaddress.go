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
	"bytes"
	"encoding/json"
	"net"
	"strconv"
	"strings"
	"unsafe"
)

// InetAddress implements net.Addr but has a ZeroTier-like string representation
type InetAddress struct {
	IP   net.IP
	Port int
}

// Less returns true if this IP/port is lexicographically less than another
func (i *InetAddress) Less(i2 *InetAddress) bool {
	c := bytes.Compare(i.IP, i2.IP)
	if c < 0 {
		return true
	}
	if c == 0 {
		return i.Port < i2.Port
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
func (i *InetAddress) Network() string {
	return "udp"
}

// String returns this address in ZeroTier-canonical IP/port format
func (i *InetAddress) String() string {
	return i.IP.String() + "/" + strconv.FormatInt(int64(i.Port), 10)
}

// Family returns the address family (AFInet etc.) or 0 if none
func (i *InetAddress) Family() int {
	switch len(i.IP) {
	case 4:
		return AFInet
	case 16:
		return AFInet6
	}
	return 0
}

// Valid returns true if both the IP and port have valid values
func (i *InetAddress) Valid() bool {
	return (len(i.IP) == 4 || len(i.IP) == 16) && (i.Port > 0 && i.Port < 65536)
}

// MarshalJSON marshals this MAC as a string
func (i *InetAddress) MarshalJSON() ([]byte, error) {
	s := i.String()
	return json.Marshal(&s)
}

// UnmarshalJSON unmarshals this MAC from a string
func (i *InetAddress) UnmarshalJSON(j []byte) error {
	var s string
	err := json.Unmarshal(j, &s)
	if err != nil {
		return err
	}
	*i = *NewInetAddressFromString(s)
	return nil
}

// key returns a short array suitable for use as a map[] key for this IP
func (i *InetAddress) key() (k [3]uint64) {
	copy(((*[16]byte)(unsafe.Pointer(&k[0])))[:], i.IP)
	k[2] = uint64(i.Port)
	return
}
