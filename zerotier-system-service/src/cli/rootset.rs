// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::io::Write;

use clap::ArgMatches;

use crate::{exitcode, Flags};

use zerotier_network_hypervisor::util::marshalable::Marshalable;
use zerotier_network_hypervisor::vl1::RootSet;

pub async fn cmd(_: Flags, cmd_args: &ArgMatches) -> i32 {
    match cmd_args.subcommand() {
        Some(("trust", sc_args)) => todo!(),

        Some(("untrust", sc_args)) => todo!(),

        Some(("list", _)) => todo!(),

        Some(("sign", sc_args)) => {
            let path = sc_args.value_of("path");
            let secret_arg = sc_args.value_of("secret");
            if path.is_some() && secret_arg.is_some() {
                let path = path.unwrap();
                let secret_arg = secret_arg.unwrap();
                let secret = crate::utils::parse_cli_identity(secret_arg, true).await;
                let json_data = crate::utils::read_limit(path, crate::utils::DEFAULT_FILE_IO_READ_LIMIT).await;
                if secret.is_err() {
                    eprintln!("ERROR: unable to parse '{}' or read as a file.", secret_arg);
                    return exitcode::ERR_IOERR;
                }
                let secret = secret.unwrap();
                if !secret.secret.is_some() {
                    eprintln!("ERROR: identity does not include secret key, which is required for signing.");
                    return exitcode::ERR_IOERR;
                }
                if json_data.is_err() {
                    eprintln!("ERROR: unable to read '{}'.", path);
                    return exitcode::ERR_IOERR;
                }
                let json_data = json_data.unwrap();
                let root_set = serde_json::from_slice::<RootSet>(json_data.as_slice());
                if root_set.is_err() {
                    eprintln!("ERROR: root set JSON parsing failed: {}", root_set.err().unwrap().to_string());
                    return exitcode::ERR_IOERR;
                }
                let mut root_set = root_set.unwrap();
                if !root_set.sign(&secret) {
                    eprintln!("ERROR: root set signing failed, invalid identity?");
                    return exitcode::ERR_INTERNAL;
                }
                println!("{}", crate::utils::to_json_pretty(&root_set));
            } else {
                eprintln!("ERROR: 'rootset sign' requires a path to a root set in JSON format and a secret identity.");
                return exitcode::ERR_IOERR;
            }
        }

        Some(("verify", sc_args)) => {
            let path = sc_args.value_of("path");
            if path.is_some() {
                let path = path.unwrap();
                let json_data = crate::utils::read_limit(path, crate::utils::DEFAULT_FILE_IO_READ_LIMIT).await;
                if json_data.is_err() {
                    eprintln!("ERROR: unable to read '{}'.", path);
                    return exitcode::ERR_IOERR;
                }
                let json_data = json_data.unwrap();
                let root_set = serde_json::from_slice::<RootSet>(json_data.as_slice());
                if root_set.is_err() {
                    eprintln!("ERROR: root set JSON parsing failed: {}", root_set.err().unwrap().to_string());
                    return exitcode::ERR_IOERR;
                }
                let root_set = root_set.unwrap();
                if root_set.verify() {
                    println!("OK");
                } else {
                    println!("FAILED");
                    return exitcode::ERR_DATA_FORMAT;
                }
            } else {
                eprintln!("ERROR: 'rootset marshal' requires a path to a root set in JSON format.");
                return exitcode::ERR_IOERR;
            }
        }

        Some(("marshal", sc_args)) => {
            let path = sc_args.value_of("path");
            if path.is_some() {
                let path = path.unwrap();
                let json_data = crate::utils::read_limit(path, 1048576).await;
                if json_data.is_err() {
                    eprintln!("ERROR: unable to read '{}'.", path);
                    return exitcode::ERR_IOERR;
                }
                let json_data = json_data.unwrap();
                let root_set = serde_json::from_slice::<RootSet>(json_data.as_slice());
                if root_set.is_err() {
                    eprintln!("ERROR: root set JSON parsing failed: {}", root_set.err().unwrap().to_string());
                    return exitcode::ERR_IOERR;
                }
                let _ = std::io::stdout().write_all(root_set.unwrap().to_bytes().as_slice());
            } else {
                eprintln!("ERROR: 'rootset marshal' requires a path to a root set in JSON format.");
                return exitcode::ERR_IOERR;
            }
        }

        Some(("default", _)) => {
            let _ = std::io::stdout().write_all(crate::utils::to_json_pretty(&RootSet::zerotier_default()).as_bytes());
        }

        _ => panic!(),
    }
    return exitcode::OK;
}
