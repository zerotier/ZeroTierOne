/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

use std::str::FromStr;
use clap::{App, Arg, ArgMatches, ErrorKind};

fn make_help() -> String {
    let ver = zerotier_core::version();
    format!(r###"ZeroTier Network Hypervisor Service Version {}.{}.{}
(c)2013-2021 ZeroTier, Inc.
Licensed under the ZeroTier BSL (see LICENSE.txt)

Usage: zerotier [-global options] <command> [command args]

Global Options:

  -j                                     Output raw JSON where applicable
  -p <path>                              Use alternate base path
  -t <path>                              Load secret auth token from a file
  -T <token>                             Set secret auth token on command line

Common Operations:

  help                                    Show this help
  version                                 Print version

· status                                  Show node status and configuration

· set [setting] [value]                   List all settings (with no args)
·    port <port>                          Primary P2P port
·    secondaryport <port/0>               Secondary P2P port (0 to disable)
·    blacklist cidr <IP/bits> <boolean>   Toggle physical path blacklisting
·    blacklist if <prefix> <boolean>      Toggle interface prefix blacklisting
·    portmap <boolean>                    Toggle use of uPnP or NAT-PMP

· peer <command> [option]
·   show <address>                        Show detailed peer information
·   list                                  List peers
·   listroots                             List root peers
·   try <address> <endpoint> [...]        Try peer at explicit endpoint

· network <command> [option]
·   show <network ID>                     Show detailed network information
·   list                                  List networks
·   set <network ID> [option] [value]     Get or set network options
·     manageips <boolean>                 Is IP management allowed?
·     manageroutes <boolean>              Is route management allowed?
·     managedns <boolean>                 Allow network to push DNS config
·     globalips <boolean>                 Allow assignment of global IPs?
·     globalroutes <boolean>              Can global IP space routes be set?
·     defaultroute <boolean>              Can default route be overridden?

· join [-options] <network>               Join a virtual network
    -t <token>                            Token to submit to controller
    -c <identity | fingerprint>           Controller identity or fingerprint
· leave <network>                         Leave a virtual network

Advanced Operations:

  service                                 Start this node (runs until stopped)

  controller <command> [option]
·   list                                  List networks on controller
·   new                                   Create a new network
·   set <network> [setting] [value]       Show or modify network settings
·   show <network> [<address>]            Show network or member status
·   auth <address>                        Authorize a peer
·   deauth <address>                      Deauthorize a peer

  identity <command> [args]
    new [c25519 | p384]                   Create identity (default: c25519)
    getpublic <identity>                  Extract only public part of identity
    fingerprint <identity>                Get an identity's fingerprint
    validate <identity>                   Locally validate an identity
    sign <identity> <file>                Sign a file with an identity's key
    verify <identity> <file> <sig>        Verify a signature

  locator <command> [args]
    new <identity> <endpoint> [...]       Create new signed locator
    verify <identity> <locator>           Verify locator signature
    show <locator>                        Show contents of a locator

  cert <command> [args]
·   list                                  List certificates at local node
·   show <serial>                         Show certificate details
    newsid <secret out>                   Create a new subject unique ID
    newcsr <subject> <secret> <csr out>   Create a subject CSR
    sign <csr> <identity> <cert out>      Sign a CSR to create a certificate
    verify <cert>                         Verify certificate (not chain)
    dump <cert>                           Verify and print certificate
·   import <cert> [trust] [...]           Import certificate into this node
      trust flag: rootca                  Certificate is a root CA
      trust flag: ztrootset               ZeroTier root node set
·   restore                               Re-import default certificates
·   export <serial> [path]                Export a certificate from this node
·   delete <serial|ALL>                   Delete certificate from this node

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
an exit signal and is normally not used directly."###, ver.0, ver.1, ver.2)
}

pub(crate) fn print_help() {
    println!("{}", make_help());
}

fn is_bool(v: String) -> Result<(), String> {
    if !v.is_empty() {
        match v.chars().next().unwrap() {
            'y' | 'Y' | '1' | 't' | 'T' | 'n' | 'N' | '0' | 'f' | 'F' => { return Ok(()); }
            _ => {}
        }
    }
    Err(format!("invalid boolean value: '{}'", v))
}

fn is_valid_port(v: String) -> Result<(), String> {
    let i = u16::from_str(v.as_str()).unwrap_or(0);
    if i >= 1 {
        return Ok(());
    }
    Err(format!("invalid TCP/IP port number: {}", v))
}

/// Parses CLI arguments, prints error and exits on failure.
pub(crate) fn parse_cli_args() -> ArgMatches<'static> {
    let help = make_help();
    let args = App::new("zerotier")
        .arg(Arg::with_name("json").short("j"))
        .arg(Arg::with_name("path").short("p").takes_value(true))
        .arg(Arg::with_name("token_path").short("t").takes_value(true))
        .arg(Arg::with_name("token").short("T").takes_value(true))
        .subcommand(App::new("help"))
        .subcommand(App::new("version"))
        .subcommand(App::new("status"))
        .subcommand(App::new("set")
            .subcommand(App::new("port")
                .arg(Arg::with_name("port#").index(1).validator(is_valid_port)))
            .subcommand(App::new("secondaryport")
                .arg(Arg::with_name("port#").index(1).validator(is_valid_port)))
            .subcommand(App::new("blacklist")
                .subcommand(App::new("cidr")
                    .arg(Arg::with_name("ip/bits").index(1))
                    .arg(Arg::with_name("boolean").index(2).validator(is_bool)))
                .subcommand(App::new("if")
                    .arg(Arg::with_name("prefix").index(1))
                    .arg(Arg::with_name("boolean").index(2).validator(is_bool))))
            .subcommand(App::new("portmap")
                .arg(Arg::with_name("boolean").index(1).validator(is_bool))))
        .subcommand(App::new("peer")
            .subcommand(App::new("show")
                .arg(Arg::with_name("address").index(1).required(true)))
            .subcommand(App::new("list"))
            .subcommand(App::new("listroots"))
            .subcommand(App::new("try")))
        .subcommand(App::new("network")
            .subcommand(App::new("show"))
            .subcommand(App::new("list"))
            .subcommand(App::new("set")))
        .subcommand(App::new("join")
            .arg(Arg::with_name("token").short("t").takes_value(true))
            .arg(Arg::with_name("controller").short("c").takes_value(true))
            .arg(Arg::with_name("id").index(1).required(true)))
        .subcommand(App::new("leave")
            .arg(Arg::with_name("id").index(1).required(true)))
        .subcommand(App::new("service"))
        .subcommand(App::new("controller")
            .subcommand(App::new("list"))
            .subcommand(App::new("new"))
            .subcommand(App::new("set")
                .arg(Arg::with_name("id").index(1).required(true))
                .arg(Arg::with_name("setting").index(2))
                .arg(Arg::with_name("value").index(3)))
            .subcommand(App::new("show")
                .arg(Arg::with_name("id").index(1).required(true))
                .arg(Arg::with_name("member").index(2)))
            .subcommand(App::new("auth")
                .arg(Arg::with_name("member").index(1).required(true)))
            .subcommand(App::new("deauth")
                .arg(Arg::with_name("member").index(1).required(true))))
        .subcommand(App::new("identity")
            .subcommand(App::new("new")
                .arg(Arg::with_name("type").possible_value("p384").possible_value("c25519").index(1)))
            .subcommand(App::new("getpublic")
                .arg(Arg::with_name("identity").index(1).required(true)))
            .subcommand(App::new("fingerprint")
                .arg(Arg::with_name("identity").index(1).required(true)))
            .subcommand(App::new("validate")
                .arg(Arg::with_name("identity").index(1).required(true)))
            .subcommand(App::new("sign")
                .arg(Arg::with_name("identity").index(1).required(true))
                .arg(Arg::with_name("file").index(2).required(true)))
            .subcommand(App::new("verify")
                .arg(Arg::with_name("identity").index(1).required(true))
                .arg(Arg::with_name("file").index(2).required(true))
                .arg(Arg::with_name("signature").index(3).required(true))))
        .subcommand(App::new("locator")
            .subcommand(App::new("new")
                .arg(Arg::with_name("identity").index(1).required(true))
                .arg(Arg::with_name("endpoint").index(2).multiple(true).required(true)))
            .subcommand(App::new("verify")
                .arg(Arg::with_name("identity").index(1).required(true))
                .arg(Arg::with_name("locator").index(2).required(true)))
            .subcommand(App::new("show")
                .arg(Arg::with_name("locator").index(1).required(true))))
        .subcommand(App::new("cert")
            .subcommand(App::new("list"))
            .subcommand(App::new("show")
                .arg(Arg::with_name("serial").index(1).required(true)))
            .subcommand(App::new("newsid")
                .arg(Arg::with_name("path").index(1).required(true)))
            .subcommand(App::new("newcsr")
                .arg(Arg::with_name("subject").index(1).required(true))
                .arg(Arg::with_name("secret").index(2).required(true))
                .arg(Arg::with_name("output").index(3).required(true)))
            .subcommand(App::new("sign")
                .arg(Arg::with_name("csr").index(1).required(true))
                .arg(Arg::with_name("identity").index(2).required(true))
                .arg(Arg::with_name("output").index(3).required(true)))
            .subcommand(App::new("verify")
                .arg(Arg::with_name("cert").index(1).required(true)))
            .subcommand(App::new("dump")
                .arg(Arg::with_name("cert").index(1).required(true)))
            .subcommand(App::new("import")
                .arg(Arg::with_name("cert").index(1).required(true))
                .arg(Arg::with_name("trust").possible_value("rootca").possible_value("ztrootset").index(2).multiple(true)))
            .subcommand(App::new("restore"))
            .subcommand(App::new("export")
                .arg(Arg::with_name("serial").index(1).required(true))
                .arg(Arg::with_name("path").index(2)))
            .subcommand(App::new("delete")
                .arg(Arg::with_name("serial").index(1).required(true))))
        .help(help.as_str())
        .get_matches_from_safe(std::env::args());

    if args.is_err() {
        let e = args.err().unwrap();
        match e.kind {
            ErrorKind::HelpDisplayed => {}
            _ => { print_help(); }
        }
        std::process::exit(1);
    }
    let args = args.unwrap();
    if args.subcommand_name().is_none() {
        print_help();
        std::process::exit(1);
    }

    args
}
