// +build !windows

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
	"context"
	"net"
	"net/http"
	"os"
	"path"
	"time"
)

func createNamedSocketListener(basePath, name string) (net.Listener, error) {
	apiSockPath := path.Join(basePath, name)
	os.Remove(apiSockPath)
	return net.Listen("unix", apiSockPath)
}

func createNamedSocketHTTPClient(basePath, name string) (*http.Client, error) {
	apiSockPath := path.Join(basePath, name)
	return &http.Client{
		Timeout: 10 * time.Second,
		Transport: &http.Transport{
			DialContext: func(_ context.Context, _, _ string) (net.Conn, error) {
				return net.Dial("unix", apiSockPath)
			},
			DisableKeepAlives:  true,
			DisableCompression: true,
		},
	}, nil
}
