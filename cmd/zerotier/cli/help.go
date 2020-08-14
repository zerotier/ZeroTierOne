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

package cli

import (
	"fmt"
	"zerotier/pkg/zerotier"
)

func Help() {
	fmt.Printf(`ZeroTier Network Hypervisor Service Version %d.%d.%d
(c)2013-2020 ZeroTier, Inc.
Licensed under the ZeroTier BSL (see LICENSE.txt)

Usage: zerotier [-global options] <command> [command args]

Global Options:

  -j                                     Output raw JSON where applicable
  -p <path>                              Use alternate base path
  -t <path>                              Load secret auth token from a file
  -T <token>                             Set secret auth token on command line

Common Operations:

  help                                   Show this help
  version                                Print version
  now [duration]                         Print current time [-]#[ms|s|m|h]

· status                                 Show node status and configuration

· set [option] [value]                 - Get or set node configuration
    port <port>                          Primary P2P port
    secondaryport <port/0>               Secondary P2P port (0 to disable)
    blacklist cidr <IP/bits> <boolean>   Toggle physical path blacklisting
    blacklist if <prefix> <boolean>      Toggle interface prefix blacklisting
    portmap <boolean>                    Toggle use of uPnP or NAT-PMP

· peer [address] [command] [option]    - Peer management commands
    list                                 List peers
    listroots                            List root peers
    show                                 Show peer details
    try <endpoint> [...]                 Try peer at explicit endpoint

· network list                           List VL2 networks
· network <network> [command] [option] - Network management commands
    show                                 Show network details (default)
    set [option] [value]               - Get or set network options
      manageips <boolean>                Is IP management allowed?
      manageroutes <boolean>             Is route management allowed?
      globalips <boolean>                Allow assignment of global IPs?
      globalroutes <boolean>             Can global IP space routes be set?
      defaultroute <boolean>             Can default route be overridden?

· join [-options] <network>              Join a virtual network
    -a <token>                           Token to submit to controller
    -c <identity | fingerprint>          Controller identity or fingerprint
· leave <network>                        Leave a virtual network

Advanced Operations:

  service                                Start node (seldom used from CLI)

· controller <command> [option]        - Local controller management commands
·   list                                 List networks run by local controller
·   new                                  Create a new network
·   set <network> [setting] [value]      Show or modify network settings
·   members <network>                    List members of a network
·   member <network> [setting] [value]   Show or modify member level settings
·   auth <address>                       Authorize a peer
·   deauth <address>                     Deauthorize a peer

  identity <command> [args]            - Identity management
    new [c25519 | p384]                  Create identity (default: c25519)
    getpublic <identity>                 Extract only public part of identity
    fingerprint <identity>               Get an identity's fingerprint
    validate <identity>                  Locally validate an identity
    sign <identity> <file>               Sign a file with an identity's key
    verify <identity> <file> <sig>       Verify a signature

  locator <command> [args]             - Locator management
    new <identity> <endpoint> [...]      Create new signed locator
    verify <identity> <locator>          Verify locator signature
    show <locator>                       Show contents of a locator

  cert <command> [args]                - Certificate management
·   list                                 List certificates in local node store
·   show <serial>                        List or show details of a certificate
    newsid <secret out>                  Create a new subject unique ID
    newcsr <subject> <secret> <csr out>  Create a subject CSR
    sign <csr> <identity> <cert out>     Sign a CSR to create a certificate
    verify <cert>                        Verify certificate (not entire chain)
    dump <cert>                          Verify and print certificate
·   import <cert> [trust,[trust]]        Import certificate into this node
      rootca                             Certificate is a root CA (trust flag)
      ztrootset                          ZeroTier root node set (trust flag)
·   restore                              Re-import default certificates
·   export <serial> [path]               Export a certificate from this node
·   delete <serial|ALL>                  Delete certificate from this node

· Command requires a running node and access to a local API token.

An <address> may be specified as a 10-digit short ZeroTier address, a
fingerprint containing both an address and a SHA384 hash, or an identity.
The latter two options are equivalent in terms of specificity and may be
used if stronger security guarantees are desired than those provided by
the basic ZeroTier addressing system. Fields of type <identity> must be
full identities and may be specified either verbatim or as a path to a file.

An <endpoint> is a place where a peer may be reached. Currently these are
just 'IP/port' format addresses but other types may be added in the future.

The 'service' command starts a node. It will run until the node receives
an exit signal and is normally not used directly.
`,zerotier.CoreVersionMajor, zerotier.CoreVersionMinor, zerotier.CoreVersionRevision)
}
