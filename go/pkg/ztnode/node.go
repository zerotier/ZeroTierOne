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

package ztnode

//#cgo CFLAGS: -O3
//#cgo LDFLAGS: ${SRCDIR}/../../../build/node/libzt_core.a -lc++
//#define ZT_CGO 1
//#include <stdint.h>
//#include "../../../include/ZeroTierCore.h"
//#if __has_include("../../../version.h")
//#include "../../../version.h"
//#else
//#define ZEROTIER_ONE_VERSION_MAJOR 255
//#define ZEROTIER_ONE_VERSION_MINOR 255
//#define ZEROTIER_ONE_VERSION_REVISION 255
//#define ZEROTIER_ONE_VERSION_BUILD 255
//#endif
import "C"

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

// Node is an instance of a ZeroTier node
type Node struct {
	node *C.ZT_Node
}
