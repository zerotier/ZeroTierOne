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

mod commands;
mod fastudpsocket;
mod localconfig;
mod getifaddrs;
#[macro_use] mod log;
mod store;
mod network;
mod vnic;
mod service;
mod utils;
mod webclient;
mod weblistener;

#[allow(non_snake_case, non_upper_case_globals, non_camel_case_types, dead_code, improper_ctypes)]
mod osdep; // bindgen generated

use std::io::Write;
use std::sync::Arc;
use std::str::FromStr;

use clap::{App, Arg, ArgMatches, ErrorKind};

use crate::store::Store;

pub const HTTP_API_OBJECT_SIZE_LIMIT: usize = 131072;

fn make_help() -> String {
    let ver = zerotier_core::version();
    format!(r###"ZeroTier Network Hypervisor Service Version {}.{}.{}
(c)2013-2021 ZeroTier, Inc.
Licensed under the ZeroTier BSL (see LICENSE.txt)

Usage: zerotier [-...] <command> [command args]

Global Options:

  -j                                       Output raw JSON where applicable
  -p <path>                                Use alternate base path
  -t <path>                                Load secret auth token from a file
  -T <token>                               Set secret token on command line

Common Operations:

  help                                     Show this help
  version                                  Print version

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

· join [-...] <network>                    Join a virtual network
    -c <identity | fingerprint>              Controller identity / fingerprint
· leave <network>                          Leave a virtual network

Advanced Operations:

  service                                  Start this node
                                           (usually not invoked directly)

  controller <command> [option]
·   list                                   List networks on controller
·   new                                    Create a new network
·   set <network> [setting] [value]        Show or modify network settings
·   show <network> [<address>]             Show network or member status
·   auth <address>                         Authorize a peer
·   deauth <address>                       Deauthorize a peer

  identity <command> [args]
    new [c25519 | p384]                    Create identity (default: c25519)
    getpublic <identity>                   Extract public part of identity
    fingerprint <identity>                 Get an identity's fingerprint
    validate <identity>                    Locally validate an identity
    sign <identity> <file>                 Sign a file with an identity's key
    verify <identity> <file> <sig>         Verify a signature

  locator <command> [args]
    new [-...] <identity> <endpoint> [...] Create new signed locator
      -t <timestamp>                         Timestamp (default: system time)
    verify <identity> <locator>            Verify locator signature
    show <locator>                         Show contents of a locator

  cert <command> [args]
·   list                                   List certificates at local node
·   show <serial>                          Show certificate details
    newsid [sid secret out]                Create a new subject unique ID
    newcsr <csr output path>               Create a subject CSR (interactive)
    sign <csr> <identity> [cert out]       Sign a CSR to create a certificate
    verify <cert>                          Verify certificate (not chain)
    dump <cert>                            Verify and print certificate
·   import <cert> [trust,trust,...]        Import certificate into this node
      trust flag: rootca                     Certificate is a root CA
      trust flag: ztrootset                  ZeroTier root node set
·   factoryreset                           Re-import compiled-in default certs
·   export <serial> [path]                 Export a certificate from this node
·   delete <serial|ALL>                    Delete certificate from this node

· Command requires a running node and access to a local API token.

An <address> may be specified as a 10-digit short ZeroTier address, a
fingerprint containing both an address and a SHA384 hash, or an identity.
Identities and locators can be specified as either paths to files on the
filesystem or verbatim objects in string format. This is auto-detected.
"###, ver.0, ver.1, ver.2)
}

pub(crate) fn print_help() {
    let h = make_help();
    let _ = std::io::stdout().write_all(h.as_bytes());
}

pub(crate) fn parse_bool(v: &str) -> Result<bool, String> {
    if !v.is_empty() {
        match v.chars().next().unwrap() {
            'y' | 'Y' | '1' | 't' | 'T' => { return Ok(true); }
            'n' | 'N' | '0' | 'f' | 'F' => { return Ok(false); }
            _ => {}
        }
    }
    Err(format!("invalid boolean value: '{}'", v))
}

fn is_valid_bool(v: String) -> Result<(), String> {
    parse_bool(v.as_str()).map(|_| ())
}

fn is_valid_port(v: String) -> Result<(), String> {
    let i = u16::from_str(v.as_str()).unwrap_or(0);
    if i >= 1 {
        return Ok(());
    }
    Err(format!("invalid TCP/IP port number: {}", v))
}

fn make_store(cli_args: &ArgMatches) -> Arc<Store> {
    let zerotier_path = cli_args.value_of("path").map_or_else(|| unsafe { zerotier_core::cstr_to_string(osdep::platformDefaultHomePath(), -1) }, |ztp| ztp.to_string());
    let store = Store::new(zerotier_path.as_str(), cli_args.value_of("token_path").map_or(None, |tp| Some(tp.to_string())), cli_args.value_of("token").map_or(None, |tok| Some(tok.trim().to_string())));
    if store.is_err() {
        eprintln!("FATAL: error accessing directory '{}': {}", zerotier_path, store.err().unwrap().to_string());
        std::process::exit(1);
    }
    Arc::new(store.unwrap())
}

#[derive(Clone)]
pub struct GlobalFlags {
    pub json_output: bool,
}

fn get_global_flags(cli_args: &ArgMatches) -> GlobalFlags {
    GlobalFlags {
        json_output: cli_args.is_present("json")
    }
}

fn main() {
    let cli_args = {
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
                        .arg(Arg::with_name("ip_bits").index(1))
                        .arg(Arg::with_name("boolean").index(2).validator(is_valid_bool)))
                    .subcommand(App::new("if")
                        .arg(Arg::with_name("prefix").index(1))
                        .arg(Arg::with_name("boolean").index(2).validator(is_valid_bool))))
                .subcommand(App::new("portmap")
                    .arg(Arg::with_name("boolean").index(1).validator(is_valid_bool))))
            .subcommand(App::new("peer")
                .subcommand(App::new("show")
                    .arg(Arg::with_name("address").index(1).required(true)))
                .subcommand(App::new("list"))
                .subcommand(App::new("listroots"))
                .subcommand(App::new("try")))
            .subcommand(App::new("network")
                .subcommand(App::new("show")
                    .arg(Arg::with_name("nwid").index(1).required(true)))
                .subcommand(App::new("list"))
                .subcommand(App::new("set")
                    .arg(Arg::with_name("nwid").index(1).required(true))
                    .arg(Arg::with_name("setting").index(2).required(false))
                    .arg(Arg::with_name("value").index(3).required(false))))
            .subcommand(App::new("join")
                .arg(Arg::with_name("controller").short("c").takes_value(true))
                .arg(Arg::with_name("nwid").index(1).required(true)))
            .subcommand(App::new("leave")
                .arg(Arg::with_name("nwid").index(1).required(true)))
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
                    .arg(Arg::with_name("type").possible_value("p384").possible_value("c25519").default_value("c25519").index(1)))
                .subcommand(App::new("getpublic")
                    .arg(Arg::with_name("identity").index(1).required(true)))
                .subcommand(App::new("fingerprint")
                    .arg(Arg::with_name("identity").index(1).required(true)))
                .subcommand(App::new("validate")
                    .arg(Arg::with_name("identity").index(1).required(true)))
                .subcommand(App::new("sign")
                    .arg(Arg::with_name("identity").index(1).required(true))
                    .arg(Arg::with_name("path").index(2).required(true)))
                .subcommand(App::new("verify")
                    .arg(Arg::with_name("identity").index(1).required(true))
                    .arg(Arg::with_name("path").index(2).required(true))
                    .arg(Arg::with_name("signature").index(3).required(true))))
            .subcommand(App::new("locator")
                .subcommand(App::new("new")
                    .arg(Arg::with_name("timestamp").short("t").required(false))
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
                    .arg(Arg::with_name("path").index(1).required(false)))
                .subcommand(App::new("newcsr")
                    .arg(Arg::with_name("path").index(1).required(true)))
                .subcommand(App::new("sign")
                    .arg(Arg::with_name("csr").index(1).required(true))
                    .arg(Arg::with_name("identity").index(2).required(true))
                    .arg(Arg::with_name("output").index(3).required(false)))
                .subcommand(App::new("verify")
                    .arg(Arg::with_name("cert").index(1).required(true)))
                .subcommand(App::new("dump")
                    .arg(Arg::with_name("cert").index(1).required(true)))
                .subcommand(App::new("import")
                    .arg(Arg::with_name("cert").index(1).required(true))
                    .arg(Arg::with_name("trust").index(2).required(false)))
                .subcommand(App::new("factoryreset"))
                .subcommand(App::new("export")
                    .arg(Arg::with_name("serial").index(1).required(true))
                    .arg(Arg::with_name("path").index(2).required(false)))
                .subcommand(App::new("delete")
                    .arg(Arg::with_name("serial").index(1).required(true))))
            .help(help.as_str())
            .get_matches_from_safe(std::env::args());
        if args.is_err() {
            let e = args.err().unwrap();
            if e.kind != ErrorKind::HelpDisplayed {
                print_help();
            }
            std::process::exit(1);
        }
        let args = args.unwrap();
        if args.subcommand_name().is_none() {
            print_help();
            std::process::exit(1);
        }
        args
    };

    std::process::exit(match cli_args.subcommand() {
        ("help", _) => {
            print_help();
            0
        }
        ("version", _) => {
            let ver = zerotier_core::version();
            println!("{}.{}.{}", ver.0, ver.1, ver.2);
            0
        }
        ("status", _) => crate::webclient::run_command(make_store(&cli_args), get_global_flags(&cli_args), crate::commands::status::run),
        ("set", Some(sub_cli_args)) => { 0 }
        ("peer", Some(sub_cli_args)) => { 0 }
        ("network", Some(sub_cli_args)) => { 0 }
        ("join", Some(sub_cli_args)) => { 0 }
        ("leave", Some(sub_cli_args)) => { 0 }
        ("service", _) => {
            let store = make_store(&cli_args);
            drop(cli_args); // free no longer needed memory before entering service
            service::run(store)
        },
        ("controller", Some(sub_cli_args)) => { 0 }
        ("identity", Some(sub_cli_args)) => crate::commands::identity::run(sub_cli_args),
        ("locator", Some(sub_cli_args)) => crate::commands::locator::run(sub_cli_args),
        ("cert", Some(sub_cli_args)) => crate::commands::cert::run(make_store(&cli_args), sub_cli_args),
        _ => {
            print_help();
            1
        }
    });
}
