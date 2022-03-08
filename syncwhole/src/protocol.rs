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

/// Payload is a list of keys of records. Usually sent to advertise recently received new records.
pub const MESSAGE_TYPE_HAVE_RECORDS: u8 = 4;

/// Payload is a list of keys of records the sending node wants.
pub const MESSAGE_TYPE_GET_RECORDS: u8 = 5;

/// Payload is a record, with key being omitted if the data store's KEY_IS_COMPUTED constant is true.
pub const MESSAGE_TYPE_RECORD: u8 = 6;

pub mod msg {
    use serde::{Serialize, Deserialize};

    #[derive(Serialize, Deserialize)]
    pub struct IPv4 {
        #[serde(rename = "i")]
        pub ip: [u8; 4],
        #[serde(rename = "p")]
        pub port: u16
    }

    #[derive(Serialize, Deserialize)]
    pub struct IPv6 {
        #[serde(rename = "i")]
        pub ip: [u8; 16],
        #[serde(rename = "p")]
        pub port: u16
    }

    #[derive(Serialize, Deserialize)]
    pub struct Init<'a> {
        /// A random challenge to be hashed with a secret to detect and drop connections to self.
        #[serde(rename = "alc")]
        #[serde(with = "serde_bytes")]
        pub anti_loopback_challenge: &'a [u8],

        /// A random challenge for login/authentication.
        #[serde(with = "serde_bytes")]
        pub challenge: &'a [u8],

        /// An arbitrary name for this data set to avoid connecting to peers not replicating it.
        #[serde(rename = "d")]
        pub domain: String,

        /// Size of keys in this data set in bytes.
        #[serde(rename = "ks")]
        pub key_size: u16,

        /// Maximum allowed size of values in this data set in bytes.
        #[serde(rename = "mvs")]
        pub max_value_size: u64,

        /// Optional name to advertise for this node.
        #[serde(rename = "nn")]
        pub node_name: Option<String>,

        /// Optional contact information for this node, such as a URL or an e-mail address.
        #[serde(rename = "nc")]
        pub node_contact: Option<String>,

        /// Port to which this node has locally bound.
        /// This is used to try to auto-detect whether a NAT is in the way.
        pub locally_bound_port: u16,

        /// An IPv4 address where this node can be reached.
        /// If both explicit_ipv4 and explicit_ipv6 are omitted the physical source IP:port may be used.
        #[serde(rename = "ei4")]
        pub explicit_ipv4: Option<IPv4>,

        /// An IPv6 address where this node can be reached.
        /// If both explicit_ipv4 and explicit_ipv6 are omitted the physical source IP:port may be used.
        #[serde(rename = "ei6")]
        pub explicit_ipv6: Option<IPv6>,
    }

    #[derive(Serialize, Deserialize)]
    pub struct InitResponse<'a> {
        /// HMAC-SHA512(local secret, anti_loopback_challenge) to detect and drop loops.
        #[serde(rename = "alr")]
        #[serde(with = "serde_bytes")]
        pub anti_loopback_response: &'a [u8],

        /// HMAC-SHA512(secret, challenge) for authentication. (If auth is not enabled, an all-zero secret is used.)
        #[serde(with = "serde_bytes")]
        pub challenge_response: &'a [u8],
    }

    #[derive(Serialize, Deserialize, Clone)]
    pub struct Status {
        /// Total number of records in data set.
        #[serde(rename = "rc")]
        pub record_count: u64,

        /// Local wall clock time in milliseconds since Unix epoch.
        #[serde(rename = "c")]
        pub clock: u64,
    }
}
