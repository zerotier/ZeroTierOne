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

use crate::MAC;
use std::cmp::Ordering;

#[derive(Clone, PartialEq, Eq)]
pub struct MulticastGroup {
    pub mac: MAC,
    pub adi: u32,
}

impl Ord for MulticastGroup {
    fn cmp(&self, other: &Self) -> Ordering {
        let o1 = self.mac.0.cmp(&other.mac.0);
        if o1 == Ordering::Equal {
            self.adi.cmp(&other.adi)
        } else {
            o1
        }
    }
}

impl PartialOrd for MulticastGroup {
    #[inline(always)]
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}
