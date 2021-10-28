use std::sync::{Arc, Mutex};
use std::thread::{sleep, spawn, JoinHandle};
use std::time::Duration;

pub struct ZeroIDC {
    inner: Arc<Mutex<Inner>>,
}

struct Inner {
    running: bool,
    oidc_thread: Option<JoinHandle<()>>,
}

impl ZeroIDC {
    fn new() -> ZeroIDC {
        ZeroIDC {
            inner: Arc::new(Mutex::new(Inner {
                running: false,
                oidc_thread: None,
            })),
        }
    }

    fn start(&mut self) {
        let local = Arc::clone(&self.inner);

        if !(*local.lock().unwrap()).running {
            let inner_local = Arc::clone(&self.inner);
            (*local.lock().unwrap()).oidc_thread = Some(spawn(move || {
                (*inner_local.lock().unwrap()).running = true;

                while (*inner_local.lock().unwrap()).running {
                    println!("tick");
                    sleep(Duration::from_secs(1));
                }

                println!("thread done!")
            }));
        }
    }

    fn stop(&mut self) {
        let local = self.inner.clone();
        if (*local.lock().unwrap()).running {
            if let Some(u) = (*local.lock().unwrap()).oidc_thread.take() {
                u.join().expect("join failed");
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn zeroidc_new() -> Box<ZeroIDC> {
    Box::new(ZeroIDC::new())
}

#[no_mangle]
pub extern "C" fn zeroidc_delete(_: Option<Box<ZeroIDC>>) {}

#[no_mangle]
pub extern "C" fn zeroidc_start(idc: &'static mut ZeroIDC) {
    idc.start();
}

#[no_mangle]
pub extern "C" fn zeroidc_stop(idc: &'static mut ZeroIDC) {
    idc.stop();
}
