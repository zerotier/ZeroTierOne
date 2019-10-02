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
//#cgo darwin LDFLAGS: ${SRCDIR}/../../../build/go/native/libzt_go_native.a ${SRCDIR}/../../../build/node/libzt_core.a ${SRCDIR}/../../../build/osdep/libzt_osdep.a -lc++ -lpthread
//#cgo linux android LDFLAGS: ${SRCDIR}/../../../build/go/native/libzt_go_native.a ${SRCDIR}/../../../build/node/libzt_core.a ${SRCDIR}/../../../build/osdep/libzt_osdep.a -lstdc++ -lpthread -lm
//#include "../../native/GoGlue.h"
import "C"

import (
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
	"io/ioutil"
	"log"
	"math/rand"
	"net"
	"net/http"
	"os"
	"path"
	"sort"
	"strings"
	"sync"
	"sync/atomic"
	"time"
	"unsafe"

	"github.com/hectane/go-acl"
)

var nullLogger = log.New(ioutil.Discard, "", 0)

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

	// AFInet is the address family for IPv4
	AFInet = C.AF_INET

	// AFInet6 is the address family for IPv6
	AFInet6 = C.AF_INET6

	networkConfigOpUp     int = C.ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_UP
	networkConfigOpUpdate int = C.ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE

	defaultVirtualNetworkMTU = C.ZT_DEFAULT_MTU
)

var (
	// PlatformDefaultHomePath is the default location of ZeroTier's working path on this system
	PlatformDefaultHomePath string = C.GoString(C.ZT_PLATFORM_DEFAULT_HOMEPATH)

	// This map is used to get the Go Node object from a pointer passed back in via C callbacks
	nodesByUserPtr     = make(map[uintptr]*Node)
	nodesByUserPtrLock sync.RWMutex
)

