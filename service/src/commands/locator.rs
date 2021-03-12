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

use zerotier_core::*;

use crate::store::Store;

fn new_<'a>(store: &Store, cli_args: &ArgMatches<'a>) -> i32 {
    let timestamp = cli_args.value_of("timestamp").map_or(crate::utils::ms_since_epoch(), |ts| {
        if ts.is_empty() {
            0_i64
        } else {
            i64::from_str_radix(ts, 10).unwrap_or(0_i64) * 1000_i64 // internally uses ms since epoch
        }
    });
    if timestamp <= 0 {
        println!("ERROR: invalid or empty timestamp specified.");
        return 1;
    }

    let identity = crate::utils::read_identity(cli_args.value_of("identity").unwrap(), true);
    if identity.is_err() {
        println!("ERROR: identity invalid: {}", identity.err().unwrap());
        return 1;
    }
    let identity = identity.unwrap();
    if !identity.has_private() {
        println!("ERROR: identity must include secret key to create and sign a locator.");
        return 1;
    }

    let endpoints_cli = cli_args.values_of("endpoint");
    if endpoints_cli.is_none() {
        println!("ERROR: at least one endpoint required.");
        return 1;
    }
    let mut endpoints: Vec<Endpoint> = Vec::new();
    let mut endpoint_bad = false;
    endpoints_cli.unwrap().for_each(|ep_str| {
        Endpoint::new_from_string(ep_str).map_or_else(|e| {
            println!("ERROR: endpoint {} invalid: {}", ep_str, e.to_str());
            endpoint_bad = true;
        }, |ep| {
            endpoints.push(ep);
        });
    });
    if endpoint_bad {
        return 1;
    }

    Locator::new(&identity, timestamp, &endpoints).map_or_else(|e| {
        println!("ERROR: failure creating locator: {}", e.to_str());
        1
    }, |loc| {
        println!("{}", loc.to_string());
        0
    })
}

fn verify<'a>(store: &Store, cli_args: &ArgMatches<'a>) -> i32 {
    let identity = crate::utils::read_identity(cli_args.value_of("identity").unwrap(), true);
    if identity.is_err() {
        println!("ERROR: identity invalid: {}", identity.err().unwrap());
        return 1;
    }
    let identity = identity.unwrap();
    let locator = crate::utils::read_locator(cli_args.value_of("locator").unwrap());
    if locator.is_err() {
        println!("ERROR: locator invalid: {}", locator.err().unwrap());
        return 1;
    }
    if locator.unwrap().verify(&identity) {
        println!("OK");
        0
    } else {
        println!("FAILED");
        1
    }
}

fn show<'a>(store: &Store, cli_args: &ArgMatches<'a>) -> i32 {
    let locator = crate::utils::read_locator(cli_args.value_of("locator").unwrap());
    if locator.is_err() {
        println!("ERROR: locator invalid: {}", locator.err().unwrap());
        return 1;
    }
    let locator = locator.unwrap();
    println!("{} timestamp {}", locator.signer().to_string(), (locator.timestamp() as f64) / 1000.0);
    let endpoints = locator.endpoints();
    for ep in endpoints.iter() {
        println!("  {}", (*ep).to_string())
    }
    0
}

pub(crate) fn run<'a>(store: &Store, cli_args: &ArgMatches<'a>, _: &Option<String>) -> i32 {
    match cli_args.subcommand() {
        ("new", Some(sub_cli_args)) => new_(store, sub_cli_args),
        ("verify", Some(sub_cli_args)) => verify(store, sub_cli_args),
        ("show", Some(sub_cli_args)) => show(store, sub_cli_args),
        _ => {
            crate::cli::print_help();
            1
        }
    }
}
