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

#[allow(non_snake_case,non_upper_case_globals,non_camel_case_types,dead_code,improper_ctypes)]
mod osdep; // bindgen generated

use std::boxed::Box;
use std::ffi::CStr;
use std::path::Path;
use std::sync::Arc;

use crate::store::Store;

fn main() {
    let mut process_exit_value: i32 = 0;

    let mut cli_args = Some(Box::new(cli::parse_cli_args()));
    let mut zerotier_path = unsafe { zerotier_core::cstr_to_string(osdep::platformDefaultHomePath(), -1) };

    let json_output: bool;
    let mut auth_token: Option<String> = None;
    let mut auth_token_path: Option<String> = None;
    {
        let a = cli_args.as_ref().unwrap();
        json_output = a.is_present("json");
        let v = a.value_of("path");
        if v.is_some() {
            zerotier_path = String::from(v.unwrap());
        }
        let v = a.value_of("token");
        if v.is_some() {
            auth_token = Some(v.unwrap().trim().to_string());
        }
        let v = a.value_of("token_path");
        if v.is_some() {
            auth_token_path = Some(v.unwrap().to_string());
        }
    }

    let store = Store::new(zerotier_path.as_str());
    if store.is_err() {
        eprintln!("FATAL: error accessing directory '{}': {}", zerotier_path, store.err().unwrap().to_string());
        std::process::exit(1);
    }
    let store = Arc::new(store.unwrap());

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
        auth_token = Some(auth_token.unwrap().trim().to_string());
    }

    match cli_args.as_ref().unwrap().subcommand_name().unwrap() {
        "version" => {
            let ver = zerotier_core::version();
            println!("{}.{}.{}", ver.0, ver.1, ver.2);
        },
        "service" => {
            cli_args = None; // free any memory we can when launching service
            process_exit_value = service::run(&store, auth_token);
        },
        _ => cli::print_help(), // includes "help"
    }

    std::process::exit(process_exit_value);
}
