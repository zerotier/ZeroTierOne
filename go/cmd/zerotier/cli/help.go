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
  join <network> [fingerprint]          Join a virtual network
  leave <network>                       Leave a virtual network
  networks                              List VL2 virtual networks
  network <network>                     Show verbose network info
  set <network> [option] [value]        Get or set a network config option
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
    show <locator> [identity]           Show locator information
  root [command]                        Root management commands
    list                                List root peers (same as no command)
    add <identity> <locator>            Add or manually update a root
    add <url>                           Add or update root(s) from a URL
    remove <address>                    Un-designate a peer as a root
  controller <command> [option]         Local controller management commands
    networks                            List networks run by local controller
    new                                 Create a new network
    set <network> [setting] [value]     Show or modify network settings
    members <network>                   List members of a network
    member <network> [setting] [value]  Show or modify member level settings
    auth <address|fingerprint>          Authorize a peer
    deauth <address|fingerprint>        Deauthorize a peer

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
