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
//#cgo LDFLAGS: ${SRCDIR}/../../../build/node/libzt_core.a ${SRCDIR}/../../../build/osdep/libzt_osdep.a ${SRCDIR}/../../../build/go/native/libzt_go_native.a -lc++ -lpthread
//#define ZT_CGO 1
//#include "../../native/GoGlue.h"
import "C"

import (
	"encoding/binary"
	"errors"
	"fmt"
	"io/ioutil"
	"net"
	"os"
	"path"
	"sync"
	"sync/atomic"
	"unsafe"

	acl "github.com/hectane/go-acl"
)

// Network status states
const (
	NetworkStatusRequestConfiguration int = C.ZT_NETWORK_STATUS_REQUESTING_CONFIGURATION
	NetworkStatusOK                   int = C.ZT_NETWORK_STATUS_OK
	NetworkStatusAccessDenied         int = C.ZT_NETWORK_STATUS_ACCESS_DENIED
	NetworkStatusNotFound             int = C.ZT_NETWORK_STATUS_NOT_FOUND
	NetworkStatusPortError            int = C.ZT_NETWORK_STATUS_PORT_ERROR
	NetworkStatusClientTooOld         int = C.ZT_NETWORK_STATUS_CLIENT_TOO_OLD

	NetworkTypePrivate int = C.ZT_NETWORK_TYPE_PRIVATE
	NetworkTypePublic  int = C.ZT_NETWORK_TYPE_PUBLIC

	// CoreVersionMajor is the major version of the ZeroTier core
	CoreVersionMajor int = C.ZEROTIER_ONE_VERSION_MAJOR

	// CoreVersionMinor is the minor version of the ZeroTier core
	CoreVersionMinor int = C.ZEROTIER_ONE_VERSION_MINOR

	// CoreVersionRevision is the revision of the ZeroTier core
	CoreVersionRevision int = C.ZEROTIER_ONE_VERSION_REVISION

	// CoreVersionBuild is the build version of the ZeroTier core
	CoreVersionBuild int = C.ZEROTIER_ONE_VERSION_BUILD

	afInet  int = C.AF_INET
	afInet6 int = C.AF_INET6
)

var (
	nodesByUserPtr     map[uintptr]*Node
	nodesByUserPtrLock sync.RWMutex
)

//////////////////////////////////////////////////////////////////////////////

// Node represents an instance of the ZeroTier core node and related C++ I/O code
type Node struct {
	path         string
	networks     map[uint64]*Network
	networksLock sync.RWMutex

	gn *C.ZT_GoNode
	zn *C.ZT_Node

	online  uint32
	running uint32
}

// NewNode creates and initializes a new instance of the ZeroTier node service
func NewNode(path string) (*Node, error) {
	os.MkdirAll(path, 0755)
	if _, err := os.Stat(path); err != nil {
		return nil, err
	}

	n := new(Node)
	n.path = path
	n.networks = make(map[uint64]*Network)

	cpath := C.CString(path)
	n.gn = C.ZT_GoNode_new(cpath)
	C.free(unsafe.Pointer(cpath))
	if n.gn == nil {
		return nil, ErrNodeInitFailed
	}
	n.zn = (*C.ZT_Node)(C.ZT_GoNode_getNode(n.gn))

	gnRawAddr := uintptr(unsafe.Pointer(n.gn))
	nodesByUserPtrLock.Lock()
	nodesByUserPtr[gnRawAddr] = n
	nodesByUserPtrLock.Unlock()

	n.online = 0
	n.running = 1

	return n, nil
}

// Close closes this Node and frees its underlying C++ Node structures
func (n *Node) Close() {
	if atomic.SwapUint32(&n.running, 0) != 0 {
		C.ZT_GoNode_delete(n.gn)
		nodesByUserPtrLock.Lock()
		delete(nodesByUserPtr, uintptr(unsafe.Pointer(n.gn)))
		nodesByUserPtrLock.Unlock()
	}
}

