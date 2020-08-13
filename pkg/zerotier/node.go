/*
 * Copyright (C)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

package zerotier

// #cgo CFLAGS: -I${SRCDIR}/../../build/core
// #include "../../serviceiocore/GoGlue.h"
import "C"

import (
	"bytes"
	"errors"
	"fmt"
	"io"
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

const (
	NetworkIDStringLength     = 16
	NetworkIDLength           = 8
	AddressStringLength       = 10
	AddressLength             = 5
	DefaultPort               = int(C.ZT_DEFAULT_PORT)
	DefaultRawIPProto         = int(C.ZT_DEFAULT_RAW_IP_PROTOCOL)
	DefaultEthernetProto      = int(C.ZT_DEFAULT_ETHERNET_PROTOCOL)
	NetworkMaxShortNameLength = int(C.ZT_MAX_NETWORK_SHORT_NAME_LENGTH)

	NetworkStatusRequestingConfiguration = int(C.ZT_NETWORK_STATUS_REQUESTING_CONFIGURATION)
	NetworkStatusOK                      = int(C.ZT_NETWORK_STATUS_OK)
	NetworkStatusAccessDenied            = int(C.ZT_NETWORK_STATUS_ACCESS_DENIED)
	NetworkStatusNotFound                = int(C.ZT_NETWORK_STATUS_NOT_FOUND)

	NetworkTypePrivate = int(C.ZT_NETWORK_TYPE_PRIVATE)
	NetworkTypePublic  = int(C.ZT_NETWORK_TYPE_PUBLIC)

	networkConfigOpUp        = int(C.ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_UP)
	networkConfigOpUpdate    = int(C.ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE)
	defaultVirtualNetworkMTU = int(C.ZT_DEFAULT_MTU)
	maxCNodeRefs             = 8 // perfectly fine to increase this
)

var (
	PlatformDefaultHomePath string
	CoreVersionMajor        int
	CoreVersionMinor        int
	CoreVersionRevision     int
	CoreVersionBuild        int

	// cNodeRefs maps an index to a *Node
	cNodeRefs [maxCNodeRefs]*Node

	// cNodeRefUsed maps an index to whether or not the corresponding cNodeRefs[] entry is used.
	// This is accessed atomically to provide a really fast way to gate cNodeRefs.
	cNodeRefUsed [maxCNodeRefs]uintptr
)

func init() {
	PlatformDefaultHomePath = C.GoString(C.ZT_PLATFORM_DEFAULT_HOMEPATH)
	var vMaj, vMin, vRev, vBuild C.int
	C.ZT_version(&vMaj, &vMin, &vRev, &vBuild)
	CoreVersionMajor = int(vMaj)
	CoreVersionMinor = int(vMin)
	CoreVersionRevision = int(vRev)
	CoreVersionBuild = int(vBuild)
}

// Node is an instance of a virtual port on the global switch.
type Node struct {
	// Time this node was created
	startupTime int64

	// cPtr is an arbitrary pseudo-pointer given to the core to map back to our Go object.
	// This is an index into the cNodeRefs array.
	cPtr uintptr

	networks                    map[NetworkID]*Network
	networksByMAC               map[MAC]*Network // locked by networksLock
	networksLock                sync.RWMutex
	localInterfaceAddresses     map[string]net.IP
	localInterfaceAddressesLock sync.Mutex
	running                     uintptr // atomic flag
	online                      uintptr // atomic flag
	basePath                    string
	peersPath                   string
	certsPath                   string
	networksPath                string
	localConfigPath             string
	infoLogPath                 string
	errorLogPath                string
	localConfig                 *LocalConfig
	previousLocalConfig         *LocalConfig
	localConfigLock             sync.RWMutex
	infoLogW                    *sizeLimitWriter
	errLogW                     *sizeLimitWriter
	traceLogW                   io.Writer
	infoLog                     *log.Logger
	errLog                      *log.Logger
	traceLog                    *log.Logger
	namedSocketAPIServer        *http.Server
	tcpAPIServer                *http.Server

	// gn is the GoNode instance, see serviceiocore/GoNode.hpp
	gn *C.ZT_GoNode

	// zn is the underlying ZT_Node (ZeroTier::Node) instance
	zn unsafe.Pointer

	// id is the identity of this node (includes private key)
	id *Identity

	// runWaitGroup is used to wait for all node goroutines on shutdown.
	// Any new goroutine is tracked via this wait group so node shutdown can
	// itself wait until all goroutines have exited.
	runWaitGroup sync.WaitGroup
}

// NewNode creates and initializes a new instance of the ZeroTier node service
func NewNode(basePath string) (n *Node, err error) {
	n = new(Node)
	n.startupTime = TimeMs()

	// Register this with the cNodeRefs lookup array and set up a deferred function
	// to unregister this if we exit before the end of the constructor such as by
	// returning an error.
	cPtr := -1
	for i := 0; i < maxCNodeRefs; i++ {
		if atomic.CompareAndSwapUintptr(&cNodeRefUsed[i], 0, 1) {
			cNodeRefs[i] = n
			cPtr = i
			n.cPtr = uintptr(i)
			break
		}
	}
	if cPtr < 0 {
		return nil, ErrInternal
	}
	defer func() {
		if cPtr >= 0 {
			atomic.StoreUintptr(&cNodeRefUsed[cPtr], 0)
			cNodeRefs[cPtr] = nil
		}
	}()

	n.networks = make(map[NetworkID]*Network)
	n.networksByMAC = make(map[MAC]*Network)
	n.localInterfaceAddresses = make(map[string]net.IP)
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
	n.certsPath = path.Join(basePath, "certs.d")
	_ = os.MkdirAll(n.certsPath, 0755)
	n.networksPath = path.Join(basePath, "networks.d")
	_ = os.MkdirAll(n.networksPath, 0755)
	n.localConfigPath = path.Join(basePath, "local.conf")

	_, isTotallyNewNode := os.Stat(path.Join(basePath, "identity.secret"))
	n.localConfig = new(LocalConfig)
	err = n.localConfig.Read(n.localConfigPath, true, isTotallyNewNode != nil)
	if err != nil {
		return
	}

	n.infoLogPath = path.Join(basePath, "info.log")
	n.errorLogPath = path.Join(basePath, "error.log")
	if n.localConfig.Settings.LogSizeMax >= 0 {
		n.infoLogW, err = sizeLimitWriterOpen(n.infoLogPath)
		if err != nil {
			return
		}
		n.errLogW, err = sizeLimitWriterOpen(n.errorLogPath)
		if err != nil {
			return
		}
		n.infoLog = log.New(n.infoLogW, "", log.LstdFlags)
		n.errLog = log.New(n.errLogW, "", log.LstdFlags)
	} else {
		n.infoLog = nullLogger
		n.errLog = nullLogger
	}

	portsChanged := false
	portCheckCount := 0
	origPort := n.localConfig.Settings.PrimaryPort
	for portCheckCount < 256 {
		portCheckCount++
		if checkPort(n.localConfig.Settings.PrimaryPort) {
			if n.localConfig.Settings.PrimaryPort != origPort {
				n.infoLog.Printf("primary port %d unavailable, found port %d and saved in local.conf", origPort, n.localConfig.Settings.PrimaryPort)
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
					n.infoLog.Printf("secondary port %d unavailable, found port %d (port search enabled)", origPort, n.localConfig.Settings.SecondaryPort)
				}
				break
			}
			n.infoLog.Printf("secondary port %d unavailable, trying a random port (port search enabled)", n.localConfig.Settings.SecondaryPort)
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

	n.namedSocketAPIServer, n.tcpAPIServer, err = createAPIServer(basePath, n)
	if err != nil {
		n.infoLog.Printf("FATAL: unable to start API server: %s", err.Error())
		return nil, err
	}

	cPath := cStr(basePath)
	n.gn = C.ZT_GoNode_new((*C.char)(unsafe.Pointer(&cPath[0])), C.uintptr_t(n.cPtr))
	if n.gn == nil {
		n.infoLog.Println("FATAL: node initialization failed")
		return nil, ErrNodeInitFailed
	}
	n.zn = unsafe.Pointer(C.ZT_GoNode_getNode(n.gn))
	n.id, err = newIdentityFromCIdentity(C.ZT_Node_identity(n.zn))
	if err != nil {
		n.infoLog.Printf("FATAL: error obtaining node's identity")
		C.ZT_GoNode_delete(n.gn)
		return nil, err
	}

	n.runWaitGroup.Add(1)
	go func() {
		defer n.runWaitGroup.Done()
		lastMaintenanceRun := int64(0)
		for atomic.LoadUintptr(&n.running) != 0 {
			time.Sleep(250 * time.Millisecond)
			nowS := time.Now().Unix()
			if (nowS - lastMaintenanceRun) >= 30 {
				lastMaintenanceRun = nowS
				n.runMaintenance()
			}
			time.Sleep(250 * time.Millisecond)
		}
	}()

	// Stop deferred cPtr table cleanup function from deregistering this instance
	cPtr = -1

	return n, nil
}

// Close closes this Node and frees its underlying C++ Node structures
func (n *Node) Close() {
	if atomic.SwapUintptr(&n.running, 0) != 0 {
		if n.namedSocketAPIServer != nil {
			_ = n.namedSocketAPIServer.Close()
		}
		if n.tcpAPIServer != nil {
			_ = n.tcpAPIServer.Close()
		}

		C.ZT_GoNode_delete(n.gn)

		n.runWaitGroup.Wait()

		cNodeRefs[n.cPtr] = nil
		atomic.StoreUintptr(&cNodeRefUsed[n.cPtr], 0)
	}
}

// Address returns this node's address
func (n *Node) Address() Address { return n.id.address }

// Identity returns this node's identity (including secret portion)
func (n *Node) Identity() *Identity { return n.id }

// Online returns true if this node can reach something
func (n *Node) Online() bool { return atomic.LoadUintptr(&n.online) != 0 }

// LocalInterfaceAddresses are external IPs belonging to physical interfaces on this machine
func (n *Node) LocalInterfaceAddresses() []net.IP {
	n.localInterfaceAddressesLock.Lock()
	defer n.localInterfaceAddressesLock.Unlock()
	var ea []net.IP
	for _, a := range n.localInterfaceAddresses {
		ea = append(ea, a)
	}
	sort.Slice(ea, func(a, b int) bool { return bytes.Compare(ea[a], ea[b]) < 0 })
	return ea
}

// LocalConfig gets this node's local configuration
func (n *Node) LocalConfig() *LocalConfig {
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

	if n.localConfig.Settings.PrimaryPort != lc.Settings.PrimaryPort || n.localConfig.Settings.SecondaryPort != lc.Settings.SecondaryPort || n.localConfig.Settings.LogSizeMax != lc.Settings.LogSizeMax {
		restartRequired = true
	}

	n.previousLocalConfig = n.localConfig
	n.localConfig = lc

	return
}

// Join a network.
// If tap is nil, the default system tap for this OS/platform is used (if available).
func (n *Node) Join(nwid NetworkID, controllerFingerprint *Fingerprint, settings *NetworkLocalSettings, tap Tap) (*Network, error) {
	n.networksLock.RLock()
	defer n.networksLock.RUnlock()

	if nw, have := n.networks[nwid]; have {
		n.infoLog.Printf("join network %.16x ignored: already a member", nwid)
		if settings != nil {
			go nw.SetLocalSettings(settings) // "go" this to avoid possible deadlocks
		}
		return nw, nil
	}

	if tap != nil {
		panic("non-native taps not yet implemented")
	}
	var fp *C.ZT_Fingerprint
	if controllerFingerprint != nil {
		fp = controllerFingerprint.cFingerprint()
	}
	ntap := C.ZT_GoNode_join(n.gn, C.uint64_t(nwid), fp)
	if ntap == nil {
		n.infoLog.Printf("join network %.16x failed: tap device failed to initialize (check drivers / kernel modules)", uint64(nwid))
		return nil, ErrTapInitFailed
	}

	nw, err := newNetwork(n, nwid, &nativeTap{tap: unsafe.Pointer(ntap), enabled: 1})
	if err != nil {
		n.infoLog.Printf("join network %.16x failed: network failed to initialize: %s", nwid, err.Error())
		C.ZT_GoNode_leave(n.gn, C.uint64_t(nwid))
		return nil, err
	}
	n.networks[nwid] = nw
	if settings != nil {
		go nw.SetLocalSettings(settings)
	}

	return nw, nil
}

// Leave a network.
func (n *Node) Leave(nwid NetworkID) error {
	n.networksLock.Lock()
	nw := n.networks[nwid]
	delete(n.networks, nwid)
	n.networksLock.Unlock()

	if nw != nil {
		n.infoLog.Printf("leaving network %.16x", nwid)
		nw.leaving()
		C.ZT_GoNode_leave(n.gn, C.uint64_t(nwid))
	}
	return nil
}

// Network looks up a network by ID or returns nil if not joined
func (n *Node) Network(nwid NetworkID) *Network {
	n.networksLock.RLock()
	nw := n.networks[nwid]
	n.networksLock.RUnlock()
	return nw
}

// Networks returns a list of networks that this node has joined
func (n *Node) Networks() []*Network {
	n.networksLock.RLock()
	defer n.networksLock.RUnlock()
	var nws []*Network
	for _, nw := range n.networks {
		nws = append(nws, nw)
	}
	return nws
}

// Peers retrieves a list of current peers
func (n *Node) Peers() []*Peer {
	var peers []*Peer
	pl := C.ZT_Node_peers(n.zn)
	if pl != nil {
		defer C.ZT_freeQueryResult(unsafe.Pointer(pl))
		for i := uintptr(0); i < uintptr(pl.peerCount); i++ {
			p, _ := newPeerFromCPeer((*C.ZT_Peer)(unsafe.Pointer(uintptr(unsafe.Pointer(pl.peers)) + (i * C.sizeof_ZT_Peer))))
			if p != nil {
				peers = append(peers, p)
			}
		}
	}
	sort.Slice(peers, func(a, b int) bool {
		return peers[a].Address < peers[b].Address
	})
	return peers
}

// Peer looks up a single peer by address or full fingerprint.
// The fpOrAddress parameter may be either. If it is neither nil is returned.
// A nil pointer is returned if nothing is found.
func (n *Node) Peer(fpOrAddress interface{}) *Peer {
	fp, _ := fpOrAddress.(*Fingerprint)
	if fp == nil {
		a, _ := fpOrAddress.(*Address)
		if a == nil {
			return nil
		}
		fp = &Fingerprint{Address: *a}
	}
	pl := C.ZT_Node_peers(n.zn)
	if pl != nil {
		defer C.ZT_freeQueryResult(unsafe.Pointer(pl))
		for i := uintptr(0); i < uintptr(pl.peerCount); i++ {
			p, _ := newPeerFromCPeer((*C.ZT_Peer)(unsafe.Pointer(uintptr(unsafe.Pointer(pl.peers)) + (i * C.sizeof_ZT_Peer))))
			if p != nil && p.Identity.Fingerprint().BestSpecificityEquals(fp) {
				return p
			}
		}
	}
	return nil
}

// AddPeer adds a peer by explicit identity.
func (n *Node) AddPeer(id *Identity) error {
	if id == nil {
		return ErrInvalidParameter
	}
	rc := C.ZT_Node_addPeer(n.zn, nil, id.cIdentity())
	if rc != 0 {
		return ErrInvalidParameter
	}
	return nil
}

// TryPeer attempts to contact a peer at a given explicit endpoint.
// The peer may be identified by an Address or a full Fingerprint. Any other
// type for fpOrAddress will return false.
func (n *Node) TryPeer(fpOrAddress interface{}, ep *Endpoint, retries int) bool {
	if ep == nil {
		return false
	}
	fp, _ := fpOrAddress.(*Fingerprint)
	if fp == nil {
		a, _ := fpOrAddress.(*Address)
		if a == nil {
			return false
		}
		fp = &Fingerprint{Address: *a}
	}
	return C.ZT_Node_tryPeer(n.zn, nil, fp.cFingerprint(), &ep.cep, C.int(retries)) != 0
}

// ListCertificates lists certificates and their corresponding local trust flags.
func (n *Node) ListCertificates() (certs []LocalCertificate, err error) {
	cl := C.ZT_Node_listCertificates(n.zn)
	if cl != nil {
		defer C.ZT_freeQueryResult(unsafe.Pointer(cl))
		for i := uintptr(0); i < uintptr(cl.certCount); i++ {
			c := newCertificateFromCCertificate(unsafe.Pointer(uintptr(unsafe.Pointer(cl.certs)) + (i * pointerSize)))
			if c != nil {
				lt := *((*C.uint)(unsafe.Pointer(uintptr(unsafe.Pointer(cl.localTrust)) + (i * C.sizeof_int))))
				certs = append(certs, LocalCertificate{Certificate: c, LocalTrust: uint(lt)})
			}
		}
	}
	return
}

// AddCertificate adds a certificate to this node's local certificate store (after verification).
func (n *Node) AddCertificate(cert *Certificate, localTrust uint) error {
	ccert := cert.cCertificate()
	defer deleteCCertificate(ccert)
	return certificateErrorToError(int(C.ZT_Node_addCertificate(n.zn, nil, C.int64_t(TimeMs()), C.uint(localTrust), (*C.ZT_Certificate)(ccert), nil, 0)))
}

// DeleteCertificate deletes a certificate from this node's local certificate store.
func (n *Node) DeleteCertificate(serialNo []byte) error {
	if len(serialNo) != CertificateSerialNoSize {
		return ErrInvalidParameter
	}
	C.ZT_Node_deleteCertificate(n.zn, nil, unsafe.Pointer(&serialNo[0]))
	return nil
}

// -------------------------------------------------------------------------------------------------------------------

func (n *Node) runMaintenance() {
	n.localConfigLock.RLock()
	defer n.localConfigLock.RUnlock()

	// Get local physical interface addresses, excluding blacklisted and
	// ZeroTier-created interfaces.
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
					if ipn != nil && len(ipn.IP) > 0 && !ipn.IP.IsLoopback() && !ipn.IP.IsMulticast() && !ipn.IP.IsInterfaceLocalMulticast() && !ipn.IP.IsLinkLocalMulticast() && !ipn.IP.IsLinkLocalUnicast() {
						isTemporary := false
						if len(ipn.IP) == 16 {
							var ss C.struct_sockaddr_storage
							if makeSockaddrStorage(ipn.IP, 0, &ss) {
								cIfName := C.CString(i.Name)
								if C.ZT_isTemporaryV6Address(cIfName, &ss) != 0 {
									isTemporary = true
								}
								C.free(unsafe.Pointer(cIfName))
							}
						}
						if !isTemporary {
							interfaceAddresses[ipn.IP.String()] = ipn.IP
						}
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
	n.localInterfaceAddressesLock.Lock()
	for astr, ipn := range interfaceAddresses {
		if _, alreadyKnown := n.localInterfaceAddresses[astr]; !alreadyKnown {
			interfaceAddressesChanged = true
			ipCstr := C.CString(ipn.String())
			for pn, p := range ports {
				n.infoLog.Printf("UDP binding to port %d on interface %s", p, astr)
				primary := C.int(0)
				if pn == 0 {
					primary = 1
				}
				C.ZT_GoNode_phyStartListen(n.gn, nil, ipCstr, C.int(p), primary)
			}
			C.free(unsafe.Pointer(ipCstr))
		}
	}
	for astr, ipn := range n.localInterfaceAddresses {
		if _, stillPresent := interfaceAddresses[astr]; !stillPresent {
			interfaceAddressesChanged = true
			ipCstr := C.CString(ipn.String())
			for _, p := range ports {
				n.infoLog.Printf("UDP closing socket bound to port %d on interface %s", p, astr)
				C.ZT_GoNode_phyStopListen(n.gn, nil, ipCstr, C.int(p))
			}
			C.free(unsafe.Pointer(ipCstr))
		}
	}
	n.localInterfaceAddresses = interfaceAddresses
	n.localInterfaceAddressesLock.Unlock()

	// Update node's interface address list if detected or configured addresses have changed.
	if interfaceAddressesChanged || n.previousLocalConfig == nil || !reflect.DeepEqual(n.localConfig.Settings.ExplicitAddresses, n.previousLocalConfig.Settings.ExplicitAddresses) {
		var cAddrs []C.ZT_InterfaceAddress
		externalAddresses := make(map[[3]uint64]*InetAddress)
		for _, a := range n.localConfig.Settings.ExplicitAddresses {
			ak := a.key()
			if _, have := externalAddresses[ak]; !have {
				externalAddresses[ak] = &a
				cAddrs = append(cAddrs, C.ZT_InterfaceAddress{})
				makeSockaddrStorage(a.IP, a.Port, &(cAddrs[len(cAddrs)-1].address))
				cAddrs[len(cAddrs)-1].permanent = 1 // explicit addresses are permanent, meaning they can be put in a locator
			}
		}
		for _, ip := range interfaceAddresses {
			for _, p := range ports {
				a := InetAddress{IP: ip, Port: p}
				ak := a.key()
				if _, have := externalAddresses[ak]; !have {
					externalAddresses[ak] = &a
					cAddrs = append(cAddrs, C.ZT_InterfaceAddress{})
					makeSockaddrStorage(a.IP, a.Port, &(cAddrs[len(cAddrs)-1].address))
					cAddrs[len(cAddrs)-1].permanent = 0
				}
			}
		}

		if len(cAddrs) > 0 {
			C.ZT_Node_setInterfaceAddresses(n.zn, &cAddrs[0], C.uint(len(cAddrs)))
		} else {
			C.ZT_Node_setInterfaceAddresses(n.zn, nil, 0)
		}
	}

	// Trim infoLog if it's gone over its size limit
	if n.localConfig.Settings.LogSizeMax > 0 && n.infoLogW != nil {
		_ = n.infoLogW.trim(n.localConfig.Settings.LogSizeMax*1024, 0.5, true)
	}
}

func (n *Node) multicastSubscribe(nwid uint64, mg *MulticastGroup) {
	C.ZT_Node_multicastSubscribe(n.zn, nil, C.uint64_t(nwid), C.uint64_t(mg.MAC), C.ulong(mg.ADI))
}

func (n *Node) multicastUnsubscribe(nwid uint64, mg *MulticastGroup) {
	C.ZT_Node_multicastUnsubscribe(n.zn, C.uint64_t(nwid), C.uint64_t(mg.MAC), C.ulong(mg.ADI))
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

func (n *Node) makeStateObjectPath(objType int, id []uint64) (string, bool) {
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
		_ = os.Mkdir(n.peersPath, 0700)
		fp = path.Join(n.peersPath, fmt.Sprintf("%.10x.peer", id[0]))
		secret = true
	case C.ZT_STATE_OBJECT_NETWORK_CONFIG:
		_ = os.Mkdir(n.networksPath, 0755)
		fp = path.Join(n.networksPath, fmt.Sprintf("%.16x.conf", id[0]))
	case C.ZT_STATE_OBJECT_TRUST_STORE:
		fp = path.Join(n.basePath, "truststore")
	case C.ZT_STATE_OBJECT_CERT:
		_ = os.Mkdir(n.certsPath, 0755)
		fp = path.Join(n.certsPath, Base32StdLowerCase.EncodeToString((*[48]byte)(unsafe.Pointer(&id[0]))[:]))
	}
	return fp, secret
}

func (n *Node) stateObjectPut(objType int, id []uint64, data []byte) {
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

func (n *Node) stateObjectDelete(objType int, id []uint64) {
	fp, _ := n.makeStateObjectPath(objType, id)
	if len(fp) > 0 {
		_ = os.Remove(fp)
	}
}

func (n *Node) stateObjectGet(objType int, id []uint64) ([]byte, bool) {
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
		n.infoLog.Print("TRACE: " + traceMessage)
	}
}

// These are callbacks called by the core and GoGlue stuff to talk to the
// service. These launch goroutines to do their work where possible to
// avoid blocking anything in the core.

//export goPathCheckFunc
func goPathCheckFunc(gn, _ unsafe.Pointer, af C.int, ip unsafe.Pointer, _ C.int) C.int {
	node := cNodeRefs[uintptr(gn)]
	if node == nil {
		return 0
	}

	var nip net.IP
	if af == syscall.AF_INET {
		nip = ((*[4]byte)(ip))[:]
	} else if af == syscall.AF_INET6 {
		nip = ((*[16]byte)(ip))[:]
	} else {
		return 0
	}
	if len(nip) > 0 && node.pathCheck(nip) {
		return 1
	}

	return 0
}

//export goPathLookupFunc
func goPathLookupFunc(gn unsafe.Pointer, _ C.uint64_t, _ int, identity, familyP, ipP, portP unsafe.Pointer) C.int {
	node := cNodeRefs[uintptr(gn)]
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
	node := cNodeRefs[uintptr(gn)]
	if node == nil {
		return
	}

	// NOTE: this is unsafe and depends on node.stateObjectDelete() and node.stateObjectPut()
	// not accessing beyond the expected number of elements in the id.
	id2 := (*[6]uint64)(id)
	var data2 []byte
	if len > 0 {
		data2 = C.GoBytes(data, len)
	}

	if len < 0 {
		node.stateObjectDelete(int(objType), id2[:])
	} else {
		node.stateObjectPut(int(objType), id2[:], data2)
	}
}

//export goStateObjectGetFunc
func goStateObjectGetFunc(gn unsafe.Pointer, objType C.int, id, dataP unsafe.Pointer) C.int {
	node := cNodeRefs[uintptr(gn)]
	if node == nil {
		return -1
	}

	*((*uintptr)(dataP)) = 0
	tmp, found := node.stateObjectGet(int(objType), ((*[6]uint64)(id))[:])
	if found && len(tmp) > 0 {
		cData := C.ZT_malloc(C.ulong(len(tmp))) // GoGlue sends free() to the core as the free function
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
	node := cNodeRefs[uintptr(gn)]
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
					_, bits := a.Mask.Size()
					nc.AssignedAddresses = append(nc.AssignedAddresses, InetAddress{IP: a.IP, Port: bits})
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
	node := cNodeRefs[uintptr(gn)]
	if node == nil {
		return
	}

	switch eventType {
	case C.ZT_EVENT_OFFLINE:
		atomic.StoreUintptr(&node.online, 0)
	case C.ZT_EVENT_ONLINE:
		atomic.StoreUintptr(&node.online, 1)
	}
}
