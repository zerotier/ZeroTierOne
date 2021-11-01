/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use clap::ArgMatches;

use zerotier_core::{Identity, IdentityType};

fn new_(cli_args: &ArgMatches) -> i32 {
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

fn getpublic(cli_args: &ArgMatches) -> i32 {
    let identity = crate::utils::read_identity(cli_args.value_of("identity").unwrap_or(""), false);
    identity.map_or_else(|e| {
        println!("ERROR: identity invalid: {}", e.to_string());
        1
    }, |id| {
        println!("{}", id.to_string());
        0
    })
}

fn fingerprint(cli_args: &ArgMatches) -> i32 {
    let identity = crate::utils::read_identity(cli_args.value_of("identity").unwrap_or(""), false);
    identity.map_or_else(|e| {
        println!("ERROR: identity invalid: {}", e.to_string());
        1
    }, |id| {
        println!("{}", id.fingerprint().to_string());
        0
    })
}

fn validate(cli_args: &ArgMatches) -> i32 {
    crate::utils::read_identity(cli_args.value_of("identity").unwrap_or(""), false).map_or_else(|e| {
        println!("FAILED");
        1
    }, |id| {
        if id.validate() {
            println!("OK");
            0
        } else {
            println!("FAILED");
            1
        }
    })
}

fn sign(cli_args: &ArgMatches) -> i32 {
    crate::utils::read_identity(cli_args.value_of("identity").unwrap_or(""), false).map_or_else(|e| {
        println!("ERROR: invalid or unreadable identity: {}", e.as_str());
        1
    }, |id| {
        if id.has_private() {
            std::fs::read(cli_args.value_of("path").unwrap()).map_or_else(|e| {
                println!("ERROR: unable to read file: {}", e.to_string());
                1
            }, |data| {
                id.sign(data.as_slice()).map_or_else(|e| {
                    println!("ERROR: failed to sign: {}", e.to_str());
                    1
                }, |sig| {
                    println!("{}", hex::encode(sig.as_ref()));
                    0
                })
            })
        } else {
            println!("ERROR: identity must include secret key to sign.");
            1
        }
    })
}

fn verify(cli_args: &ArgMatches) -> i32 {
    crate::utils::read_identity(cli_args.value_of("identity").unwrap_or(""), false).map_or_else(|e| {
        println!("ERROR: invalid or unreadable identity: {}", e.as_str());
        1
    }, |id| {
        std::fs::read(cli_args.value_of("path").unwrap()).map_or_else(|e| {
            println!("ERROR: unable to read file: {}", e.to_string());
            1
        }, |data| {
            hex::decode(cli_args.value_of("signature").unwrap()).map_or_else(|e| {
                println!("FAILED");
                1
            }, |sig| {
                if id.verify(data.as_slice(), sig.as_slice()) {
                    println!("OK");
                    0
                } else {
                    println!("FAILED");
                    1
                }
            })
        })
    })
}

pub(crate) fn run<'a>(cli_args: &ArgMatches<'a>) -> i32 {
    match cli_args.subcommand() {
        ("new", Some(sub_cli_args)) => new_(sub_cli_args),
        ("getpublic", Some(sub_cli_args)) => getpublic(sub_cli_args),
        ("fingerprint", Some(sub_cli_args)) => fingerprint(sub_cli_args),
        ("validate", Some(sub_cli_args)) => validate(sub_cli_args),
        ("sign", Some(sub_cli_args)) => sign(sub_cli_args),
        ("verify", Some(sub_cli_args)) => verify(sub_cli_args),
        _ => {
            crate::print_help(true);
            1
        }
    }
}
