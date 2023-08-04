/*
 * Copyright (c)2022 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */

use thiserror::Error;

#[derive(Error, Debug)]
pub enum ZeroIDCError {
    #[error(transparent)]
    DiscoveryError(#[from] openidconnect::DiscoveryError<openidconnect::reqwest::Error<reqwest::Error>>),

    #[error(transparent)]
    ParseError(#[from] url::ParseError),
}

#[derive(Error, Debug)]
#[error("SSO Exchange Error: {message:}")]
pub struct SSOExchangeError {
    message: String,
}

impl SSOExchangeError {
    pub fn new(message: String) -> Self {
        SSOExchangeError { message }
    }
}