func sockaddrStorageToIPNet(ss *C.struct_sockaddr_storage) *net.IPNet {
	var a net.IPNet
	switch ss.ss_family {
	case AFInet:
		sa4 := (*C.struct_sockaddr_in)(unsafe.Pointer(ss))
		var ip4 [4]byte
		copy(ip4[:], (*[4]byte)(unsafe.Pointer(&sa4.sin_addr))[:])
		a.IP = ip4[:]
		a.Mask = net.CIDRMask(int(binary.BigEndian.Uint16(((*[2]byte)(unsafe.Pointer(&sa4.sin_port)))[:])), 32)
		return &a
	case AFInet6:
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
	case AFInet:
		sa4 := (*C.struct_sockaddr_in)(unsafe.Pointer(ss))
		var ip4 [4]byte
		copy(ip4[:], (*[4]byte)(unsafe.Pointer(&sa4.sin_addr))[:])
		a.IP = ip4[:]
		a.Port = int(binary.BigEndian.Uint16(((*[2]byte)(unsafe.Pointer(&sa4.sin_port)))[:]))
		return &a
	case AFInet6:
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

func makeSockaddrStorage(ip net.IP, port int, ss *C.struct_sockaddr_storage) bool {
	C.memset(unsafe.Pointer(ss), 0, C.sizeof_struct_sockaddr_storage)
	if len(ip) == 4 {
		sa4 := (*C.struct_sockaddr_in)(unsafe.Pointer(ss))
		sa4.sin_family = AFInet
		copy(((*[4]byte)(unsafe.Pointer(&sa4.sin_addr)))[:], ip)
		binary.BigEndian.PutUint16(((*[2]byte)(unsafe.Pointer(&sa4.sin_port)))[:], uint16(port))
		return true
	}
	if len(ip) == 16 {
		sa6 := (*C.struct_sockaddr_in6)(unsafe.Pointer(ss))
		sa6.sin6_family = AFInet6
		copy(((*[16]byte)(unsafe.Pointer(&sa6.sin6_addr)))[:], ip)
		binary.BigEndian.PutUint16(((*[2]byte)(unsafe.Pointer(&sa6.sin6_port)))[:], uint16(port))
		return true
	}
	return false
}

//////////////////////////////////////////////////////////////////////////////

// Node is an instance of the ZeroTier core node and related C++ I/O code
type Node struct {
	networks               map[NetworkID]*Network
	networksByMAC          map[MAC]*Network  // locked by networksLock
	interfaceAddresses     map[string]net.IP // physical external IPs on the machine
	basePath               string
	localConfigPath        string
	localConfig            LocalConfig
	localConfigLock        sync.RWMutex
	networksLock           sync.RWMutex
	interfaceAddressesLock sync.Mutex
	logW                   *sizeLimitWriter
	log                    *log.Logger
	gn                     *C.ZT_GoNode
	zn                     *C.ZT_Node
	id                     *Identity
	apiServer              *http.Server
	tcpApiServer           *http.Server
	online                 uint32
	running                uint32
	runLock                sync.Mutex
}

// NewNode creates and initializes a new instance of the ZeroTier node service
func NewNode(basePath string) (*Node, error) {
	var err error

	_ = os.MkdirAll(basePath, 0755)
	if _, err := os.Stat(basePath); err != nil {
		return nil, err
	}

	n := new(Node)

	n.networks = make(map[NetworkID]*Network)
	n.networksByMAC = make(map[MAC]*Network)
	n.interfaceAddresses = make(map[string]net.IP)

	n.basePath = basePath
	n.localConfigPath = path.Join(basePath, "local.conf")
	err = n.localConfig.Read(n.localConfigPath, true)
	if err != nil {
		return nil, err
	}

	if n.localConfig.Settings.LogSizeMax >= 0 {
		n.logW, err = sizeLimitWriterOpen(path.Join(basePath, "service.log"))
		if err != nil {
			return nil, err
		}
		n.log = log.New(n.logW, "", log.LstdFlags)
	} else {
		n.log = nullLogger
	}

	if n.localConfig.Settings.PortSearch {
		portsChanged := false

		portCheckCount := 0
		for portCheckCount < 2048 {
			portCheckCount++
			if checkPort(n.localConfig.Settings.PrimaryPort) {
				break
			}
			n.log.Printf("primary port %d unavailable, trying next port (port search enabled)", n.localConfig.Settings.PrimaryPort)
			n.localConfig.Settings.PrimaryPort++
			n.localConfig.Settings.PrimaryPort &= 0xffff
			portsChanged = true
		}
		if portCheckCount == 2048 {
			return nil, errors.New("unable to bind to primary port, tried 2048 later ports")
		}

		if n.localConfig.Settings.SecondaryPort > 0 {
			portCheckCount = 0
			for portCheckCount < 2048 {
				portCheckCount++
				if checkPort(n.localConfig.Settings.SecondaryPort) {
					break
				}
				n.log.Printf("secondary port %d unavailable, trying next port (port search enabled)", n.localConfig.Settings.SecondaryPort)
				n.localConfig.Settings.SecondaryPort++
				n.localConfig.Settings.SecondaryPort &= 0xffff
				portsChanged = true
			}
			if portCheckCount == 2048 {
				n.localConfig.Settings.SecondaryPort = 0
			}
		}

		if n.localConfig.Settings.TertiaryPort > 0 {
			portCheckCount = 0
			for portCheckCount < 2048 {
				portCheckCount++
				if checkPort(n.localConfig.Settings.TertiaryPort) {
					break
				}
				n.log.Printf("tertiary port %d unavailable, trying next port (port search enabled)", n.localConfig.Settings.TertiaryPort)
				n.localConfig.Settings.TertiaryPort++
				n.localConfig.Settings.TertiaryPort &= 0xffff
				portsChanged = true
			}
			if portCheckCount == 2048 {
				n.localConfig.Settings.TertiaryPort = 0
			}
		}

		if portsChanged {
			_ = n.localConfig.Write(n.localConfigPath)
		}
	} else if !checkPort(n.localConfig.Settings.PrimaryPort) {
		return nil, errors.New("unable to bind to primary port")
	}

	cPath := C.CString(basePath)
	n.gn = C.ZT_GoNode_new(cPath)
	C.free(unsafe.Pointer(cPath))
	if n.gn == nil {
		n.log.Println("FATAL: node initialization failed")
		return nil, ErrNodeInitFailed
	}
	n.zn = (*C.ZT_Node)(C.ZT_GoNode_getNode(n.gn))

	var ns C.ZT_NodeStatus
	C.ZT_Node_status(unsafe.Pointer(n.zn), &ns)
	idString := C.GoString(ns.secretIdentity)
	n.id, err = NewIdentityFromString(idString)
	if err != nil {
		n.log.Printf("FATAL: node's identity does not seem valid (%s)", string(idString))
		C.ZT_GoNode_delete(n.gn)
		return nil, err
	}

	n.apiServer, n.tcpApiServer, err = createAPIServer(basePath, n)
	if err != nil {
		n.log.Printf("FATAL: unable to start API server: %s", err.Error())
		C.ZT_GoNode_delete(n.gn)
		return nil, err
	}

	gnRawAddr := uintptr(unsafe.Pointer(n.gn))
	nodesByUserPtrLock.Lock()
	nodesByUserPtr[gnRawAddr] = n
	nodesByUserPtrLock.Unlock()

	n.online = 0
	n.running = 1

	n.runLock.Lock() // used to block Close() until below gorountine exits
	go func() {
		lastMaintenanceRun := int64(0)
		for atomic.LoadUint32(&n.running) != 0 {
			time.Sleep(1 * time.Second)

			now := TimeMs()
			if (now - lastMaintenanceRun) >= 30000 {
				lastMaintenanceRun = now
				n.localConfigLock.RLock()

				// Get local physical interface addresses, excluding blacklisted and ZeroTier-created interfaces
				interfaceAddresses := make(map[string]net.IP)
				ifs, _ := net.Interfaces()
				if len(ifs) > 0 {
					n.networksLock.RLock()
				scanInterfaces:
					for _, i := range ifs {
						for _, bl := range n.localConfig.Settings.InterfacePrefixBlacklist {
							if strings.HasPrefix(strings.ToLower(i.Name), strings.ToLower(bl)) {
								continue scanInterfaces
							}
						}
						m, _ := NewMACFromBytes(i.HardwareAddr)
						if _, isZeroTier := n.networksByMAC[m]; !isZeroTier {
							addrs, _ := i.Addrs()
							for _, a := range addrs {
								ipn, _ := a.(*net.IPNet)
								if ipn != nil && len(ipn.IP) > 0 && !ipn.IP.IsLinkLocalUnicast() && !ipn.IP.IsMulticast() {
									interfaceAddresses[ipn.IP.String()] = ipn.IP
								}
							}
						}
					}
					n.networksLock.RUnlock()
				}

				// Open or close locally bound UDP ports for each local interface address.
				// This opens ports if they are not already open and then closes ports if
				// they are open but no longer seem to exist.
				n.interfaceAddressesLock.Lock()
				for astr, ipn := range interfaceAddresses {
					if _, alreadyKnown := n.interfaceAddresses[astr]; !alreadyKnown {
						ipCstr := C.CString(ipn.String())
						if n.localConfig.Settings.PrimaryPort > 0 && n.localConfig.Settings.PrimaryPort < 65536 {
							n.log.Printf("UDP binding to port %d on interface %s", n.localConfig.Settings.PrimaryPort, astr)
							C.ZT_GoNode_phyStartListen(n.gn, nil, ipCstr, C.int(n.localConfig.Settings.PrimaryPort))
						}
						if n.localConfig.Settings.SecondaryPort > 0 && n.localConfig.Settings.SecondaryPort < 65536 {
							n.log.Printf("UDP binding to port %d on interface %s", n.localConfig.Settings.SecondaryPort, astr)
							C.ZT_GoNode_phyStartListen(n.gn, nil, ipCstr, C.int(n.localConfig.Settings.SecondaryPort))
						}
						if n.localConfig.Settings.TertiaryPort > 0 && n.localConfig.Settings.TertiaryPort < 65536 {
							n.log.Printf("UDP binding to port %d on interface %s", n.localConfig.Settings.TertiaryPort, astr)
							C.ZT_GoNode_phyStartListen(n.gn, nil, ipCstr, C.int(n.localConfig.Settings.TertiaryPort))
						}
						C.free(unsafe.Pointer(ipCstr))
					}
				}
				for astr, ipn := range n.interfaceAddresses {
					if _, stillPresent := interfaceAddresses[astr]; !stillPresent {
						ipCstr := C.CString(ipn.String())
						if n.localConfig.Settings.PrimaryPort > 0 && n.localConfig.Settings.PrimaryPort < 65536 {
							n.log.Printf("UDP closing socket bound to port %d on interface %s", n.localConfig.Settings.PrimaryPort, astr)
							C.ZT_GoNode_phyStopListen(n.gn, nil, ipCstr, C.int(n.localConfig.Settings.PrimaryPort))
						}
						if n.localConfig.Settings.SecondaryPort > 0 && n.localConfig.Settings.SecondaryPort < 65536 {
							n.log.Printf("UDP closing socket bound to port %d on interface %s", n.localConfig.Settings.SecondaryPort, astr)
							C.ZT_GoNode_phyStopListen(n.gn, nil, ipCstr, C.int(n.localConfig.Settings.SecondaryPort))
						}
						if n.localConfig.Settings.TertiaryPort > 0 && n.localConfig.Settings.TertiaryPort < 65536 {
							n.log.Printf("UDP closing socket bound to port %d on interface %s", n.localConfig.Settings.TertiaryPort, astr)
							C.ZT_GoNode_phyStopListen(n.gn, nil, ipCstr, C.int(n.localConfig.Settings.TertiaryPort))
						}
						C.free(unsafe.Pointer(ipCstr))
					}
				}
				n.interfaceAddresses = interfaceAddresses
				n.interfaceAddressesLock.Unlock()

				// Trim log if it's gone over its size limit
				if n.localConfig.Settings.LogSizeMax > 0 && n.logW != nil {
					_ = n.logW.trim(n.localConfig.Settings.LogSizeMax*1024, 0.5, true)
				}

				n.localConfigLock.RUnlock()
			}
		}
		n.runLock.Unlock() // signal Close() that maintenance goroutine is done
	}()

	return n, nil
}

// Close closes this Node and frees its underlying C++ Node structures
func (n *Node) Close() {
	if atomic.SwapUint32(&n.running, 0) != 0 {
		if n.apiServer != nil {
			_ = n.apiServer.Close()
		}
		if n.tcpApiServer != nil {
			_ = n.tcpApiServer.Close()
		}

		C.ZT_GoNode_delete(n.gn)

		n.runLock.Lock() // wait for maintenance gorountine to die
		n.runLock.Unlock()

		nodesByUserPtrLock.Lock()
		delete(nodesByUserPtr, uintptr(unsafe.Pointer(n.gn)))
		nodesByUserPtrLock.Unlock()
	}
}

// Address returns this node's address
func (n *Node) Address() Address { return n.id.address }

// Identity returns this node's identity (including secret portion)
func (n *Node) Identity() *Identity { return n.id }

// Online returns true if this node can reach something
func (n *Node) Online() bool { return atomic.LoadUint32(&n.online) != 0 }

// InterfaceAddresses are external IPs belonging to physical interfaces on this machine
func (n *Node) InterfaceAddresses() []net.IP {
	var ea []net.IP
	n.interfaceAddressesLock.Lock()
	for _, a := range n.interfaceAddresses {
		ea = append(ea, a)
	}
	n.interfaceAddressesLock.Unlock()
	sort.Slice(ea, func(a, b int) bool { return bytes.Compare(ea[a], ea[b]) < 0 })
	return ea
}

// LocalConfig gets this node's local configuration
func (n *Node) LocalConfig() LocalConfig {
	n.localConfigLock.RLock()
	defer n.localConfigLock.RUnlock()
	return n.localConfig
}

// SetLocalConfig updates this node's local configuration
func (n *Node) SetLocalConfig(lc *LocalConfig) (restartRequired bool, err error) {
	n.networksLock.RLock()
	n.localConfigLock.Lock()
	defer n.localConfigLock.Unlock()
	defer n.networksLock.RUnlock()

	for nid, nc := range lc.Network {
		nw := n.networks[nid]
		if nw != nil {
			nw.SetLocalSettings(nc)
		}
	}

	if n.localConfig.Settings.PrimaryPort != lc.Settings.PrimaryPort || n.localConfig.Settings.SecondaryPort != lc.Settings.SecondaryPort || n.localConfig.Settings.TertiaryPort != lc.Settings.TertiaryPort {
		restartRequired = true
	}
	if lc.Settings.LogSizeMax < 0 {
		n.log = nullLogger
		_ = n.logW.Close()
		n.logW = nil
	} else if n.logW != nil {
		n.logW, err = sizeLimitWriterOpen(path.Join(n.basePath, "service.log"))
		if err == nil {
			n.log = log.New(n.logW, "", log.LstdFlags)
		} else {
			n.log = nullLogger
			n.logW = nil
		}
	}

	n.localConfig = *lc

	return
}

// Join joins a network
// If tap is nil, the default system tap for this OS/platform is used (if available).
func (n *Node) Join(nwid NetworkID, settings *NetworkLocalSettings, tap Tap) (*Network, error) {
	n.networksLock.RLock()
	if nw, have := n.networks[nwid]; have {
		n.log.Printf("join network %.16x ignored: already a member", nwid)
		if settings != nil {
			nw.SetLocalSettings(settings)
		}
		return nw, nil
	}
	n.networksLock.RUnlock()

	if tap != nil {
		panic("non-native taps not yet implemented")
	}
	ntap := C.ZT_GoNode_join(n.gn, C.uint64_t(nwid))
	if ntap == nil {
		n.log.Printf("join network %.16x failed: tap device failed to initialize (check drivers / kernel modules)", uint64(nwid))
		return nil, ErrTapInitFailed
	}

	nw, err := newNetwork(n, nwid, &nativeTap{tap: unsafe.Pointer(ntap), enabled: 1})
	if err != nil {
		n.log.Printf("join network %.16x failed: network failed to initialize: %s", nwid, err.Error())
		C.ZT_GoNode_leave(n.gn, C.uint64_t(nwid))
		return nil, err
	}
	n.networksLock.Lock()
	n.networks[nwid] = nw
	n.networksLock.Unlock()
	if settings != nil {
		nw.SetLocalSettings(settings)
	}

	return nw, nil
}

// Leave leaves a network
func (n *Node) Leave(nwid NetworkID) error {
	n.log.Printf("leaving network %.16x", nwid)
	n.networksLock.Lock()
	nw := n.networks[nwid]
	delete(n.networks, nwid)
	n.networksLock.Unlock()
	if nw != nil {
		nw.leaving()
	}
	C.ZT_GoNode_leave(n.gn, C.uint64_t(nwid))
	return nil
}

// GetNetwork looks up a network by ID or returns nil if not joined
func (n *Node) GetNetwork(nwid NetworkID) *Network {
	n.networksLock.RLock()
	nw := n.networks[nwid]
	n.networksLock.RUnlock()
	return nw
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

// Roots retrieves a list of root servers on this node and their preferred and online status.
func (n *Node) Roots() []*Root {
	var roots []*Root
	rl := C.ZT_Node_listRoots(unsafe.Pointer(n.zn), C.int64_t(TimeMs()))
	if rl != nil {
		for i := 0; i < int(rl.count); i++ {
			root := (*C.ZT_Root)(unsafe.Pointer(uintptr(unsafe.Pointer(rl)) + C.sizeof_ZT_RootList))
			loc, _ := NewLocatorFromBytes(C.GoBytes(root.locator, C.int(root.locatorSize)))
			if loc != nil {
				roots = append(roots, &Root{
					Name:    C.GoString(root.name),
					Locator: loc,
				})
			}
		}
		C.ZT_Node_freeQueryResult(unsafe.Pointer(n.zn), unsafe.Pointer(rl))
	}
	return roots
}

// SetRoot sets or updates a root.
// Name can be a DNS name (preferably secure) for DNS fetched locators or can be
// the empty string for static roots. If the name is empty then the locator must
// be non-nil.
func (n *Node) SetRoot(name string, locator *Locator) error {
	if len(name) == 0 {
		if locator == nil {
			return ErrInvalidParameter
		}
		name = locator.Identity.address.String()
	}
	var lb []byte
	if locator != nil {
		lb = locator.Bytes
	}
	var lbp unsafe.Pointer
	if len(lb) > 0 {
		lbp = unsafe.Pointer(&lb[0])
	}
	cn := C.CString(name)
	defer C.free(unsafe.Pointer(cn))
	if C.ZT_Node_setRoot(unsafe.Pointer(n.zn), cn, lbp, C.uint(len(lb))) != 0 {
		return ErrInternal
	}
	return nil
}

// RemoveRoot removes a root.
// For static roots the name should be the ZeroTier address.
func (n *Node) RemoveRoot(name string) {
	cn := C.CString(name)
	defer C.free(unsafe.Pointer(cn))
	C.ZT_Node_removeRoot(unsafe.Pointer(n.zn), cn)
	return
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
			usingAllocation := false
			for j := uintptr(0); j < uintptr(p.pathCount); j++ {
				pt := &p.paths[j]
				if pt.alive != 0 {
					a := sockaddrStorageToUDPAddr(&pt.address)
					if a != nil {
						alloc := float32(pt.allocation)
						if alloc > 0.0 {
							usingAllocation = true
						}
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
							Allocation:             alloc,
						})
					}
				}
			}
			if !usingAllocation { // if all allocations are zero fall back to single path mode that uses the preferred flag
				for i, j := 0, uintptr(0); j < uintptr(p.pathCount); j++ {
					pt := &p.paths[j]
					if pt.alive != 0 {
						if pt.preferred == 0 {
							p2.Paths[i].Allocation = 0.0
						} else {
							p2.Paths[i].Allocation = 1.0
						}
						i++
					}
				}
			}
			sort.Slice(p2.Paths, func(a, b int) bool {
				pa := &p2.Paths[a]
				pb := &p2.Paths[b]
				if pb.Allocation < pa.Allocation { // invert order, put highest allocation paths first
					return true
				}
				if pa.Allocation == pb.Allocation {
					return pa.LastReceive < pb.LastReceive // then sort by most recent activity
				}
				return false
			})

			p2.Clock = TimeMs()
			peers = append(peers, p2)
		}
		C.ZT_Node_freeQueryResult(unsafe.Pointer(n.zn), unsafe.Pointer(pl))
	}
	sort.Slice(peers, func(a, b int) bool {
		return peers[a].Address < peers[b].Address
	})
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
		idx := rand.Int() % len(virt.Try)
		return virt.Try[idx].IP, virt.Try[idx].Port
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
		_ = os.Mkdir(fp, 0700)
		fp = path.Join(fp, fmt.Sprintf("%.10x.peer", id[0]))
		secret = true
	case C.ZT_STATE_OBJECT_NETWORK_CONFIG:
		fp = path.Join(n.basePath, "networks.d")
		_ = os.Mkdir(fp, 0755)
		fp = path.Join(fp, fmt.Sprintf("%.16x.conf", id[0]))
	case C.ZT_STATE_OBJECT_ROOT_LIST:
		fp = path.Join(n.basePath, "roots")
	}
	return fp, secret
}

