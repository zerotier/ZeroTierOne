// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

pub mod cli;
pub mod cmdline_help;
pub mod datadir;
pub mod exitcode;
pub mod jsonformatter;
pub mod localconfig;
pub mod utils;
pub mod vnic;

use std::io::Write;
use std::sync::Arc;

use clap::error::{ContextKind, ContextValue};
use clap::{Arg, ArgMatches, Command};

use zerotier_network_hypervisor::{VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION};
use zerotier_vl1_service::VL1Service;

use crate::datadir::DataDir;

pub fn print_help() {
    let h = crate::cmdline_help::make_cmdline_help();
    let _ = std::io::stdout().write_all(h.as_bytes());
}

#[cfg(target_os = "macos")]
pub fn platform_default_home_path() -> String {
    "/Library/Application Support/ZeroTier".into()
}

#[cfg(target_os = "linux")]
pub fn platform_default_home_path() -> String {
    "/var/lib/zerotier".into()
}

pub struct Flags {
    pub json_output: bool,
    pub base_path: String,
    pub auth_token_path_override: Option<String>,
    pub auth_token_override: Option<String>,
}

async fn open_datadir(flags: &Flags) -> Arc<DataDir> {
    let datadir = DataDir::open(flags.base_path.as_str()).await;
    if datadir.is_ok() {
        return Arc::new(datadir.unwrap());
    }
    eprintln!(
        "FATAL: unable to open data directory {}: {}",
        flags.base_path,
        datadir.err().unwrap().to_string()
    );
    std::process::exit(exitcode::ERR_IOERR);
}

async fn async_main(flags: Flags, global_args: Box<ArgMatches>) -> i32 {
    #[allow(unused)]
    match global_args.subcommand() {
        Some(("help", _)) => {
            print_help();
            exitcode::OK
        }
        Some(("version", _)) => {
            println!("{}.{}.{}", VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION);
            exitcode::OK
        }
        Some(("status", _)) => todo!(),
        Some(("set", cmd_args)) => todo!(),
        Some(("peer", cmd_args)) => todo!(),
        Some(("network", cmd_args)) => todo!(),
        Some(("join", cmd_args)) => todo!(),
        Some(("leave", cmd_args)) => todo!(),
        Some(("service", _)) => {
            drop(global_args); // free unnecessary heap before starting service as we're done with CLI args

            let test_inner = Arc::new(zerotier_network_hypervisor::vl1::DummyInnerProtocol::default());
            let test_path_filter = Arc::new(zerotier_network_hypervisor::vl1::DummyPathFilter::default());
            let datadir = open_datadir(&flags).await;
            let svc = VL1Service::new(datadir, test_inner, test_path_filter, zerotier_vl1_service::Settings::default()).await;
            if svc.is_ok() {
                let _ = tokio::signal::ctrl_c().await;
                println!("Terminate signal received, shutting down...");
                exitcode::OK
            } else {
                println!("FATAL: error launching service: {}", svc.err().unwrap().to_string());
                exitcode::ERR_IOERR
            }
        }
        Some(("identity", cmd_args)) => todo!(),
        Some(("rootset", cmd_args)) => cli::rootset::cmd(flags, cmd_args).await,
        _ => {
            eprintln!("Invalid command line. Use 'help' for help.");
            exitcode::ERR_USAGE
        }
    }
}

