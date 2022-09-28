// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;

use std::time::Duration;

use zerotier_utils::exitcode;
use zerotier_vl1_service::VL1Service;

fn main() {
    std::process::exit(
        if let Ok(_tokio_runtime) = zerotier_utils::tokio::runtime::Builder::new_multi_thread().enable_all().build() {
            let test_inner = Arc::new(zerotier_network_hypervisor::vl1::DummyInnerProtocol::default());
            let test_path_filter = Arc::new(zerotier_network_hypervisor::vl1::DummyPathFilter::default());
            let datadir = open_datadir(&flags);
            let svc = VL1Service::new(datadir, test_inner, test_path_filter, zerotier_vl1_service::VL1Settings::default());
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
        } else {
            eprintln!("FATAL: error launching service: can't start async runtime");
            exitcode::ERR_IOERR
        },
    );
}
