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
//#cgo LDFLAGS: ${SRCDIR}/../../../build/node/libzt_core.a ${SRCDIR}/../../../build/go/native/libzt_go_native.a -lc++
//#define ZT_CGO 1
//#include <stdint.h>
//#include "../../native/GoGlue.h"
//#if __has_include("../../../version.h")
//#include "../../../version.h"
//#else
//#define ZEROTIER_ONE_VERSION_MAJOR 255
//#define ZEROTIER_ONE_VERSION_MINOR 255
//#define ZEROTIER_ONE_VERSION_REVISION 255
//#define ZEROTIER_ONE_VERSION_BUILD 255
//#endif
import "C"
import (
	"net"
	"runtime"
	"sync"
	"sync/atomic"
	"unsafe"
)

const (
	// CoreVersionMajor is the major version of the ZeroTier core
	CoreVersionMajor int = C.ZEROTIER_ONE_VERSION_MAJOR

	// CoreVersionMinor is the minor version of the ZeroTier core
	CoreVersionMinor int = C.ZEROTIER_ONE_VERSION_MINOR

	// CoreVersionRevision is the revision of the ZeroTier core
	CoreVersionRevision int = C.ZEROTIER_ONE_VERSION_REVISION

	// CoreVersionBuild is the build version of the ZeroTier core
	CoreVersionBuild int = C.ZEROTIER_ONE_VERSION_BUILD
)

// Tap is an instance of an EthernetTap object
type Tap struct {
	tap           *C.ZT_GoTap
	networkStatus uint32
}

// Node is an instance of a ZeroTier node
type Node struct {
	gn *C.ZT_GoNode
	zn *C.ZT_Node

	taps     map[uint64]*Tap
	tapsLock sync.RWMutex

	online  uint32
	running uint32
}

// NewNode creates and initializes a new instance of the ZeroTier node service
func NewNode() *Node {
	n := new(Node)

	gnRawAddr := uintptr(unsafe.Pointer(n.gn))
	nodesByUserPtrLock.Lock()
	nodesByUserPtr[gnRawAddr] = n
	nodesByUserPtrLock.Unlock()
	runtime.SetFinalizer(n, func(obj interface{}) { // make sure this always happens
		nodesByUserPtrLock.Lock()
		delete(nodesByUserPtr, gnRawAddr)
		nodesByUserPtrLock.Unlock()
	})

	n.running = 1

	return n
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

func (n *Node) pathCheck(ztAddress uint64, af int, ip net.IP, port int) bool {
	return true
}

func (n *Node) pathLookup(ztAddress uint64) (net.IP, int) {
	return nil, 0
}

func (n *Node) stateObjectPut(objType int, id [2]uint64, data []byte) {
}

func (n *Node) stateObjectDelete(objType int, id [2]uint64) {
}

func (n *Node) stateObjectGet(objType int, id [2]uint64) ([]byte, bool) {
	return nil, false
}

func (n *Node) handleTrace(traceMessage string) {
}

func (n *Node) handleUserMessage(originAddress, messageTypeID uint64, data []byte) {
}

func (n *Node) handleRemoteTrace(originAddress uint64, dictData []byte) {
}

func (n *Node) handleNetworkConfigUpdate(op int, config *C.ZT_VirtualNetworkConfig) int {
	return 0
}
