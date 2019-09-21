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
//#define ZT_CGO 1
//#include <stdint.h>
//#include <stdlib.h>
//#include <string.h>
//#include "../../native/GoGlue.h"
import "C"

import (
	"net"
	"sync"
	"sync/atomic"
	"unsafe"
)

const (
	afInet  = C.AF_INET
	afInet6 = C.AF_INET6

	networkStatusRequestingConfiguration = C.ZT_NETWORK_STATUS_REQUESTING_CONFIGURATION
	networkStatusOK                      = C.ZT_NETWORK_STATUS_OK
	networkStatusAccessDenied            = C.ZT_NETWORK_STATUS_ACCESS_DENIED
	networkStatusNotFound                = C.ZT_NETWORK_STATUS_NOT_FOUND
	networkStatusPortError               = C.ZT_NETWORK_STATUS_PORT_ERROR
	networkStatusClientTooOld            = C.ZT_NETWORK_STATUS_CLIENT_TOO_OLD
)

var (
	nodesByUserPtr     map[uintptr]*Node
	nodesByUserPtrLock sync.RWMutex
)

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
	ip4 := ip.To4()
	if len(ip4) == 4 {
		*((*C.int)(familyP)) = afInet
		copy((*[4]byte)(ipP)[:], ip4)
		*((*C.int)(portP)) = C.int(port)
	} else if len(ip) == 16 {
		*((*C.int)(familyP)) = afInet6
		copy((*[16]byte)(ipP)[:], ip)
		*((*C.int)(portP)) = C.int(port)
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
func goVirtualNetworkConfigFunc(gn, tapP unsafe.Pointer, nwid C.uint64_t, op C.int, conf unsafe.Pointer) C.int {
	nodesByUserPtrLock.RLock()
	node := nodesByUserPtr[uintptr(gn)]
	nodesByUserPtrLock.RUnlock()
	if node == nil {
		return 255
	}
	return C.int(node.handleNetworkConfigUpdate(int(op), (*C.ZT_VirtualNetworkConfig)(conf)))
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
	case C.ZT_EVENT_USER_MESSAGE:
		um := (*C.ZT_UserMessage)(data)
		node.handleUserMessage(uint64(um.origin), uint64(um.typeId), C.GoBytes(um.data, C.int(um.length)))
	case C.ZT_EVENT_REMOTE_TRACE:
		rt := (*C.ZT_RemoteTrace)(data)
		node.handleRemoteTrace(uint64(rt.origin), C.GoBytes(unsafe.Pointer(rt.data), C.int(rt.len)))
	}
}
