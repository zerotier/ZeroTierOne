/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use zerotier_crypto::p384::{P384KeyPair, P384PublicKey};

/// Trait to implement to integrate the session into an application.
///
/// Templating the session on this trait lets the code here be almost entirely transport, OS,
/// and use case independent.
pub trait ApplicationLayer: Sized {
    /// Arbitrary opaque object associated with a session, such as a connection state object.
    type Data;

    /// A buffer containing data read from the network that can be cached.
    ///
    /// This can be e.g. a pooled buffer that automatically returns itself to the pool when dropped.
    /// It can also just be a Vec<u8> or Box<[u8]> or something like that.
    type IncomingPacketBuffer: AsRef<[u8]> + AsMut<[u8]>;

    /// Rate limit for attempts to rekey existing sessions in milliseconds (default: 2000).
    const REKEY_RATE_LIMIT_MS: i64 = 2000;

    /// Extract the NIST P-384 ECC public key component from a static public key blob or return None on failure.
    ///
    /// This is called to parse the static public key blob from the other end and extract its NIST P-384 public
    /// key. SECURITY NOTE: the information supplied here is from the wire so care must be taken to parse it
    /// safely and fail on any error or corruption.
    fn extract_s_public_from_static_public_blob(static_public: &[u8]) -> Option<P384PublicKey>;

    /// Get a reference to this host's static public key blob.
    ///
    /// This must contain a NIST P-384 public key but can contain other information. In ZeroTier this
    /// is a byte serialized identity. It could just be a naked NIST P-384 key if that's all you need.
    fn get_local_s_public_blob(&self) -> &[u8];

    /// Get a reference to this host's static public key's NIST P-384 secret key pair.
    ///
    /// This must return the NIST P-384 public key that is contained within the static public key blob.
    fn get_local_s_keypair(&self) -> &P384KeyPair;
}
