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
mod log;
mod store;
mod network;
mod vnic;

#[allow(non_snake_case,non_upper_case_globals,non_camel_case_types,dead_code,improper_ctypes)]
mod osdep; // bindgen generated

use std::boxed::Box;
use std::ffi::CStr;
use std::path::Path;

fn main() {
    let mut process_exit_value: i32 = 0;
    let mut zerotier_path = unsafe { zerotier_core::cstr_to_string(osdep::platformDefaultHomePath(), 256) };
    let mut cli_args = Some(Box::new(cli::parse_cli_args()));

    let json_output;
    let mut token: Option<String> = None;
    let mut token_path = Path::new(&zerotier_path).join("authtoken.secret");
    {
        let a = cli_args.as_ref().unwrap();
        json_output = a.is_present("json");
        let v = a.value_of("path");
        if v.is_some() {
            zerotier_path = String::from(v.unwrap());
        }
        let v = a.value_of("token");
        if v.is_some() {
            token = Some(String::from(v.unwrap().trim()));
        }
        let v = a.value_of("token_path");
        if v.is_some() {
            token_path = Path::new(v.unwrap().trim()).to_path_buf();
        }
    }

    match cli_args.as_ref().unwrap().subcommand_name().unwrap() {
        "version" => {
            let ver = zerotier_core::version();
            println!("{}.{}.{}", ver.0, ver.1, ver.2);
        },
        "service" => {
            cli_args = None; // free any memory we can when launching service
            process_exit_value = commands::service::run()
        },
        _ => cli::print_help(), // includes "help"
    }

    std::process::exit(process_exit_value);
}