func (n *Node) stateObjectPut(objType int, id [2]uint64, data []byte) {
	fp, secret := n.makeStateObjectPath(objType, id)
	if len(fp) > 0 {
		fileMode := os.FileMode(0644)
		if secret {
			fileMode = os.FileMode(0600)
		}
		_ = ioutil.WriteFile(fp, data, fileMode)
		if secret {
			_ = acl.Chmod(fp, 0600) // this emulates Unix chmod on Windows and uses os.Chmod on Unix-type systems
		}
	}
}

func (n *Node) stateObjectDelete(objType int, id [2]uint64) {
	fp, _ := n.makeStateObjectPath(objType, id)
	if len(fp) > 0 {
		_ = os.Remove(fp)
	}
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
	if len(traceMessage) > 0 {
		n.log.Print("TRACE: " + traceMessage)
	}
}

func (n *Node) handleUserMessage(originAddress, messageTypeID uint64, data []byte) {
}

func (n *Node) handleRemoteTrace(originAddress uint64, dictData []byte) {
}

//////////////////////////////////////////////////////////////////////////////

// These are callbacks called by the core and GoGlue stuff to talk to the
// service. These launch gorountines to do their work where possible to
// avoid blocking anything in the core.

//export goPathCheckFunc
func goPathCheckFunc(gn unsafe.Pointer, ztAddress C.uint64_t, af C.int, ip unsafe.Pointer, port C.int) C.int {
	nodesByUserPtrLock.RLock()
	node := nodesByUserPtr[uintptr(gn)]
	nodesByUserPtrLock.RUnlock()
	var nip net.IP
	if af == AFInet {
		nip = ((*[4]byte)(ip))[:]
	} else if af == AFInet6 {
		nip = ((*[16]byte)(ip))[:]
	} else {
		return 0
	}
	if node != nil && len(nip) > 0 && node.pathCheck(Address(ztAddress), int(af), nip, int(port)) {
		return 1
	}
	return 0
}

