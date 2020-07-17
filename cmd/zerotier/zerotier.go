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

package main

import (
	"flag"
	"fmt"
	"io/ioutil"
	"os"
	"path"
	"runtime"
	"runtime/debug"
	"strings"

	"zerotier/cmd/zerotier/cli"
	"zerotier/pkg/zerotier"
)

func getAuthTokenPaths(basePath string) (p []string) {
	p = append(p, path.Join(basePath, "authtoken.secret"))
	userHome, _ := os.UserHomeDir()
	if len(userHome) > 0 {
		if runtime.GOOS == "darwin" {
			p = append(p, path.Join(userHome, "Library", "Application Support", "ZeroTier", "authtoken.secret"))
			p = append(p, path.Join(userHome, "Library", "Application Support", "ZeroTier", "One", "authtoken.secret"))
		}
		p = append(p, path.Join(userHome, ".zerotierauth"))
		p = append(p, path.Join(userHome, ".zeroTierOneAuthToken"))
	}
	return p
}

func authTokenRequired(basePath, tflag, tTflag string) string {
	authTokenPaths := getAuthTokenPaths(basePath)
	var authToken string
	if len(tflag) > 0 {
		at, err := ioutil.ReadFile(tflag)
		if err != nil || len(at) == 0 {
			fmt.Println("FATAL: unable to read local service API authorization token from " + tflag)
			os.Exit(1)
		}
		authToken = string(at)
	} else if len(tTflag) > 0 {
		authToken = tTflag
	} else {
		for _, p := range authTokenPaths {
			tmp, _ := ioutil.ReadFile(p)
			if len(tmp) > 0 {
				authToken = string(tmp)
				break
			}
		}
		if len(authToken) == 0 {
			fmt.Println("FATAL: unable to read local service API authorization token from any of:")
			for _, p := range authTokenPaths {
				fmt.Println("  " + p)
			}
			os.Exit(1)
		}
	}
	authToken = strings.TrimSpace(authToken)
	if len(authToken) == 0 {
		fmt.Println("FATAL: unable to read API authorization token from command line or any filesystem location.")
		os.Exit(1)
	}
	return authToken
}

func main() {
	// Reduce Go's thread and memory footprint. This would slow things down if the Go code
	// were doing a lot, but it's not. It just manages the core and is not directly involved
	// in pushing a lot of packets around. If that ever changes this should be adjusted.
	runtime.GOMAXPROCS(1)
	debug.SetGCPercent(15)

	globalOpts := flag.NewFlagSet("global", flag.ContinueOnError)
	hflag := globalOpts.Bool("h", false, "") // support -h to be canonical with other Unix utilities
	jflag := globalOpts.Bool("j", false, "")
	pflag := globalOpts.String("p", "", "")
	tflag := globalOpts.String("t", "", "")
	tTflag := globalOpts.String("T", "", "")
	err := globalOpts.Parse(os.Args[1:])
	if err != nil {
		cli.Help()
		os.Exit(1)
		return
	}
	args := globalOpts.Args()
	if len(args) < 1 || *hflag {
		cli.Help()
		os.Exit(0)
		return
	}
	var cmdArgs []string
	if len(args) > 1 {
		cmdArgs = args[1:]
	}
	basePath := zerotier.PlatformDefaultHomePath
	if len(*pflag) > 0 {
		basePath = *pflag
	}

	switch args[0] {
	default:
	//case "help":
		cli.Help()
	case "version":
		fmt.Printf("%d.%d.%d\n", zerotier.CoreVersionMajor, zerotier.CoreVersionMinor, zerotier.CoreVersionRevision)
	case "service":
		cli.Service(basePath, cmdArgs)
	case "status", "info":
		cli.Status(basePath, authTokenRequired(basePath, *tflag, *tTflag), cmdArgs, *jflag)
	case "join":
		cli.Join(basePath, authTokenRequired(basePath, *tflag, *tTflag), cmdArgs)
	case "leave":
		cli.Leave(basePath, authTokenRequired(basePath, *tflag, *tTflag), cmdArgs)
	case "networks", "listnetworks":
		cli.Networks(basePath, authTokenRequired(basePath, *tflag, *tTflag), cmdArgs, *jflag)
	case "network":
		cli.Network(basePath, authTokenRequired(basePath, *tflag, *tTflag), cmdArgs, *jflag)
	case "peers", "listpeers", "lspeers":
		cli.Peers(basePath, authTokenRequired(basePath, *tflag, *tTflag), cmdArgs, *jflag, false)
	case "peer":
		authTokenRequired(basePath, *tflag, *tTflag)
	case "roots":
		cli.Peers(basePath, authTokenRequired(basePath, *tflag, *tTflag), cmdArgs, *jflag, true)
	case "controller":
	case "set":
		cli.Set(basePath, authTokenRequired(basePath, *tflag, *tTflag), cmdArgs)
	case "identity":
		cli.Identity(cmdArgs)
	case "cert":
		cli.Cert(basePath, authTokenRequired(basePath, *tflag, *tTflag), cmdArgs, *jflag)
	}
	os.Exit(0)
}
