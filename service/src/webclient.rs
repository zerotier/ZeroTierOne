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
use std::time::Duration;
use hyper::Uri;
use std::sync::Arc;
use crate::store::Store;
use std::future::Future;

/// Launch the supplied function inside the tokio runtime.
/// The return value of this function should be the process exit code.
/// This is for implementation of commands that query the HTTP API, not HTTP
/// requests from a running server.
pub(crate) fn command<'a, R: Future<Output = hyper::Result<i32>>, F: FnOnce(Arc<hyper::Client<hyper::client::HttpConnector, hyper::Body>>, hyper::Uri) -> R>(store: Arc<Store>, func: F) -> i32 {
    let rt = tokio::runtime::Builder::new_current_thread().enable_all().build().unwrap();
    let code = rt.block_on(async move {
        let uri = store.load_uri();
        if uri.is_err() {
            println!("ERROR: unable to read 'zerotier.uri' to get local HTTP API address.");
            1
        } else {
            let f = func(Arc::new(hyper::Client::new()), uri.unwrap());
            f.await.map_or_else(|e| {
                println!("ERROR: HTTP request failed: {}", e.to_string());
                1
            }, |code| {
                code
            })
        }
    });
    rt.shutdown_timeout(Duration::from_millis(10));
    code
}
