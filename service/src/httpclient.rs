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
use std::rc::Rc;
use std::str::FromStr;
use std::sync::Arc;
use std::time::Duration;

use futures::stream::StreamExt;
use hyper::{Body, Method, Request, Response, StatusCode, Uri};
use hyper::http::uri::{Authority, PathAndQuery, Scheme};
use serde::de::DeserializeOwned;

use crate::GlobalFlags;
use crate::store::Store;

pub(crate) type HttpClient = Rc<hyper::Client<hyper::client::HttpConnector, Body>>;

#[derive(Debug)]
pub(crate) struct IncorrectAuthTokenError;

impl Error for IncorrectAuthTokenError {}

impl std::fmt::Display for IncorrectAuthTokenError {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "401 UNAUTHORIZED (incorrect authorization token or not allowed to read token)")
    }
}

#[derive(Debug)]
pub(crate) struct UnexpectedStatusCodeError(pub StatusCode, pub &'static str);

impl Error for UnexpectedStatusCodeError {}

impl std::fmt::Display for UnexpectedStatusCodeError {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        if self.1.is_empty() {
            write!(f, "unexpected status code: {} {}", self.0.as_str(), self.0.canonical_reason().unwrap_or("???"))
        } else {
            write!(f, "unexpected status code: {} {} ({})", self.0.as_str(), self.0.canonical_reason().unwrap_or("???"), self.1)
        }
    }
}

/// Launch the supplied function with a ready to go HTTP client, the auth token, and the API URI.
/// This is boilerplate code for CLI commands that invoke the HTTP API. Since it instantiates and
/// then kills a tokio runtime, it's not for use in the service code that runs in a long-running
/// tokio runtime.
pub(crate) fn run_command<
    R: Future<Output = Result<i32, Box<dyn Error>>>,
    F: FnOnce(Arc<Store>, GlobalFlags, HttpClient, Uri, String) -> R
>(store: Arc<Store>, global_flags: GlobalFlags, func: F) -> i32 {
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
                func(store, global_flags, Rc::new(hyper::Client::builder().http1_max_buf_size(65536).build_http()), uri, auth_token.unwrap()).await.map_or_else(|e| {
                    println!("ERROR: service API HTTP request ({}) failed: {}", uri_str, e);
                    println!();
                    println!("Common causes: service is not running, authorization token incorrect");
                    println!("or not readable, or a local firewall is blocking loopback connections.");
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
/// failure such as connection refused. A returned result must still have its status checked.
/// Note that if authorization is required and the auth token doesn't work, IncorrectAuthTokenError
/// is returned as an error instead of a 401 response object.
pub(crate) async fn request(client: &HttpClient, method: Method, uri: Uri, data: Option<&[u8]>, auth_token: &str) -> Result<Response<Body>, Box<dyn Error>> {
    let body: Vec<u8> = data.map_or_else(|| Vec::new(), |data| data.to_vec());

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
            return Err(Box::new(UnexpectedStatusCodeError(StatusCode::UNAUTHORIZED, "host returned 401 but no WWW-Authenticate header found")))
        }
        let auth = auth.unwrap().to_str();
        if auth.is_err() {
            return Err(Box::new(auth.err().unwrap()));
        }
        let auth = digest_auth::parse(auth.unwrap());
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
        let res = res.unwrap();

        if res.status() == StatusCode::UNAUTHORIZED {
            return Err(Box::new(IncorrectAuthTokenError));
        }

        return Ok(res);
    }

    return Ok(res);
}

/// Append to a URI path, returning None on error or a new Uri.
pub(crate) fn append_uri_path(uri: Uri, new_path: &str) -> Option<Uri> {
    let parts = uri.into_parts();
    let mut path = parts.path_and_query.map_or_else(|| String::new(), |pq| pq.to_string());
    while path.ends_with("/") {
        let _ = path.pop();
    }
    path.push_str(new_path);
    let path = PathAndQuery::from_str(path.as_str());
    if path.is_err() {
        None
    } else {
        Uri::builder()
            .scheme(parts.scheme.unwrap_or(Scheme::HTTP))
            .authority(parts.authority.unwrap_or(Authority::from_static("127.0.0.1")))
            .path_and_query(path.unwrap())
            .build()
            .map_or_else(|_| None, |uri| Some(uri))
    }
}

/// Read HTTP body with a size limit.
pub(crate) async fn read_body_limited(body: &mut Body, max_size: usize) -> Result<Vec<u8>, Box<dyn Error>> {
    let mut data: Vec<u8> = Vec::new();
    loop {
        let blk = body.next().await;
        if blk.is_some() {
            let blk = blk.unwrap();
            if blk.is_err() {
                return Err(Box::new(blk.err().unwrap()));
            }
            for b in blk.unwrap().iter() {
                data.push(*b);
                if data.len() >= max_size {
                    return Ok(data);
                }
            }
        } else {
            break;
        }
    }
    Ok(data)
}

pub(crate) async fn read_object_limited<O: DeserializeOwned>(body: &mut Body, max_size: usize) -> Result<O, Box<dyn Error>> {
    let data = read_body_limited(body, max_size).await?;
    let obj = serde_json::from_slice(data.as_slice());
    if obj.is_err() {
        Err(Box::new(obj.err().unwrap()))
    } else {
        Ok(obj.unwrap())
    }
}
