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
use std::rc::Rc;
use std::sync::Arc;

use crate::store::Store;
use clap::ArgMatches;

fn main() {
    let cli_args = cli::parse_cli_args();
    let store = || {
        //let json_output = cli_args.is_present("json"); // TODO
        let zerotier_path = cli_args.value_of("path").map_or_else(|| unsafe { zerotier_core::cstr_to_string(osdep::platformDefaultHomePath(), -1) }, |ztp| ztp.to_string());
        let store = Store::new(zerotier_path.as_str(), cli_args.value_of("token_path").map_or(None, |tp| Some(tp.to_string())), cli_args.value_of("token").map_or(None, |tok| Some(tok.trim().to_string())));
        if store.is_err() {
            eprintln!("FATAL: error accessing directory '{}': {}", zerotier_path, store.err().unwrap().to_string());
            std::process::exit(1);
        }
        Arc::new(store.unwrap())
    };
    std::process::exit(match cli_args.subcommand() {
        ("help", None) => {
            cli::print_help();
            0
        }
        ("version", None) => {
            let ver = zerotier_core::version();
            println!("{}.{}.{}", ver.0, ver.1, ver.2);
            0
        }
        ("status", None) => { 0 }
        ("set", Some(sub_cli_args)) => { 0 }
        ("peer", Some(sub_cli_args)) => { 0 }
        ("network", Some(sub_cli_args)) => { 0 }
        ("join", Some(sub_cli_args)) => { 0 }
        ("leave", Some(sub_cli_args)) => { 0 }
        ("service", None) => service::run(store()),
        ("controller", Some(sub_cli_args)) => { 0 }
        ("identity", Some(sub_cli_args)) => crate::commands::identity::run(sub_cli_args),
        ("locator", Some(sub_cli_args)) => crate::commands::locator::run(sub_cli_args),
        ("cert", Some(sub_cli_args)) => crate::commands::cert::run(store(), sub_cli_args),
        _ => {
            cli::print_help();
            1
        }
    });
}
