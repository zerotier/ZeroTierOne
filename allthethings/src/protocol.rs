/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

pub const PROTOCOL_VERSION: u8 = 1;
pub const HASH_ALGORITHM_SHA384: u8 = 1;

pub const MESSAGE_TYPE_NOP: u8 = 0;
pub const MESSAGE_TYPE_HAVE_NEW_OBJECT: u8 = 1;
pub const MESSAGE_TYPE_OBJECT: u8 = 2;
pub const MESSAGE_TYPE_GET_OBJECTS: u8 = 3;

/// HELLO message, which is all u8's and is packed and so can be parsed directly in place.
/// This message is sent at the start of any connection by both sides.
#[repr(packed)]
pub struct Hello {
    pub hello_size: u8, // technically a varint but below 0x80
    pub protocol_version: u8,
    pub hash_algorithm: u8,
    pub flags: [u8; 4], // u32, little endian
    pub clock: [u8; 8], // u64, little endian
    pub last_object_receive_time: [u8; 8], // u64, little endian, u64::MAX if unspecified
    pub domain_hash: [u8; 48],
    pub instance_id: [u8; 16],
    pub loopback_check_code_salt: [u8; 16],
    pub loopback_check_code: [u8; 16],
}

#[cfg(test)]
mod tests {
    use std::mem::size_of;
    use crate::protocol::*;

    #[test]
    fn check_sizing() {
        // Make sure packed structures are really packed.
        assert_eq!(size_of::<Hello>(), 1 + 1 + 1 + 4 + 8 + 8 + 48 + 16 + 16 + 16);
    }
}
