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

use std::io::Write;
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

fn newsid(cli_args: Option<&ArgMatches>) -> i32 {
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
        }, |_| {
            0
        })
    }
}

fn newcsr(cli_args: &ArgMatches) -> i32 {
    let theme = &dialoguer::theme::SimpleTheme;

    let subject_unique_id: String = Input::with_theme(theme)
        .with_prompt("Path to subject unique ID secret key (recommended)")
        .allow_empty(true)
        .interact_text()
        .unwrap_or_default();
    let subject_unique_id: Option<CertificateSubjectUniqueIdSecret> = if subject_unique_id.is_empty() {
        None
    } else {
        let b = crate::utils::read_limit(subject_unique_id, 16384);
        if b.is_err() {
            println!("ERROR: unable to read subject unique ID secret file: {}", b.err().unwrap().to_string());
            return 1;
        }
        let json = String::from_utf8(b.unwrap());
        if json.is_err() {
            println!("ERROR: invalid subject unique ID secret: {}", json.err().unwrap().to_string());
            return 1;
        }
        let sid = CertificateSubjectUniqueIdSecret::new_from_json(json.unwrap().as_str());
        if sid.is_err() {
            println!("ERROR: invalid subject unique ID secret: {}", sid.err().unwrap());
            return 1;
        }
        Some(sid.unwrap())
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

    println!("Identities to include in subject");
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
                println!("ERROR: locator not signed by this identity.");
                return 1;
            }
            l
        };

        identities.push(CertificateIdentity {
            identity,
            locator,
        });
    }

    println!("Networks to include in subject (empty to end)");
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
        let nwid = NetworkId::new_from_string(nwid.as_str());

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

    println!("Certificates to reference in subject (empty to end)");
    let mut certificates: Vec<CertificateSerialNo> = Vec::new();
    loop {
        let sn: String = Input::with_theme(theme)
            .with_prompt(format!("  [{}] Certificate serial number (empty to end)", certificates.len() + 1))
            .allow_empty(true)
            .interact_text()
            .unwrap_or_default();
        if sn.is_empty() {
            break;
        }
        let sn = CertificateSerialNo::new_from_string(sn.as_str());
        if sn.is_err() {
            println!("ERROR: invalid certificate serial number: {}", sn.err().unwrap().to_str());
            return 1;
        }
        certificates.push(sn.ok().unwrap());
    }

    println!("URLs to check for updated certificates for this subject");
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

    println!("Certificate \"name\" (same as X509 certificates, all fields optional)");
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
        certificates,
        update_urls,
        name,
        unique_id: Vec::new(),
        unique_id_proof_signature: Vec::new(),
    };
    subject.new_csr(subject_unique_id.as_ref()).map_or(1, |csr| {
        let p = cli_args.value_of("path").unwrap();
        std::fs::write(p, csr).map_or_else(|e| {
            println!("ERROR: unable to write CSR: {}", e.to_string());
            1
        }, |_| {
            println!("CSR written to {}", p);
            0
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

pub(crate) fn run<'a>(store: Arc<Store>, cli_args: &ArgMatches<'a>) -> i32 {
    match cli_args.subcommand() {
        ("list", None) => list(&store),
        ("show", Some(sub_cli_args)) => show(&store, sub_cli_args),
        ("newsid", sub_cli_args) => newsid(sub_cli_args),
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