// Join joins a network
// If tap is nil, the default system tap for this OS/platform is used (if available).
func (n *Node) Join(nwid uint64, tap Tap) (*Network, error) {
	n.networksLock.RLock()
	if nw, have := n.networks[nwid]; have {
		return nw, nil
	}
	n.networksLock.RUnlock()

	if tap != nil {
		return nil, errors.New("non-native taps not implemented yet")
	}
	ntap := C.ZT_GoNode_join(n.gn, C.uint64_t(nwid))
	if ntap == nil {
		return nil, ErrTapInitFailed
	}

	nw, err := NewNetwork(NetworkID(nwid), &nativeTap{tap: unsafe.Pointer(ntap), enabled: 1})
	if err != nil {
		C.ZT_GoNode_leave(n.gn, C.uint64_t(nwid))
		return nil, err
	}
	n.networksLock.Lock()
	n.networks[nwid] = nw
	n.networksLock.Unlock()

	return nw, nil
}

// Leave leaves a network
func (n *Node) Leave(nwid uint64) error {
	C.ZT_GoNode_leave(n.gn, C.uint64_t(nwid))
	n.networksLock.Lock()
	delete(n.networks, nwid)
	n.networksLock.Unlock()
	return nil
}

//////////////////////////////////////////////////////////////////////////////

func (n *Node) pathCheck(ztAddress uint64, af int, ip net.IP, port int) bool {
	return true
}

func (n *Node) pathLookup(ztAddress uint64) (net.IP, int) {
	return nil, 0
}

func (n *Node) makeStateObjectPath(objType int, id [2]uint64) (string, bool) {
	var fp string
	secret := false
	switch objType {
	case C.ZT_STATE_OBJECT_IDENTITY_PUBLIC:
		fp = path.Join(n.path, "identity.public")
	case C.ZT_STATE_OBJECT_IDENTITY_SECRET:
		fp = path.Join(n.path, "identity.secret")
		secret = true
	case C.ZT_STATE_OBJECT_PEER:
		fp = path.Join(n.path, "peers.d")
		os.Mkdir(fp, 0700)
		fp = path.Join(fp, fmt.Sprintf("%.10x.peer", id[0]))
		secret = true
	case C.ZT_STATE_OBJECT_NETWORK_CONFIG:
		fp = path.Join(n.path, "networks.d")
		os.Mkdir(fp, 0755)
		fp = path.Join(fp, fmt.Sprintf("%.16x.conf", id[0]))
	case C.ZT_STATE_OBJECT_ROOT_LIST:
		fp = path.Join(n.path, "roots")
	}
	return fp, secret
}

func (n *Node) stateObjectPut(objType int, id [2]uint64, data []byte) {
	go func() {
		fp, secret := n.makeStateObjectPath(objType, id)
		if len(fp) > 0 {
			fileMode := os.FileMode(0644)
			if secret {
				fileMode = os.FileMode(0600)
			}
			ioutil.WriteFile(fp, data, fileMode)
			if secret {
				acl.Chmod(fp, 0600) // this emulates Unix chmod on Windows and uses os.Chmod on Unix-type systems
			}
		}
	}()
}

func (n *Node) stateObjectDelete(objType int, id [2]uint64) {
	go func() {
		fp, _ := n.makeStateObjectPath(objType, id)
		if len(fp) > 0 {
			os.Remove(fp)
		}
	}()
}

func (n *Node) stateObjectGet(objType int, id [2]uint64) ([]byte, bool) {
	fp, _ := n.makeStateObjectPath(objType, id)
	if len(fp) > 0 {
		fd, err := ioutil.ReadFile(fp)
		if err != nil {
			return nil, false
		}
		return fd, true
	}
	return nil, false
}

func (n *Node) handleTrace(traceMessage string) {
}

func (n *Node) handleUserMessage(originAddress, messageTypeID uint64, data []byte) {
}

func (n *Node) handleRemoteTrace(originAddress uint64, dictData []byte) {
}

//////////////////////////////////////////////////////////////////////////////

//export goPathCheckFunc
func goPathCheckFunc(gn unsafe.Pointer, ztAddress C.uint64_t, af C.int, ip unsafe.Pointer, port C.int) C.int {
	nodesByUserPtrLock.RLock()
	node := nodesByUserPtr[uintptr(gn)]
	nodesByUserPtrLock.RUnlock()
	if node != nil && node.pathCheck(uint64(ztAddress), int(af), nil, int(port)) {
		return 1
	}
	return 0
}

