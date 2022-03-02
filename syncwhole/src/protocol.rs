/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2022 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

pub const MESSAGE_TYPE_NOP: u8 = 0;
pub const MESSAGE_TYPE_INIT: u8 = 1;
pub const MESSAGE_TYPE_INIT_RESPONSE: u8 = 2;
pub const MESSAGE_TYPE_HAVE_RECORDS: u8 = 3;
pub const MESSAGE_TYPE_GET_RECORDS: u8 = 4;

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
        #[serde(rename = "alc")]
        pub anti_loopback_challenge: &'a [u8],
        #[serde(rename = "d")]
        pub domain: String,
        #[serde(rename = "ks")]
        pub key_size: u16,
        #[serde(rename = "mvs")]
        pub max_value_size: u64,
        #[serde(rename = "nn")]
        pub node_name: Option<String>,
        #[serde(rename = "nc")]
        pub node_contact: Option<String>,
        #[serde(rename = "ei4")]
        pub explicit_ipv4: Option<IPv4>,
        #[serde(rename = "ei6")]
        pub explicit_ipv6: Option<IPv6>,
    }

    #[derive(Serialize, Deserialize)]
    pub struct InitResponse<'a> {
        #[serde(rename = "alr")]
        pub anti_loopback_response: &'a [u8],
    }
}
