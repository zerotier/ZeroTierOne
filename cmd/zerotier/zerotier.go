/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
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

// authToken returns a function that reads the authorization token if needed.
// If the authorization token can't be read, the function terminates the program with a fatal error.
func authToken(basePath, tflag, tTflag string) func () string {
	savedAuthToken := new(string)
	return func() string {
		authToken := *savedAuthToken
		if len(authToken) > 0 {
			return authToken
		}

		if len(tflag) > 0 {
			at, err := ioutil.ReadFile(tflag)
			if err != nil || len(at) == 0 {
				fmt.Println("FATAL: unable to read local service API authorization token from " + tflag)
				os.Exit(1)
				return ""
			}
			authToken = string(at)
		} else if len(tTflag) > 0 {
			authToken = tTflag
		} else {
			var authTokenPaths []string
			authTokenPaths = append(authTokenPaths, path.Join(basePath, "authtoken.secret"))
			userHome, _ := os.UserHomeDir()
			if len(userHome) > 0 {
				if runtime.GOOS == "darwin" {
					authTokenPaths = append(authTokenPaths, path.Join(userHome, "Library", "Application Support", "ZeroTier", "authtoken.secret"))
					authTokenPaths = append(authTokenPaths, path.Join(userHome, "Library", "Application Support", "ZeroTier", "One", "authtoken.secret"))
				}
				authTokenPaths = append(authTokenPaths, path.Join(userHome, ".zerotierauth"))
				authTokenPaths = append(authTokenPaths, path.Join(userHome, ".zeroTierOneAuthToken"))
			}

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
				return ""
			}
		}

		authToken = strings.TrimSpace(authToken)
		if len(authToken) == 0 {
			fmt.Println("FATAL: unable to read API authorization token from command line or any filesystem location.")
			os.Exit(1)
			return ""
		}

		*savedAuthToken = authToken
		return authToken
	}
}

func main() {
	// Reduce Go's thread and memory footprint. This would slow things down if the Go code
	// were doing a lot, but it's not. It just manages the core and is not directly involved
	// in pushing a lot of packets around. If that ever changes this should be adjusted.
	runtime.GOMAXPROCS(1)
	debug.SetGCPercent(10)

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

	exitCode := 0
	switch args[0] {
	default:
		cli.Help()
		exitCode = 1
	case "help":
		cli.Help()
	case "version":
		fmt.Printf("%d.%d.%d\n", zerotier.CoreVersionMajor, zerotier.CoreVersionMinor, zerotier.CoreVersionRevision)
	case "service":
		exitCode = cli.Service(basePath, cmdArgs)
	case "status", "info":
		exitCode = cli.Status(basePath, authToken(basePath, *tflag, *tTflag), cmdArgs, *jflag)
	case "join":
		exitCode = cli.Join(basePath, authToken(basePath, *tflag, *tTflag), cmdArgs)
	case "leave":
		exitCode = cli.Leave(basePath, authToken(basePath, *tflag, *tTflag), cmdArgs)
	case "networks", "listnetworks":
		exitCode = cli.Network(basePath, authToken(basePath, *tflag, *tTflag), []string{"list"}, *jflag)
	case "network":
		exitCode = cli.Network(basePath, authToken(basePath, *tflag, *tTflag), cmdArgs, *jflag)
	case "peers", "listpeers":
		exitCode = cli.Peer(basePath, authToken(basePath, *tflag, *tTflag), []string{"list"}, *jflag)
	case "peer":
		exitCode = cli.Peer(basePath, authToken(basePath, *tflag, *tTflag), cmdArgs, *jflag)
	case "controller":
		exitCode = cli.Controller(basePath, authToken(basePath, *tflag, *tTflag), cmdArgs, *jflag)
	case "set":
		exitCode = cli.Set(basePath, authToken(basePath, *tflag, *tTflag), cmdArgs)
	case "identity":
		exitCode = cli.Identity(cmdArgs)
	case "locator":
		exitCode = cli.Locator(cmdArgs)
	case "certs", "listcerts", "lscerts": // same as "cert show" with no specific serial to show
		exitCode = cli.Cert(basePath, authToken(basePath, *tflag, *tTflag), []string{"show"}, *jflag)
	case "cert":
		exitCode = cli.Cert(basePath, authToken(basePath, *tflag, *tTflag), cmdArgs, *jflag)
	}
	os.Exit(exitCode)
}