fn main() {
    let global_args = Box::new({
        Command::new("zerotier")
            .arg(Arg::new("json").short('j'))
            .arg(Arg::new("path").short('p').takes_value(true))
            .arg(Arg::new("token_path").short('t').takes_value(true))
            .arg(Arg::new("token").short('T').takes_value(true))
            .subcommand_required(true)
            .subcommand(Command::new("help"))
            .subcommand(Command::new("version"))
            .subcommand(Command::new("status"))
            .subcommand(
                Command::new("set")
                    .subcommand(Command::new("port").arg(Arg::new("port#").index(1).validator(utils::is_valid_port)))
                    .subcommand(Command::new("secondaryport").arg(Arg::new("port#").index(1).validator(utils::is_valid_port)))
                    .subcommand(
                        Command::new("blacklist")
                            .subcommand(
                                Command::new("cidr")
                                    .arg(Arg::new("ip_bits").index(1))
                                    .arg(Arg::new("boolean").index(2).validator(utils::is_valid_bool)),
                            )
                            .subcommand(
                                Command::new("if")
                                    .arg(Arg::new("prefix").index(1))
                                    .arg(Arg::new("boolean").index(2).validator(utils::is_valid_bool)),
                            ),
                    )
                    .subcommand(Command::new("portmap").arg(Arg::new("boolean").index(1).validator(utils::is_valid_bool))),
            )
            .subcommand(
                Command::new("peer")
                    .subcommand(Command::new("show").arg(Arg::new("address").index(1).required(true)))
                    .subcommand(Command::new("list"))
                    .subcommand(Command::new("listroots"))
                    .subcommand(Command::new("try")),
            )
            .subcommand(
                Command::new("network")
                    .subcommand(Command::new("show").arg(Arg::new("nwid").index(1).required(true)))
                    .subcommand(Command::new("list"))
                    .subcommand(
                        Command::new("set")
                            .arg(Arg::new("nwid").index(1).required(true))
                            .arg(Arg::new("setting").index(2).required(false))
                            .arg(Arg::new("value").index(3).required(false)),
                    ),
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
                    .subcommand(
                        Command::new("sign")
                            .arg(Arg::new("identity").index(1).required(true))
                            .arg(Arg::new("path").index(2).required(true)),
                    )
                    .subcommand(
                        Command::new("verify")
                            .arg(Arg::new("identity").index(1).required(true))
                            .arg(Arg::new("path").index(2).required(true))
                            .arg(Arg::new("signature").index(3).required(true)),
                    ),
            )
            .subcommand(
                Command::new("rootset")
                    .subcommand(Command::new("add").arg(Arg::new("path").index(1).required(true)))
                    .subcommand(Command::new("remove").arg(Arg::new("name").index(1).required(true)))
                    .subcommand(Command::new("list"))
                    .subcommand(
                        Command::new("sign")
                            .arg(Arg::new("path").index(1).required(true))
                            .arg(Arg::new("secret").index(2).required(true)),
                    )
                    .subcommand(Command::new("verify").arg(Arg::new("path").index(1).required(true)))
                    .subcommand(Command::new("marshal").arg(Arg::new("path").index(1).required(true)))
                    .subcommand(Command::new("restoredefault")),
            )
            .override_help(crate::cmdline_help::make_cmdline_help().as_str())
            .override_usage("")
            .disable_version_flag(true)
            .disable_help_subcommand(false)
            .disable_help_flag(true)
            .try_get_matches_from(std::env::args())
            .unwrap_or_else(|e| {
                if e.kind() == clap::ErrorKind::DisplayHelp || e.kind() == clap::ErrorKind::MissingSubcommand {
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
                        eprintln!("Invalid command line. Use 'help' for help.");
                    } else {
                        if suggested.is_empty() {
                            eprintln!("Unrecognized option '{}'. Use 'help' for help.", invalid);
                        } else {
                            eprintln!(
                                "Unrecognized option '{}', did you mean {}? Use 'help' for help.",
                                invalid, suggested
                            );
                        }
                    }
                    std::process::exit(exitcode::ERR_USAGE);
                }
            })
    });

    let flags = Flags {
        json_output: global_args.is_present("json"),
        base_path: global_args
            .value_of("path")
            .map_or_else(platform_default_home_path, |p| p.to_string()),
        auth_token_path_override: global_args.value_of("token_path").map(|p| p.to_string()),
        auth_token_override: global_args.value_of("token").map(|t| t.to_string()),
    };

    std::process::exit(
        tokio::runtime::Builder::new_multi_thread()
            .enable_all()
            .build()
            .unwrap()
            .block_on(async_main(flags, global_args)),
    );
}