//export goPathLookupFunc
func goPathLookupFunc(gn unsafe.Pointer, ztAddress C.uint64_t, desiredAddressFamily int, familyP, ipP, portP unsafe.Pointer) C.int {
	nodesByUserPtrLock.RLock()
	node := nodesByUserPtr[uintptr(gn)]
	nodesByUserPtrLock.RUnlock()
	if node == nil {
		return 0
	}

	ip, port := node.pathLookup(uint64(ztAddress))
	if len(ip) > 0 && port > 0 && port <= 65535 {
		ip4 := ip.To4()
		if len(ip4) == 4 {
			*((*C.int)(familyP)) = C.int(afInet)
			copy((*[4]byte)(ipP)[:], ip4)
			*((*C.int)(portP)) = C.int(port)
			return 1
		} else if len(ip) == 16 {
			*((*C.int)(familyP)) = C.int(afInet6)
			copy((*[16]byte)(ipP)[:], ip)
			*((*C.int)(portP)) = C.int(port)
			return 1
		}
	}
	return 0
}

//export goStateObjectPutFunc
func goStateObjectPutFunc(gn unsafe.Pointer, objType C.int, id, data unsafe.Pointer, len C.int) {
	nodesByUserPtrLock.RLock()
	node := nodesByUserPtr[uintptr(gn)]
	nodesByUserPtrLock.RUnlock()
	if node == nil {
		return
	}
	if len < 0 {
		node.stateObjectDelete(int(objType), *((*[2]uint64)(id)))
	} else {
		node.stateObjectPut(int(objType), *((*[2]uint64)(id)), C.GoBytes(data, len))
	}
}

//export goStateObjectGetFunc
func goStateObjectGetFunc(gn unsafe.Pointer, objType C.int, id, data unsafe.Pointer, bufSize C.uint) C.int {
	nodesByUserPtrLock.RLock()
	node := nodesByUserPtr[uintptr(gn)]
	nodesByUserPtrLock.RUnlock()
	if node == nil {
		return -1
	}
	tmp, found := node.stateObjectGet(int(objType), *((*[2]uint64)(id)))
	if found && len(tmp) < int(bufSize) {
		if len(tmp) > 0 {
			C.memcpy(data, unsafe.Pointer(&(tmp[0])), C.ulong(len(tmp)))
		}
		return C.int(len(tmp))
	}
	return -1
}

//export goDNSResolverFunc
func goDNSResolverFunc(gn unsafe.Pointer, dnsRecordTypes unsafe.Pointer, numDNSRecordTypes C.int, name unsafe.Pointer, requestID C.uintptr_t) {
	nodesByUserPtrLock.RLock()
	node := nodesByUserPtr[uintptr(gn)]
	nodesByUserPtrLock.RUnlock()
	if node == nil {
		return
	}

	recordTypes := C.GoBytes(dnsRecordTypes, numDNSRecordTypes)
	recordName := C.GoString((*C.char)(name))

	go func() {
		recordNameCStrCopy := C.CString(recordName)
		for _, rt := range recordTypes {
			switch rt {
			case C.ZT_DNS_RECORD_TXT:
				recs, _ := net.LookupTXT(recordName)
				for _, rec := range recs {
					if len(rec) > 0 {
						rnCS := C.CString(rec)
						C.ZT_Node_processDNSResult(unsafe.Pointer(node.zn), nil, requestID, recordNameCStrCopy, C.ZT_DNS_RECORD_TXT, unsafe.Pointer(rnCS), C.uint(len(rec)), 0)
						C.free(unsafe.Pointer(rnCS))
					}
				}
			}
		}
		C.ZT_Node_processDNSResult(unsafe.Pointer(node.zn), nil, requestID, recordNameCStrCopy, C.ZT_DNS_RECORD__END_OF_RESULTS, nil, 0, 0)
		C.free(unsafe.Pointer(recordNameCStrCopy))
	}()
}

func sockaddrStorageToIPNet(ss *C.struct_sockaddr_storage) *net.IPNet {
	var a net.IPNet
	switch ss.ss_family {
	case afInet:
		sa4 := (*C.struct_sockaddr_in)(unsafe.Pointer(ss))
		var ip4 [4]byte
		copy(ip4[:], (*[4]byte)(unsafe.Pointer(&sa4.sin_addr))[:])
		a.IP = net.IP(ip4[:])
		a.Mask = net.CIDRMask(int(binary.BigEndian.Uint16(((*[2]byte)(unsafe.Pointer(&sa4.sin_port)))[:])), 32)
		return &a
	case afInet6:
		sa6 := (*C.struct_sockaddr_in6)(unsafe.Pointer(ss))
		var ip6 [16]byte
		copy(ip6[:], (*[16]byte)(unsafe.Pointer(&sa6.sin6_addr))[:])
		a.IP = net.IP(ip6[:])
		a.Mask = net.CIDRMask(int(binary.BigEndian.Uint16(((*[2]byte)(unsafe.Pointer(&sa6.sin6_port)))[:])), 128)
		return &a
	}
	return nil
}

