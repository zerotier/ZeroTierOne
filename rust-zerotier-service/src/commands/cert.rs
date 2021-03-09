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

use clap::ArgMatches;
use crate::store::Store;
use zerotier_core::{CertificateSubjectUniqueIdSecret, CertificateUniqueIdType};
use std::io::Write;

#[inline(always)]
fn list(store: &Store, auth_token: &Option<String>) -> i32 {
    0
}

#[inline(always)]
fn show<'a>(store: &Store, cli_args: &ArgMatches<'a>, auth_token: &Option<String>) -> i32 {
    0
}

#[inline(always)]
fn newsid<'a>(store: &Store, cli_args: Option<&ArgMatches<'a>>, auth_token: &Option<String>) -> i32 {
    let sid = CertificateSubjectUniqueIdSecret::new(CertificateUniqueIdType::NistP384); // right now there's only one type
    let sid = sid.to_json();
    let path = cli_args.map_or("", |cli_args| { cli_args.value_of("path").unwrap_or("") });
    if path.is_empty() {
        let _ = std::io::stdout().write_all(sid.as_bytes());
        0
    } else {
        std::fs::write(path, sid.as_bytes()).map_or_else(|e| {
            eprintln!("FATAL: error writing '{}': {}", path, e.to_string());
            e.raw_os_error().unwrap_or(1)
        }, || {
            0
        })
    }
}

#[inline(always)]
fn sign<'a>(store: &Store, cli_args: &ArgMatches<'a>, auth_token: &Option<String>) -> i32 {
    0
}

#[inline(always)]
fn verify<'a>(store: &Store, cli_args: &ArgMatches<'a>, auth_token: &Option<String>) -> i32 {
    0
}

#[inline(always)]
fn dump<'a>(store: &Store, cli_args: &ArgMatches<'a>, auth_token: &Option<String>) -> i32 {
    0
}

#[inline(always)]
fn import<'a>(store: &Store, cli_args: &ArgMatches<'a>, auth_token: &Option<String>) -> i32 {
    0
}

#[inline(always)]
fn restore(store: &Store, auth_token: &Option<String>) -> i32 {
    0
}

#[inline(always)]
fn export<'a>(store: &Store, cli_args: &ArgMatches<'a>, auth_token: &Option<String>) -> i32 {
    0
}

#[inline(always)]
fn delete<'a>(store: &Store, cli_args: &ArgMatches<'a>, auth_token: &Option<String>) -> i32 {
    0
}

pub(crate) fn run<'a>(store: &Store, cli_args: &ArgMatches<'a>, auth_token: &Option<String>) -> i32 {
    match cli_args.subcommand() {
        ("list", None) => list(store, auth_token),
        ("show", Some(sub_cli_args)) => show(store, sub_cli_args, auth_token),
        ("newsid", sub_cli_args) => newsid(store, sub_cli_args, auth_token),
        ("newcsr", Some(sub_cli_args)) => newcsr(store, sub_cli_args, auth_token),
        ("sign", Some(sub_cli_args)) => sign(store, sub_cli_args, auth_token),
        ("verify", Some(sub_cli_args)) => verify(store, sub_cli_args, auth_token),
        ("dump", Some(sub_cli_args)) => dump(store, sub_cli_args, auth_token),
        ("import", Some(sub_cli_args)) => import(store, sub_cli_args, auth_token),
        ("restore", None) => restore(store, auth_token),
        ("export", Some(sub_cli_args)) => export(store, sub_cli_args, auth_token),
        ("delete", Some(sub_cli_args)) => delete(store, sub_cli_args, auth_token),
        _ => {
            crate::cli::print_help();
            1
        }
    }
}
