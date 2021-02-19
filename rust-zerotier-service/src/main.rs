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

use std::ffi::CStr;
use std::path::Path;
use std::boxed::Box;
use std::sync::Arc;
use std::mem::MaybeUninit;
use std::os::raw::c_uint;

use crate::store::Store;

#[inline(always)]
pub(crate) fn sha512<T: AsRef<[u8]>>(data: T) -> [u8; 64] {
    unsafe {
        let mut r: MaybeUninit<[u8; 64]> = MaybeUninit::uninit();
        let d = data.as_ref();
        osdep::sha512(d.as_ptr().cast(), d.len() as c_uint, r.as_mut_ptr().cast());
        r.assume_init()
    }
}

#[inline(always)]
pub(crate) fn sha384<T: AsRef<[u8]>>(data: T) -> [u8; 48] {
    unsafe {
        let mut r: MaybeUninit<[u8; 48]> = MaybeUninit::uninit();
        let d = data.as_ref();
        osdep::sha384(d.as_ptr().cast(), d.len() as c_uint, r.as_mut_ptr().cast());
        r.assume_init()
    }
}

#[inline(always)]
pub(crate) fn ms_since_epoch() -> i64 {
    // This is easy to do in the Rust stdlib, but the version in OSUtils is probably faster.
    unsafe { osdep::msSinceEpoch() }
}

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
    if store.write_pid().is_err() {
        eprintln!("FATAL: error writing to directory '{}': unable to write zerotier.pid", zerotier_path);
        std::process::exit(1);
    }

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

    match cli_args.as_ref().subcommand_name().unwrap() {
        "version" => {
            let ver = zerotier_core::version();
            println!("{}.{}.{}", ver.0, ver.1, ver.2);
        },
        "service" => {
            drop(cli_args); // free unnecssary memory before launching service
            process_exit_value = service::run(&store, auth_token);
        },
        _ => cli::print_help(), // includes "help"
    }

    store.erase_pid();
    std::process::exit(process_exit_value);
}
