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
use std::hash::{Hash, Hasher};

#[derive(Clone, PartialEq, Eq)]
pub struct MulticastGroup {
    pub mac: MAC,
    pub adi: u32,
}

impl ToString for MulticastGroup {
    fn to_string(&self) -> String {
        format!("{}/{}", self.mac.to_string(), self.adi)
    }
}

impl Hash for MulticastGroup {
    #[inline(always)]
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.mac.0.hash(state);
        self.adi.hash(state);
    }
}

impl Ord for MulticastGroup {
    fn cmp(&self, other: &Self) -> Ordering {
        if self.mac.0 < other.mac.0 {
            Ordering::Less
        } else if self.mac.0 > other.mac.0 {
            Ordering::Greater
        } else {
            if self.adi < other.adi {
                Ordering::Less
            } else if self.adi > other.adi {
                Ordering::Greater
            } else {
                Ordering::Equal
            }
        }
    }
}

impl PartialOrd for MulticastGroup {
    #[inline(always)]
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}
