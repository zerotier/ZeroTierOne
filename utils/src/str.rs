/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use crate::hex::HEX_CHARS;

/// Escape non-ASCII-printable characters in a string.
/// This also escapes quotes and other sensitive characters that cause issues on terminals.
pub fn escape(b: &[u8]) -> String {
    let mut s = String::with_capacity(b.len() * 2);
    for b in b.iter() {
        let b = *b;
        if b >= 43 && b <= 126 && b != 92 && b != 96 {
            s.push(b as char);
        } else {
            s.push('\\');
            s.push(HEX_CHARS[(b.wrapping_shr(4) & 0xf) as usize] as char);
            s.push(HEX_CHARS[(b & 0xf) as usize] as char);
        }
    }
    s
}

/// Unescape a string with \XX hexadecimal escapes.
pub fn unescape(s: &str) -> Vec<u8> {
    let mut b = Vec::with_capacity(s.len());
    let mut s = s.as_bytes();
    while let Some(c) = s.first() {
        let c = *c;
        if c == b'\\' {
            if s.len() < 3 {
                break;
            }
            let mut cc = 0u8;
            for c in [s[1], s[2]] {
                if c >= 48 && c <= 57 {
                    cc = cc.wrapping_shl(4) | (c - 48);
                } else if c >= 65 && c <= 70 {
                    cc = cc.wrapping_shl(4) | (c - 55);
                } else if c >= 97 && c <= 102 {
                    cc = cc.wrapping_shl(4) | (c - 87);
                }
            }
            b.push(cc);
            s = &s[3..];
        } else {
            b.push(c);
            s = &s[1..];
        }
    }
    b
}
