/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::sync::Arc;

use hyper::{Request, Body, StatusCode, Method};

use crate::service::Service;

pub(crate) fn status(service: Arc<Service>, req: Request<Body>) -> (StatusCode, Body) {
    if req.method() == Method::GET {
        service.status().map_or_else(|| {
            (StatusCode::SERVICE_UNAVAILABLE, Body::from("node shutdown in progress"))
        }, |status| {
            (StatusCode::OK, Body::from(serde_json::to_string(&status).unwrap()))
        })
    } else {
        (StatusCode::METHOD_NOT_ALLOWED, Body::from("/status allows method(s): GET"))
    }
}

pub(crate) fn config(service: Arc<Service>, req: Request<Body>) -> (StatusCode, Body) {
    let config = service.local_config();
    if req.method() == Method::POST || req.method() == Method::PUT {
        // TODO: diff config
    }
    (StatusCode::OK, Body::from(serde_json::to_string(config.as_ref()).unwrap()))
}

pub(crate) fn peer(service: Arc<Service>, req: Request<Body>) -> (StatusCode, Body) {
    (StatusCode::NOT_IMPLEMENTED, Body::from(""))
}

pub(crate) fn network(service: Arc<Service>, req: Request<Body>) -> (StatusCode, Body) {
    (StatusCode::NOT_IMPLEMENTED, Body::from(""))
}
