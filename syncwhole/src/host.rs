/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2022 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::net::SocketAddr;

#[cfg(feature = "include_sha2_lib")]
use sha2::digest::{Digest, FixedOutput};

use serde::{Deserialize, Serialize};

use crate::node::RemoteNodeInfo;

/// Configuration setttings for a syncwhole node.
#[derive(Serialize, Deserialize, Clone, Eq, PartialEq)]
pub struct Config {
    /// A list of peer addresses to which we always want to stay connected.
    /// The library will try to maintain connectivity to these regardless of connection limits.
    pub anchors: Vec<SocketAddr>,

    /// A list of peer addresses that we can try in order to achieve desired_connection_count.
    pub seeds: Vec<SocketAddr>,

    /// The maximum number of TCP connections we should allow.
    pub max_connection_count: usize,

    /// The desired number of peering links.
    pub desired_connection_count: usize,

    /// An optional name for this node to advertise to other nodes.
    pub name: String,

    /// An optional contact string for this node to advertise to other nodes.
    /// Example: bighead@stanford.edu or https://www.piedpiper.com/
    pub contact: String,
}

/// A trait that users of syncwhole implement to provide configuration information and listen for events.
pub trait Host: Sync + Send {
    /// Get a copy of the current configuration for this syncwhole node.
    fn node_config(&self) -> Config;

    /// Test whether an inbound connection should be allowed from an address.
    ///
    /// This is called on first incoming connection before any init is received. The authenticate()
    /// method is called once init has been received and is another decision point. The default
    /// implementation of this always returns true.
    #[allow(unused_variables)]
    fn allow(&self, remote_address: &SocketAddr) -> bool {
        true
    }

    /// Compute HMAC-SHA512(secret, challenge).
    ///
    /// A return of None indicates that the connection should be dropped. If authentication is
    /// not enabled, the response should be computed using an all-zero secret key. This is
    /// what the default implementation does, so if you don't want authentication there is no
    /// need to override and implement this.
    ///
    /// This actually gets called twice per link: once when Init is received to compute the
    /// response, and once when InitResponse is received to verify the response to our challenge.
    ///
    /// The default implementation authenticates with an all-zero key. Leave it this way if
    /// you don't want authentication.
    #[allow(unused_variables)]
    fn authenticate(&self, info: &RemoteNodeInfo, challenge: &[u8]) -> Option<[u8; 64]> {
        Some(Self::hmac_sha512(&[0_u8; 64], challenge))
    }

    /// Called when an attempt is made to connect to a remote address.
    fn on_connect_attempt(&self, address: &SocketAddr);

    /// Called when a connection has been successfully established.
    ///
    /// Hosts are encouraged to learn endpoints when a successful outbound connection is made. Check the
    /// inbound flag in the remote node info structure.
    fn on_connect(&self, info: &RemoteNodeInfo);

    /// Called when an open connection is closed for any reason.
    fn on_connection_closed(&self, info: &RemoteNodeInfo, reason: String);

    /// Fill a buffer with secure random bytes.
    ///
    /// This is supplied to reduce inherent dependencies and allow the user to choose the implementation.
    fn get_secure_random(&self, buf: &mut [u8]);

    /// Compute a SHA512 digest of the input.
    ///
    /// Input can consist of one or more slices that will be processed in order.
    ///
    /// If the feature "include_sha2_lib" is enabled a default implementation in terms of the
    /// Rust sha2 crate is generated. Otherwise the user must supply their own implementation.
    #[cfg(not(feature = "include_sha2_lib"))]
    fn sha512(msg: &[&[u8]]) -> [u8; 64];
    #[cfg(feature = "include_sha2_lib")]
    fn sha512(msg: &[&[u8]]) -> [u8; 64] {
        let mut h = sha2::Sha512::new();
        for b in msg.iter() {
            h.update(*b);
        }
        h.finalize_fixed().as_ref().try_into().unwrap()
    }

    /// Compute HMAC-SHA512 using key and input.
    ///
    /// Supplied key will always be 64 bytes in length.
    ///
    /// The default implementation is HMAC implemented in terms of sha512() above. Specialize
    /// to provide your own implementation.
    fn hmac_sha512(key: &[u8], msg: &[u8]) -> [u8; 64] {
        let mut opad = [0x5c_u8; 128];
        let mut ipad = [0x36_u8; 128];
        assert!(key.len() >= 64);
        for i in 0..64 {
            opad[i] ^= key[i];
        }
        for i in 0..64 {
            ipad[i] ^= key[i];
        }
        Self::sha512(&[&opad, &Self::sha512(&[&ipad, msg])])
    }
}
