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
use std::sync::Arc;

use hyper::Uri;
use crate::store::Store;

pub(crate) fn run(store: Arc<Store>) -> i32 {
    crate::webclient::command(store.clone(), move |client, uri| {
        async move {
            let mut res = client.get(uri).await?;
            println!("status: {}", res.status().as_str());
            let body = hyper::body::to_bytes(res.body_mut()).await?;
            String::from_utf8(body.to_vec()).map(|body| {
                println!("body: {}", body.as_str());
            });
            Ok(0)
        }
    })
}
