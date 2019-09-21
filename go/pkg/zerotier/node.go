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
	"errors"
	"fmt"
	"io/ioutil"
	"net"
	"os"
	"path"
	"sync"
	"sync/atomic"
	"time"
	"unsafe"

	acl "github.com/hectane/go-acl"
)

//#cgo CFLAGS: -O3
//#cgo LDFLAGS: ${SRCDIR}/../../../build/node/libzt_core.a ${SRCDIR}/../../../build/osdep/libzt_osdep.a ${SRCDIR}/../../../build/go/native/libzt_go_native.a -lc++ -lpthread
//#define ZT_CGO 1
//#include <stdint.h>
//#include <stdlib.h>
//#include <string.h>
//#include "../../native/GoGlue.h"
import "C"

// Network status states
const (
	NetworkStatusRequestConfiguration int = C.ZT_NETWORK_STATUS_REQUESTING_CONFIGURATION
	NetworkStatusOK                   int = C.ZT_NETWORK_STATUS_OK
	NetworkStatusAccessDenied         int = C.ZT_NETWORK_STATUS_ACCESS_DENIED
	NetworkStatusNotFound             int = C.ZT_NETWORK_STATUS_NOT_FOUND
	NetworkStatusPortError            int = C.ZT_NETWORK_STATUS_PORT_ERROR
	NetworkStatusClientTooOld         int = C.ZT_NETWORK_STATUS_CLIENT_TOO_OLD
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

	nw := &Network{
		id: NetworkID(nwid),
		config: NetworkConfig{
			ID:          NetworkID(nwid),
			Status:      NetworkStatusRequestConfiguration,
			LastUpdated: time.Now(),
			Enabled:     true,
		},
		tap: &nativeTap{tap: unsafe.Pointer(ntap), enabled: 1},
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

func (n *Node) handleNetworkConfigUpdate(nwid uint64, op int, config *C.ZT_VirtualNetworkConfig) int {
	return 0
}
