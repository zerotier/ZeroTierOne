/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::hash::{Hash, Hasher};
use std::num::NonZeroU64;
use std::str::FromStr;

use crate::error::InvalidFormatError;
use crate::util::hex::HEX_CHARS;
use crate::vl1::protocol::{ADDRESS_RESERVED_PREFIX, ADDRESS_SIZE};
use crate::vl1::buffer::Buffer;

#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
#[repr(transparent)]
pub struct Address(NonZeroU64);

impl Address {
    /// Get an address from a 64-bit integer or return None if it is zero or reserved.
    #[inline(always)]
    pub fn from_u64(mut i: u64) -> Option<Address> {
        i &= 0xffffffffff;
        if i != 0 && (i >> 32) != ADDRESS_RESERVED_PREFIX as u64 {
            Some(Address(unsafe { NonZeroU64::new_unchecked(i) }))
        } else {
            None
        }
    }

    #[inline(always)]
    pub fn from_bytes(b: &[u8]) -> Option<Address> {
        if b.len() >= ADDRESS_SIZE {
            let i = (b[0] as u64) << 32 | (b[1] as u64) << 24 | (b[2] as u64) << 16 | (b[3] as u64) << 8 | b[4] as u64;
            if i != 0 && (i >> 32) != ADDRESS_RESERVED_PREFIX as u64 {
                Some(Address(unsafe { NonZeroU64::new_unchecked(i) }))
            } else {
                None
            }
        } else {
            None
        }
    }

    #[inline(always)]
    pub fn from_bytes_fixed(b: &[u8; ADDRESS_SIZE]) -> Option<Address> {
        let i = (b[0] as u64) << 32 | (b[1] as u64) << 24 | (b[2] as u64) << 16 | (b[3] as u64) << 8 | b[4] as u64;
        if i != 0 && (i >> 32) != ADDRESS_RESERVED_PREFIX as u64 {
            Some(Address(unsafe { NonZeroU64::new_unchecked(i) }))
        } else {
            None
        }
    }

    #[inline(always)]
    pub fn to_bytes(&self) -> [u8; ADDRESS_SIZE] {
        let i = self.0.get();
        [(i >> 32) as u8, (i >> 24) as u8, (i >> 16) as u8, (i >> 8) as u8, i as u8]
    }

    #[inline(always)]
    pub fn to_u64(&self) -> u64 { self.0.get() }

    #[inline(always)]
    pub(crate) fn marshal<const BL: usize>(&self, buf: &mut Buffer<BL>) -> std::io::Result<()> {
        buf.append_and_init_bytes_fixed(|b: &mut [u8; ADDRESS_SIZE]| {
            let i = self.0.get();
            b[0] = (i >> 32) as u8;
            b[1] = (i >> 24) as u8;
            b[2] = (i >> 16) as u8;
            b[3] = (i >> 8) as u8;
            b[4] = i as u8;
        })
    }

    #[inline(always)]
    pub(crate) fn unmarshal<const BL: usize>(buf: &Buffer<BL>, cursor: &mut usize) -> std::io::Result<Option<Self>> {
        buf.read_bytes_fixed::<{ ADDRESS_SIZE }>(cursor).map(|b| Self::from_bytes_fixed(b))
    }
}

impl ToString for Address {
    fn to_string(&self) -> String {
        let mut v = self.0.get() << 24;
        let mut s = String::with_capacity(ADDRESS_SIZE * 2);
        for _ in 0..(ADDRESS_SIZE * 2) {
            s.push(HEX_CHARS[(v >> 60) as usize] as char);
            v <<= 4;
        }
        s
    }
}

impl FromStr for Address {
    type Err = InvalidFormatError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Address::from_bytes(crate::util::hex::from_string(s).as_slice()).map_or_else(|| Err(InvalidFormatError), |a| Ok(a))
    }
}

impl Hash for Address {
    #[inline(always)]
    fn hash<H: Hasher>(&self, state: &mut H) {
        state.write_u64(self.0.get());
    }
}
