// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::Duration;

use clap::{Arg, Command};

use zerotier_network_controller::database::Database;
use zerotier_network_controller::filedatabase::FileDatabase;
use zerotier_network_controller::handler::Handler;

use zerotier_network_hypervisor::{VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION};
use zerotier_utils::exitcode;
use zerotier_utils::tokio::runtime::Runtime;
use zerotier_vl1_service::VL1Service;

async fn run<DatabaseImpl: Database>(database: Arc<DatabaseImpl>, runtime: &Runtime) -> i32 {
    let handler = Handler::new(database.clone(), runtime.handle().clone(), todo!());

    let svc = VL1Service::new(
        database.clone(),
        handler.clone(),
        handler.clone(),
        zerotier_vl1_service::VL1Settings::default(),
    );
    if svc.is_ok() {
        let svc = svc.unwrap();
        svc.node().init_default_roots();

        // Wait for kill signal on Unix-like platforms.
        #[cfg(unix)]
        {
            let term = Arc::new(AtomicBool::new(false));
            let _ = signal_hook::flag::register(libc::SIGINT, term.clone());
            let _ = signal_hook::flag::register(libc::SIGTERM, term.clone());
            let _ = signal_hook::flag::register(libc::SIGQUIT, term.clone());
            while !term.load(Ordering::Relaxed) {
                std::thread::sleep(Duration::from_secs(1));
            }
        }

        println!("Terminate signal received, shutting down...");
        exitcode::OK
    } else {
        eprintln!("FATAL: error launching service: {}", svc.err().unwrap().to_string());
        exitcode::ERR_IOERR
    }
}

fn main() {
    const REQUIRE_ONE_OF_ARGS: [&'static str; 2] = ["postgres", "filedb"];
    let global_args = Command::new("zerotier-controller")
        .arg(
            Arg::new("filedb")
                .short('f')
                .long("filedb")
                .takes_value(true)
                .forbid_empty_values(true)
                .value_name("path")
                .help(Some("Use filesystem database at path"))
                .required_unless_present_any(&REQUIRE_ONE_OF_ARGS),
        )
        .arg(
            Arg::new("postgres")
                .short('p')
                .long("postgres")
                .takes_value(true)
                .forbid_empty_values(true)
                .value_name("path")
                .help(Some("Connect to postgres with parameters in JSON file"))
                .required_unless_present_any(&REQUIRE_ONE_OF_ARGS),
        )
        .version(format!("{}.{}.{}", VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION).as_str())
        .arg_required_else_help(true)
        .try_get_matches_from(std::env::args())
        .unwrap_or_else(|e| {
            let _ = e.print();
            std::process::exit(exitcode::ERR_USAGE);
        });

    if let Ok(tokio_runtime) = zerotier_utils::tokio::runtime::Builder::new_multi_thread().enable_all().build() {
        tokio_runtime.block_on(async {
            if let Some(filedb_base_path) = global_args.value_of("filedb") {
                std::process::exit(run(Arc::new(FileDatabase::new(filedb_base_path).await), &tokio_runtime).await);
            } else {
                eprintln!("FATAL: no database type selected.");
                std::process::exit(exitcode::ERR_USAGE);
            };
        });
    } else {
        eprintln!("FATAL: error launching service: can't start async runtime");
        std::process::exit(exitcode::ERR_IOERR)
    }
}
