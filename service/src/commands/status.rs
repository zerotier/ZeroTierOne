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

use std::rc::Rc;
use std::str::FromStr;
use std::sync::Arc;

use hyper::Uri;

use crate::store::Store;
use crate::webclient::HttpClient;

pub(crate) async fn run(store: Arc<Store>, client: HttpClient, api_base_uri: Uri, auth_token: String) -> hyper::Result<i32> {
    let mut res = client.get(api_base_uri).await?;
    let body = hyper::body::to_bytes(res.body_mut()).await?;
    Ok(0)
}
