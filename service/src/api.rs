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

use crate::service::Service;
use hyper::{Request, Body, StatusCode, Response, Method};

pub(crate) fn status(service: Service, req: Request<Body>) -> (StatusCode, Body) {
    if req.method() == Method::GET {
        let status = service.status();
        if status.is_none() {
            (StatusCode::SERVICE_UNAVAILABLE, Body::from("node shutdown in progress"))
        } else {
            (StatusCode::OK, Body::from(serde_json::to_string(status.as_ref().unwrap()).unwrap()))
        }
    } else {
        (StatusCode::METHOD_NOT_ALLOWED, Body::from("/status allows method(s): GET"))
    }
}

pub(crate) fn peer(service: Service, req: Request<Body>) -> (StatusCode, Body) {
    (StatusCode::NOT_IMPLEMENTED, Body::from(""))
}

pub(crate) fn network(service: Service, req: Request<Body>) -> (StatusCode, Body) {
    (StatusCode::NOT_IMPLEMENTED, Body::from(""))
}
