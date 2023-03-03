/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

#[derive(PartialEq, Eq)]
pub enum Error {
    /// The packet was addressed to an unrecognized local session (should usually be ignored)
    UnknownLocalSessionId,

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

    /// The other peer specified an unrecognized protocol version
    UnknownProtocolVersion,

    /// Caller supplied data buffer is too small to receive data
    DataBufferTooSmall,

    /// Data object is too large to send, even with fragmentation
    DataTooLarge,

    /// Packet counter was outside window or packet arrived with session in an unexpected state.
    OutOfSequence,

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
        f.write_str(match self {
            Self::UnknownLocalSessionId => "UnknownLocalSessionId",
            Self::InvalidPacket => "InvalidPacket",
            Self::InvalidParameter => "InvalidParameter",
            Self::FailedAuthentication => "FailedAuthentication",
            Self::MaxKeyLifetimeExceeded => "MaxKeyLifetimeExceeded",
            Self::SessionNotEstablished => "SessionNotEstablished",
            Self::UnknownProtocolVersion => "UnknownProtocolVersion",
            Self::DataBufferTooSmall => "DataBufferTooSmall",
            Self::DataTooLarge => "DataTooLarge",
            Self::OutOfSequence => "OutOfSequence",
            Self::UnexpectedBufferOverrun => "UnexpectedBufferOverrun",
        })
    }
}

impl std::error::Error for Error {}

impl std::fmt::Debug for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        std::fmt::Display::fmt(self, f)
    }
}
