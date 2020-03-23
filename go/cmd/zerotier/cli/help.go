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

package cli

import (
	"fmt"

	"zerotier/pkg/zerotier"
)

var copyrightText = fmt.Sprintf(`ZeroTier Network Virtualization Service Version %d.%d.%d
(c)2013-2020 ZeroTier, Inc.
Licensed under the ZeroTier BSL (see LICENSE.txt)`, zerotier.CoreVersionMajor, zerotier.CoreVersionMinor, zerotier.CoreVersionRevision)

// Help dumps help to stdout
func Help() {
	fmt.Println(copyrightText)
	fmt.Println(`
Usage: zerotier [-options] <command> [command args]

Global Options:
  -j                                   Output raw JSON where applicable
  -p <path>                            Use alternate base path
  -t <path>                            Use secret auth token from this file

Commands:
  help                                 Show this help
  version                              Print version
  service                              Start as service
  status                               Show ZeroTier status and config
  peers                                Show VL1 peers and link information
  roots                                Show only root peers
  addroot <identity> [IP/port]         Add root with optional bootstrap IP
  removeroot <address|identity>        Remove root
  join <network ID>                    Join a virtual network
  leave <network ID>                   Leave a virtual network
  networks                             List joined VL2 virtual networks
  network <network ID>                 Show verbose network info
  set <network ID> [option] [value]    Get or set a network config option
    manageips <boolean>                Is IP management allowed?
    manageroutes <boolean>             Is route management allowed?
    globalips <boolean>                Allow assignment of global IPs?
    globalroutes <boolean>             Can global IP space routes be set?
    defaultroute <boolean>             Can default route be overridden?
  set [option] [value]                 Get or set a service config option
    phy <IP/bits> blacklist <boolean>  Set or clear blacklist for CIDR
    phy <IP/bits> trust <path ID/0>    Set or clear trusted path ID for CIDR
    port <port>                        Set primary port for P2P links
    secondaryport <port/0>             Set secondary P2P port (0 disables)
    portsearch <boolean>               Enable/disable port search on startup
    portmapping <boolean>              Enable/disable use of uPnP/NAT-PMP
  identity <command> [args]            Identity management commands
    new [c25519|p384]                  Create identity pair (default: c25519)
    getpublic <identity>               Extract only public part of identity
    validate <identity>                Locally validate an identity
    sign <identity> <file>             Sign a file with an identity's key
    verify <identity> <file> <sig>     Verify a signature

The 'service' command does not exit until the service receives a signal.
This is typically run from launchd (Mac), systemd or init (Linux), etc.

If 'set' is followed by a 16-digit hex number it will get/set network config
options. Otherwise it will get/set service options. Run with no arguments to
see all options.

An identity can be specified as a file or directly. This is auto-detected.

Most commands require a secret token to permit control of a running
service. The CLI will automatically try to read this token from the
authtoken.secret file in the service's working directory and then from a
file called .zerotierauth in the user's home directory. The -t option can
be used to explicitly specify a location.`)
	fmt.Println()
}
