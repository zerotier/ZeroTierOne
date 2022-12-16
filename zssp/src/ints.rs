use zerotier_crypto::random;
use zerotier_utils::memory;

/// "Canonical header" for generating 96-bit AES-GCM nonce and for inclusion in HMACs.
///
/// This is basically the actual header but with fragment count and fragment total set to zero.
/// Fragmentation is not considered when authenticating the entire packet. A separate header
/// check code is used to make fragmentation itself more robust, but that's outside the scope
/// of AEAD authentication.
#[derive(Clone, Copy)]
#[repr(C, packed)]
pub(crate) struct CanonicalHeader(pub u64, pub u32);
impl CanonicalHeader {
    #[inline(always)]
    pub fn make(session_id: SessionId, packet_type: u8, counter: u32) -> Self {
        CanonicalHeader(
            (u64::from(session_id) | (packet_type as u64).wrapping_shl(48)).to_le(),
            counter.to_le(),
        )
    }

    #[inline(always)]
    pub fn as_bytes(&self) -> &[u8; 12] {
        memory::as_byte_array(self)
    }
}

/// 48-bit session ID (most significant 16 bits of u64 are unused)
#[derive(Copy, Clone, PartialEq, Eq, Hash, PartialOrd, Ord)]
#[repr(transparent)]
pub struct SessionId(pub(crate) u64);

impl SessionId {
    /// The nil session ID used in messages initiating a new session.
    ///
    /// This is all 1's so that ZeroTier can easily tell the difference between ZSSP init packets
    /// and ZeroTier V1 packets.
    pub const NIL: SessionId = SessionId(0xffffffffffff);

    #[inline]
    pub fn new_from_u64(i: u64) -> Option<SessionId> {
        if i < Self::NIL.0 {
            Some(Self(i))
        } else {
            None
        }
    }

    #[inline]
    pub fn new_random() -> Self {
        Self(random::next_u64_secure() % Self::NIL.0)
    }
}

impl From<SessionId> for u64 {
    #[inline(always)]
    fn from(sid: SessionId) -> Self {
        sid.0
    }
}

/// Was this side the one who sent the first offer (Alice) or countered (Bob).
/// Note that role is not fixed. Either side can take either role. It's just who
/// initiated first.
pub enum Role {
    Alice,
    Bob,
}
