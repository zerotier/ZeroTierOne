/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Clone, PartialEq, Eq)]
#[serde(default)]
pub struct Config {
    /// Maximum allowed size of a protocol message.
    pub max_message_size: usize,

    /// TCP port to which this should bind.
    pub tcp_port: u16,

    /// Connection timeout in seconds.
    pub io_timeout: u64,

    /// A name for this replicated data set. This is just used to prevent linking to peers replicating different data.
    pub domain: String,
}

impl Default for Config {
    fn default() -> Self {
        Self {
            max_message_size: 1024 * 256, // 256KiB
            tcp_port: 19993,
            io_timeout: 300, // 5 minutes
            domain: String::new(),
        }
    }
}