//export goVirtualNetworkConfigFunc
func goVirtualNetworkConfigFunc(gn, tapP unsafe.Pointer, nwid C.uint64_t, op C.int, conf unsafe.Pointer) {
	go func() {
		nodesByUserPtrLock.RLock()
		node := nodesByUserPtr[uintptr(gn)]
		nodesByUserPtrLock.RUnlock()
		if node == nil {
			return
		}
		node.networksLock.RLock()
		network := node.networks[uint64(nwid)]
		node.networksLock.RUnlock()
		if network != nil {
			ncc := (*C.ZT_VirtualNetworkConfig)(conf)
			if network.networkConfigRevision() > uint64(ncc.netconfRevision) {
				return
			}
			var nc NetworkConfig
			nc.ID = uint64(ncc.nwid)
			nc.MAC = MAC(ncc.mac)
			nc.Name = C.GoString(ncc.name)
			nc.Status = int(ncc.status)
			nc.Type = int(ncc._type)
			nc.MTU = int(ncc.mtu)
			nc.Bridge = (ncc.bridge != 0)
			nc.BroadcastEnabled = (ncc.broadcastEnabled != 0)
			nc.NetconfRevision = uint64(ncc.netconfRevision)
			for i := 0; i < int(ncc.assignedAddressCount); i++ {
				a := sockaddrStorageToIPNet(&ncc.assignedAddresses[i])
				if a != nil {
					nc.AssignedAddresses = append(nc.AssignedAddresses, *a)
				}
			}
			for i := 0; i < int(ncc.routeCount); i++ {
				tgt := sockaddrStorageToIPNet(&ncc.routes[i].target)
				viaN := sockaddrStorageToIPNet(&ncc.routes[i].via)
				var via net.IP
				if viaN != nil {
					via = viaN.IP
				}
				if tgt != nil {
					nc.Routes = append(nc.Routes, Route{
						Target: *tgt,
						Via:    via,
						Flags:  uint16(ncc.routes[i].flags),
						Metric: uint16(ncc.routes[i].metric),
					})
				}
			}
			network.updateConfig(&nc, nil)
		}
	}()
}

//export goZtEvent
func goZtEvent(gn unsafe.Pointer, eventType C.int, data unsafe.Pointer) {
	go func() {
		nodesByUserPtrLock.RLock()
		node := nodesByUserPtr[uintptr(gn)]
		nodesByUserPtrLock.RUnlock()
		if node == nil {
			return
		}
		switch eventType {
		case C.ZT_EVENT_OFFLINE:
			atomic.StoreUint32(&node.online, 0)
		case C.ZT_EVENT_ONLINE:
			atomic.StoreUint32(&node.online, 1)
		case C.ZT_EVENT_TRACE:
			node.handleTrace(C.GoString((*C.char)(data)))
		case C.ZT_EVENT_USER_MESSAGE:
			um := (*C.ZT_UserMessage)(data)
			node.handleUserMessage(uint64(um.origin), uint64(um.typeId), C.GoBytes(um.data, C.int(um.length)))
		case C.ZT_EVENT_REMOTE_TRACE:
			rt := (*C.ZT_RemoteTrace)(data)
			node.handleRemoteTrace(uint64(rt.origin), C.GoBytes(unsafe.Pointer(rt.data), C.int(rt.len)))
		}
	}()
}

//////////////////////////////////////////////////////////////////////////////

