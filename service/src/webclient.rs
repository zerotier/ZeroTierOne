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

use std::error::Error;
use std::future::Future;
use std::str::FromStr;
use std::time::Duration;
use std::rc::Rc;
use std::sync::Arc;

use hyper::{Uri, Response, Body, Method, Request, StatusCode};
use crate::store::Store;

pub(crate) type HttpClient = Rc<hyper::Client<hyper::client::HttpConnector, Body>>;

/// Launch the supplied function with a ready to go HTTP client, the auth token, and the API URI.
/// This is boilerplate code for CLI commands that invoke the HTTP API. Since it instantiates and
/// then kills a tokio runtime, it's not for use in the service code that runs in a long-running
/// tokio runtime.
pub(crate) fn run_command<
    R: Future<Output = hyper::Result<i32>>,
    F: FnOnce(Arc<Store>, HttpClient, Uri, String) -> R
>(store: Arc<Store>, func: F) -> i32 {
    let rt = tokio::runtime::Builder::new_current_thread().enable_all().build().unwrap();
    let code = rt.block_on(async move {
        let uri = store.load_uri();
        if uri.is_err() {
            println!("ERROR: 'zerotier.uri' not found in '{}', unable to get service API endpoint.", store.base_path.to_str().unwrap());
            1
        } else {
            let auth_token = store.auth_token(false);
            if auth_token.is_err() {
                println!("ERROR: unable to read API authorization token from '{}': {}", store.base_path.to_str().unwrap(), auth_token.err().unwrap().to_string());
                1
            } else {
                let uri = uri.unwrap();
                let uri_str = uri.to_string();
                func(store, Rc::new(hyper::Client::new()), uri, auth_token.unwrap()).await.map_or_else(|e| {
                    println!("ERROR: service API HTTP request failed: {}", e.to_string());
                    println!("ZeroTier service may not be running or '{}' may be unreachable.", uri_str);
                    1
                }, |code| {
                    code
                })
            }
        }
    });
    rt.shutdown_timeout(Duration::from_millis(1)); // all tasks should be done in a command anyway, this is just a sanity check
    code
}

/// Send a request to the API with support for HTTP digest authentication.
/// The data option is for PUT and POST requests. For GET it is ignored. Errors indicate total
/// failure such as connection refused. A returned result must still have its status checked. If
/// it's 401 (unauthorized) it likely means the auth_token is wrong.
pub(crate) async fn request<D: AsRef<[u8]>>(client: &HttpClient, method: Method, uri: Uri, data: D, auth_token: String) -> Result<Response<Body>, Box<dyn Error>> {
    let body = data.as_ref().to_vec();

    let req = Request::builder().method(&method).version(hyper::Version::HTTP_11).uri(&uri).body(Body::from(body.clone()));
    if req.is_err() {
        return Err(Box::new(req.err().unwrap()));
    }
    let res = client.request(req.unwrap()).await;
    if res.is_err() {
        return Err(Box::new(res.err().unwrap()));
    }
    let res = res.unwrap();

    if res.status() == StatusCode::UNAUTHORIZED {
        let auth = res.headers().get(hyper::header::WWW_AUTHENTICATE);
        if auth.is_none() {
            return Ok(res); // return the 401 reply
        }
        let auth = auth.unwrap().to_str();
        if auth.is_err() {
            return Err(Box::new(auth.err().unwrap()));
        }
        let mut auth = digest_auth::parse(auth.unwrap());
        if auth.is_err() {
            return Err(Box::new(auth.err().unwrap()));
        }
        let ac = digest_auth::AuthContext::new("zerotier", auth_token, uri.to_string());
        let auth = auth.unwrap().respond(&ac);
        if auth.is_err() {
            return Err(Box::new(auth.err().unwrap()));
        }

        let req = Request::builder().method(&method).version(hyper::Version::HTTP_11).uri(&uri).header(hyper::header::WWW_AUTHENTICATE, auth.unwrap().to_header_string()).body(Body::from(body));
        if req.is_err() {
            return Err(Box::new(req.err().unwrap()));
        }
        let res = client.request(req.unwrap()).await;
        if res.is_err() {
            return Err(Box::new(res.err().unwrap()));
        }
        return Ok(res.unwrap());
    }

    return Ok(res);
}
