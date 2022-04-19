/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2022 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

#[derive(Clone, Copy, Eq, PartialEq)]
#[repr(u8)]
pub enum MessageType {
    /// No operation, payload ignored.
    Nop = 0_u8,

    /// msg::Init (msgpack)
    Init = 1_u8,

    /// msg::InitResponse (msgpack)
    InitResponse = 2_u8,

    /// <full record key>[<full record key>...]
    HaveRecords = 3_u8,

    /// <u8 length of each key prefix in bytes>[<key>...]
    GetRecords = 4_u8,

    /// <record>
    Record = 5_u8,

    /// msg::SyncRequest (msgpack)
    SyncRequest = 6_u8,

    /// msg::Sync (msgpack)
    Sync = 7_u8,
}

const MESSAGE_TYPE_MAX: u8 = 7;

impl From<u8> for MessageType {
    /// Get a type from a byte, returning the Nop type if the byte is out of range.
    #[inline(always)]
    fn from(b: u8) -> Self {
        if b <= MESSAGE_TYPE_MAX {
            unsafe { std::mem::transmute(b) }
        } else {
            Self::Nop
        }
    }
}

impl MessageType {
    #[allow(unused)]
    pub fn name(&self) -> &'static str {
        match *self {
            Self::Nop => "NOP",
            Self::Init => "INIT",
            Self::InitResponse => "INIT_RESPONSE",
            Self::HaveRecords => "HAVE_RECORDS",
            Self::GetRecords => "GET_RECORDS",
            Self::Record => "RECORD",
            Self::SyncRequest => "SYNC_REQUEST",
            Self::Sync => "SYNC",
        }
    }
}

/// Msgpack serializable message types.
/// Some that are frequently transferred use shortened names to save bandwidth.
pub mod msg {
    use serde::{Deserialize, Serialize};

    #[derive(Serialize, Deserialize)]
    pub struct IPv4 {
        pub ip: [u8; 4],
        pub port: u16,
    }

    #[derive(Serialize, Deserialize)]
    pub struct IPv6 {
        pub ip: [u8; 16],
        pub port: u16,
    }

    #[derive(Serialize, Deserialize)]
    pub struct Init<'a> {
        /// A random challenge to be hashed with a secret to detect and drop connections to self.
        #[serde(with = "serde_bytes")]
        pub anti_loopback_challenge: &'a [u8],

        /// A random challenge for checking the data set domain.
        #[serde(with = "serde_bytes")]
        pub domain_challenge: &'a [u8],

        /// A random challenge for login/authentication.
        #[serde(with = "serde_bytes")]
        pub auth_challenge: &'a [u8],

        /// Optional name to advertise for this node.
        pub node_name: &'a str,

        /// Optional contact information for this node, such as a URL or an e-mail address.
        pub node_contact: &'a str,

        /// Port to which this node has locally bound.
        /// This is used to try to auto-detect whether a NAT is in the way.
        pub locally_bound_port: u16,

        /// An IPv4 address where this node can be reached.
        /// If both explicit_ipv4 and explicit_ipv6 are omitted the physical source IP:port may be used.
        pub explicit_ipv4: Option<IPv4>,

        /// An IPv6 address where this node can be reached.
        /// If both explicit_ipv4 and explicit_ipv6 are omitted the physical source IP:port may be used.
        pub explicit_ipv6: Option<IPv6>,
    }

    #[derive(Serialize, Deserialize)]
    pub struct InitResponse<'a> {
        /// HMAC-SHA512(local secret, anti_loopback_challenge) to detect and drop loops.
        #[serde(with = "serde_bytes")]
        pub anti_loopback_response: &'a [u8],

        /// HMAC-SHA512(SHA512(domain), domain_challenge) to check that the data set domain matches.
        #[serde(with = "serde_bytes")]
        pub domain_response: &'a [u8],

        /// HMAC-SHA512(secret, challenge) for authentication. (If auth is not enabled, an all-zero secret is used.)
        #[serde(with = "serde_bytes")]
        pub auth_response: &'a [u8],
    }

    #[derive(Serialize, Deserialize)]
    pub struct SyncRequest<'a> {
        /// Starting range to query, padded with zeroes if shorter than KEY_SIZE.
        #[serde(with = "serde_bytes")]
        #[serde(rename = "s")]
        pub range_start: &'a [u8],

        /// Ending range to query, padded with 0xff if shorter than KEY_SIZE.
        #[serde(with = "serde_bytes")]
        #[serde(rename = "e")]
        pub range_end: &'a [u8],

        /// Data-store-specific subset selector indicating what subset of items desired
        #[serde(with = "serde_bytes")]
        #[serde(rename = "q")]
        pub subset: Option<&'a [u8]>,
    }

    #[derive(Serialize, Deserialize)]
    pub struct Sync<'a> {
        /// Starting range summarized, padded with zeroes if shorter than KEY_SIZE.
        #[serde(with = "serde_bytes")]
        #[serde(rename = "s")]
        pub range_start: &'a [u8],

        /// Ending range summarized, padded with 0xff if shorter than KEY_SIZE.
        #[serde(with = "serde_bytes")]
        #[serde(rename = "e")]
        pub range_end: &'a [u8],

        /// Data-store-specific subset selector indicating what subset of items were included
        #[serde(with = "serde_bytes")]
        #[serde(rename = "q")]
        pub subset: Option<&'a [u8]>,

        /// Number of buckets in IBLT
        #[serde(rename = "b")]
        pub iblt_buckets: usize,

        /// Number of bytes in each IBLT item (key prefix)
        #[serde(rename = "l")]
        pub iblt_item_bytes: usize,

        /// Set summary for keys under prefix within subset
        #[serde(with = "serde_bytes")]
        #[serde(rename = "i")]
        pub iblt: &'a [u8],
    }
}
