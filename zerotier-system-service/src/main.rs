/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::io::Write;

use clap::error::{ContextKind, ContextValue};
use clap::{Arg, ArgMatches, Command};

use zerotier_network_hypervisor::{VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION};

pub mod exitcode;
pub mod getifaddrs;
pub mod localconfig;
pub mod utils;
pub mod vnic;

fn make_help() -> String {
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
·   trust <@root set>                      Add or update a root set
·   untrust <root set name>                Stop using a root set
·   list                                   List root sets in use
    sign <path> <?identity secret>         Sign a root set with an identity

  service                                  Start local service
   (usually not invoked manually)

    · Command requires a running node to control.
    @ Argument is the path to a file containing the object.
    ? Argument can be either the object or a path to it (auto-detected).

"###,
        VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION,
    )
}

pub fn print_help() {
    let h = make_help();
    let _ = std::io::stdout().write_all(h.as_bytes());
}

pub struct GlobalCommandLineFlags {
    pub json_output: bool,
    pub base_path: String,
    pub auth_token_path_override: Option<String>,
    pub auth_token_override: Option<String>,
}

#[cfg(any(target_os = "macos"))]
pub fn platform_default_home_path() -> String {
    "/Library/Application Support/ZeroTier".into()
}

#[cfg(any(target_os = "linux"))]
pub fn platform_default_home_path() -> String {
    "/var/lib/zerotier".into()
}

async fn async_main(cli_args: Box<ArgMatches>) -> i32 {
    let global_cli_flags = GlobalCommandLineFlags {
        json_output: cli_args.is_present("json"),
        base_path: cli_args.value_of("path").map_or_else(platform_default_home_path, |p| p.to_string()),
        auth_token_path_override: cli_args.value_of("token_path").map(|p| p.to_string()),
        auth_token_override: cli_args.value_of("token").map(|t| t.to_string()),
    };

    #[allow(unused)]
    return match cli_args.subcommand() {
        Some(("help", _)) => {
            print_help();
            exitcode::OK
        }
        Some(("version", _)) => {
            println!("{}.{}.{}", VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION);
            exitcode::OK
        }
        Some(("status", _)) => todo!(),
        Some(("set", args)) => todo!(),
        Some(("peer", args)) => todo!(),
        Some(("network", args)) => todo!(),
        Some(("join", args)) => todo!(),
        Some(("leave", args)) => todo!(),
        Some(("service", _)) => todo!(),
        Some(("identity", args)) => todo!(),
        Some(("rootset", args)) => todo!(),
        _ => {
            print_help();
            exitcode::ERR_USAGE
        }
    };
}

fn main() {
    let cli_args = Box::new({
        let help = make_help();
        Command::new("zerotier")
            .arg(Arg::new("json").short('j'))
            .arg(Arg::new("path").short('p').takes_value(true))
            .arg(Arg::new("token_path").short('t').takes_value(true))
            .arg(Arg::new("token").short('T').takes_value(true))
            .subcommand(Command::new("help"))
            .subcommand(Command::new("version"))
            .subcommand(Command::new("status"))
            .subcommand(
                Command::new("set")
                    .subcommand(Command::new("port").arg(Arg::new("port#").index(1).validator(utils::is_valid_port)))
                    .subcommand(Command::new("secondaryport").arg(Arg::new("port#").index(1).validator(utils::is_valid_port)))
                    .subcommand(
                        Command::new("blacklist")
                            .subcommand(Command::new("cidr").arg(Arg::new("ip_bits").index(1)).arg(Arg::new("boolean").index(2).validator(utils::is_valid_bool)))
                            .subcommand(Command::new("if").arg(Arg::new("prefix").index(1)).arg(Arg::new("boolean").index(2).validator(utils::is_valid_bool))),
                    )
                    .subcommand(Command::new("portmap").arg(Arg::new("boolean").index(1).validator(utils::is_valid_bool))),
            )
            .subcommand(Command::new("peer").subcommand(Command::new("show").arg(Arg::new("address").index(1).required(true))).subcommand(Command::new("list")).subcommand(Command::new("listroots")).subcommand(Command::new("try")))
            .subcommand(
                Command::new("network")
                    .subcommand(Command::new("show").arg(Arg::new("nwid").index(1).required(true)))
                    .subcommand(Command::new("list"))
                    .subcommand(Command::new("set").arg(Arg::new("nwid").index(1).required(true)).arg(Arg::new("setting").index(2).required(false)).arg(Arg::new("value").index(3).required(false))),
            )
            .subcommand(Command::new("join").arg(Arg::new("nwid").index(1).required(true)))
            .subcommand(Command::new("leave").arg(Arg::new("nwid").index(1).required(true)))
            .subcommand(Command::new("service"))
            .subcommand(
                Command::new("identity")
                    .subcommand(Command::new("new"))
                    .subcommand(Command::new("getpublic").arg(Arg::new("identity").index(1).required(true)))
                    .subcommand(Command::new("fingerprint").arg(Arg::new("identity").index(1).required(true)))
                    .subcommand(Command::new("validate").arg(Arg::new("identity").index(1).required(true)))
                    .subcommand(Command::new("sign").arg(Arg::new("identity").index(1).required(true)).arg(Arg::new("path").index(2).required(true)))
                    .subcommand(Command::new("verify").arg(Arg::new("identity").index(1).required(true)).arg(Arg::new("path").index(2).required(true)).arg(Arg::new("signature").index(3).required(true))),
            )
            .subcommand(
                Command::new("rootset")
                    .subcommand(Command::new("trust").arg(Arg::new("path").index(1).required(true)))
                    .subcommand(Command::new("untrust").arg(Arg::new("name").index(1).required(true)))
                    .subcommand(Command::new("list"))
                    .subcommand(Command::new("sign").arg(Arg::new("path").index(1).required(true)).arg(Arg::new("secret").index(2).required(true))),
            )
            .override_help(help.as_str())
            .override_usage("")
            .disable_version_flag(true)
            .disable_help_subcommand(false)
            .disable_help_flag(true)
            .try_get_matches_from(std::env::args())
            .unwrap_or_else(|e| {
                if e.kind() == clap::ErrorKind::DisplayHelp {
                    print_help();
                    std::process::exit(exitcode::OK);
                } else {
                    let mut invalid = String::default();
                    let mut suggested = String::default();
                    for c in e.context() {
                        match c {
                            (ContextKind::SuggestedSubcommand | ContextKind::SuggestedArg, ContextValue::String(name)) => {
                                suggested = name.clone();
                            }
                            (ContextKind::InvalidArg | ContextKind::InvalidSubcommand, ContextValue::String(name)) => {
                                invalid = name.clone();
                            }
                            _ => {}
                        }
                    }
                    if invalid.is_empty() {
                        println!("Invalid command line. Use 'help' for help.");
                    } else {
                        if suggested.is_empty() {
                            println!("Unrecognized option '{}'. Use 'help' for help.", invalid);
                        } else {
                            println!("Unrecognized option '{}', did you mean {}? Use 'help' for help.", invalid, suggested);
                        }
                    }
                    std::process::exit(exitcode::ERR_USAGE);
                }
            })
    });

    std::process::exit(tokio::runtime::Builder::new_multi_thread().enable_all().build().unwrap().block_on(async_main(cli_args)));
}
