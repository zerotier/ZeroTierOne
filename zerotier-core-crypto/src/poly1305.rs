/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use poly1305::universal_hash::{NewUniversalHash, UniversalHash};

/// The poly1305 message authentication function.
#[repr(transparent)]
pub struct Poly1305(poly1305::Poly1305);

pub const POLY1305_ONE_TIME_KEY_SIZE: usize = 32;
pub const POLY1305_MAC_SIZE: usize = 16;

impl Poly1305 {
    #[inline(always)]
    pub fn new(key: &[u8]) -> Option<Poly1305> {
        if key.len() == 32 {
            Some(Self(poly1305::Poly1305::new(poly1305::Key::from_slice(key))))
        } else {
            None
        }
    }

    #[inline(always)]
    pub fn update(&mut self, data: &[u8]) {
        self.0.update(poly1305::Block::from_slice(data));
    }

    #[inline(always)]
    pub fn finish(self) -> [u8; POLY1305_MAC_SIZE] {
        self.0.finalize().into_bytes().into()
    }
}
