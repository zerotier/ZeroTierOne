/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::num::NonZeroU64;
use std::str::FromStr;
use std::hash::{Hash, Hasher};

use crate::error::InvalidFormatError;
use crate::util::buffer::Buffer;

#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
#[repr(transparent)]
pub struct MAC(NonZeroU64);

impl MAC {
    #[inline(always)]
    pub fn from_u64(i: u64) -> Option<MAC> { NonZeroU64::new(i & 0xffffffffffff).map(|i| MAC(i)) }

    #[inline(always)]
    pub fn from_bytes(b: &[u8]) -> Option<MAC> {
        if b.len() >= 6 {
            NonZeroU64::new((b[0] as u64) << 40 | (b[1] as u64) << 32 | (b[2] as u64) << 24 | (b[3] as u64) << 16 as u64 | (b[4] as u64) << 8 | b[5] as u64).map(|i| MAC(i))
        } else {
            None
        }
    }

    #[inline(always)]
    pub fn from_bytes_fixed(b: &[u8; 6]) -> Option<MAC> {
        NonZeroU64::new((b[0] as u64) << 40 | (b[1] as u64) << 32 | (b[2] as u64) << 24 | (b[3] as u64) << 16 as u64 | (b[4] as u64) << 8 | b[5] as u64).map(|i| MAC(i))
    }

    #[inline(always)]
    pub fn to_bytes(&self) -> [u8; 6] {
        let i = self.0.get();
        [(i >> 40) as u8, (i >> 32) as u8, (i >> 24) as u8, (i >> 16) as u8, (i >> 8) as u8, i as u8]
    }

    #[inline(always)]
    pub fn to_u64(&self) -> u64 { self.0.get() }

    #[inline(always)]
    pub(crate) fn marshal<const BL: usize>(&self, buf: &mut Buffer<BL>) -> std::io::Result<()> {
        buf.append_and_init_bytes_fixed(|b: &mut [u8; 6]| {
            let i = self.0.get();
            b[0] = (i >> 40) as u8;
            b[1] = (i >> 32) as u8;
            b[2] = (i >> 24) as u8;
            b[3] = (i >> 16) as u8;
            b[4] = (i >> 8) as u8;
            b[5] = i as u8;
        })
    }

    #[inline(always)]
    pub(crate) fn unmarshal<const BL: usize>(buf: &Buffer<BL>, cursor: &mut usize) -> std::io::Result<Option<Self>> {
        buf.read_bytes_fixed::<6>(cursor).map(|b| Self::from_bytes_fixed(b))
    }
}

impl ToString for MAC {
    fn to_string(&self) -> String {
        let b: [u8; 6] = self.to_bytes();
        format!("{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}", b[0], b[1], b[2], b[3], b[4], b[5])
    }
}

impl FromStr for MAC {
    type Err = InvalidFormatError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        MAC::from_bytes(crate::util::hex::from_string(s).as_slice()).map_or_else(|| Err(InvalidFormatError), |m| Ok(m))
    }
}

impl Hash for MAC {
    #[inline(always)]
    fn hash<H: Hasher>(&self, state: &mut H) {
        state.write_u64(self.0.get());
    }
}
