// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use zerotier_network_hypervisor::{VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION};

pub fn make_cmdline_help() -> String {
    format!(
        r###"ZeroTier Network Hypervisor Service Version {}.{}.{}
(c)2013-2022 ZeroTier, Inc.
Licensed under the Mozilla Public License (MPL) 2.0

Usage: zerotier [-...] <command> [command args]

Global Options:

  -j                                       Output raw JSON where applicable
  -p <path>                                Use alternate base path
  -t <path>                                Load secret auth token from a file
  -T <token>                               Set secret token on command line

Common Operations:

  help                                     Show this help
  version                                  Print version (of this binary)

· status                                   Show node status and configuration

· set [setting] [value]                    List all settings (with no args)
·   port <port>                              Primary P2P port
·   secondaryport <port/0>                   Secondary P2P port (0 to disable)
·   blacklist cidr <IP/bits> <boolean>       Toggle physical path blacklisting
·   blacklist if <prefix> <boolean>          [Un]blacklist interface prefix
·   portmap <boolean>                        Toggle use of uPnP and NAT-PMP

· peer <command> [option]
·   show <address>                         Show detailed peer information
·   list                                   List peers
·   listroots                              List root peers
·   try <address> <endpoint> [...]         Try peer at explicit endpoint

· network <command> [option]
·   show <network ID>                      Show detailed network information
·   list                                   List networks
·   set <network ID> [option] [value]      Get or set network options
·     manageips <boolean>                    Is IP management allowed?
·     manageroutes <boolean>                 Is route management allowed?
·     managedns <boolean>                    Allow network to push DNS config
·     globalips <boolean>                    Allow assignment of global IPs?
·     globalroutes <boolean>                 Can global IP routes be set?
·     defaultroute <boolean>                 Can default route be overridden?

· join <network>                           Join a virtual network
· leave <network>                          Leave a virtual network

Advanced Operations:

  identity <command> [args]
    new                                    Create new identity
    getpublic <?identity>                  Extract public part of identity
    fingerprint <?identity>                Get an identity's fingerprint
    validate <?identity>                   Locally validate an identity
    sign <?identity> <@file>               Sign a file with an identity's key
    verify <?identity> <@file> <sig>       Verify a signature

  rootset <command> [args]
·   add <@root set>                        Add or update a root set
·   remove <root set name>                 Stop using a root set
·   list                                   List root sets in use
    sign <path> <?identity secret>         Sign a root set with an identity
    verify <path>                          Load and verify a root set
    marshal <path>                         Dump root set as binary to stdout
    restoredefault                         (Re-)add built-in default root set

  service                                  Start local service
   (usually not invoked manually)

    · Command requires a running node to control.
    @ Argument is the path to a file containing the object.
    ? Argument can be either the object or a path to it (auto-detected).

"###,
        VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION,
    )
}
