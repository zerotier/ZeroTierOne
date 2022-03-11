/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2022 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

/// No operation, payload ignored.
pub const MESSAGE_TYPE_NOP: u8 = 0;

/// Sent by both sides of a TCP link when it's established.
pub const MESSAGE_TYPE_INIT: u8 = 1;

/// Reply sent to INIT.
pub const MESSAGE_TYPE_INIT_RESPONSE: u8 = 2;

/// Sent every few seconds to notify peers of number of records, clock, etc.
pub const MESSAGE_TYPE_STATUS: u8 = 3;

/// Get a set summary of a prefix in the data set.
pub const MESSAGE_TYPE_GET_SUMMARY: u8 = 4;

/// Set summary of a prefix.
pub const MESSAGE_TYPE_SUMMARY: u8 = 5;

/// Payload is a list of keys of records. Usually sent to advertise recently received new records.
pub const MESSAGE_TYPE_HAVE_RECORDS: u8 = 6;

/// Payload is a list of keys of records the sending node wants.
pub const MESSAGE_TYPE_GET_RECORDS: u8 = 7;

/// Payload is a record, with key being omitted if the data store's KEY_IS_COMPUTED constant is true.
pub const MESSAGE_TYPE_RECORD: u8 = 8;

/// Summary type: simple array of keys under the given prefix.
pub const SUMMARY_TYPE_KEYS: u8 = 0;

/// An IBLT set summary.
pub const SUMMARY_TYPE_IBLT: u8 = 1;

/// Number of bytes of each SHA512 hash to announce, request, etc. This is okay to change but 16 is plenty.
pub const ANNOUNCE_HASH_BYTES: usize = 16;

pub mod msg {
    use serde::{Serialize, Deserialize};

    #[derive(Serialize, Deserialize)]
    pub struct IPv4 {
        pub ip: [u8; 4],
        pub port: u16
    }

    #[derive(Serialize, Deserialize)]
    pub struct IPv6 {
        pub ip: [u8; 16],
        pub port: u16
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
        pub node_name: Option<String>,

        /// Optional contact information for this node, such as a URL or an e-mail address.
        pub node_contact: Option<String>,

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

    #[derive(Serialize, Deserialize, Clone)]
    pub struct Status {
        /// Total number of records in data set.
        #[serde(rename = "c")]
        pub total_record_count: u64,

        /// Reference wall clock time in milliseconds since Unix epoch.
        #[serde(rename = "t")]
        pub reference_time: i64,
    }

    #[derive(Serialize, Deserialize, Clone)]
    pub struct GetSummary<'a> {
        /// Reference wall clock time in milliseconds since Unix epoch.
        #[serde(rename = "t")]
        pub reference_time: i64,

        /// Prefix within key space.
        #[serde(rename = "p")]
        #[serde(with = "serde_bytes")]
        pub prefix: &'a [u8],

        /// Length of prefix in bits (trailing bits in byte array are ignored).
        #[serde(rename = "b")]
        pub prefix_bits: u8,

        /// Number of records in this range the requesting node already has, used to choose summary type.
        #[serde(rename = "r")]
        pub record_count: u64,
    }

    #[derive(Serialize, Deserialize, Clone)]
    pub struct SummaryHeader<'a> {
        /// Total number of records in data set, for easy rapid generation of next query.
        #[serde(rename = "c")]
        pub total_record_count: u64,

        /// Reference wall clock time in milliseconds since Unix epoch.
        #[serde(rename = "t")]
        pub reference_time: i64,

        /// Random salt value used by some summary types.
        #[serde(rename = "x")]
        #[serde(with = "serde_bytes")]
        pub salt: &'a [u8],

        /// Prefix within key space.
        #[serde(rename = "p")]
        #[serde(with = "serde_bytes")]
        pub prefix: &'a [u8],

        /// Length of prefix in bits (trailing bits in byte array are ignored).
        #[serde(rename = "b")]
        pub prefix_bits: u8,

        /// Type of summary that follows this header.
        #[serde(rename = "s")]
        pub summary_type: u8,
    }
}
