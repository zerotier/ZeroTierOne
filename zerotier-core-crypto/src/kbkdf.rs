/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use crate::secret::Secret;

// HMAC'd message is: preface | iteration[4], preface[2], label, 0x00, context, hash size[4]
// See: https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-108.pdf page 12

pub fn zt_kbkdf_hmac_sha384(key: &[u8], label: u8, context: u8, iter: u32) -> Secret<48> {
    Secret(crate::hash::hmac_sha384(
        key,
        &[
            (iter >> 24) as u8,
            (iter >> 16) as u8,
            (iter >> 8) as u8,
            iter as u8,
            b'Z',
            b'T', // can also be considered part of "label"
            label,
            0,
            context,
            0,
            0,
            0x01,
            0x80, // 384 bits
        ],
    ))
}

pub fn zt_kbkdf_hmac_sha512(key: &[u8], label: u8, context: u8, iter: u32) -> Secret<64> {
    Secret(crate::hash::hmac_sha512(
        key,
        &[
            (iter >> 24) as u8,
            (iter >> 16) as u8,
            (iter >> 8) as u8,
            iter as u8,
            b'Z',
            b'T', // can also be considered part of "label"
            label,
            0,
            context,
            0,
            0,
            0x01,
            0x80, // 384 bits
        ],
    ))
}
