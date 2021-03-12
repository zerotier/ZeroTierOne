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
use zerotier_core::{IdentityType, Identity};

/*
  identity <command> [args]
    new [c25519 | p384]                    Create identity (default: c25519)
    getpublic <identity>                   Extract public part of identity
    fingerprint <identity>                 Get an identity's fingerprint
    validate <identity>                    Locally validate an identity
    sign <identity> <file>                 Sign a file with an identity's key
    verify <identity> <file> <sig>         Verify a signature

 */

fn new_<'a>(store: &Store, cli_args: &ArgMatches<'a>) -> i32 {
    let id_type = cli_args.value_of("type").map_or(IdentityType::Curve25519, |idt| {
        match idt {
            "p384" => IdentityType::NistP384,
            _ => IdentityType::Curve25519,
        }
    });
    let id = Identity::new_generate(id_type);
    if id.is_err() {
        println!("ERROR: identity generation failed: {}", id.err().unwrap().to_str());
        return 1;
    }
    println!("{}", id.ok().unwrap().to_secret_string());
    0
}

fn getpublic<'a>(store: &Store, cli_args: &ArgMatches<'a>) -> i32 {
    0
}

fn fingerprint<'a>(store: &Store, cli_args: &ArgMatches<'a>) -> i32 {
    0
}

fn validate<'a>(store: &Store, cli_args: &ArgMatches<'a>) -> i32 {
    0
}

fn sign<'a>(store: &Store, cli_args: &ArgMatches<'a>) -> i32 {
    0
}

fn verify<'a>(store: &Store, cli_args: &ArgMatches<'a>) -> i32 {
    0
}

pub(crate) fn run<'a>(store: &Store, cli_args: &ArgMatches<'a>, _: &Option<String>) -> i32 {
    match cli_args.subcommand() {
        ("new", Some(sub_cli_args)) => new_(store, sub_cli_args),
        ("getpublic", Some(sub_cli_args)) => getpublic(store, sub_cli_args),
        ("fingerprint", Some(sub_cli_args)) => fingerprint(store, sub_cli_args),
        ("validate", Some(sub_cli_args)) => validate(store, sub_cli_args),
        ("sign", Some(sub_cli_args)) => sign(store, sub_cli_args),
        ("verify", Some(sub_cli_args)) => verify(store, sub_cli_args),
        _ => {
            crate::cli::print_help();
            1
        }
    }
}
