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

mod api;
mod cli;
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
mod weblistener;

#[allow(non_snake_case, non_upper_case_globals, non_camel_case_types, dead_code, improper_ctypes)]
mod osdep; // bindgen generated

use std::boxed::Box;
use std::sync::Arc;

use crate::store::Store;
use clap::ArgMatches;

fn main() {
    let mut process_exit_value: i32 = 0;

    let cli_args = Box::new(cli::parse_cli_args());
    let mut zerotier_path = unsafe { zerotier_core::cstr_to_string(osdep::platformDefaultHomePath(), -1) };
    let mut auth_token: Option<String> = None;
    let mut auth_token_path: Option<String> = None;
    //let json_output = cli_args.is_present("json");
    let v = cli_args.value_of("path");
    if v.is_some() {
        zerotier_path = String::from(v.unwrap());
    }
    let v = cli_args.value_of("token");
    if v.is_some() {
        auth_token = Some(v.unwrap().trim().to_string());
    }
    let v = cli_args.value_of("token_path");
    if v.is_some() {
        auth_token_path = Some(v.unwrap().to_string());
    }

    let store = Store::new(zerotier_path.as_str());
    if store.is_err() {
        eprintln!("FATAL: error accessing directory '{}': {}", zerotier_path, store.err().unwrap().to_string());
        std::process::exit(1);
    }
    let store = Arc::new(store.unwrap());

    // From this point on we shouldn't call std::process::exit() since that would
    // fail to erase zerotier.pid from the working directory.

    if auth_token.is_none() {
        let t;
        if auth_token_path.is_some() {
            t = store.read_file_str(auth_token_path.unwrap().trim());
        } else {
            t = store.read_authtoken_secret();
        }
        if t.is_ok() {
            auth_token = Some(t.unwrap().trim().to_string());
        }
    } else {
        drop(auth_token_path);
        auth_token = Some(auth_token.unwrap().trim().to_string());
    }

    drop(zerotier_path);

    match cli_args.subcommand() {
        ("help", None) => {
            cli::print_help()
        }
        ("version", None) => {
            let ver = zerotier_core::version();
            println!("{}.{}.{}", ver.0, ver.1, ver.2);
        }
        ("status", None) => {}
        ("set", Some(sub_cli_args)) => {}
        ("peer", Some(sub_cli_args)) => {}
        ("network", Some(sub_cli_args)) => {}
        ("join", Some(sub_cli_args)) => {}
        ("leave", Some(sub_cli_args)) => {}
        ("service", None) => {
            drop(cli_args); // free unnecssary memory before launching service
            process_exit_value = service::run(&store, auth_token);
        }
        ("controller", Some(sub_cli_args)) => {}
        ("identity", Some(sub_cli_args)) => {}
        ("locator", Some(sub_cli_args)) => {
            process_exit_value = crate::commands::locator::run(&store, sub_cli_args, &auth_token);
        }
        ("cert", Some(sub_cli_args)) => {
            process_exit_value = crate::commands::cert::run(&store, sub_cli_args, &auth_token);
        }
        _ => {
            cli::print_help();
            process_exit_value = 1;
        }
    }

    std::process::exit(process_exit_value);
}
