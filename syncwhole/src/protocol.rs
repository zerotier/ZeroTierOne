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

#[derive(Clone, Copy, Eq, PartialEq)]
#[repr(u8)]
pub enum MessageType {
    Nop = 0_u8,
    Init = 1_u8,
    InitResponse = 2_u8,
    HaveRecord = 3_u8,
    HaveRecords = 4_u8,
    GetRecords = 5_u8,
    Record = 6_u8,
    SyncStatus = 7_u8,
    SyncRequest = 8_u8,
    SyncResponse = 9_u8,
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
    pub fn name(&self) -> &'static str {
        match *self {
            Self::Nop => "NOP",
            Self::Init => "INIT",
            Self::InitResponse => "INIT_RESPONSE",
            Self::HaveRecord => "HAVE_RECORD",
            Self::HaveRecords => "HAVE_RECORDS",
            Self::GetRecords => "GET_RECORDS",
            Self::Record => "RECORD",
            Self::SyncStatus => "SYNC_STATUS",
            Self::SyncRequest => "SYNC_REQUEST",
            Self::SyncResponse => "SYNC_RESPONSE",
        }
    }
}

#[derive(Clone, Copy, Eq, PartialEq)]
#[repr(u8)]
pub enum SyncResponseType {
    /// No response, do nothing.
    None = 0_u8,

    /// Response is a msgpack-encoded HaveRecords message.
    HaveRecords = 1_u8,

    /// Response is a series of records prefixed by varint record sizes.
    Records = 2_u8,

    /// Response is an IBLT set summary.
    IBLT = 3_u8,
}

impl From<u8> for SyncResponseType {
    /// Get response type from a byte, returning None if the byte is out of range.
    #[inline(always)]
    fn from(b: u8) -> Self {
        if b <= 3 {
            unsafe { std::mem::transmute(b) }
        } else {
            Self::None
        }
    }
}

impl SyncResponseType {
    pub fn as_str(&self) -> &'static str {
        match *self {
            SyncResponseType::None => "NONE",
            SyncResponseType::HaveRecords => "HAVE_RECORDS",
            SyncResponseType::Records => "RECORDS",
            SyncResponseType::IBLT => "IBLT",
        }
    }
}

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
    pub struct HaveRecords<'a> {
        /// Length of each key, chosen to ensure uniqueness.
        #[serde(rename = "l")]
        pub key_length: usize,

        /// Keys whose existence is being announced, of 'key_length' length.
        #[serde(with = "serde_bytes")]
        #[serde(rename = "k")]
        pub keys: &'a [u8],
    }

    #[derive(Serialize, Deserialize)]
    pub struct GetRecords<'a> {
        /// Length of each key, chosen to ensure uniqueness.
        #[serde(rename = "l")]
        pub key_length: usize,

        /// Keys to retrieve, of 'key_length' bytes in length.
        #[serde(with = "serde_bytes")]
        #[serde(rename = "k")]
        pub keys: &'a [u8],
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
        /// Query mask, a random string of KEY_SIZE bytes.
        #[serde(with = "serde_bytes")]
        #[serde(rename = "q")]
        pub query_mask: &'a [u8],

        /// Number of bits to match as a prefix in query_mask (0 for entire data set).
        #[serde(rename = "b")]
        pub query_mask_bits: u8,

        /// Number of records requesting node already holds under query mask prefix.
        #[serde(rename = "c")]
        pub record_count: u64,

        /// Sender's reference time.
        #[serde(rename = "t")]
        pub reference_time: u64,

        /// Random salt
        #[serde(rename = "s")]
        pub salt: u64,
    }

    #[derive(Serialize, Deserialize)]
    pub struct SyncResponse<'a> {
        /// Query mask, a random string of KEY_SIZE bytes.
        #[serde(with = "serde_bytes")]
        #[serde(rename = "q")]
        pub query_mask: &'a [u8],

        /// Number of bits to match as a prefix in query_mask (0 for entire data set).
        #[serde(rename = "b")]
        pub query_mask_bits: u8,

        /// Number of records sender has under this prefix.
        #[serde(rename = "c")]
        pub record_count: u64,

        /// Sender's reference time.
        #[serde(rename = "t")]
        pub reference_time: u64,

        /// Random salt
        #[serde(rename = "s")]
        pub salt: u64,

        /// SyncResponseType determining content of 'data'.
        #[serde(rename = "r")]
        pub response_type: u8,

        /// Data whose meaning depends on the response type.
        #[serde(with = "serde_bytes")]
        #[serde(rename = "d")]
        pub data: &'a [u8],
    }
}
