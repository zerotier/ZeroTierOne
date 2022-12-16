use std::fmt::Display;

use zerotier_crypto::random;
use zerotier_utils::memory::{array_range, as_byte_array};

use crate::constants::SESSION_ID_SIZE;

/// 48-bit session ID (most significant 16 bits of u64 are unused)
#[derive(Copy, Clone, PartialEq, Eq, Hash)]
#[repr(transparent)]
pub struct SessionId(u64); // stored little endian internally

impl SessionId {
    /// The nil session ID used in messages initiating a new session.
    ///
    /// This is all 1's so that ZeroTier can easily tell the difference between ZSSP init packets
    /// and ZeroTier V1 packets.
    pub const NIL: SessionId = SessionId(0xffffffffffffu64.to_le());

    #[inline]
    pub fn new_from_u64(i: u64) -> Option<SessionId> {
        if i < Self::NIL.0 {
            Some(Self(i.to_le()))
        } else {
            None
        }
    }

    #[inline]
    pub fn new_random() -> Self {
        Self((random::next_u64_secure() % Self::NIL.0).to_le())
    }

    /// Get this session ID as a 48-bit little endian byte array.
    #[inline(always)]
    pub fn as_bytes(&self) -> &[u8; SESSION_ID_SIZE] {
        array_range::<u8, 8, 0, SESSION_ID_SIZE>(as_byte_array(&self.0))
    }
}

impl From<SessionId> for u64 {
    fn from(sid: SessionId) -> Self {
        u64::from_le(sid.0)
    }
}

impl Display for SessionId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_fmt(format_args!("{:06x}", u64::from_le(self.0)))
    }
}
