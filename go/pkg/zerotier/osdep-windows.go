// +build windows

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
	"net"

	winio "github.com/Microsoft/go-winio"
)

const windowsAPISocketPathPrefix = "\\\\.\\pipe\\zerotier_"

func createNamedSocketListener(basePath, name string) (net.Listener, error) {
	winio.ListenPipe(windowsAPISocketPathPrefix+name, nil)
}
