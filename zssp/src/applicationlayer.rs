/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use zerotier_crypto::p384::P384KeyPair;

/// Trait to implement to integrate the session into an application.
///
/// Templating the session on this trait lets the code here be almost entirely transport, OS,
/// and use case independent.
///
/// The constants exposed in this trait can be redefined from their defaults to change rekey
/// and negotiation timeout behavior. This is discouraged except for testing purposes when low
/// key lifetime values may be desirable to test rekeying. Also note that each side takes turns
/// initiating rekey, so if both sides don't have the same values you'll get asymmetric timing
/// behavior. This will still work as long as the key usage counter doesn't exceed the
/// EXPIRE_AFTER_USES limit.
pub trait ApplicationLayer: Sized {
    /// Rekey after this many key uses.
    ///
    /// The default is 1/4 the recommended NIST limit for AES-GCM. Unless you are transferring
    /// a massive amount of data REKEY_AFTER_TIME_MS is probably going to kick in first.
    const REKEY_AFTER_USES: u64 = 536870912;

    /// Hard expiration after this many uses.
    ///
    /// Attempting to encrypt more than this many messages with a key will cause a hard error
    /// and the internal erasure of ephemeral key material. You'll only ever hit this if something
    /// goes wrong and rekeying fails.
    const EXPIRE_AFTER_USES: u64 = 2147483648;

    /// Start attempting to rekey after a key has been in use for this many milliseconds.
    ///
    /// Default is two hours.
    const REKEY_AFTER_TIME_MS: i64 = 1000 * 60 * 60 * 2;

    /// Maximum random jitter to add to rekey-after time.
    ///
    /// Default is ten minutes.
    const REKEY_AFTER_TIME_MS_MAX_JITTER: u32 = 1000 * 60 * 10;

    /// Timeout for incoming Noise_XK session negotiation in milliseconds.
    ///
    /// Default is two seconds, which should be enough for even extremely slow links or links
    /// over very long distances.
    const INCOMING_SESSION_NEGOTIATION_TIMEOUT_MS: i64 = 2000;

    /// Retry interval for outgoing connection initiation or rekey attempts.
    ///
    /// Retry attepmpts will be no more often than this, but the delay may end up being slightly more
    /// in some cases depending on where in the cycle the initial attempt falls.
    const RETRY_INTERVAL: i64 = 500;

    /// Type for arbitrary opaque object for use by the application that is attached to each session.
    type Data;

    /// Data type for incoming packet buffers.
    ///
    /// This can be something like Vec<u8> or Box<[u8]> or it can be something like a pooled reusable
    /// buffer that automatically returns to its pool when ZSSP is done with it. ZSSP may hold these
    /// for a short period of time when assembling fragmented packets on the receive path.
    type IncomingPacketBuffer: AsRef<[u8]> + AsMut<[u8]>;

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
