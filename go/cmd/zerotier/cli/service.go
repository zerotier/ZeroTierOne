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

package cli

import (
	"fmt"
	"os"
	"os/signal"
	"syscall"

	"zerotier/pkg/zerotier"
)

// Service is "zerotier service ..."
func Service(basePath, authToken string, args []string) {
	if len(args) > 0 {
		Help()
		os.Exit(1)
	}

	node, err := zerotier.NewNode(basePath)
	if err != nil {
		fmt.Println("FATAL: error initializing node: " + err.Error())
		os.Exit(1)
	}

	osSignalChannel := make(chan os.Signal, 2)
	signal.Notify(osSignalChannel, syscall.SIGTERM, syscall.SIGQUIT, syscall.SIGINT, syscall.SIGBUS)
	signal.Ignore(syscall.SIGUSR1, syscall.SIGUSR2)
	<-osSignalChannel
	node.Close()
	os.Exit(0)
}
