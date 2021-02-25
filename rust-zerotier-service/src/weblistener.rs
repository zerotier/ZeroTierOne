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

use std::net::TcpListener;
use std::time::Duration;
use std::convert::Infallible;
use std::sync::Arc;

use zerotier_core::InetAddress;
use hyper::{Request, Response, Body};
use hyper::service::{make_service_fn, service_fn};
use futures::Future;
use futures::future::{AbortHandle, abortable};
use net2::TcpBuilder;

use crate::service::Service;

/// Listener for http connections to the API or for TCP P2P.
pub(crate) struct WebListener {
    server: dyn Future,
    abort_handle: AbortHandle,
}

impl WebListener {
    /// Create a new "background" TCP WebListener using the current tokio reactor async runtime.
    pub fn new(_device_name: &str, addr: &InetAddress, service: Arc<Service>) -> Result<WebListener, dyn std::error::Error> {
        let addr = addr.to_socketaddr();
        if addr.is_none() {
            return Err(std::io::Error::new(std::io::ErrorKind::AddrNotAvailable, "invalid address"));
        }
        let addr = addr.unwrap();

        let listener = if addr.is_ipv4() {
            let l = TcpBuilder::new_v4();
            if l.is_err() {
                return Err(l.err().unwrap());
            }
            l.unwrap()
        } else {
            let l = TcpBuilder::new_v6();
            if l.is_err() {
                return Err(l.err().unwrap());
            }
            let l = l.unwrap();
            l.only_v6(true);
            l
        };
        let listener = listener.bind(addr);
        if listener.is_err() {
            return Err(listener.err().unwrap());
        }
        let listener = listener.unwrap().listen(128);
        if listener.is_err() {
            return Err(listener.err().unwrap());
        }
        let listener = listener.unwrap();

        let builder = hyper::server::Server::from_tcp(listener);
        if builder.is_err() {
            return Err(builder.err().unwrap());
        }
        let builder = builder.unwrap()
            .executor(tokio::spawn)
            .http1_half_close(false)
            .http1_keepalive(true)
            .http1_max_buf_size(131072)
            .http2_keep_alive_interval(Duration::from_secs(30))
            .http2_keep_alive_timeout(Duration::from_secs(90))
            .http2_adaptive_window(true)
            .http2_max_frame_size(131072)
            .http2_max_concurrent_streams(16);

        let (server, abort_handle) = abortable(builder.serve(make_service_fn(|_| async move {
            Ok::<_, Infallible>(service_fn(|req: Request<Body>| -> Result<Response<Body>, Infallible> async move {
                Ok(Response::new("Hello, World".into()))
            }))
        })));

        Ok(WebListener {
            server,
            abort_handle,
        })
    }
}

impl Drop for WebListener {
    fn drop(&mut self) {
        self.abort_handle.abort();
        self.server.await;
    }
}
