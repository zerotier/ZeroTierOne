/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use crate::sessionid::SessionId;

pub enum Error {
    /// The packet was addressed to an unrecognized local session (should usually be ignored)
    UnknownLocalSessionId(SessionId),

    /// Packet was not well formed
    InvalidPacket,

    /// An invalid parameter was supplied to the function
    InvalidParameter,

    /// Packet failed one or more authentication (MAC) checks
    /// IMPORTANT: Do not reply to a peer who has sent a packet that has failed authentication. Any response at all will leak to an attacker what authentication step their packet failed at (timing attack), which lowers the total authentication entropy they have to brute force.
    /// There is a safe way to reply if absolutely necessary, by sending the reply back after a constant amount of time, but this is difficult to get correct.
    FailedAuthentication,

    /// Rekeying failed and session secret has reached its hard usage count limit
    MaxKeyLifetimeExceeded,

    /// Attempt to send using session without established key
    SessionNotEstablished,

    /// Packet ignored by rate limiter.
    RateLimited,

    /// Packet counter is too far outside window.
    OutOfCounterWindow,

    /// The other peer specified an unrecognized protocol version
    UnknownProtocolVersion,

    /// Caller supplied data buffer is too small to receive data
    DataBufferTooSmall,

    /// Data object is too large to send, even with fragmentation
    DataTooLarge,

    /// An unexpected buffer overrun occured while attempting to encode or decode a packet.
    ///
    /// This can only ever happen if exceptionally large key blobs or metadata are being used,
    /// or as the result of an internal encoding bug.
    UnexpectedBufferOverrun,
}

// An I/O error in the parser means an invalid packet.
impl From<std::io::Error> for Error {
    #[inline(always)]
    fn from(_: std::io::Error) -> Self {
        Self::InvalidPacket
    }
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::UnknownLocalSessionId(id) => f.write_str(format!("UnknownLocalSessionId({})", id).as_str()),
            Self::InvalidPacket => f.write_str("InvalidPacket"),
            Self::InvalidParameter => f.write_str("InvalidParameter"),
            Self::FailedAuthentication => f.write_str("FailedAuthentication"),
            Self::MaxKeyLifetimeExceeded => f.write_str("MaxKeyLifetimeExceeded"),
            Self::SessionNotEstablished => f.write_str("SessionNotEstablished"),
            Self::RateLimited => f.write_str("RateLimited"),
            Self::OutOfCounterWindow => f.write_str("OutOfCounterWindow"),
            Self::UnknownProtocolVersion => f.write_str("UnknownProtocolVersion"),
            Self::DataBufferTooSmall => f.write_str("DataBufferTooSmall"),
            Self::DataTooLarge => f.write_str("DataTooLarge"),
            Self::UnexpectedBufferOverrun => f.write_str("UnexpectedBufferOverrun"),
        }
    }
}

impl std::error::Error for Error {}

impl std::fmt::Debug for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        std::fmt::Display::fmt(self, f)
    }
}
