/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

const BASE64_URL_SAFE_NO_PAD_ENGINE: base64::engine::fast_portable::FastPortable =
    base64::engine::fast_portable::FastPortable::from(&base64::alphabet::URL_SAFE, base64::engine::fast_portable::NO_PAD);

/// Encode base64 using URL-safe alphabet and no padding.
pub fn encode_url_nopad(bytes: &[u8]) -> String {
    base64::encode_engine(bytes, &BASE64_URL_SAFE_NO_PAD_ENGINE)
}

/// Decode base64 using URL-safe alphabet and no padding, or None on error.
pub fn decode_url_nopad(b64: &str) -> Option<Vec<u8>> {
    base64::decode_engine(b64, &BASE64_URL_SAFE_NO_PAD_ENGINE).ok()
}
