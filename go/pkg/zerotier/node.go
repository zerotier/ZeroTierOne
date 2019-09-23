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
	rand "math/rand"
	"net"
	"os"
	"path"
	"sync"
	"sync/atomic"
	"time"
	"unsafe"

	acl "github.com/hectane/go-acl"
)

// Network status states
const (
	NetworkStatusRequestConfiguration int = C.ZT_NETWORK_STATUS_REQUESTING_CONFIGURATION
	NetworkStatusOK                   int = C.ZT_NETWORK_STATUS_OK
	NetworkStatusAccessDenied         int = C.ZT_NETWORK_STATUS_ACCESS_DENIED
	NetworkStatusNotFound             int = C.ZT_NETWORK_STATUS_NOT_FOUND

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

	// PlatformDefaultHomePath is the default location of ZeroTier's working path on this system
	PlatformDefaultHomePath string = C.GoString(C.ZT_PLATFORM_DEFAULT_HOMEPATH)

	afInet  = C.AF_INET
	afInet6 = C.AF_INET6
)

var (
	nodesByUserPtr     map[uintptr]*Node
	nodesByUserPtrLock sync.RWMutex
)

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

func sockaddrStorageToUDPAddr(ss *C.struct_sockaddr_storage) *net.UDPAddr {
	var a net.UDPAddr
	switch ss.ss_family {
	case afInet:
		sa4 := (*C.struct_sockaddr_in)(unsafe.Pointer(ss))
		var ip4 [4]byte
		copy(ip4[:], (*[4]byte)(unsafe.Pointer(&sa4.sin_addr))[:])
		a.IP = net.IP(ip4[:])
		a.Port = int(binary.BigEndian.Uint16(((*[2]byte)(unsafe.Pointer(&sa4.sin_port)))[:]))
		return &a
	case afInet6:
		sa6 := (*C.struct_sockaddr_in6)(unsafe.Pointer(ss))
		var ip6 [16]byte
		copy(ip6[:], (*[16]byte)(unsafe.Pointer(&sa6.sin6_addr))[:])
		a.IP = net.IP(ip6[:])
		a.Port = int(binary.BigEndian.Uint16(((*[2]byte)(unsafe.Pointer(&sa6.sin6_port)))[:]))
		return &a
	}
	return nil
}

func makeSockaddrStorage(ip net.IP, port int, ss *C.struct_sockaddr_storage) bool {
	C.memset(unsafe.Pointer(ss), 0, C.sizeof_struct_sockaddr_storage)
	if len(ip) == 4 {
		sa4 := (*C.struct_sockaddr_in)(unsafe.Pointer(ss))
		sa4.sin_family = afInet
		copy(((*[4]byte)(unsafe.Pointer(&sa4.sin_addr)))[:], ip)
		binary.BigEndian.PutUint16(((*[2]byte)(unsafe.Pointer(&sa4.sin_port)))[:], uint16(port))
		return true
	}
	if len(ip) == 16 {
		sa6 := (*C.struct_sockaddr_in6)(unsafe.Pointer(ss))
		sa6.sin6_family = afInet6
		copy(((*[16]byte)(unsafe.Pointer(&sa6.sin6_addr)))[:], ip)
		binary.BigEndian.PutUint16(((*[2]byte)(unsafe.Pointer(&sa6.sin6_port)))[:], uint16(port))
		return true
	}
	return false
}

//////////////////////////////////////////////////////////////////////////////

// Node is an instance of the ZeroTier core node and related C++ I/O code
type Node struct {
	basePath              string
	localConfig           LocalConfig
	networks              map[NetworkID]*Network
	networksByMAC         map[MAC]*Network // locked by networksLock
	externalAddresses     map[string]*net.IPNet
	localConfigLock       sync.RWMutex
	networksLock          sync.RWMutex
	externalAddressesLock sync.Mutex
	gn                    *C.ZT_GoNode
	zn                    *C.ZT_Node
	id                    *Identity
	online                uint32
	running               uint32
	runLock               sync.Mutex
}