// nativeTap is a Tap implementation that wraps a native C++ interface to a system tun/tap device
type nativeTap struct {
	tap                        unsafe.Pointer
	networkStatus              uint32
	enabled                    uint32
	multicastGroupHandlers     []func(bool, *MulticastGroup)
	multicastGroupHandlersLock sync.Mutex
}

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
func (t *nativeTap) AddIP(ip *net.IPNet) error {
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
func (t *nativeTap) RemoveIP(ip *net.IPNet) error {
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

// AddRoute adds or updates a managed route on this tap's interface
func (t *nativeTap) AddRoute(r *Route) error {
	rc := 0
	if r != nil {
		if len(r.Target.IP) == 4 {
			mask, _ := r.Target.Mask.Size()
			if len(r.Via) == 4 {
				rc = int(C.ZT_GoTap_addRoute(t.tap, afInet, unsafe.Pointer(&r.Target.IP[0]), C.int(mask), afInet, unsafe.Pointer(&r.Via[0]), C.int(r.Metric)))
			} else {
				rc = int(C.ZT_GoTap_addRoute(t.tap, afInet, unsafe.Pointer(&r.Target.IP[0]), C.int(mask), 0, nil, C.int(r.Metric)))
			}
		} else if len(r.Target.IP) == 16 {
			mask, _ := r.Target.Mask.Size()
			if len(r.Via) == 4 {
				rc = int(C.ZT_GoTap_addRoute(t.tap, afInet6, unsafe.Pointer(&r.Target.IP[0]), C.int(mask), afInet6, unsafe.Pointer(&r.Via[0]), C.int(r.Metric)))
			} else {
				rc = int(C.ZT_GoTap_addRoute(t.tap, afInet6, unsafe.Pointer(&r.Target.IP[0]), C.int(mask), 0, nil, C.int(r.Metric)))
			}
		}
	}
	if rc != 0 {
		return fmt.Errorf("tap device error adding route: %d", rc)
	}
	return nil
}

// RemoveRoute removes a managed route on this tap's interface
func (t *nativeTap) RemoveRoute(r *Route) error {
	rc := 0
	if r != nil {
		if len(r.Target.IP) == 4 {
			mask, _ := r.Target.Mask.Size()
			if len(r.Via) == 4 {
				rc = int(C.ZT_GoTap_removeRoute(t.tap, afInet, unsafe.Pointer(&r.Target.IP[0]), C.int(mask), afInet, unsafe.Pointer(&r.Via[0]), C.int(r.Metric)))
			} else {
				rc = int(C.ZT_GoTap_removeRoute(t.tap, afInet, unsafe.Pointer(&r.Target.IP[0]), C.int(mask), 0, nil, C.int(r.Metric)))
			}
		} else if len(r.Target.IP) == 16 {
			mask, _ := r.Target.Mask.Size()
			if len(r.Via) == 4 {
				rc = int(C.ZT_GoTap_removeRoute(t.tap, afInet6, unsafe.Pointer(&r.Target.IP[0]), C.int(mask), afInet6, unsafe.Pointer(&r.Via[0]), C.int(r.Metric)))
			} else {
				rc = int(C.ZT_GoTap_removeRoute(t.tap, afInet6, unsafe.Pointer(&r.Target.IP[0]), C.int(mask), 0, nil, C.int(r.Metric)))
			}
		}
	}
	if rc != 0 {
		return fmt.Errorf("tap device error removing route: %d", rc)
	}
	return nil
}

// SyncRoutes synchronizes managed routes
func (t *nativeTap) SyncRoutes() error {
	C.ZT_GoTap_syncRoutes(t.tap)
	return nil
}

//////////////////////////////////////////////////////////////////////////////

func handleTapMulticastGroupChange(gn unsafe.Pointer, nwid, mac C.uint64_t, adi C.uint32_t, added bool) {
	go func() {
		nodesByUserPtrLock.RLock()
		node := nodesByUserPtr[uintptr(gn)]
		nodesByUserPtrLock.RUnlock()
		if node == nil {
			return
		}
		node.networksLock.RLock()
		network := node.networks[uint64(nwid)]
		node.networksLock.RUnlock()
		if network != nil {
			tap, _ := network.tap.(*nativeTap)
			if tap != nil {
				mg := &MulticastGroup{MAC: MAC(mac), ADI: uint32(adi)}
				tap.multicastGroupHandlersLock.Lock()
				defer tap.multicastGroupHandlersLock.Unlock()
				for _, h := range tap.multicastGroupHandlers {
					h(added, mg)
				}
			}
		}
	}()
}

//export goHandleTapAddedMulticastGroup
func goHandleTapAddedMulticastGroup(gn, tapP unsafe.Pointer, nwid, mac C.uint64_t, adi C.uint32_t) {
	handleTapMulticastGroupChange(gn, nwid, mac, adi, true)
}

//export goHandleTapRemovedMulticastGroup
func goHandleTapRemovedMulticastGroup(gn, tapP unsafe.Pointer, nwid, mac C.uint64_t, adi C.uint32_t) {
	handleTapMulticastGroupChange(gn, nwid, mac, adi, false)
}
