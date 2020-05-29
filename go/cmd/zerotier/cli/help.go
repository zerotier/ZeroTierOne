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

// Help dumps help to stdout
func Help() {
	fmt.Printf(`ZeroTier Network Hypervisor Service Version %d.%d.%d
(c)2013-2020 ZeroTier, Inc.
Licensed under the ZeroTier BSL (see LICENSE.txt)

Usage: zerotier [-options] <command> [command args]

Global Options:
  -j                                    Output raw JSON where applicable
  -p <path>                             Use alternate base path
  -t <path>                             Load secret auth token from a file
  -T <token>                            Set secret auth token on command line

Commands:
  help                                  Show this help
  version                               Print version
  service                               Start as service
  status                                Show node status, identity, and config
  peers                                 List all VL1 peers
  join <network ID> [fingerprint]       Join a virtual network
  leave <network ID>                    Leave a virtual network
  networks                              List VL2 virtual networks
  network <network ID>                  Show verbose network info
  set <network ID> [option] [value]     Get or set a network config option
    manageips <boolean>                 Is IP management allowed?
    manageroutes <boolean>              Is route management allowed?
    globalips <boolean>                 Allow assignment of global IPs?
    globalroutes <boolean>              Can global IP space routes be set?
    defaultroute <boolean>              Can default route be overridden?
  set [option] [value]                  Get or set a service config option
    port <port>                         Primary P2P port
    secondaryport <port/0>              Secondary P2P port (0 to disable)
    blacklist cidr <IP/bits> <boolean>  Toggle physical path blacklisting
    blacklist if <prefix> <boolean>     Toggle interface prefix blacklisting
    portmap <boolean>                   Toggle use of uPnP or NAT-PMP
  identity <command> [args]             Identity management commands
    new [c25519|p384]                   Create identity pair (default: c25519)
    getpublic <identity>                Extract only public part of identity
    validate <identity>                 Locally validate an identity
    sign <identity> <file>              Sign a file with an identity's key
    verify <identity> <file> <sig>      Verify a signature
  locator <command> [args]              Locator management commands
    new <identity> <address> [...]      Create and sign a new locator
    show <locator> [<identity>]         Show locator information
  roots                                 List root peers
  addroot <identity> <locator>          Add a root or update its locator
  addroot <url>                         Add or update roots from a URL
  removeroot <address>                  Remove a peer from the root list

The 'service' command does not exit until the service receives a signal.
This is typically run from launchd (Mac), systemd or init (Linux), a Windows
service harness (Windows), etc.

If 'set' is followed by a 16-digit hex number it will get/set network config
options. Otherwise it will get/set local options that pertain to the entire
node.

Identities can be specified verbatim on the command line or as a path to
a file. This is detected automatically.

`,zerotier.CoreVersionMajor, zerotier.CoreVersionMinor, zerotier.CoreVersionRevision)
}