// NewNode creates and initializes a new instance of the ZeroTier node service
func NewNode(basePath string) (*Node, error) {
	var err error

	os.MkdirAll(basePath, 0755)
	if _, err := os.Stat(basePath); err != nil {
		return nil, err
	}

	n := new(Node)
	n.basePath = basePath
	n.networks = make(map[NetworkID]*Network)
	n.networksByMAC = make(map[MAC]*Network)
	n.externalAddresses = make(map[string]*net.IPNet)

	cpath := C.CString(basePath)
	n.gn = C.ZT_GoNode_new(cpath)
	C.free(unsafe.Pointer(cpath))
	if n.gn == nil {
		return nil, ErrNodeInitFailed
	}
	n.zn = (*C.ZT_Node)(C.ZT_GoNode_getNode(n.gn))

	var ns C.ZT_NodeStatus
	C.ZT_Node_status(unsafe.Pointer(n.zn), &ns)
	n.id, err = NewIdentityFromString(C.GoString(ns.secretIdentity))
	if err != nil {
		C.ZT_GoNode_delete(n.gn)
		return nil, err
	}

	gnRawAddr := uintptr(unsafe.Pointer(n.gn))
	nodesByUserPtrLock.Lock()
	nodesByUserPtr[gnRawAddr] = n
	nodesByUserPtrLock.Unlock()

	n.online = 0
	n.running = 1

	n.runLock.Lock()
	go func() {
		lastScannedInterfaces := int64(0)
		for atomic.LoadUint32(&n.running) != 0 {
			time.Sleep(1 * time.Second)

			now := TimeMs()
			if (now - lastScannedInterfaces) >= 30000 {
				lastScannedInterfaces = now

				externalAddresses := make(map[string]*net.IPNet)
				ifs, _ := net.Interfaces()
				if len(ifs) > 0 {
					n.networksLock.RLock()
					for _, i := range ifs {
						m, _ := NewMACFromBytes(i.HardwareAddr)
						if _, isZeroTier := n.networksByMAC[m]; !isZeroTier {
							addrs, _ := i.Addrs()
							if len(addrs) > 0 {
								for _, a := range addrs {
									ipn, _ := a.(*net.IPNet)
									if ipn != nil {
										externalAddresses[ipn.String()] = ipn
									}
								}
							}
						}
					}
					n.networksLock.RUnlock()
				}

				n.localConfigLock.RLock()
				n.externalAddressesLock.Lock()
				for astr, ipn := range externalAddresses {
					if _, alreadyKnown := n.externalAddresses[astr]; !alreadyKnown {
						ipCstr := C.CString(ipn.IP.String())
						if n.localConfig.Settings.PrimaryPort > 0 && n.localConfig.Settings.PrimaryPort < 65536 {
							C.ZT_GoNode_phyStartListen(n.gn, nil, ipCstr, C.int(n.localConfig.Settings.PrimaryPort))
						}
						if n.localConfig.Settings.SecondaryPort > 0 && n.localConfig.Settings.SecondaryPort < 65536 {
							C.ZT_GoNode_phyStartListen(n.gn, nil, ipCstr, C.int(n.localConfig.Settings.SecondaryPort))
						}
						if n.localConfig.Settings.TertiaryPort > 0 && n.localConfig.Settings.TertiaryPort < 65536 {
							C.ZT_GoNode_phyStartListen(n.gn, nil, ipCstr, C.int(n.localConfig.Settings.TertiaryPort))
						}
						C.free(unsafe.Pointer(ipCstr))
					}
				}
				for astr, ipn := range n.externalAddresses {
					if _, stillPresent := externalAddresses[astr]; !stillPresent {
						ipCstr := C.CString(ipn.IP.String())
						if n.localConfig.Settings.PrimaryPort > 0 && n.localConfig.Settings.PrimaryPort < 65536 {
							C.ZT_GoNode_phyStopListen(n.gn, nil, ipCstr, C.int(n.localConfig.Settings.PrimaryPort))
						}
						if n.localConfig.Settings.SecondaryPort > 0 && n.localConfig.Settings.SecondaryPort < 65536 {
							C.ZT_GoNode_phyStopListen(n.gn, nil, ipCstr, C.int(n.localConfig.Settings.SecondaryPort))
						}
						if n.localConfig.Settings.TertiaryPort > 0 && n.localConfig.Settings.TertiaryPort < 65536 {
							C.ZT_GoNode_phyStopListen(n.gn, nil, ipCstr, C.int(n.localConfig.Settings.TertiaryPort))
						}
						C.free(unsafe.Pointer(ipCstr))
					}
				}
				n.externalAddresses = externalAddresses
				n.externalAddressesLock.Unlock()
				n.localConfigLock.RUnlock()
			}
		}
		n.runLock.Unlock()
	}()

	return n, nil
}

