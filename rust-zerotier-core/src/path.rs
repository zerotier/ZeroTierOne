/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

use serde::{Deserialize, Serialize};

use crate::*;
use crate::bindings::capi as ztcore;

#[derive(Serialize, Deserialize)]
pub struct Path {
    pub endpoint: Endpoint,
    #[serde(rename = "lastSend")]
    pub last_send: i64,
    #[serde(rename = "lastReceive")]
    pub last_receive: i64,
    pub alive: bool,
    pub preferred: bool
}

impl Path {
    #[inline(always)]
    pub(crate) fn new_from_capi(p: &ztcore::ZT_Path) -> Path {
        Path{
            endpoint: Endpoint::new_from_capi(&p.endpoint),
            last_send: p.lastSend,
            last_receive: p.lastReceive,
            alive: p.alive != 0,
            preferred: p.preferred != 0
        }
    }
}
