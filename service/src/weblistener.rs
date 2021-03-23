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

use std::cell::RefCell;
use std::convert::Infallible;
use std::net::SocketAddr;

use hyper::{Body, Request, Response};
use hyper::server::Server;
use hyper::service::{make_service_fn, service_fn};
use tokio::task::JoinHandle;

use crate::service::Service;

#[cfg(target_os = "linux")]
use std::os::unix::io::AsRawFd;

/// Handles API dispatch and other HTTP handler stuff.
async fn web_handler(service: Service, req: Request<Body>) -> Result<Response<Body>, Infallible> {
    Ok(Response::new("Hello, World".into()))
}

/// Listener for http connections to the API or for TCP P2P.
/// Dropping a listener initiates shutdown of the background hyper Server instance,
/// but it might not shut down instantly as this occurs asynchronously.
pub(crate) struct WebListener {
    pub address: SocketAddr,
    shutdown_tx: RefCell<Option<tokio::sync::oneshot::Sender<()>>>,
    server: JoinHandle<hyper::Result<()>>,
}

impl WebListener {
    /// Create a new "background" TCP WebListener using the current tokio reactor async runtime.
    pub async fn new(_device_name: &str, address: SocketAddr, service: &Service) -> Result<WebListener, Box<dyn std::error::Error>> {
        let listener = if addr.is_ipv4() {
            let listener = socket2::Socket::new(socket2::Domain::ipv4(), socket2::Type::stream(), Some(socket2::Protocol::tcp()));
            if listener.is_err() {
                return Err(Box::new(listener.err().unwrap()));
            }
            let listener = listener.unwrap();
            #[cfg(unix)] {
                let _ = listener.set_reuse_port(true);
            }
            listener
        } else {
            let listener = socket2::Socket::new(socket2::Domain::ipv6(), socket2::Type::stream(), Some(socket2::Protocol::tcp()));
            if listener.is_err() {
                return Err(Box::new(listener.err().unwrap()));
            }
            let listener = listener.unwrap();
            #[cfg(unix)] {
                let _ = listener.set_reuse_port(true);
            }
            let _ = listener.set_only_v6(true);
            listener
        };

        #[cfg(target_os = "linux")] {
            if !_device_name.is_empty() {
                let sock = listener.as_raw_fd();
                unsafe {
                    let _ = std::ffi::CString::new(_device_name).map(|dn| {
                        let dnb = dn.as_bytes_with_nul();
                        let _ = crate::osdep::setsockopt(sock as std::os::raw::c_int, crate::osdep::SOL_SOCKET as std::os::raw::c_int, crate::osdep::SO_BINDTODEVICE as std::os::raw::c_int, dnb.as_ptr().cast(), (dnb.len() - 1) as crate::osdep::socklen_t);
                    });
                }
            }
        }

        let addr = socket2::SockAddr::from(address);
        if let Err(e) = listener.bind(&addr) {
            return Err(Box::new(e));
        }
        if let Err(e) = listener.listen(128) {
            return Err(Box::new(e));
        }
        let listener = listener.into_tcp_listener();

        let builder = Server::from_tcp(listener);
        if builder.is_err() {
            return Err(Box::new(builder.err().unwrap()));
        }
        let builder = builder.unwrap().http1_half_close(false).http1_keepalive(true).http1_max_buf_size(131072);

        let (shutdown_tx, shutdown_rx) = tokio::sync::oneshot::channel::<()>();
        let service = service.clone();
        let server = tokio::task::spawn(builder.serve(make_service_fn(move |_| {
            let service = service.clone();
            async move {
                Ok::<_, Infallible>(service_fn(move |req: Request<Body>| {
                    let service = service.clone();
                    async move {
                        web_handler(service, req).await
                    }
                }))
            }
        })).with_graceful_shutdown(async { let _ = shutdown_rx.await; }));

        Ok(WebListener {
            address,
            shutdown_tx: RefCell::new(Some(shutdown_tx)),
            server,
        })
    }
}

impl Drop for WebListener {
    fn drop(&mut self) {
        let _ = self.shutdown_tx.take().map(|tx| {
            let _ = tx.send(());
            self.server.abort();
        });
    }
}
