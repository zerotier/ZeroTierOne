/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

/*
 * Wire protocol notes:
 *
 * Messages are prefixed by a type byte followed by a message size in the form
 * of a variable length integer (varint). Each message is followed by a
 * 16-byte GMAC message authentication code.
 *
 * HELLO is an exception. It's sent on connect, is prefixed only by a varint
 * size, and is not followed by a MAC. Instead HelloAck is sent after it
 * containing a full HMAC ACK for key negotiation.
 *
 * GMAC is keyed using a KBKDF-derived key from a shared key currently made
 * with HKDF as HMAC(SHA384(ephemeral key), node key). The first 32 bytes of
 * the key are the GMAC key while the nonce is the first 96 bytes of
 * the nonce where this 96-bit integer is incremented (as little-endian)
 * for each message sent. Increment should wrap at 2^96. The connection should
 * close after no more than 2^96 messages, but that's a crazy long session
 * anyway.
 *
 * The wire protocol is only authenticated to prevent network level attacks.
 * Data is not encrypted since this is typically used to replicate a public
 * "well known" data set and encryption would add needless overhead.
 */

use serde::{Deserialize, Serialize};

/// KBKDF label for the HMAC in HelloAck.
pub const KBKDF_LABEL_HELLO_ACK_HMAC: u8 = b'A';

/// KBKDF label for GMAC key derived from main key.
pub const KBKDF_LABEL_GMAC: u8 = b'G';

/// Sanity limit on the size of HELLO.
pub const HELLO_SIZE_MAX: usize = 4096;

/// Size of nonce sent with HELLO.
pub const HELLO_NONCE_SIZE: usize = 64;

/// Overall protocol version.
pub const PROTOCOL_VERSION: u16 = 1;

/// No operation, no payload, sent without size or MAC.
/// This is a special single byte message used for connection keepalive.
pub const MESSAGE_TYPE_KEEPALIVE: u8 = 0;

/// Acknowledgement of HELLO.
pub const MESSAGE_TYPE_HELLO_ACK: u8 = 1;

/// A series of objects with each prefixed by a varint size.
pub const MESSAGE_TYPE_OBJECTS: u8 = 2;

/// A series of identity hashes concatenated together advertising objects we have.
pub const MESSAGE_TYPE_HAVE_OBJECTS: u8 = 3;

/// A series of identity hashes concatenated together of objects being requested.
pub const MESSAGE_TYPE_WANT_OBJECTS: u8 = 4;

/// Report state, requesting possible sync response.
pub const MESSAGE_TYPE_STATE: u8 = 5;

/// IBLT sync digest, payload is IBLTSyncDigest.
pub const MESSAGE_TYPE_IBLT_SYNC_DIGEST: u8 = 6;

/// Initial message sent by both sides on TCP connection establishment.
/// This is sent with no type or message authentication code and is only
/// sent on connect. It is prefixed by a varint size.
#[derive(Deserialize, Serialize)]
pub struct Hello<'a> {
    /// Local value of PROTOCOL_VERSION.
    pub protocol_version: u16,
    /// Flags, currently unused and always zero.
    pub flags: u64,
    /// Local clock in milliseconds since Unix epoch.
    pub clock: u64,
    /// The data set name ("domain") to which this node belongs.
    pub domain: &'a str,
    /// Random nonce, must be at least 64 bytes in length.
    pub nonce: &'a [u8],
    /// Random ephemeral ECDH session key.
    pub p521_ecdh_ephemeral_key: &'a [u8],
    /// Long-lived node-identifying ECDH public key.
    pub p521_ecdh_node_key: &'a [u8],
}

/// Sent in response to Hello and contains an acknowledgement HMAC for the shared key.
#[derive(Deserialize, Serialize)]
pub struct HelloAck<'a> {
    /// HMAC-SHA384(KBKDF(ack key, KBKDF_LABEL_HELLO_ACK_HMAC), SHA384(original raw Hello))
    pub ack: &'a [u8],
    /// Value of clock in original hello, for measuring latency.
    pub clock_echo: u64,
}

/// Report the state of the sender's data set.
///
/// The peer may respond in one of three ways:
///
/// (1) It may send an IBLTSyncDigest over a range of identity hashes of its
/// choice so that the requesting node may compute a difference and request
/// objects it does not have.
///
/// (2) It may send HAVE_OBJECTS with a simple list of objects.
///
/// (3) It may simply send a batch of objects.
///
/// (4) It may not respond at all.
///
/// Which option is chosen is up to the responding node and should be chosen
/// via a heuristic to maximize sync efficiency and minimize sync time.
///
/// A central assumption is that identity hashes are uniformly distributed
/// since they are cryptographic hashes (currently SHA-384). This allows a
/// simple calculation to be made with the sending node's total count to
/// estimate set difference density across the entire hash range.
#[derive(Deserialize, Serialize)]
pub struct State<'a> {
    /// Total number of hashes in the entire data set.
    pub total_count: u64,
    /// Our clock to use as a reference time for filtering the data set (if applicable).
    pub reference_time: u64,
}

/// An IBLT digest of identity hashes over a range.
#[derive(Deserialize, Serialize)]
pub struct IBLTSyncDigest<'a> {
    /// Start of range. Right-pad with zeroes if too short.
    pub range_start: &'a [u8],
    /// End of range. Right-pad with zeroes if too short.
    pub range_end: &'a [u8],
    /// IBLT digest of hashes in this range.
    pub iblt: &'a [u8],
    /// Number of hashes in this range.
    pub count: u64,
    /// Total number of hashes in entire data set.
    pub total_count: u64,
    /// Reference time from SyncRequest or 0 if this is being sent synthetically.
    pub reference_time: u64,
}
