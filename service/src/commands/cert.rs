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

use std::str::FromStr;
use std::sync::Arc;

use clap::ArgMatches;
use dialoguer::Input;
use zerotier_core::*;

use crate::store::Store;

fn list(store: &Arc<Store>) -> i32 {
    0
}

fn show<'a>(store: &Arc<Store>, cli_args: &ArgMatches<'a>) -> i32 {
    0
}

fn newsuid(cli_args: Option<&ArgMatches>) -> i32 {
    let key_pair = Certificate::new_key_pair(CertificatePublicKeyAlgorithm::ECDSANistP384);
    if key_pair.is_err() {
        println!("ERROR: internal error creating key pair: {}", key_pair.err().unwrap().to_str());
        1
    } else {
        let (_, privk) = key_pair.ok().unwrap();
        let privk_hex = hex::encode(privk);
        let path = cli_args.map_or("", |cli_args| { cli_args.value_of("path").unwrap_or("") });
        if path.is_empty() {
            println!("{}", privk_hex);
            0
        } else {
            std::fs::write(path, privk_hex.as_bytes()).map_or_else(|e| {
                eprintln!("FATAL: error writing '{}': {}", path, e.to_string());
                e.raw_os_error().unwrap_or(1)
            }, |_| {
                println!("Subject unique ID secret written to: {} (public is included)", path);
                0
            })
        }
    }
}

fn newcsr(cli_args: &ArgMatches) -> i32 {
    let theme = &dialoguer::theme::SimpleTheme;

    let subject_unique_id: String = Input::with_theme(theme)
        .with_prompt("Path to subject unique ID secret key (empty to create unsigned subject)")
        .allow_empty(true)
        .interact_text()
        .unwrap_or_default();
    let subject_unique_id_private_key = if subject_unique_id.is_empty() {
        None
    } else {
        let b = crate::utils::read_limit(subject_unique_id, 1024);
        if b.is_err() {
            println!("ERROR: unable to read subject unique ID secret file: {}", b.err().unwrap().to_string());
            return 1;
        }
        let privk_hex = String::from_utf8(b.unwrap());
        if privk_hex.is_err() {
            println!("ERROR: invalid UTF-8 in secret");
            return 1;
        }
        let privk = hex::decode(privk_hex.unwrap().trim());
        if privk.is_err() || privk.as_ref().unwrap().is_empty() {
            println!("ERROR: invalid unique ID secret: {}", privk.err().unwrap().to_string());
            return 1;
        }
        Some(privk.unwrap())
    };

    let timestamp: i64 = Input::with_theme(theme)
        .with_prompt("Subject timestamp (seconds since epoch)")
        .with_initial_text((crate::utils::ms_since_epoch() / 1000).to_string())
        .allow_empty(false)
        .interact_text()
        .unwrap_or(0);
    if timestamp < 0 {
        println!("ERROR: invalid timestamp");
        return 1;
    }

    println!("Subject identities");
    let mut identities: Vec<CertificateIdentity> = Vec::new();
    loop {
        let identity: String = Input::with_theme(theme)
            .with_prompt(format!("  [{}] Identity or path to identity (empty to end)", identities.len() + 1))
            .allow_empty(true)
            .interact_text()
            .unwrap_or_default();
        if identity.is_empty() {
            break;
        }
        let identity = crate::utils::read_identity(identity.as_str(), true);
        if identity.is_err() {
            println!("ERROR: identity invalid or unable to read from file.");
            return 1;
        }
        let identity = identity.unwrap();
        if identity.has_private() {
            println!("ERROR: identity contains private key, use public only for CSR!");
            return 1;
        }

        let locator: String = Input::with_theme(theme)
            .with_prompt(format!("  [{}] Locator or path to locator for {} (optional)", identities.len() + 1, identity.address.to_string()))
            .allow_empty(true)
            .interact_text()
            .unwrap_or_default();
        let locator = if locator.is_empty() {
            None
        } else {
            let l = crate::utils::read_locator(locator.as_str());
            if l.is_err() {
                println!("ERROR: locator invalid: {}", l.err().unwrap());
                return 1;
            }
            let l = l.ok();
            if !l.as_ref().unwrap().verify(&identity) {
                println!("ERROR: locator was not signed by this identity.");
                return 1;
            }
            l
        };

        identities.push(CertificateIdentity {
            identity,
            locator,
        });
    }

    println!("Subject networks (empty to end)");
    let mut networks: Vec<CertificateNetwork> = Vec::new();
    loop {
        let nwid: String = Input::with_theme(theme)
            .with_prompt(format!("  [{}] Network ID (empty to end)", networks.len() + 1))
            .allow_empty(true)
            .interact_text()
            .unwrap_or_default();
        if nwid.len() != 16 {
            break;
        }
        let nwid = NetworkId::from(nwid.as_str());

        let fingerprint: String = Input::with_theme(theme)
            .with_prompt(format!("  [{}] Fingerprint of primary controller (optional)", networks.len() + 1))
            .allow_empty(true)
            .interact_text()
            .unwrap_or_default();
        let fingerprint = if fingerprint.is_empty() {
            None
        } else {
            let f = Fingerprint::new_from_string(fingerprint.as_str());
            if f.is_err() {
                println!("ERROR: fingerprint invalid: {}", f.err().unwrap().to_str());
                return 1;
            }
            f.ok()
        };

        networks.push(CertificateNetwork {
            id: nwid,
            controller: fingerprint,
        })
    }

    println!("Subject certificate update URLs");
    let mut update_urls: Vec<String> = Vec::new();
    loop {
        let url: String = Input::with_theme(theme)
            .with_prompt(format!("  [{}] URL (empty to end)", update_urls.len() + 1))
            .allow_empty(true)
            .interact_text()
            .unwrap_or_default();
        if url.is_empty() {
            break;
        }
        let url_parsed = hyper::Uri::from_str(url.as_str());
        if url_parsed.is_err() {
            println!("ERROR: invalid URL: {}", url_parsed.err().unwrap().to_string());
            return 1;
        }
        update_urls.push(url);
    }

    println!("Certificate name information (all fields are optional)");
    let name = CertificateName {
        serial_no: Input::with_theme(theme).with_prompt("  Serial").allow_empty(true).interact_text().unwrap_or_default(),
        common_name: Input::with_theme(theme).with_prompt("  Common Name").allow_empty(true).interact_text().unwrap_or_default(),
        organization: Input::with_theme(theme).with_prompt("  Organization").allow_empty(true).interact_text().unwrap_or_default(),
        unit: Input::with_theme(theme).with_prompt("  Organizational Unit").allow_empty(true).interact_text().unwrap_or_default(),
        country: Input::with_theme(theme).with_prompt("  Country").allow_empty(true).interact_text().unwrap_or_default(),
        province: Input::with_theme(theme).with_prompt("  State/Province").allow_empty(true).interact_text().unwrap_or_default(),
        locality: Input::with_theme(theme).with_prompt("  Locality").allow_empty(true).interact_text().unwrap_or_default(),
        street_address: Input::with_theme(theme).with_prompt("  Street Address").allow_empty(true).interact_text().unwrap_or_default(),
        postal_code: Input::with_theme(theme).with_prompt("  Postal Code").allow_empty(true).interact_text().unwrap_or_default(),
        email: Input::with_theme(theme).with_prompt("  E-Mail").allow_empty(true).interact_text().unwrap_or_default(),
        url: Input::with_theme(theme).with_prompt("  URL (informational)").allow_empty(true).interact_text().unwrap_or_default(),
        host: Input::with_theme(theme).with_prompt("  Host").allow_empty(true).interact_text().unwrap_or_default(),
    };

    let subject = CertificateSubject {
        timestamp,
        identities,
        networks,
        update_urls,
        name,
        unique_id: Vec::new(),
        unique_id_signature: Vec::new(),
    };
    let (pubk, privk) = Certificate::new_key_pair(CertificatePublicKeyAlgorithm::ECDSANistP384).ok().unwrap();
    subject.new_csr(pubk.as_ref(), subject_unique_id_private_key.as_ref().map(|k| k.as_ref())).map_or_else(|e| {
        println!("ERROR: error creating CRL: {}", e.to_str());
        1
    }, |csr| {
        let csr_path = cli_args.value_of("csrpath").unwrap();
        std::fs::write(csr_path, csr).map_or_else(|e| {
            println!("ERROR: unable to write CSR: {}", e.to_string());
            1
        }, |_| {
            let secret_path = cli_args.value_of("secretpath").unwrap();
            std::fs::write(secret_path, hex::encode(privk)).map_or_else(|e| {
                let _ = std::fs::remove_file(csr_path);
                println!("ERROR: unable to write secret: {}", e.to_string());
                1
            }, |_| {
                println!("CSR written to '{}', certificate secret to '{}'", csr_path, secret_path);
                0
            })
        })
    })
}

