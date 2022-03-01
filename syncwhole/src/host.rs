/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2022 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::collections::HashSet;
use std::error::Error;
use std::net::SocketAddr;

use crate::node::RemoteNodeInfo;

/// A trait that users of syncwhole implement to provide configuration information and listen for events.
pub trait Host: Sync + Send {
    /// Compute SHA512.
    fn sha512(msg: &[u8]) -> [u8; 64];

    /// Get a list of endpoints to which we always want to try to stay connected.
    ///
    /// The node will repeatedly try to connect to these until a link is established and
    /// reconnect on link failure. They should be stable well known nodes for this domain.
    fn get_static_endpoints(&self) -> &[SocketAddr];

    /// Get additional endpoints to try.
    ///
    /// This should return any endpoints not in the supplied endpoint set if the size
    /// of the set is less than the minimum active link count the host wishes to maintain.
    fn get_more_endpoints(&self, current_endpoints: &HashSet<SocketAddr>) -> Vec<SocketAddr>;

    /// Get the maximum number of endpoints allowed.
    ///
    /// This is checked on incoming connect and incoming links are refused if the total is over this count.
    fn max_endpoints(&self) -> usize;

    /// Called whenever we have successfully connected to a remote node (after connection is initialized).
    fn on_connect(&self, info: &RemoteNodeInfo);

    /// Called when an open connection is closed.
    fn on_connection_closed(&self, endpoint: &SocketAddr, reason: Option<Box<dyn Error>>);

    /// Fill a buffer with secure random bytes.
    fn get_secure_random(&self, buf: &mut [u8]);
}