// Close closes this Node and frees its underlying C++ Node structures
func (n *Node) Close() {
	if atomic.SwapUint32(&n.running, 0) != 0 {
		C.ZT_GoNode_delete(n.gn)
		nodesByUserPtrLock.Lock()
		delete(nodesByUserPtr, uintptr(unsafe.Pointer(n.gn)))
		nodesByUserPtrLock.Unlock()
		n.runLock.Lock() // wait for gorountine to die
		n.runLock.Unlock()
	}
}

// Address returns this node's address
func (n *Node) Address() Address { return n.id.address }

// Identity returns this node's identity (including secret portion)
func (n *Node) Identity() *Identity { return n.id }

// LocalConfig gets this node's local configuration
func (n *Node) LocalConfig() LocalConfig {
	n.localConfigLock.RLock()
	defer n.localConfigLock.RUnlock()
	return n.localConfig
}

// Join joins a network
// If tap is nil, the default system tap for this OS/platform is used (if available).
func (n *Node) Join(nwid uint64, tap Tap) (*Network, error) {
	n.networksLock.RLock()
	if nw, have := n.networks[NetworkID(nwid)]; have {
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

	nw, err := newNetwork(n, NetworkID(nwid), &nativeTap{tap: unsafe.Pointer(ntap), enabled: 1})
	if err != nil {
		C.ZT_GoNode_leave(n.gn, C.uint64_t(nwid))
		return nil, err
	}
	n.networksLock.Lock()
	n.networks[NetworkID(nwid)] = nw
	n.networksLock.Unlock()

	return nw, nil
}

// Leave leaves a network
func (n *Node) Leave(nwid uint64) error {
	C.ZT_GoNode_leave(n.gn, C.uint64_t(nwid))
	n.networksLock.Lock()
	delete(n.networks, NetworkID(nwid))
	n.networksLock.Unlock()
	return nil
}

// Networks returns a list of networks that this node has joined
func (n *Node) Networks() []*Network {
	var nws []*Network
	n.networksLock.RLock()
	for _, nw := range n.networks {
		nws = append(nws, nw)
	}
	n.networksLock.RUnlock()
	return nws
}

// AddStaticRoot adds a statically defined root server to this node.
// If a static root with the given identity already exists this will update its IP and port information.
func (n *Node) AddStaticRoot(id *Identity, addrs []net.Addr) {
	var saddrs []C.struct_sockaddr_storage
	for _, a := range addrs {
		aa, _ := a.(*net.UDPAddr)
		if aa != nil {
			ss := new(C.struct_sockaddr_storage)
			if makeSockaddrStorage(aa.IP, aa.Port, ss) {
				saddrs = append(saddrs, *ss)
			}
		}
	}
	if len(saddrs) > 0 {
		ids := C.CString(id.String())
		C.ZT_Node_setStaticRoot(unsafe.Pointer(n.zn), ids, &saddrs[0], C.uint(len(saddrs)))
		C.free(unsafe.Pointer(ids))
	}
}

// RemoveStaticRoot removes a statically defined root server from this node.
func (n *Node) RemoveStaticRoot(id *Identity) {
	ids := C.CString(id.String())
	C.ZT_Node_removeStaticRoot(unsafe.Pointer(n.zn), ids)
	C.free(unsafe.Pointer(ids))
}

// AddDynamicRoot adds a dynamic root to this node.
// If the locator parameter is non-empty it can contain a binary serialized locator
// to use if (or until) one can be fetched via DNS.
func (n *Node) AddDynamicRoot(dnsName string, locator []byte) {
	dn := C.CString(dnsName)
	if len(locator) > 0 {
		C.ZT_Node_setDynamicRoot(unsafe.Pointer(n.zn), dn, unsafe.Pointer(&locator[0]), C.uint(len(locator)))
	} else {
		C.ZT_Node_setDynamicRoot(unsafe.Pointer(n.zn), dn, nil, 0)
	}
	C.free(unsafe.Pointer(dn))
}

// RemoveDynamicRoot removes a dynamic root from this node.
func (n *Node) RemoveDynamicRoot(dnsName string) {
	dn := C.CString(dnsName)
	C.ZT_Node_removeDynamicRoot(unsafe.Pointer(n.zn), dn)
	C.free(unsafe.Pointer(dn))
}

// Roots retrieves a list of root servers on this node and their preferred and online status.
func (n *Node) Roots() []*Root {
	var roots []*Root
	rl := C.ZT_Node_listRoots(unsafe.Pointer(n.zn), C.int64_t(TimeMs()))
	if rl != nil {
		for i := 0; i < int(rl.count); i++ {
			root := (*C.ZT_Root)(unsafe.Pointer(uintptr(unsafe.Pointer(rl)) + C.sizeof_ZT_RootList))
			id, err := NewIdentityFromString(C.GoString(root.identity))
			if err == nil {
				var addrs []net.Addr
				for j := uintptr(0); j < uintptr(root.addressCount); j++ {
					a := sockaddrStorageToUDPAddr((*C.struct_sockaddr_storage)(unsafe.Pointer(uintptr(unsafe.Pointer(root.addresses)) + (j * C.sizeof_struct_sockaddr_storage))))
					if a != nil {
						addrs = append(addrs, a)
					}
				}
				roots = append(roots, &Root{
					DNSName:   C.GoString(root.dnsName),
					Identity:  id,
					Addresses: addrs,
					Preferred: (root.preferred != 0),
					Online:    (root.online != 0),
				})
			}
		}
		C.ZT_Node_freeQueryResult(unsafe.Pointer(n.zn), unsafe.Pointer(rl))
	}
	return roots
}

// Peers retrieves a list of current peers
func (n *Node) Peers() []*Peer {
	var peers []*Peer
	pl := C.ZT_Node_peers(unsafe.Pointer(n.zn))
	if pl != nil {
		for i := uintptr(0); i < uintptr(pl.peerCount); i++ {
			p := (*C.ZT_Peer)(unsafe.Pointer(uintptr(unsafe.Pointer(pl.peers)) + (i * C.sizeof_ZT_Peer)))
			p2 := new(Peer)
			p2.Address = Address(p.address)
			p2.Version = [3]int{int(p.versionMajor), int(p.versionMinor), int(p.versionRev)}
			p2.Latency = int(p.latency)
			p2.Role = int(p.role)
			p2.Paths = make([]Path, 0, int(p.pathCount))
			for j := uintptr(0); j < uintptr(p.pathCount); j++ {
				pt := &p.paths[j]
				a := sockaddrStorageToUDPAddr(&pt.address)
				if a != nil {
					p2.Paths = append(p2.Paths, Path{
						IP:                     a.IP,
						Port:                   a.Port,
						LastSend:               int64(pt.lastSend),
						LastReceive:            int64(pt.lastReceive),
						TrustedPathID:          uint64(pt.trustedPathId),
						Latency:                float32(pt.latency),
						PacketDelayVariance:    float32(pt.packetDelayVariance),
						ThroughputDisturbCoeff: float32(pt.throughputDisturbCoeff),
						PacketErrorRatio:       float32(pt.packetErrorRatio),
						PacketLossRatio:        float32(pt.packetLossRatio),
						Stability:              float32(pt.stability),
						Throughput:             uint64(pt.throughput),
						MaxThroughput:          uint64(pt.maxThroughput),
						Allocation:             float32(pt.allocation),
					})
				}
			}
			peers = append(peers, p2)
		}
		C.ZT_Node_freeQueryResult(unsafe.Pointer(n.zn), unsafe.Pointer(pl))
	}
	return peers
}

//////////////////////////////////////////////////////////////////////////////

func (n *Node) multicastSubscribe(nwid uint64, mg *MulticastGroup) {
	C.ZT_Node_multicastSubscribe(unsafe.Pointer(n.zn), nil, C.uint64_t(nwid), C.uint64_t(mg.MAC), C.ulong(mg.ADI))
}

func (n *Node) multicastUnsubscribe(nwid uint64, mg *MulticastGroup) {
	C.ZT_Node_multicastUnsubscribe(unsafe.Pointer(n.zn), C.uint64_t(nwid), C.uint64_t(mg.MAC), C.ulong(mg.ADI))
}

func (n *Node) pathCheck(ztAddress Address, af int, ip net.IP, port int) bool {
	n.localConfigLock.RLock()
	defer n.localConfigLock.RUnlock()
	for cidr, phy := range n.localConfig.Physical {
		if phy.Blacklist {
			_, ipn, _ := net.ParseCIDR(cidr)
			if ipn != nil && ipn.Contains(ip) {
				return false
			}
		}
	}
	return true
}

func (n *Node) pathLookup(ztAddress Address) (net.IP, int) {
	n.localConfigLock.RLock()
	defer n.localConfigLock.RUnlock()
	virt := n.localConfig.Virtual[ztAddress]
	if virt != nil && len(virt.Try) > 0 {
		udpA, _ := virt.Try[rand.Int()%len(virt.Try)].(*net.UDPAddr)
		if udpA != nil {
			return udpA.IP, udpA.Port
		}
	}
	return nil, 0
}

func (n *Node) makeStateObjectPath(objType int, id [2]uint64) (string, bool) {
	var fp string
	secret := false
	switch objType {
	case C.ZT_STATE_OBJECT_IDENTITY_PUBLIC:
		fp = path.Join(n.basePath, "identity.public")
	case C.ZT_STATE_OBJECT_IDENTITY_SECRET:
		fp = path.Join(n.basePath, "identity.secret")
		secret = true
	case C.ZT_STATE_OBJECT_PEER:
		fp = path.Join(n.basePath, "peers.d")
		os.Mkdir(fp, 0700)
		fp = path.Join(fp, fmt.Sprintf("%.10x.peer", id[0]))
		secret = true
	case C.ZT_STATE_OBJECT_NETWORK_CONFIG:
		fp = path.Join(n.basePath, "networks.d")
		os.Mkdir(fp, 0755)
		fp = path.Join(fp, fmt.Sprintf("%.16x.conf", id[0]))
	case C.ZT_STATE_OBJECT_ROOT_LIST:
		fp = path.Join(n.basePath, "roots")
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
	if node != nil && node.pathCheck(Address(ztAddress), int(af), nil, int(port)) {
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

	ip, port := node.pathLookup(Address(ztAddress))
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
			nc.ID = NetworkID(ncc.nwid)
			nc.MAC = MAC(ncc.mac)
			nc.Name = C.GoString(&ncc.name[0])
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
				rc = int(C.ZT_GoTap_addRoute(t.tap, afInet, unsafe.Pointer(&r.Target.IP[0]), C.int(mask), afInet, unsafe.Pointer(&r.Via[0]), C.uint(r.Metric)))
			} else {
				rc = int(C.ZT_GoTap_addRoute(t.tap, afInet, unsafe.Pointer(&r.Target.IP[0]), C.int(mask), 0, nil, C.uint(r.Metric)))
			}
		} else if len(r.Target.IP) == 16 {
			mask, _ := r.Target.Mask.Size()
			if len(r.Via) == 4 {
				rc = int(C.ZT_GoTap_addRoute(t.tap, afInet6, unsafe.Pointer(&r.Target.IP[0]), C.int(mask), afInet6, unsafe.Pointer(&r.Via[0]), C.uint(r.Metric)))
			} else {
				rc = int(C.ZT_GoTap_addRoute(t.tap, afInet6, unsafe.Pointer(&r.Target.IP[0]), C.int(mask), 0, nil, C.uint(r.Metric)))
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
				rc = int(C.ZT_GoTap_removeRoute(t.tap, afInet, unsafe.Pointer(&r.Target.IP[0]), C.int(mask), afInet, unsafe.Pointer(&r.Via[0]), C.uint(r.Metric)))
			} else {
				rc = int(C.ZT_GoTap_removeRoute(t.tap, afInet, unsafe.Pointer(&r.Target.IP[0]), C.int(mask), 0, nil, C.uint(r.Metric)))
			}
		} else if len(r.Target.IP) == 16 {
			mask, _ := r.Target.Mask.Size()
			if len(r.Via) == 4 {
				rc = int(C.ZT_GoTap_removeRoute(t.tap, afInet6, unsafe.Pointer(&r.Target.IP[0]), C.int(mask), afInet6, unsafe.Pointer(&r.Via[0]), C.uint(r.Metric)))
			} else {
				rc = int(C.ZT_GoTap_removeRoute(t.tap, afInet6, unsafe.Pointer(&r.Target.IP[0]), C.int(mask), 0, nil, C.uint(r.Metric)))
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
