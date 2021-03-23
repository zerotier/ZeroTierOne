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
use std::rc::Rc;
use std::str::FromStr;
use std::sync::Arc;

use hyper::{Uri, Method, StatusCode};
use colored::*;

use crate::store::Store;
use crate::httpclient::*;
use crate::service::ServiceStatus;
use crate::{GlobalFlags, HTTP_API_OBJECT_SIZE_LIMIT};

pub(crate) async fn run(store: Arc<Store>, global_flags: GlobalFlags, client: HttpClient, api_base_uri: Uri, auth_token: String) -> Result<i32, Box<dyn Error>> {
    let uri = append_uri_path(api_base_uri, "/status").unwrap();
    let mut res = request(&client, Method::GET, uri, None, auth_token.as_str()).await?;

    match res.status() {
        StatusCode::OK => {
            let status = read_object_limited::<ServiceStatus>(res.body_mut(), HTTP_API_OBJECT_SIZE_LIMIT).await?;

            if global_flags.json_output {
                println!("{}", serde_json::to_string_pretty(&status).unwrap())
            } else {
                println!("address {} version {} status {}",
                    status.address.to_string().as_str().bright_white(),
                    status.version.as_str().bright_white(),
                    if status.online {
                        "ONLINE".bright_green()
                    } else {
                        "OFFLINE".bright_red()
                    });
                // TODO: print more detailed status information
            }

            Ok(0)
        },
        _ => Err(Box::new(UnexpectedStatusCodeError(res.status(), "")))
    }
}
