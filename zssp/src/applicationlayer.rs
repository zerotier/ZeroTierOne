use std::ops::Deref;

use zerotier_crypto::{
    p384::{P384KeyPair, P384PublicKey},
    secret::Secret,
};

use crate::{
    sessionid::SessionId,
    zssp::{ReceiveContext, Session},
};

/// Trait to implement to integrate the session into an application.
///
/// Templating the session on this trait lets the code here be almost entirely transport, OS,
/// and use case independent.
pub trait ApplicationLayer: Sized {
    /// Arbitrary opaque object associated with a session, such as a connection state object.
    type Data;

    /// Arbitrary object that dereferences to the session, such as Arc<Session<Self>>.
    type SessionRef: Deref<Target = Session<Self>>;

    /// A buffer containing data read from the network that can be cached.
    ///
    /// This can be e.g. a pooled buffer that automatically returns itself to the pool when dropped.
    /// It can also just be a Vec<u8> or Box<[u8]> or something like that.
    type IncomingPacketBuffer: AsRef<[u8]>;

    /// Remote physical address on whatever transport this session is using.
    type RemoteAddress;

    /// Rate limit for attempts to rekey existing sessions in milliseconds (default: 2000).
    const REKEY_RATE_LIMIT_MS: i64 = 2000;

    /// Get a reference to this host's static public key blob.
    ///
    /// This must contain a NIST P-384 public key but can contain other information. In ZeroTier this
    /// is a byte serialized identity. It could just be a naked NIST P-384 key if that's all you need.
    fn get_local_s_public_blob(&self) -> &[u8];

    /// Get SHA384(this host's static public key blob).
    ///
    /// This allows us to avoid computing SHA384(public key blob) over and over again.
    fn get_local_s_public_blob_hash(&self) -> &[u8; 48];

    /// Get a reference to this hosts' static public key's NIST P-384 secret key pair.
    ///
    /// This must return the NIST P-384 public key that is contained within the static public key blob.
    fn get_local_s_keypair(&self) -> &P384KeyPair;

    /// Extract the NIST P-384 ECC public key component from a static public key blob or return None on failure.
    ///
    /// This is called to parse the static public key blob from the other end and extract its NIST P-384 public
    /// key. SECURITY NOTE: the information supplied here is from the wire so care must be taken to parse it
    /// safely and fail on any error or corruption.
    fn extract_s_public_from_raw(static_public: &[u8]) -> Option<P384PublicKey>;

    /// Look up a local session by local session ID or return None if not found.
    fn lookup_session(&self, local_session_id: SessionId) -> Option<Self::SessionRef>;

    /// Rate limit and check an attempted new session (called before accept_new_session).
    fn check_new_session(&self, rc: &ReceiveContext<Self>, remote_address: &Self::RemoteAddress) -> bool;

    /// Check whether a new session should be accepted.
    ///
    /// On success a tuple of local session ID, static secret, and associated object is returned. The
    /// static secret is whatever results from agreement between the local and remote static public
    /// keys.
    ///
    /// When `accept_new_session` is called, `remote_static_public` and `remote_metadata` have not yet been
    /// authenticated. As such avoid mutating state until OkNewSession(Session) is returned, as the connection
    /// may be adversarial.
    ///
    /// When `remote_static_public` and `remote_metadata` are eventually authenticated, the zssp protocol cannot
    /// guarantee that they are unique, i.e. `remote_static_public` and `remote_metadata` may be duplicates from
    /// an old attempt to establish a session, and may even have been replayed by an adversary. If your use-case
    /// needs uniqueness for reliability or security, consider either including a timestamp in the metadata, or
    /// sending the metadata as an extra transport packet after the session is fully established.
    fn accept_new_session(
        &self,
        receive_context: &ReceiveContext<Self>,
        remote_address: &Self::RemoteAddress,
        remote_static_public: &[u8],
        remote_metadata: &[u8],
    ) -> Option<(SessionId, Secret<64>, Self::Data)>;
}