fn sign<'a>(store: &Arc<Store>, cli_args: &ArgMatches<'a>) -> i32 {
    0
}

fn verify<'a>(store: &Arc<Store>, cli_args: &ArgMatches<'a>) -> i32 {
    0
}

fn dump<'a>(store: &Arc<Store>, cli_args: &ArgMatches<'a>) -> i32 {
    0
}

fn import<'a>(store: &Arc<Store>, cli_args: &ArgMatches<'a>) -> i32 {
    0
}

fn factoryreset(store: &Arc<Store>) -> i32 {
    0
}

fn export<'a>(store: &Arc<Store>, cli_args: &ArgMatches<'a>) -> i32 {
    0
}

fn delete<'a>(store: &Arc<Store>, cli_args: &ArgMatches<'a>) -> i32 {
    0
}

pub(crate) fn run(store: Arc<Store>, cli_args: &ArgMatches) -> i32 {
    match cli_args.subcommand() {
        ("list", None) => list(&store),
        ("show", Some(sub_cli_args)) => show(&store, sub_cli_args),
        ("newsuid", sub_cli_args) => newsuid(sub_cli_args),
        ("newcsr", Some(sub_cli_args)) => newcsr(sub_cli_args),
        ("sign", Some(sub_cli_args)) => sign(&store, sub_cli_args),
        ("verify", Some(sub_cli_args)) => verify(&store, sub_cli_args),
        ("dump", Some(sub_cli_args)) => dump(&store, sub_cli_args),
        ("import", Some(sub_cli_args)) => import(&store, sub_cli_args),
        ("factoryreset", None) => factoryreset(&store),
        ("export", Some(sub_cli_args)) => export(&store, sub_cli_args),
        ("delete", Some(sub_cli_args)) => delete(&store, sub_cli_args),
        _ => {
            crate::print_help();
            1
        }
    }
}
