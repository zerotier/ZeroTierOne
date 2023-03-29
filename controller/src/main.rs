// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::path::Path;
use std::str::FromStr;
use std::sync::Arc;

use zerotier_network_controller::database::Database;
use zerotier_network_controller::filedatabase::FileDatabase;
use zerotier_network_controller::Controller;
use zerotier_network_hypervisor::vl1::identity::IdentitySecret;
use zerotier_network_hypervisor::{VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION};
use zerotier_service::vl1::{VL1Service, VL1Settings};
use zerotier_utils::exitcode;
use zerotier_utils::tokio;
use zerotier_utils::tokio::runtime::Runtime;

async fn run(database: Arc<dyn Database>, identity: IdentitySecret, runtime: &Runtime) -> i32 {
    match Controller::new(runtime.handle().clone(), identity.clone(), database.clone()).await {
        Err(err) => {
            eprintln!("FATAL: error initializing handler: {}", err.to_string());
            exitcode::ERR_CONFIG
        }
        Ok(handler) => match VL1Service::new(identity, handler.clone(), VL1Settings::default()) {
            Err(err) => {
                eprintln!("FATAL: error launching service: {}", err.to_string());
                exitcode::ERR_IOERR
            }
            Ok(svc) => {
                svc.node.init_default_roots();
                handler.start(&svc).await;
                zerotier_utils::wait_for_process_abort();
                println!("Terminate signal received, shutting down...");
                exitcode::OK
            }
        },
    }
}

fn main() {
    const REQUIRE_ONE_OF_ARGS: [&'static str; 2] = ["postgres", "filedb"];
    let global_args = clap::Command::new("zerotier-controller")
        .arg(
            clap::Arg::new("identity")
                .short('i')
                .long("identity")
                .takes_value(true)
                .forbid_empty_values(true)
                .value_name("identity")
                .help(Some("Path to secret ZeroTier identity"))
                .required(true),
        )
        .arg(
            clap::Arg::new("logfile")
                .short('l')
                .long("logfile")
                .takes_value(true)
                .forbid_empty_values(true)
                .value_name("logfile")
                .help(Some("Path to log file"))
                .required(false),
        )
        .arg(
            clap::Arg::new("filedb")
                .short('f')
                .long("filedb")
                .takes_value(true)
                .forbid_empty_values(true)
                .value_name("filedb")
                .help(Some("Use filesystem database at path"))
                .required_unless_present_any(&REQUIRE_ONE_OF_ARGS),
        )
        .arg(
            clap::Arg::new("postgres")
                .short('p')
                .long("postgres")
                .takes_value(true)
                .forbid_empty_values(true)
                .value_name("postgres")
                .help(Some("Connect to postgres with supplied URL"))
                .required_unless_present_any(&REQUIRE_ONE_OF_ARGS),
        )
        .version(format!("{}.{}.{}", VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION).as_str())
        .arg_required_else_help(true)
        .try_get_matches_from(std::env::args())
        .unwrap_or_else(|e| {
            let _ = e.print();
            std::process::exit(exitcode::ERR_USAGE);
        });

    if let Ok(tokio_runtime) = tokio::runtime::Builder::new_multi_thread().enable_all().build() {
        tokio_runtime.block_on(async {
            let identity = if let Ok(identity_data) = tokio::fs::read(global_args.value_of("identity").unwrap()).await {
                if let Ok(identity) = IdentitySecret::from_str(String::from_utf8_lossy(identity_data.as_slice()).as_ref()) {
                    identity
                } else {
                    eprintln!("FATAL: invalid secret identity");
                    std::process::exit(exitcode::ERR_CONFIG);
                }
            } else {
                eprintln!("FATAL: unable to read secret identity");
                std::process::exit(exitcode::ERR_IOERR);
            };

            let db: Arc<dyn Database> = if let Some(filedb_path) = global_args.value_of("filedb") {
                let file_db = FileDatabase::new(Path::new(filedb_path), global_args.value_of("logfile").map(|l| Path::new(l))).await;
                if file_db.is_err() {
                    eprintln!(
                        "FATAL: unable to open filesystem database at {}: {}",
                        filedb_path,
                        file_db.as_ref().err().unwrap().to_string()
                    );
                    std::process::exit(exitcode::ERR_IOERR)
                }
                file_db.unwrap()
            } else if let Some(_postgres_url) = global_args.value_of("postgres") {
                panic!("not implemented yet");
            } else {
                eprintln!("FATAL: no database type selected.");
                std::process::exit(exitcode::ERR_USAGE);
            };

            std::process::exit(run(db, identity, &tokio_runtime).await);
        });
    } else {
        eprintln!("FATAL: can't start async runtime");
        std::process::exit(exitcode::ERR_IOERR)
    }
}
