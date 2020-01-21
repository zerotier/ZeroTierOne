/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
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

//#cgo CFLAGS: -O3
//#cgo darwin LDFLAGS: ${SRCDIR}/../../../build/go/native/libzt_go_native.a ${SRCDIR}/../../../build/node/libzt_core.a ${SRCDIR}/../../../build/osdep/libzt_osdep.a -lc++ -lpthread
//#cgo linux android LDFLAGS: ${SRCDIR}/../../../build/go/native/libzt_go_native.a ${SRCDIR}/../../../build/node/libzt_core.a ${SRCDIR}/../../../build/osdep/libzt_osdep.a -lstdc++ -lpthread -lm
//#include "../../native/GoGlue.h"
import "C"

import (
	"bytes"
	"encoding/hex"
	"errors"
	"fmt"
	"io/ioutil"
	"log"
	"math/rand"
	"net"
	"net/http"
	"os"
	"path"
	"reflect"
	"sort"
	"strings"
	"sync"
	"sync/atomic"
	"syscall"
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

// Node is an instance of the ZeroTier core node and related C++ I/O code
type Node struct {
	networks               map[NetworkID]*Network
	networksByMAC          map[MAC]*Network  // locked by networksLock
	networksLock           sync.RWMutex
	interfaceAddresses     map[string]net.IP // physical external IPs on the machine
	interfaceAddressesLock sync.Mutex
	basePath               string
	peersPath              string
	networksPath           string
	localConfigPath        string
	localConfig            LocalConfig
	localConfigLock        sync.RWMutex
	logW                   *sizeLimitWriter
	log                    *log.Logger
	gn                     *C.ZT_GoNode
	zn                     *C.ZT_Node
	id                     *Identity
	namedSocketApiServer   *http.Server
	tcpApiServer           *http.Server
	online                 uint32
	running                uint32
	runWaitGroup           sync.WaitGroup
}

// NewNode creates and initializes a new instance of the ZeroTier node service
func NewNode(basePath string) (n *Node, err error) {
	n = new(Node)
	n.networks = make(map[NetworkID]*Network)
	n.networksByMAC = make(map[MAC]*Network)
	n.interfaceAddresses = make(map[string]net.IP)
	n.online = 0
	n.running = 1

	_ = os.MkdirAll(basePath, 0755)
	if _, err = os.Stat(basePath); err != nil {
		return
	}
	n.basePath = basePath
	n.peersPath = path.Join(basePath, "peers.d")
	_ = os.MkdirAll(n.peersPath, 0700)
	_ = acl.Chmod(n.peersPath, 0700)
	if _, err = os.Stat(n.peersPath); err != nil {
		return
	}
	n.networksPath = path.Join(basePath, "networks.d")
	_ = os.MkdirAll(n.networksPath, 0755)
	if _, err = os.Stat(n.networksPath); err != nil {
		return
	}
	n.localConfigPath = path.Join(basePath, "local.conf")

	_, identitySecretNotFoundErr := os.Stat(path.Join(basePath, "identity.secret"))
	err = n.localConfig.Read(n.localConfigPath, true, identitySecretNotFoundErr != nil)
	if err != nil {
		return
	}

	if n.localConfig.Settings.LogSizeMax >= 0 {
		n.logW, err = sizeLimitWriterOpen(path.Join(basePath, "node.log"))
		if err != nil {
			return
		}
		n.log = log.New(n.logW, "", log.LstdFlags)
	} else {
		n.log = nullLogger
	}

	if n.localConfig.Settings.PortSearch {
		portsChanged := false

		portCheckCount := 0
		origPort := n.localConfig.Settings.PrimaryPort
		for portCheckCount < 256 {
			portCheckCount++
			if checkPort(n.localConfig.Settings.PrimaryPort) {
				if n.localConfig.Settings.PrimaryPort != origPort {
					n.log.Printf("primary port %d unavailable, found port %d and saved in local.conf", origPort, n.localConfig.Settings.PrimaryPort)
				}
				break
			}
			n.localConfig.Settings.PrimaryPort = int(4096 + (randomUInt() % 16384))
			portsChanged = true
		}
		if portCheckCount == 256 {
			return nil, errors.New("unable to bind to primary port: tried configured port and 256 other random ports")
		}

		if n.localConfig.Settings.SecondaryPort > 0 {
			portCheckCount = 0
			origPort = n.localConfig.Settings.SecondaryPort
			for portCheckCount < 256 {
				portCheckCount++
				if checkPort(n.localConfig.Settings.SecondaryPort) {
					if n.localConfig.Settings.SecondaryPort != origPort {
						n.log.Printf("secondary port %d unavailable, found port %d (port search enabled)", origPort, n.localConfig.Settings.SecondaryPort)
					}
					break
				}
				n.log.Printf("secondary port %d unavailable, trying a random port (port search enabled)", n.localConfig.Settings.SecondaryPort)
				if portCheckCount <= 64 {
					n.localConfig.Settings.SecondaryPort = unassignedPrivilegedPorts[randomUInt()%uint(len(unassignedPrivilegedPorts))]
				} else {
					n.localConfig.Settings.SecondaryPort = int(16384 + (randomUInt() % 16384))
				}
				portsChanged = true
			}
		}

		if portsChanged {
			_ = n.localConfig.Write(n.localConfigPath)
		}
	} else {
		if !checkPort(n.localConfig.Settings.PrimaryPort) {
			return nil, errors.New("unable to bind to primary port")
		}
		if n.localConfig.Settings.SecondaryPort > 0 && n.localConfig.Settings.SecondaryPort < 65536 {
			if !checkPort(n.localConfig.Settings.SecondaryPort) {
				n.log.Printf("WARNING: unable to bind secondary port %d",n.localConfig.Settings.SecondaryPort)
			}
		}
	}

	n.namedSocketApiServer, n.tcpApiServer, err = createAPIServer(basePath, n)
	if err != nil {
		n.log.Printf("FATAL: unable to start API server: %s", err.Error())
		return nil, err
	}

	nodesByUserPtrLock.Lock()
	nodesByUserPtr[uintptr(unsafe.Pointer(n))] = n
	nodesByUserPtrLock.Unlock()

	cPath := C.CString(basePath)
	n.gn = C.ZT_GoNode_new(cPath, C.uintptr_t(uintptr(unsafe.Pointer(n))))
	C.free(unsafe.Pointer(cPath))
	if n.gn == nil {
		n.log.Println("FATAL: node initialization failed")
		nodesByUserPtrLock.Lock()
		delete(nodesByUserPtr, uintptr(unsafe.Pointer(n)))
		nodesByUserPtrLock.Unlock()
		return nil, ErrNodeInitFailed
	}
	n.zn = (*C.ZT_Node)(C.ZT_GoNode_getNode(n.gn))

	n.id, err = newIdentityFromCIdentity(C.ZT_Node_identity(unsafe.Pointer(n.zn)))
	if err != nil {
		n.log.Printf("FATAL: error obtaining node's identity")
		nodesByUserPtrLock.Lock()
		delete(nodesByUserPtr, uintptr(unsafe.Pointer(n)))
		nodesByUserPtrLock.Unlock()
		C.ZT_GoNode_delete(n.gn)
		return nil, err
	}

	n.runWaitGroup.Add(1)
	go func() {
		defer n.runWaitGroup.Done()
		var previousExplicitExternalAddresses []ExternalAddress // empty at first so these will be configured
		lastMaintenanceRun := int64(0)

		for atomic.LoadUint32(&n.running) != 0 {
			time.Sleep(500 * time.Millisecond)

			nowS := time.Now().Unix()
			if (nowS - lastMaintenanceRun) >= 30 {
				lastMaintenanceRun = nowS

				//////////////////////////////////////////////////////////////////////
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
				interfaceAddressesChanged := false
				ports := make([]int, 0, 2)
				if n.localConfig.Settings.PrimaryPort > 0 && n.localConfig.Settings.PrimaryPort < 65536 {
					ports = append(ports, n.localConfig.Settings.PrimaryPort)
				}
				if n.localConfig.Settings.SecondaryPort > 0 && n.localConfig.Settings.SecondaryPort < 65536 {
					ports = append(ports, n.localConfig.Settings.SecondaryPort)
				}
				n.interfaceAddressesLock.Lock()
				for astr, ipn := range interfaceAddresses {
					if _, alreadyKnown := n.interfaceAddresses[astr]; !alreadyKnown {
						interfaceAddressesChanged = true
						ipCstr := C.CString(ipn.String())
						for pn, p := range ports {
							n.log.Printf("UDP binding to port %d on interface %s", p, astr)
							primary := C.int(0)
							if pn == 0 {
								primary = 1
							}
							C.ZT_GoNode_phyStartListen(n.gn, nil, ipCstr, C.int(p), primary)
						}
						C.free(unsafe.Pointer(ipCstr))
					}
				}
				for astr, ipn := range n.interfaceAddresses {
					if _, stillPresent := interfaceAddresses[astr]; !stillPresent {
						interfaceAddressesChanged = true
						ipCstr := C.CString(ipn.String())
						for _, p := range ports {
							n.log.Printf("UDP closing socket bound to port %d on interface %s", p, astr)
							C.ZT_GoNode_phyStopListen(n.gn, nil, ipCstr, C.int(p))
						}
						C.free(unsafe.Pointer(ipCstr))
					}
				}
				n.interfaceAddresses = interfaceAddresses
				n.interfaceAddressesLock.Unlock()

				// Update node's understanding of our interface addresses if they've changed
				if interfaceAddressesChanged || reflect.DeepEqual(n.localConfig.Settings.ExplicitAddresses, previousExplicitExternalAddresses) {
					previousExplicitExternalAddresses = n.localConfig.Settings.ExplicitAddresses

					// Consolidate explicit and detected interface addresses, removing duplicates.
					externalAddresses := make(map[[3]uint64]*ExternalAddress)
					for _, ip := range interfaceAddresses {
						for _, p := range ports {
							a := &ExternalAddress{
								InetAddress: InetAddress{
									IP:   ip,
									Port: p,
								},
								Permanent: false,
							}
							externalAddresses[a.key()] = a
						}
					}
					for _, a := range n.localConfig.Settings.ExplicitAddresses {
						externalAddresses[a.key()] = &a
					}

					if len(externalAddresses) > 0 {
						cAddrs := make([]C.ZT_InterfaceAddress, len(externalAddresses))
						cAddrCount := 0
						for _, a := range externalAddresses {
							makeSockaddrStorage(a.IP, a.Port, &(cAddrs[cAddrCount].address))
							if a.Permanent {
								cAddrs[cAddrCount].permanent = 1
							} else {
								cAddrs[cAddrCount].permanent = 0
							}
							cAddrCount++
						}
						C.ZT_Node_setInterfaceAddresses(unsafe.Pointer(n.zn), &cAddrs[0], C.uint(cAddrCount))
					} else {
						C.ZT_Node_setInterfaceAddresses(unsafe.Pointer(n.zn), nil, 0)
					}
				}

				// Trim log if it's gone over its size limit
				if n.localConfig.Settings.LogSizeMax > 0 && n.logW != nil {
					_ = n.logW.trim(n.localConfig.Settings.LogSizeMax*1024, 0.5, true)
				}

				n.localConfigLock.RUnlock()
				//////////////////////////////////////////////////////////////////////
			}
		}
	}()

	return n, nil
}

// Close closes this Node and frees its underlying C++ Node structures
func (n *Node) Close() {
	if atomic.SwapUint32(&n.running, 0) != 0 {
		if n.namedSocketApiServer != nil {
			_ = n.namedSocketApiServer.Close()
		}
		if n.tcpApiServer != nil {
			_ = n.tcpApiServer.Close()
		}

		C.ZT_GoNode_delete(n.gn)

		n.runWaitGroup.Wait()

		nodesByUserPtrLock.Lock()
		delete(nodesByUserPtr, uintptr(unsafe.Pointer(n)))
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
			nw.SetLocalSettings(&nc)
		}
	}

	if n.localConfig.Settings.PrimaryPort != lc.Settings.PrimaryPort || n.localConfig.Settings.SecondaryPort != lc.Settings.SecondaryPort {
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

// Peers retrieves a list of current peers
func (n *Node) Peers() []*Peer {
	var peers []*Peer
	pl := C.ZT_Node_peers(unsafe.Pointer(n.zn))
	if pl != nil {
		for i := uintptr(0); i < uintptr(pl.peerCount); i++ {
			p := (*C.ZT_Peer)(unsafe.Pointer(uintptr(unsafe.Pointer(pl.peers)) + (i * C.sizeof_ZT_Peer)))
			p2 := new(Peer)
			p2.Address = Address(p.address)
			p2.Identity, _ = newIdentityFromCIdentity(unsafe.Pointer(p.identity))
			p2.IdentityHash = hex.EncodeToString((*[48]byte)(unsafe.Pointer(&p.identityHash[0]))[:])
			p2.Version = [3]int{int(p.versionMajor), int(p.versionMinor), int(p.versionRev)}
			p2.Latency = int(p.latency)
			p2.Role = int(p.role)
			p2.Bootstrap = NewInetAddressFromSockaddr(unsafe.Pointer(&p.bootstrap))

			p2.Paths = make([]Path, 0, int(p.pathCount))
			for j := 0; j < len(p2.Paths); j++ {
				pt := &p.paths[j]
				if pt.alive != 0 {
					a := sockaddrStorageToUDPAddr(&pt.address)
					if a != nil {
						p2.Paths = append(p2.Paths, Path{
							IP:            a.IP,
							Port:          a.Port,
							LastSend:      int64(pt.lastSend),
							LastReceive:   int64(pt.lastReceive),
							TrustedPathID: uint64(pt.trustedPathId),
						})
					}
				}
			}

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

func (n *Node) pathCheck(ip net.IP) bool {
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

func (n *Node) pathLookup(id *Identity) (net.IP, int) {
	n.localConfigLock.RLock()
	defer n.localConfigLock.RUnlock()
	virt := n.localConfig.Virtual[id.address]
	if len(virt.Try) > 0 {
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
	case C.ZT_STATE_OBJECT_LOCATOR:
		fp = path.Join(n.basePath, "locator")
	case C.ZT_STATE_OBJECT_PEER:
		fp = path.Join(n.basePath, "peers.d")
		_ = os.Mkdir(fp, 0700)
		fp = path.Join(fp, fmt.Sprintf("%.10x.peer", id[0]))
		secret = true
	case C.ZT_STATE_OBJECT_NETWORK_CONFIG:
		fp = path.Join(n.basePath, "networks.d")
		_ = os.Mkdir(fp, 0755)
		fp = path.Join(fp, fmt.Sprintf("%.16x.conf", id[0]))
	case C.ZT_STATE_OBJECT_ROOTS:
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

// func (n *Node) handleUserMessage(origin *Identity, messageTypeID uint64, data []byte) {
// }

//////////////////////////////////////////////////////////////////////////////

// These are callbacks called by the core and GoGlue stuff to talk to the
// service. These launch goroutines to do their work where possible to
// avoid blocking anything in the core.

//export goPathCheckFunc
func goPathCheckFunc(gn, _ unsafe.Pointer, af C.int, ip unsafe.Pointer, _ C.int) C.int {
	nodesByUserPtrLock.RLock()
	node := nodesByUserPtr[uintptr(gn)]
	nodesByUserPtrLock.RUnlock()
	var nip net.IP
	if af == syscall.AF_INET {
		nip = ((*[4]byte)(ip))[:]
	} else if af == syscall.AF_INET6 {
		nip = ((*[16]byte)(ip))[:]
	} else {
		return 0
	}
	if node != nil && len(nip) > 0 && node.pathCheck(nip) {
		return 1
	}
	return 0
}

//export goPathLookupFunc
func goPathLookupFunc(gn unsafe.Pointer, _ C.uint64_t, _ int, identity, familyP, ipP, portP unsafe.Pointer) C.int {
	nodesByUserPtrLock.RLock()
	node := nodesByUserPtr[uintptr(gn)]
	nodesByUserPtrLock.RUnlock()
	if node == nil {
		return 0
	}

	id, err := newIdentityFromCIdentity(identity)
	if err != nil {
		return 0
	}

	ip, port := node.pathLookup(id)

	if len(ip) > 0 && port > 0 && port <= 65535 {
		ip4 := ip.To4()
		if len(ip4) == 4 {
			*((*C.int)(familyP)) = C.int(syscall.AF_INET)
			copy((*[4]byte)(ipP)[:], ip4)
			*((*C.int)(portP)) = C.int(port)
			return 1
		} else if len(ip) == 16 {
			*((*C.int)(familyP)) = C.int(syscall.AF_INET6)
			copy((*[16]byte)(ipP)[:], ip)
			*((*C.int)(portP)) = C.int(port)
			return 1
		}
	}

	return 0
}

//export goStateObjectPutFunc
func goStateObjectPutFunc(gn unsafe.Pointer, objType C.int, id, data unsafe.Pointer, len C.int) {
	id2 := *((*[2]uint64)(id))
	var data2 []byte
	if len > 0 {
		data2 = C.GoBytes(data, len)
	}

	nodesByUserPtrLock.RLock()
	node := nodesByUserPtr[uintptr(gn)]
	nodesByUserPtrLock.RUnlock()
	if node == nil {
		return
	}

	node.runWaitGroup.Add(1)
	go func() {
		if len < 0 {
			node.stateObjectDelete(int(objType), id2)
		} else {
			node.stateObjectPut(int(objType), id2, data2)
		}
		node.runWaitGroup.Done()
	}()
}

//export goStateObjectGetFunc
func goStateObjectGetFunc(gn unsafe.Pointer, objType C.int, id, dataP unsafe.Pointer) C.int {
	nodesByUserPtrLock.RLock()
	node := nodesByUserPtr[uintptr(gn)]
	nodesByUserPtrLock.RUnlock()
	if node == nil {
		return -1
	}
	*((*uintptr)(dataP)) = 0
	tmp, found := node.stateObjectGet(int(objType), *((*[2]uint64)(id)))
	if found && len(tmp) > 0 {
		cData := C.malloc(C.ulong(len(tmp))) // GoGlue sends free() to the core as the free function
		if uintptr(cData) == 0 {
			return -1
		}
		*((*uintptr)(dataP)) = uintptr(cData)
		return C.int(len(tmp))
	}
	return -1
}

//export goVirtualNetworkConfigFunc
func goVirtualNetworkConfigFunc(gn, _ unsafe.Pointer, nwid C.uint64_t, op C.int, conf unsafe.Pointer) {
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

			node.runWaitGroup.Add(1)
			go func() {
				network.updateConfig(&nc, nil)
				node.runWaitGroup.Done()
			}()
		}
	}
}

//export goZtEvent
func goZtEvent(gn unsafe.Pointer, eventType C.int, data unsafe.Pointer) {
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
	}
}
