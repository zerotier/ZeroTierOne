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

use std::any::Any;
use std::cell::RefCell;
use std::convert::Infallible;
use std::net::{SocketAddr, TcpListener};
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};

use futures::TryFutureExt;
use hyper::{Body, Request, Response};
use hyper::server::Server;
use hyper::service::{make_service_fn, service_fn};
use net2::TcpBuilder;
#[cfg(unix)] use net2::unix::UnixTcpBuilderExt;

use zerotier_core::InetAddress;

use crate::service::Service;

#[inline(always)]
async fn web_handler(service: Service, req: Request<Body>) -> Result<Response<Body>, Infallible> {
    Ok(Response::new("Hello, World".into()))
}

/// Listener for http connections to the API or for TCP P2P.
pub(crate) struct WebListener {
    shutdown_tx: RefCell<Option<tokio::sync::oneshot::Sender<()>>>,
    server: Box<dyn Any>,
}

impl WebListener {
    /// Create a new "background" TCP WebListener using the current tokio reactor async runtime.
    pub fn new(_device_name: &str, addr: SocketAddr, service: &Service) -> Result<WebListener, Box<dyn std::error::Error>> {
        let listener = if addr.is_ipv4() {
            let l = TcpBuilder::new_v4();
            if l.is_err() {
                return Err(Box::new(l.err().unwrap()));
            }
            let l = l.unwrap();
            #[cfg(unix)] {
                let _ = l.reuse_port(true);
            }
            l
        } else {
            let l = TcpBuilder::new_v6();
            if l.is_err() {
                return Err(Box::new(l.err().unwrap()));
            }
            let l = l.unwrap();
            let _ = l.only_v6(true);
            #[cfg(unix)] {
                let _ = l.reuse_port(true);
            }
            l
        };
        // TODO: bind to device on Linux?
        let listener = listener.bind(addr);
        if listener.is_err() {
            return Err(Box::new(listener.err().unwrap()));
        }
        let listener = listener.unwrap().listen(128);
        if listener.is_err() {
            return Err(Box::new(listener.err().unwrap()));
        }
        let listener = listener.unwrap();

        let builder = Server::from_tcp(listener);
        if builder.is_err() {
            return Err(Box::new(builder.err().unwrap()));
        }
        let builder = builder.unwrap()
            .http1_half_close(false)
            .http1_keepalive(true)
            .http1_max_buf_size(131072);

        let (shutdown_tx, shutdown_rx) = tokio::sync::oneshot::channel::<()>();
        let service = service.clone();
        let server = builder.serve(make_service_fn(move |_| {
            let service = service.clone();
            async move {
                Ok::<_, Infallible>(service_fn(move |req: Request<Body>| {
                    let service = service.clone();
                    async move {
                        web_handler(service, req).await
                    }
                }))
            }
        })).with_graceful_shutdown(async { let _ = shutdown_rx.await; });

        Ok(WebListener {
            shutdown_tx: RefCell::new(Some(shutdown_tx)),
            server: Box::new(server),
        })
    }
}

impl Drop for WebListener {
    fn drop(&mut self) {
        let _ = self.shutdown_tx.take().map(|tx| { tx.send(()); });
    }
}
