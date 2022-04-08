/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2022 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

/// Number of bytes of SHA512 to announce, should be high enough to make collisions virtually impossible.
pub const ANNOUNCE_KEY_LEN: usize = 24;

/// Send SyncStatus this frequently, in milliseconds.
pub const SYNC_STATUS_PERIOD: i64 = 5000;

/// Check for and announce that we "have" records this often in milliseconds.
pub const ANNOUNCE_PERIOD: i64 = 100;

#[derive(Clone, Copy, Eq, PartialEq)]
#[repr(u8)]
pub enum MessageType {
    /// No operation, payload ignored.
    Nop = 0_u8,

    /// msg::Init (msgpack)
    Init = 1_u8,

    /// msg::InitResponse (msgpack)
    InitResponse = 2_u8,

    /// <u8 length of each key in bytes>[<key>...]
    HaveRecords = 3_u8,

    /// <u8 length of each key in bytes>[<key>...]
    GetRecords = 4_u8,

    /// <record>
    Record = 5_u8,

    /// msg::SyncStatus (msgpack)
    SyncStatus = 6_u8,

    /// msg::SyncRequest (msgpack)
    SyncRequest = 7_u8,

    /// msg::SyncResponse (msgpack)
    SyncResponse = 8_u8,
}

impl From<u8> for MessageType {
    /// Get a type from a byte, returning the Nop type if the byte is out of range.
    #[inline(always)]
    fn from(b: u8) -> Self {
        if b <= 7 {
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
            Self::SyncStatus => "SYNC_STATUS",
            Self::SyncRequest => "SYNC_REQUEST",
            Self::SyncResponse => "SYNC_RESPONSE",
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
    pub struct SyncStatus {
        /// Total number of records this node has in its data store.
        #[serde(rename = "c")]
        pub record_count: u64,

        /// Sending node's system clock.
        #[serde(rename = "t")]
        pub clock: u64,
    }

    #[derive(Serialize, Deserialize)]
    pub struct SyncRequest<'a> {
        /// Key range start (length: KEY_SIZE)
        #[serde(with = "serde_bytes")]
        #[serde(rename = "s")]
        pub range_start: &'a [u8],

        /// Key range end (length: KEY_SIZE)
        #[serde(with = "serde_bytes")]
        #[serde(rename = "e")]
        pub range_end: &'a [u8],

        /// Number of records requesting node already has under key range
        #[serde(rename = "c")]
        pub record_count: u64,

        /// Reference time for query
        #[serde(rename = "t")]
        pub reference_time: u64,

        /// Random salt
        #[serde(rename = "x")]
        pub salt: &'a [u8],
    }

    #[derive(Serialize, Deserialize)]
    pub struct SyncResponse<'a> {
        /// Key range start (length: KEY_SIZE)
        #[serde(rename = "s")]
        pub range_start: &'a [u8],

        /// Key range end (length: KEY_SIZE)
        #[serde(rename = "e")]
        pub range_end: &'a [u8],

        /// Number of records responder has under key range
        #[serde(rename = "c")]
        pub record_count: u64,

        /// Reference time for query
        #[serde(rename = "t")]
        pub reference_time: u64,

        /// Random salt
        #[serde(rename = "x")]
        pub salt: &'a [u8],

        /// IBLT set summary or empty if not included
        ///
        /// If an IBLT is omitted it means the sender determined it was
        /// more efficient to just send keys. In that case keys[] should have
        /// an explicit list.
        #[serde(with = "serde_bytes")]
        #[serde(rename = "i")]
        pub iblt: &'a [u8],

        /// Explicit list of keys (full key length).
        ///
        /// This may still contain keys if an IBLT is present. In that case
        /// keys included here will be any that have identical 64-bit prefixes
        /// to keys already added to the IBLT and thus would collide. These
        /// should be rare so it's most efficient to just explicitly name them.
        /// Otherwise keys with identical 64-bit prefixes may never be synced.
        #[serde(with = "serde_bytes")]
        #[serde(rename = "k")]
        pub keys: &'a [u8],
    }
}