//export goPathLookupFunc
func goPathLookupFunc(gn unsafe.Pointer, ztAddress C.uint64_t, _ int, familyP, ipP, portP unsafe.Pointer) C.int {
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
			*((*C.int)(familyP)) = C.int(AFInet)
			copy((*[4]byte)(ipP)[:], ip4)
			*((*C.int)(portP)) = C.int(port)
			return 1
		} else if len(ip) == 16 {
			*((*C.int)(familyP)) = C.int(AFInet6)
			copy((*[16]byte)(ipP)[:], ip)
			*((*C.int)(portP)) = C.int(port)
			return 1
		}
	}

	return 0
}

//export goStateObjectPutFunc
func goStateObjectPutFunc(gn unsafe.Pointer, objType C.int, id, data unsafe.Pointer, len C.int) {
	go func() {
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
	}()
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
	go func() {
		nodesByUserPtrLock.RLock()
		node := nodesByUserPtr[uintptr(gn)]
		nodesByUserPtrLock.RUnlock()
		if node == nil {
			return
		}

		recordTypes := C.GoBytes(dnsRecordTypes, numDNSRecordTypes)
		recordName := C.GoString((*C.char)(name))

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
func goVirtualNetworkConfigFunc(gn, _ unsafe.Pointer, nwid C.uint64_t, op C.int, conf unsafe.Pointer) {
	go func() {
		nodesByUserPtrLock.RLock()
		node := nodesByUserPtr[uintptr(gn)]
		nodesByUserPtrLock.RUnlock()
		if node == nil {
			return
		}

		node.networksLock.RLock()
		network := node.networks[NetworkID(nwid)]
		node.networksLock.RUnlock()

		if network != nil {
			switch int(op) {
			case networkConfigOpUp, networkConfigOpUpdate:
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
				nc.Bridge = ncc.bridge != 0
				nc.BroadcastEnabled = ncc.broadcastEnabled != 0
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
					var via *net.IP
					if viaN != nil && len(viaN.IP) > 0 {
						via = &viaN.IP
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
