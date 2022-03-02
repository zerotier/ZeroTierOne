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
    /// Get a list of peer addresses to which we always want to try to stay connected.
    ///
    /// These are always contacted until a link is established regardless of anything else.
    fn fixed_peers(&self) -> &[SocketAddr];

    /// Get a random peer address not in the supplied set.
    fn another_peer(&self, exclude: &HashSet<SocketAddr>) -> Option<SocketAddr>;

    /// Get the maximum number of endpoints allowed.
    ///
    /// This is checked on incoming connect and incoming links are refused if the total is
    /// over this count. Fixed endpoints will be contacted even if the total is over this limit.
    fn max_connection_count(&self) -> usize;

    /// Get the number of connections we ideally want.
    ///
    /// Attempts will be made to lazily contact remote endpoints if the total number of links
    /// is under this amount. Note that fixed endpoints will still be contacted even if the
    /// total is over the desired count.
    ///
    /// This should always be less than max_connection_count().
    fn desired_connection_count(&self) -> usize;

    /// Test whether an inbound connection should be allowed from an address.
    fn allow(&self, remote_address: &SocketAddr) -> bool;

    /// Called when an attempt is made to connect to a remote address.
    fn on_connect_attempt(&self, address: &SocketAddr);

    /// Called when a connection has been successfully established.
    ///
    /// Hosts are encouraged to learn endpoints when a successful outbound connection is made. Check the
    /// inbound flag in the remote node info structure.
    fn on_connect(&self, info: &RemoteNodeInfo);

    /// Called when an open connection is closed for any reason.
    fn on_connection_closed(&self, address: &SocketAddr, reason: Option<Box<dyn Error>>);

    /// Fill a buffer with secure random bytes.
    ///
    /// This is supplied to reduce inherent dependencies and allow the user to choose the implementation.
    fn get_secure_random(&self, buf: &mut [u8]);

    /// Compute a SHA512 digest of the input.
    ///
    /// This is supplied to reduce inherent dependencies and allow the user to choose the implementation.
    fn sha512(msg: &[u8]) -> [u8; 64];
}
