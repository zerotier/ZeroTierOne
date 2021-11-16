/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

mod fastudpsocket;
mod localconfig;
mod getifaddrs;
#[macro_use]
mod log;
mod store;
mod vnic;
mod service;
mod utils;

use std::io::Write;
use std::str::FromStr;

use clap::{App, Arg, ArgMatches, ErrorKind};

use zerotier_network_hypervisor::{VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION};

pub const HTTP_API_OBJECT_SIZE_LIMIT: usize = 131072;

fn make_help(long_help: bool) -> String {
    format!(r###"ZeroTier Network Hypervisor Service Version {}.{}.{}
(c)2013-2021 ZeroTier, Inc.
Licensed under the Mozilla Public License (MPL) 2.0 (see LICENSE.txt)

Usage: zerotier [-...] <command> [command args]

Global Options:

  -j                                       Output raw JSON where applicable
  -p <path>                                Use alternate base path
  -t <path>                                Load secret auth token from a file
  -T <token>                               Set secret token on command line

Common Operations:

  help                                     Show this help
  longhelp                                 Show help with advanced commands
  oldhelp                                  Show v1.x legacy commands
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
{}"###,
            VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION,
            if long_help {
            r###"
Advanced Operations:

  service                                  Start local service
                                           (usually not invoked manually)

  controller <command> [option]
·   list                                   List networks on controller
·   new                                    Create a new network
·   set <network> [setting] [value]        Show or modify network settings
·   show <network> [<address>]             Show network or member status
·   auth <address>                         Authorize a peer
·   deauth <address>                       Deauthorize a peer

  identity <command> [args]
    new [c25519 | p384]                    Create identity (default: c25519)
    getpublic <?identity>                  Extract public part of identity
    fingerprint <?identity>                Get an identity's fingerprint
    validate <?identity>                   Locally validate an identity
    sign <?identity> <@file>               Sign a file with an identity's key
    verify <?identity> <@file> <sig>       Verify a signature

    · Command (or command with argument type) requires a running node.
    @ Argument is the path to a file containing the object.
    ? Argument can be either the object or a path to it (auto-detected).
"###
        } else { "" })
}

pub fn print_help(long_help: bool) {
    let h = make_help(long_help);
    let _ = std::io::stdout().write_all(h.as_bytes());
}

pub struct GlobalCommandLineFlags {
    pub json_output: bool,
}

fn main() {
    let cli_args = Box::new({
        let help = make_help(false);
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
                    .arg(Arg::with_name("port#").index(1).validator(utils::is_valid_port)))
                .subcommand(App::new("secondaryport")
                    .arg(Arg::with_name("port#").index(1).validator(utils::is_valid_port)))
                .subcommand(App::new("blacklist")
                    .subcommand(App::new("cidr")
                        .arg(Arg::with_name("ip_bits").index(1))
                        .arg(Arg::with_name("boolean").index(2).validator(utils::is_valid_bool)))
                    .subcommand(App::new("if")
                        .arg(Arg::with_name("prefix").index(1))
                        .arg(Arg::with_name("boolean").index(2).validator(utils::is_valid_bool))))
                .subcommand(App::new("portmap")
                    .arg(Arg::with_name("boolean").index(1).validator(utils::is_valid_bool))))
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
            .help(help.as_str())
            .get_matches_from_safe(std::env::args());
        if args.is_err() {
            let e = args.err().unwrap();
            if e.kind != ErrorKind::HelpDisplayed {
                print_help(false);
            }
            std::process::exit(1);
        }
        let args = args.unwrap();
        if args.subcommand_name().is_none() {
            print_help(false);
            std::process::exit(1);
        }
        args
    });

    let global_cli_flags = GlobalCommandLineFlags {
        json_output: cli_args.is_present("json")
    };

    std::process::exit({
        match cli_args.subcommand() {
            ("help", None) => {
                print_help(false);
                0
            }
            ("longhelp", None) => {
                print_help(true);
                0
            }
            ("oldhelp", None) => todo!(),
            ("version", None) => {
                println!("{}.{}.{}", VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION);
                0
            }
            ("status", None) => todo!(),
            ("set", Some(sub_cli_args)) => todo!(),
            ("peer", Some(sub_cli_args)) => todo!(),
            ("network", Some(sub_cli_args)) => todo!(),
            ("join", Some(sub_cli_args)) => todo!(),
            ("leave", Some(sub_cli_args)) => todo!(),
            ("service", None) => {
                drop(cli_args); // free no longer needed memory before entering service
                service::run()
            }
            ("controller", Some(sub_cli_args)) => todo!(),
            ("identity", Some(sub_cli_args)) => todo!(),
            _ => {
                print_help(false);
                1
            }
        }
    });
}
