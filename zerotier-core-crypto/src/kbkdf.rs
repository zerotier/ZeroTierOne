/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use crate::secret::Secret;

/*
 * HMAC'd message is: preface | iteration[4], preface[2], label, 0x00, context, hash size[4]
 *
 * Iteration and context are always zero here. Preface is 'ZT'. Hash size is in bits. Integers
 * larger than one byte are big-endian.
 *
 * See: https://csrc.nist.gov/publications/detail/sp/800-108/final (page 12)
 */

/// Derive a key using HMAC-SHA384 and a single byte label, ZeroTier variant with "ZT" preface.
pub fn zt_kbkdf_hmac_sha384(key: &[u8], label: u8) -> Secret<48> {
    Secret(crate::hash::hmac_sha384(key, &[0, 0, 0, 0, b'Z', b'T', label, 0, 0, 0, 0, 0x01, 0x80]))
}

/// Derive a key using HMAC-SHA512 and a single byte label, ZeroTier variant with "ZT" preface.
pub fn zt_kbkdf_hmac_sha512(key: &[u8], label: u8) -> Secret<64> {
    Secret(crate::hash::hmac_sha512(key, &[0, 0, 0, 0, b'Z', b'T', label, 0, 0, 0, 0, 0x02, 0x00]))
}
