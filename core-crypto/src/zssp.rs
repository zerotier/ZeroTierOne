// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::io::{Read, Write};
use std::num::NonZeroU64;
use std::ops::Deref;
use std::sync::atomic::{AtomicU64, Ordering};

use crate::aes::AesGcm;
use crate::hash::{hmac_sha384, hmac_sha512, SHA384};
use crate::p384::{P384KeyPair, P384PublicKey, P384_PUBLIC_KEY_SIZE};
use crate::random;
use crate::secret::Secret;

use zerotier_utils::gatherarray::GatherArray;
use zerotier_utils::ringbuffermap::RingBufferMap;
use zerotier_utils::varint;

use parking_lot::{Mutex, RwLock, RwLockUpgradableReadGuard};

pub const MIN_PACKET_SIZE: usize = HEADER_SIZE;
pub const MIN_MTU: usize = 1280;

/// Start attempting to rekey after a key has been used to send packets this many times.
const REKEY_AFTER_USES: u64 = 536870912;

/// Maximum random jitter to add to rekey-after usage count.
const REKEY_AFTER_USES_MAX_JITTER: u32 = 1048576;

/// Hard expiration after this many uses.
const EXPIRE_AFTER_USES: u64 = (u32::MAX - 1024) as u64;

/// Start attempting to rekey after a key has been in use for this many milliseconds.
const REKEY_AFTER_TIME_MS: i64 = 1000 * 60 * 60; // 1 hour

/// Maximum random jitter to add to rekey-after time.
const REKEY_AFTER_TIME_MS_MAX_JITTER: u32 = 1000 * 60 * 5;

/// Rate limit for sending new offers to attempt to re-key.
const OFFER_RATE_LIMIT_MS: i64 = 2000;

/// Version 0: NIST P-384 forward secrecy and authentication with optional Kyber1024 forward secrecy (but not authentication)
const SESSION_PROTOCOL_VERSION: u8 = 0x00;

const PACKET_TYPE_DATA: u8 = 0;
const PACKET_TYPE_NOP: u8 = 1;
const PACKET_TYPE_KEY_OFFER: u8 = 2; // "alice"
const PACKET_TYPE_KEY_COUNTER_OFFER: u8 = 3; // "bob"

const E1_TYPE_NONE: u8 = 0;
const E1_TYPE_KYBER1024: u8 = 1;

const MAX_FRAGMENTS: usize = 48; // protocol max: 63
const KEY_EXCHANGE_MAX_FRAGMENTS: usize = 2; // enough room for p384 + ZT identity + kyber1024 + tag/hmac/etc.
const HEADER_SIZE: usize = 12;
const AES_GCM_TAG_SIZE: usize = 16;
const HMAC_SIZE: usize = 48;
const SESSION_ID_SIZE: usize = 6;

/// Aribitrary starting value for key derivation chain.
///
/// It doesn't matter very much what this is, but it's good for it to be unique.
const KEY_DERIVATION_CHAIN_STARTING_SALT: [u8; 64] = [
    // macOS command line to generate:
    // echo -n 'ZSSP_Noise_IKpsk2_NISTP384_?KYBER1024_AESGCM_SHA512' | shasum -a 512  | cut -d ' ' -f 1 | xxd -r -p | xxd -i
    0x35, 0x6a, 0x75, 0xc0, 0xbf, 0xbe, 0xc3, 0x59, 0x70, 0x94, 0x50, 0x69, 0x4c, 0xa2, 0x08, 0x40, 0xc7, 0xdf, 0x67, 0xa8, 0x68, 0x52, 0x6e, 0xd5, 0xdd, 0x77, 0xec, 0x59, 0x6f, 0x8e, 0xa1, 0x99,
    0xb4, 0x32, 0x85, 0xaf, 0x7f, 0x0d, 0xa9, 0x6c, 0x01, 0xfb, 0x72, 0x46, 0xc0, 0x09, 0x58, 0xb8, 0xe0, 0xa8, 0xcf, 0xb1, 0x58, 0x04, 0x6e, 0x32, 0xba, 0xa8, 0xb8, 0xf9, 0x0a, 0xa4, 0xbf, 0x36,
];

const KBKDF_KEY_USAGE_LABEL_HMAC: u8 = b'M';
const KBKDF_KEY_USAGE_LABEL_AES_GCM_ALICE_TO_BOB: u8 = b'A';
const KBKDF_KEY_USAGE_LABEL_AES_GCM_BOB_TO_ALICE: u8 = b'B';

pub enum Error {
    /// The packet was addressed to an unrecognized local session
    UnknownLocalSessionId(SessionId),

    /// Packet was not well formed
    InvalidPacket,

    /// An invalid paramter was supplied to the function
    InvalidParameter,

    /// Packet failed one or more authentication (MAC) checks
    FailedAuthentication,

    /// New session was rejected by caller's supplied authentication check function
    NewSessionRejected,

    /// Rekeying failed and session secret has reached its maximum usage count
    MaxKeyLifetimeExceeded,

    /// Attempt to send using session without established key.
    SessionNotEstablished,

    /// Packet ignored by rate limiter.
    RateLimited,

    /// Other end sent a protocol version we don't support.
    UnknownProtocolVersion,

    /// Supplied data buffer is too small to receive data.
    DataBufferTooSmall,

    /// An internal error occurred.
    OtherError(Box<dyn std::error::Error>),
}

impl From<std::io::Error> for Error {
    #[cold]
    #[inline(never)]
    fn from(e: std::io::Error) -> Self {
        Self::OtherError(Box::new(e))
    }
}

#[cold]
#[inline(never)]
extern "C" fn unlikely_branch() {}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::UnknownLocalSessionId(id) => f.write_str(format!("UnknownLocalSessionId({})", id.0.get()).as_str()),
            Self::InvalidPacket => f.write_str("InvalidPacket"),
            Self::InvalidParameter => f.write_str("InvalidParameter"),
            Self::FailedAuthentication => f.write_str("FailedAuthentication"),
            Self::NewSessionRejected => f.write_str("NewSessionRejected"),
            Self::MaxKeyLifetimeExceeded => f.write_str("MaxKeyLifetimeExceeded"),
            Self::SessionNotEstablished => f.write_str("SessionNotEstablished"),
            Self::RateLimited => f.write_str("RateLimited"),
            Self::UnknownProtocolVersion => f.write_str("UnknownProtocolVersion"),
            Self::DataBufferTooSmall => f.write_str("DataBufferTooSmall"),
            Self::OtherError(e) => f.write_str(format!("OtherError({})", e.to_string()).as_str()),
        }
    }
}

impl std::error::Error for Error {}

impl std::fmt::Debug for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        std::fmt::Display::fmt(self, f)
    }
}

pub enum ReceiveResult<'a, H: Host> {
    /// Packet is valid, no action needs to be taken.
    Ok,

    /// Packet is valid and contained a data payload.
    OkData(&'a [u8]),

    /// Packet is valid and a new session was created, also includes a reply to be sent back.
    OkNewSession(Session<H>),

    /// Packet appears valid but was ignored as a duplicate.
    Duplicate,

    /// Packet apperas valid but was ignored for another reason.
    Ignored,
}

#[derive(Copy, Clone, PartialEq, Eq, Hash, PartialOrd, Ord)]
#[repr(transparent)]
pub struct SessionId(NonZeroU64);

impl SessionId {
    pub const MAX_BIT_MASK: u64 = 0xffffffffffff;

    pub fn new_from_bytes(b: &[u8]) -> Option<SessionId> {
        if b.len() >= 6 {
            let value = (u32::from_le_bytes(b[..4].try_into().unwrap()) as u64) | (u16::from_le_bytes(b[4..6].try_into().unwrap()) as u64).wrapping_shl(32);
            if value > 0 && value <= Self::MAX_BIT_MASK {
                return Some(Self(NonZeroU64::new(value).unwrap()));
            }
        }
        return None;
    }

    pub fn new_from_reader<R: Read>(r: &mut R) -> std::io::Result<Option<SessionId>> {
        let mut tmp = [0_u8; SESSION_ID_SIZE];
        r.read_exact(&mut tmp)?;
        Ok(Self::new_from_bytes(&tmp))
    }

    pub fn new_random() -> Self {
        Self(NonZeroU64::new((random::next_u64_secure() & Self::MAX_BIT_MASK).max(1)).unwrap())
    }

    #[inline(always)]
    pub fn to_bytes(&self) -> [u8; SESSION_ID_SIZE] {
        self.0.get().to_le_bytes()[0..6].try_into().unwrap()
    }
}

impl TryFrom<u64> for SessionId {
    type Error = self::Error;

    #[inline(always)]
    fn try_from(value: u64) -> Result<Self, Self::Error> {
        if value > 0 && value <= Self::MAX_BIT_MASK {
            Ok(Self(NonZeroU64::new(value).unwrap()))
        } else {
            Err(Error::InvalidParameter)
        }
    }
}

impl From<SessionId> for u64 {
    #[inline(always)]
    fn from(sid: SessionId) -> Self {
        sid.0.get()
    }
}

pub trait Host: Sized {
    type AssociatedObject: Sized;
    type SessionRef: Deref<Target = Session<Self>>;
    type IncomingPacketBuffer: AsRef<[u8]>;

    /// Get a reference to this host's static public key blob.
    fn get_local_s_public(&self) -> &[u8];

    /// Get SHA384(this host's static public key blob)
    fn get_local_s_public_hash(&self) -> &[u8; 48];

    /// Get a reference to this hosts' static public key's NIST P-384 secret key pair
    fn get_local_s_keypair_p384(&self) -> &P384KeyPair;

    /// Extract a NIST P-384 ECC public key from a static public key blob.
    fn extract_p384_static(static_public: &[u8]) -> Option<P384PublicKey>;

    /// Look up a local session by local ID.
    fn session_lookup(&self, local_session_id: SessionId) -> Option<Self::SessionRef>;

    /// Check whether a new session should be accepted.
    ///
    /// On success a tuple of local session ID, static secret, and associated object is returned. The
    /// static secret is whatever results from agreement between the local and remote static public
    /// keys.
    fn accept_new_session(&self, remote_static_public: &[u8], remote_metadata: &[u8]) -> Option<(SessionId, Secret<64>, Self::AssociatedObject)>;
}

pub struct Session<H: Host> {
    pub id: SessionId,
    pub associated_object: H::AssociatedObject,

    send_counter: Counter,
    psk: Secret<64>,
    ss: Secret<48>,
    state: RwLock<MutableState>,
    remote_s_public_hash: [u8; 48],
    remote_s_public_p384: [u8; P384_PUBLIC_KEY_SIZE],
    defrag: Mutex<RingBufferMap<u32, GatherArray<H::IncomingPacketBuffer, MAX_FRAGMENTS>, 64, 32>>,
}

struct MutableState {
    remote_session_id: Option<SessionId>,
    keys: [Option<SessionKey>; 2], // current, next
    offer: Option<EphemeralOffer>,
}

/// State information to associate with receiving contexts such as sockets or remote paths/endpoints.
pub struct ReceiveContext<H: Host> {
    initial_offer_defrag: Mutex<RingBufferMap<u32, GatherArray<H::IncomingPacketBuffer, KEY_EXCHANGE_MAX_FRAGMENTS>, 1024, 256>>,
}

impl<H: Host> Session<H> {
    pub fn new<SendFunction: FnMut(&mut [u8])>(
        host: &H,
        send: SendFunction,
        local_session_id: SessionId,
        remote_s_public: &[u8],
        offer_metadata: &[u8],
        psk: &Secret<64>,
        associated_object: H::AssociatedObject,
        mtu: usize,
        current_time: i64,
        jedi: bool,
    ) -> Result<Self, Error> {
        if let Some(remote_s_public_p384) = H::extract_p384_static(remote_s_public) {
            if let Some(ss) = host.get_local_s_keypair_p384().agree(&remote_s_public_p384) {
                let counter = Counter::new();
                let remote_s_public_hash = SHA384::hash(remote_s_public);
                if let Ok(offer) = EphemeralOffer::create_alice_offer(
                    send,
                    counter.next(),
                    local_session_id,
                    None,
                    host.get_local_s_public(),
                    offer_metadata,
                    &remote_s_public_p384,
                    &remote_s_public_hash,
                    &ss,
                    mtu,
                    current_time,
                    jedi,
                ) {
                    return Ok(Self {
                        id: local_session_id,
                        associated_object,
                        send_counter: counter,
                        psk: psk.clone(),
                        ss,
                        state: RwLock::new(MutableState {
                            remote_session_id: None,
                            keys: [None, None],
                            offer: Some(offer),
                        }),
                        remote_s_public_hash,
                        remote_s_public_p384: remote_s_public_p384.as_bytes().clone(),
                        defrag: Mutex::new(RingBufferMap::new(random::xorshift64_random() as u32)),
                    });
                }
            }
        }
        return Err(Error::InvalidParameter);
    }

    pub fn rekey_check<SendFunction: FnMut(&mut [u8])>(&self, host: &H, send: SendFunction, offer_metadata: &[u8], mtu: usize, current_time: i64, force: bool, jedi: bool) {
        let state = self.state.upgradable_read();
        if let Some(key) = state.keys[0].as_ref() {
            if force || (key.lifetime.should_rekey(self.send_counter.current(), current_time) && state.offer.as_ref().map_or(true, |o| (current_time - o.creation_time) > OFFER_RATE_LIMIT_MS)) {
                if let Some(remote_s_public_p384) = P384PublicKey::from_bytes(&self.remote_s_public_p384) {
                    if let Ok(offer) = EphemeralOffer::create_alice_offer(
                        send,
                        self.send_counter.next(),
                        self.id,
                        state.remote_session_id,
                        host.get_local_s_public(),
                        offer_metadata,
                        &remote_s_public_p384,
                        &self.remote_s_public_hash,
                        &self.ss,
                        mtu,
                        current_time,
                        jedi,
                    ) {
                        let _ = RwLockUpgradableReadGuard::upgrade(state).offer.replace(offer);
                    }
                }
            }
        }
    }
}

impl<H: Host> ReceiveContext<H> {
    pub fn new() -> Self {
        Self {
            initial_offer_defrag: Mutex::new(RingBufferMap::new(random::xorshift64_random() as u32)),
        }
    }

    pub fn receive<'a, SendFunction: FnMut(&mut [u8])>(
        &self,
        host: &H,
        send: SendFunction,
        data_buf: &'a mut [u8],
        incoming_packet_buf: H::IncomingPacketBuffer,
        mtu: usize,
        jedi: bool,
        current_time: i64,
    ) -> Result<ReceiveResult<'a, H>, Error> {
        let incoming_packet = incoming_packet_buf.as_ref();

        if incoming_packet.len() < MIN_PACKET_SIZE {
            unlikely_branch();
            return Err(Error::InvalidPacket);
        }

        let type_and_frag_info = u16::from_le_bytes(incoming_packet[0..2].try_into().unwrap());
        let local_session_id = SessionId::new_from_bytes(&incoming_packet[2..8]);
        let counter = u32::from_le_bytes(incoming_packet[8..12].try_into().unwrap());
        let packet_type = (type_and_frag_info as u8) & 15;
        let fragment_count = type_and_frag_info.wrapping_shr(4) & 63;
        let fragment_no = type_and_frag_info.wrapping_shr(10); // & 63 not needed

        if fragment_count > 1 {
            if let Some(local_session_id) = local_session_id {
                if fragment_count < (MAX_FRAGMENTS as u16) && fragment_no < fragment_count {
                    if let Some(session) = host.session_lookup(local_session_id) {
                        let mut defrag = session.defrag.lock();
                        let fragment_gather_array = defrag.get_or_create_mut(&counter, || GatherArray::new(fragment_count as u32));
                        if let Some(assembled_packet) = fragment_gather_array.add(fragment_no as u32, incoming_packet_buf) {
                            drop(defrag); // release lock
                            return self.receive_complete(host, send, data_buf, assembled_packet.as_ref(), packet_type, Some(session), mtu, jedi, current_time);
                        }
                    } else {
                        unlikely_branch();
                        return Err(Error::UnknownLocalSessionId(local_session_id));
                    }
                } else {
                    unlikely_branch();
                    return Err(Error::InvalidPacket);
                }
            } else {
                if fragment_count < (KEY_EXCHANGE_MAX_FRAGMENTS as u16) && fragment_no < fragment_count {
                    let mut defrag = self.initial_offer_defrag.lock();
                    let fragment_gather_array = defrag.get_or_create_mut(&counter, || GatherArray::new(fragment_count as u32));
                    if let Some(assembled_packet) = fragment_gather_array.add(fragment_no as u32, incoming_packet_buf) {
                        drop(defrag); // release lock
                        return self.receive_complete(host, send, data_buf, assembled_packet.as_ref(), packet_type, None, mtu, jedi, current_time);
                    }
                } else {
                    unlikely_branch();
                    return Err(Error::InvalidPacket);
                }
            }
        } else {
            return self.receive_complete(
                host,
                send,
                data_buf,
                &[incoming_packet_buf],
                packet_type,
                local_session_id.and_then(|lsid| host.session_lookup(lsid)),
                mtu,
                jedi,
                current_time,
            );
        }

        return Ok(ReceiveResult::Ok);
    }

    fn receive_complete<'a, SendFunction: FnMut(&mut [u8])>(
        &self,
        host: &H,
        mut send: SendFunction,
        data_buf: &'a mut [u8],
        fragments: &[H::IncomingPacketBuffer],
        packet_type: u8,
        session: Option<H::SessionRef>,
        mtu: usize,
        jedi: bool,
        current_time: i64,
    ) -> Result<ReceiveResult<'a, H>, Error> {
        debug_assert!(fragments.len() >= 1);

        debug_assert_eq!(PACKET_TYPE_DATA, 0);
        debug_assert_eq!(PACKET_TYPE_NOP, 1);
        if packet_type <= PACKET_TYPE_NOP {
            if let Some(session) = session {
                let state = session.state.read();
                for ki in 0..2 {
                    if let Some(key) = state.keys[ki].as_ref() {
                        let head = fragments.first().unwrap().as_ref();
                        debug_assert!(head.len() >= MIN_PACKET_SIZE);

                        let mut c = key.get_receive_cipher();
                        c.init(&get_aes_gcm_nonce(head));

                        let mut data_len = head.len() - HEADER_SIZE;
                        if data_len > data_buf.len() {
                            unlikely_branch();
                            key.return_receive_cipher(c);
                            return Err(Error::DataBufferTooSmall);
                        }
                        c.crypt(&head[HEADER_SIZE..], &mut data_buf[..data_len]);

                        for fi in 1..(fragments.len() - 1) {
                            let f = fragments[fi].as_ref();
                            debug_assert!(f.len() >= HEADER_SIZE);
                            let current_frag_data_start = data_len;
                            data_len += f.len() - HEADER_SIZE;
                            if data_len > data_buf.len() {
                                unlikely_branch();
                                key.return_receive_cipher(c);
                                return Err(Error::DataBufferTooSmall);
                            }
                            c.crypt(&f[HEADER_SIZE..], &mut data_buf[current_frag_data_start..data_len]);
                        }

                        let tail = fragments.last().unwrap().as_ref();
                        if tail.len() < (HEADER_SIZE + AES_GCM_TAG_SIZE) {
                            unlikely_branch();
                            key.return_receive_cipher(c);
                            return Err(Error::InvalidPacket);
                        }
                        let tail_data_len = tail.len() - (HEADER_SIZE + AES_GCM_TAG_SIZE);
                        let current_frag_data_start = data_len;
                        data_len += tail_data_len;
                        if data_len > data_buf.len() {
                            unlikely_branch();
                            key.return_receive_cipher(c);
                            return Err(Error::DataBufferTooSmall);
                        }
                        c.crypt(&tail[HEADER_SIZE..tail_data_len], &mut data_buf[current_frag_data_start..data_len]);

                        let tag = c.finish();
                        key.return_receive_cipher(c);

                        if tag.eq(&tail[(tail.len() - AES_GCM_TAG_SIZE)..]) {
                            if ki == 1 {
                                // Promote next key to current key on success.
                                unlikely_branch();
                                drop(state);
                                let mut state = session.state.write();
                                state.keys[0] = state.keys[1].take();
                            }

                            if packet_type == PACKET_TYPE_DATA {
                                return Ok(ReceiveResult::OkData(&data_buf[..data_len]));
                            } else {
                                return Ok(ReceiveResult::Ok);
                            }
                        }
                    }
                }
                return Err(Error::FailedAuthentication);
            } else {
                unlikely_branch();
                return Err(Error::SessionNotEstablished);
            }
        } else {
            unlikely_branch();

            let mut incoming_packet_buf = [0_u8; MIN_MTU * KEY_EXCHANGE_MAX_FRAGMENTS];
            let mut incoming_packet_len = 0;
            for i in 0..fragments.len() {
                let mut ff = fragments[i].as_ref();
                debug_assert!(ff.len() >= MIN_PACKET_SIZE);
                if i > 0 {
                    ff = &ff[HEADER_SIZE..];
                }
                let j = incoming_packet_len + ff.len();
                if j > incoming_packet_buf.len() {
                    return Err(Error::InvalidPacket);
                }
                incoming_packet_buf[incoming_packet_len..j].copy_from_slice(ff);
                incoming_packet_len = j;
            }
            let original_ciphertext = incoming_packet_buf.clone();
            let incoming_packet = &mut incoming_packet_buf[..incoming_packet_len];

            if incoming_packet_len < (HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE + AES_GCM_TAG_SIZE + HMAC_SIZE) {
                return Err(Error::InvalidPacket);
            }
            if incoming_packet[HEADER_SIZE] != SESSION_PROTOCOL_VERSION {
                return Err(Error::UnknownProtocolVersion);
            }

            let local_s_keypair_p384 = host.get_local_s_keypair_p384();

            match packet_type {
                PACKET_TYPE_KEY_OFFER => {
                    // alice (remote) -> bob (local)

                    let payload_end = incoming_packet_len - (AES_GCM_TAG_SIZE + HMAC_SIZE + HMAC_SIZE);
                    let aes_gcm_tag_end = incoming_packet_len - (HMAC_SIZE + HMAC_SIZE);
                    let hmac1_end = incoming_packet_len - HMAC_SIZE;

                    // Check that the sender knows this host's identity before doing anything else.
                    if !hmac_sha384(host.get_local_s_public_hash(), &incoming_packet[..hmac1_end]).eq(&incoming_packet[hmac1_end..]) {
                        return Err(Error::FailedAuthentication);
                    }

                    // Check rate limit if this session is known.
                    if let Some(session) = session.as_ref() {
                        if let Some(offer) = session.state.read().offer.as_ref() {
                            if (current_time - offer.creation_time) < OFFER_RATE_LIMIT_MS {
                                return Err(Error::RateLimited);
                            }
                        }
                    }

                    let (alice_e0_public, e0s) = P384PublicKey::from_bytes(&incoming_packet[(HEADER_SIZE + 1)..(HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE)])
                        .and_then(|pk| local_s_keypair_p384.agree(&pk).map(move |s| (pk, s)))
                        .ok_or(Error::FailedAuthentication)?;

                    let key = Secret(hmac_sha512(&hmac_sha512(&KEY_DERIVATION_CHAIN_STARTING_SALT, alice_e0_public.as_bytes()), e0s.as_bytes()));

                    let mut c = AesGcm::new(kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_AES_GCM_ALICE_TO_BOB).first_n::<32>(), false);
                    c.init(&get_aes_gcm_nonce(incoming_packet));
                    c.crypt_in_place(&mut incoming_packet[(HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE)..payload_end]);
                    let c = c.finish();
                    if !c.eq(&incoming_packet[payload_end..aes_gcm_tag_end]) {
                        return Err(Error::FailedAuthentication);
                    }

                    let (alice_session_id, alice_s_public, alice_metadata, alice_e1_public) = parse_key_offer_after_header(&incoming_packet[(HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE)..], packet_type)?;

                    if let Some(session) = session.as_ref() {
                        // Important! If there's already a session, make sure the caller is the same endpoint as that session!
                        if !session.remote_s_public_hash.eq(&SHA384::hash(&alice_s_public)) {
                            return Err(Error::FailedAuthentication);
                        }
                    }

                    let alice_s_public_p384 = H::extract_p384_static(alice_s_public).ok_or(Error::InvalidPacket)?;
                    let ss = local_s_keypair_p384.agree(&alice_s_public_p384).ok_or(Error::FailedAuthentication)?;

                    let key = Secret(hmac_sha512(key.as_bytes(), ss.as_bytes()));

                    if !hmac_sha384(kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_HMAC).first_n::<48>(), &original_ciphertext[..aes_gcm_tag_end]).eq(&incoming_packet[aes_gcm_tag_end..hmac1_end]) {
                        return Err(Error::FailedAuthentication);
                    }

                    // Alice's offer has been verified and her current key state reconstructed.

                    let bob_e0_keypair = P384KeyPair::generate();
                    let e0e0 = bob_e0_keypair.agree(&alice_e0_public).ok_or(Error::FailedAuthentication)?;
                    let se0 = bob_e0_keypair.agree(&alice_s_public_p384).ok_or(Error::FailedAuthentication)?;

                    let new_session = if session.is_some() {
                        None
                    } else {
                        if let Some((new_session_id, psk, associated_object)) = host.accept_new_session(alice_s_public, alice_metadata) {
                            Some(Session::<H> {
                                id: new_session_id,
                                associated_object,
                                send_counter: Counter::new(),
                                psk,
                                ss,
                                state: RwLock::new(MutableState {
                                    remote_session_id: Some(alice_session_id),
                                    keys: [None, None],
                                    offer: None,
                                }),
                                remote_s_public_hash: SHA384::hash(&alice_s_public),
                                remote_s_public_p384: alice_s_public_p384.as_bytes().clone(),
                                defrag: Mutex::new(RingBufferMap::new(random::xorshift64_random() as u32)),
                            })
                        } else {
                            return Err(Error::NewSessionRejected);
                        }
                    };
                    let session_ref = session;
                    let session = session_ref.as_ref().map_or_else(|| new_session.as_ref().unwrap(), |s| &*s);

                    // FIPS note: the order of HMAC parameters are flipped here from the usual Noise HMAC(key, X). That's because
                    // NIST/FIPS allows HKDF with HMAC(salt, key) and salt is allowed to be anything. This way if the PSK is not
                    // FIPS compliant the compliance of the entire key derivation is not invalidated. Both inputs are secrets of
                    // fixed size so this shouldn't matter cryptographically.
                    let key = Secret(hmac_sha512(
                        session.psk.as_bytes(),
                        &hmac_sha512(&hmac_sha512(&hmac_sha512(key.as_bytes(), bob_e0_keypair.public_key_bytes()), e0e0.as_bytes()), se0.as_bytes()),
                    ));

                    // At this point we've completed Noise_IK key derivation with NIST P-384 ECDH, but see final step below...

                    let (bob_e1_public, e1e1) = if jedi && alice_e1_public.len() > 0 {
                        if let Ok((bob_e1_public, e1e1)) = pqc_kyber::encapsulate(alice_e1_public, &mut random::SecureRandom::default()) {
                            (Some(bob_e1_public), Secret(e1e1))
                        } else {
                            return Err(Error::FailedAuthentication);
                        }
                    } else {
                        (None, Secret::default()) // use all zero Kyber secret if disabled
                    };

                    let mut reply_buf = [0_u8; MIN_MTU * KEY_EXCHANGE_MAX_FRAGMENTS];
                    let reply_counter = session.send_counter.next();
                    let mut reply_len = {
                        let mut rp = &mut reply_buf[HEADER_SIZE..];

                        rp.write_all(&[SESSION_PROTOCOL_VERSION])?;
                        rp.write_all(bob_e0_keypair.public_key_bytes())?;

                        rp.write_all(&session.id.to_bytes())?;
                        varint::write(&mut rp, 0)?; // they don't need our static public; they have it
                        varint::write(&mut rp, 0)?; // no meta-data yet
                        if let Some(bob_e1_public) = bob_e1_public.as_ref() {
                            rp.write_all(&[E1_TYPE_KYBER1024])?;
                            rp.write_all(bob_e1_public)?;
                        } else {
                            rp.write_all(&[E1_TYPE_NONE])?;
                        }
                        (MIN_MTU * KEY_EXCHANGE_MAX_FRAGMENTS) - rp.len()
                    };
                    let mut header = send_with_fragmentation_init_header(reply_len, mtu, PACKET_TYPE_KEY_OFFER, session.id.into(), reply_counter);
                    reply_buf[..HEADER_SIZE].copy_from_slice(&header);

                    let mut c = AesGcm::new(kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_AES_GCM_BOB_TO_ALICE).first_n::<32>(), true);
                    c.init(&get_aes_gcm_nonce(&header));
                    c.crypt_in_place(&mut reply_buf[(HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE)..reply_len]);
                    let c = c.finish();
                    reply_buf[reply_len..(reply_len + AES_GCM_TAG_SIZE)].copy_from_slice(&c);
                    reply_len += AES_GCM_TAG_SIZE;

                    // Normal Noise_IK is done, but we have one more step: mix in the Kyber shared secret (or all zeroes if Kyber is
                    // disabled). We have to wait until this point because Kyber's keys are encrypted and can't be decrypted until
                    // the P-384 exchange is done. We also flip the HMAC parameter order here for the same reason we do in the previous
                    // key derivation step.
                    let key = Secret(hmac_sha512(e1e1.as_bytes(), key.as_bytes()));

                    let hmac = hmac_sha384(kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_HMAC).first_n::<48>(), &reply_buf[..reply_len]);
                    reply_buf[reply_len..(reply_len + HMAC_SIZE)].copy_from_slice(&hmac);
                    reply_len += HMAC_SIZE;

                    let mut state = session.state.write();
                    let _ = state.remote_session_id.replace(alice_session_id);
                    state.keys[1].replace(SessionKey::new(key, Role::Bob, current_time, reply_counter, jedi));
                    drop(state);

                    // Bob now has final key state for this exchange. Yay! Now reply to Alice so she can construct it.

                    send_with_fragmentation(send, &mut reply_buf[..reply_len], mtu, &mut header);
                    if new_session.is_some() {
                        return Ok(ReceiveResult::OkNewSession(new_session.unwrap()));
                    } else {
                        return Ok(ReceiveResult::Ok);
                    }
                }

                PACKET_TYPE_KEY_COUNTER_OFFER => {
                    // bob (remote) -> alice (local)

                    let payload_end = incoming_packet_len - (AES_GCM_TAG_SIZE + HMAC_SIZE);
                    let aes_gcm_tag_end = incoming_packet_len - HMAC_SIZE;

                    if let Some(session) = session {
                        let state = session.state.upgradable_read();
                        if let Some(offer) = state.offer.as_ref() {
                            let (bob_e0_public, e0e0) = P384PublicKey::from_bytes(&incoming_packet[(HEADER_SIZE + 1)..(HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE)])
                                .and_then(|pk| offer.alice_e0_keypair.agree(&pk).map(move |s| (pk, s)))
                                .ok_or(Error::FailedAuthentication)?;
                            let se0 = local_s_keypair_p384.agree(&bob_e0_public).ok_or(Error::FailedAuthentication)?;

                            let key = Secret(hmac_sha512(
                                session.psk.as_bytes(),
                                &hmac_sha512(&hmac_sha512(&hmac_sha512(offer.key.as_bytes(), bob_e0_public.as_bytes()), e0e0.as_bytes()), se0.as_bytes()),
                            ));

                            let mut c = AesGcm::new(kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_AES_GCM_BOB_TO_ALICE).first_n::<32>(), false);
                            c.init(&get_aes_gcm_nonce(incoming_packet));
                            c.crypt_in_place(&mut incoming_packet[(HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE)..payload_end]);
                            let c = c.finish();
                            if !c.eq(&incoming_packet[payload_end..aes_gcm_tag_end]) {
                                return Err(Error::FailedAuthentication);
                            }

                            // Alice has now completed Noise_IK for P-384 and verified with GCM auth, now for the hybrid add-on.

                            let (bob_session_id, _, _, bob_e1_public) = parse_key_offer_after_header(&incoming_packet[(HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE)..], packet_type)?;

                            let e1e1 = if jedi && bob_e1_public.len() > 0 && offer.alice_e1_keypair.is_some() {
                                if let Ok(e1e1) = pqc_kyber::decapsulate(bob_e1_public, &offer.alice_e1_keypair.as_ref().unwrap().secret) {
                                    Secret(e1e1)
                                } else {
                                    return Err(Error::FailedAuthentication);
                                }
                            } else {
                                Secret::default()
                            };

                            let key = Secret(hmac_sha512(e1e1.as_bytes(), key.as_bytes()));

                            if !hmac_sha384(kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_HMAC).first_n::<48>(), &original_ciphertext[..aes_gcm_tag_end])
                                .eq(&incoming_packet[aes_gcm_tag_end..incoming_packet.len()])
                            {
                                return Err(Error::FailedAuthentication);
                            }

                            // Alice has now completed and validated the full hybrid exchange. If this is the first exchange send
                            // a NOP back to Bob to acknowledge that the session is open and can now be used. Otherwise just queue
                            // this up as the next key to be promoted to current when Bob uses it.

                            let mut state = RwLockUpgradableReadGuard::upgrade(state);
                            let _ = state.offer.take();
                            let _ = state.remote_session_id.replace(bob_session_id);
                            if state.keys[0].is_some() {
                                let _ = state.keys[1].replace(SessionKey::new(key, Role::Alice, current_time, session.send_counter.current(), jedi));
                            } else {
                                let counter = session.send_counter.next();
                                let key = SessionKey::new(key, Role::Alice, current_time, counter, jedi);

                                let mut reply_buf = [0_u8; MIN_MTU];
                                let dummy_data_len = (random::next_u32_secure() % (mtu - (HEADER_SIZE + AES_GCM_TAG_SIZE)) as u32) as usize;
                                let reply_len = dummy_data_len + HEADER_SIZE + AES_GCM_TAG_SIZE;
                                let header = send_with_fragmentation_init_header(reply_len, mtu, PACKET_TYPE_NOP, bob_session_id.into(), counter);
                                reply_buf[..HEADER_SIZE].copy_from_slice(&header);

                                let mut c = key.get_send_cipher(counter)?;
                                c.init(&get_aes_gcm_nonce(&reply_buf));
                                c.crypt_in_place(&mut reply_buf[HEADER_SIZE..(HEADER_SIZE + dummy_data_len)]);
                                reply_buf[(HEADER_SIZE + dummy_data_len)..reply_len].copy_from_slice(&c.finish());
                                key.return_send_cipher(c);

                                send(&mut reply_buf[..reply_len]);

                                let _ = state.keys[0].replace(key);
                                let _ = state.keys[1].take();
                            }

                            return Ok(ReceiveResult::Ok);
                        }
                    }

                    // Just ignore counter-offers that are out of place. They probably indicate that this side
                    // restarted and needs to establish a new session.
                    return Ok(ReceiveResult::Ignored);
                }

                _ => return Err(Error::InvalidPacket),
            }
        }
    }
}

struct Counter(AtomicU64);

impl Counter {
    fn new() -> Self {
        Self(AtomicU64::new(random::next_u32_secure() as u64))
    }

    #[inline(always)]
    fn current(&self) -> CounterValue {
        CounterValue(self.0.load(Ordering::SeqCst))
    }

    #[inline(always)]
    fn next(&self) -> CounterValue {
        CounterValue(self.0.fetch_add(1, Ordering::SeqCst))
    }
}

/// A value of the outgoing packet counter.
///
/// The counter is internally 64-bit so we can more easily track usage limits without
/// confusing logic to handle 32-bit wrapping. The least significant 32 bits are the
/// actual counter put in the packet.
#[repr(transparent)]
#[derive(Copy, Clone)]
struct CounterValue(u64);

impl CounterValue {
    #[inline(always)]
    pub fn to_u32(&self) -> u32 {
        self.0 as u32
    }
}

struct KeyLifetime {
    rekey_at_or_after_counter: u64,
    hard_expire_at_counter: u64,
    rekey_at_or_after_timestamp: i64,
}

impl KeyLifetime {
    fn new(current_counter: CounterValue, current_time: i64) -> Self {
        Self {
            rekey_at_or_after_counter: current_counter.0 + REKEY_AFTER_USES + (random::next_u32_secure() % REKEY_AFTER_USES_MAX_JITTER) as u64,
            hard_expire_at_counter: current_counter.0 + EXPIRE_AFTER_USES,
            rekey_at_or_after_timestamp: current_time + REKEY_AFTER_TIME_MS + (random::next_u32_secure() % REKEY_AFTER_TIME_MS_MAX_JITTER) as i64,
        }
    }

    #[inline(always)]
    fn should_rekey(&self, counter: CounterValue, current_time: i64) -> bool {
        counter.0 >= self.rekey_at_or_after_counter || current_time >= self.rekey_at_or_after_timestamp
    }

    #[inline(always)]
    fn expired(&self, counter: CounterValue) -> bool {
        counter.0 >= self.hard_expire_at_counter
    }
}

/// Ephemeral offer sent with KEY_OFFER and rememebered so state can be reconstructed on COUNTER_OFFER.
struct EphemeralOffer {
    creation_time: i64,
    key: Secret<64>,
    alice_e0_keypair: P384KeyPair,
    alice_e1_keypair: Option<pqc_kyber::Keypair>,
}

impl EphemeralOffer {
    fn create_alice_offer<SendFunction: FnMut(&mut [u8])>(
        send: SendFunction,
        counter: CounterValue,
        alice_session_id: SessionId,
        bob_session_id: Option<SessionId>,
        alice_s_public: &[u8],
        alice_metadata: &[u8],
        bob_s_public_p384: &P384PublicKey,
        bob_s_public_hash: &[u8],
        ss: &Secret<48>,
        mtu: usize,
        current_time: i64,
        jedi: bool,
    ) -> Result<EphemeralOffer, Error> {
        let alice_e0_keypair = P384KeyPair::generate();
        let e0s = alice_e0_keypair.agree(bob_s_public_p384);
        if e0s.is_none() {
            return Err(Error::InvalidPacket);
        }

        let alice_e1_keypair = if jedi {
            Some(pqc_kyber::keypair(&mut random::SecureRandom::get()))
        } else {
            None
        };

        const PACKET_BUF_SIZE: usize = MIN_MTU * KEY_EXCHANGE_MAX_FRAGMENTS;
        let mut packet_buf = [0_u8; PACKET_BUF_SIZE];
        let mut packet_len = {
            let mut p = &mut packet_buf[HEADER_SIZE..];

            p.write_all(&[SESSION_PROTOCOL_VERSION])?;
            p.write_all(alice_e0_keypair.public_key_bytes())?;

            p.write_all(&alice_session_id.0.get().to_le_bytes()[..SESSION_ID_SIZE])?;
            varint::write(&mut p, alice_s_public.len() as u64)?;
            p.write_all(alice_s_public)?;
            varint::write(&mut p, alice_metadata.len() as u64)?;
            p.write_all(alice_metadata)?;
            if let Some(e1kp) = alice_e1_keypair {
                p.write_all(&[E1_TYPE_KYBER1024])?;
                p.write_all(&e1kp.public)?;
            } else {
                p.write_all(&[E1_TYPE_NONE])?;
            }

            PACKET_BUF_SIZE - p.len()
        };

        let mut header = send_with_fragmentation_init_header(packet_len, mtu, PACKET_TYPE_KEY_OFFER, bob_session_id.map_or(0_u64, |i| i.into()), counter);
        packet_buf[..HEADER_SIZE].copy_from_slice(&header);

        let key = Secret(hmac_sha512(
            &hmac_sha512(&KEY_DERIVATION_CHAIN_STARTING_SALT, alice_e0_keypair.public_key_bytes()),
            e0s.unwrap().as_bytes(),
        ));

        let gcm_tag = {
            let mut c = AesGcm::new(kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_AES_GCM_ALICE_TO_BOB).first_n::<32>(), true);
            c.init(&get_aes_gcm_nonce(&packet_buf));
            c.crypt_in_place(&mut packet_buf[(HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE)..packet_len]);
            c.finish()
        };
        packet_buf[packet_len..(packet_len + AES_GCM_TAG_SIZE)].copy_from_slice(&gcm_tag);
        packet_len += AES_GCM_TAG_SIZE;

        let key = Secret(hmac_sha512(key.as_bytes(), ss.as_bytes()));

        let hmac = hmac_sha384(kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_HMAC).first_n::<48>(), &packet_buf[..packet_len]);
        packet_buf[packet_len..(packet_len + HMAC_SIZE)].copy_from_slice(&hmac);
        packet_len += HMAC_SIZE;

        let hmac = hmac_sha384(bob_s_public_hash, &packet_buf[..packet_len]);
        packet_buf[packet_len..(packet_len + HMAC_SIZE)].copy_from_slice(&hmac);
        packet_len += HMAC_SIZE;

        send_with_fragmentation(send, &mut packet_buf[..packet_len], mtu, &mut header);

        Ok(EphemeralOffer {
            creation_time: current_time,
            key,
            alice_e0_keypair,
            alice_e1_keypair,
        })
    }
}

#[inline(always)]
fn send_with_fragmentation_init_header(packet_len: usize, mtu: usize, packet_type: u8, recipient_session_id: u64, counter: CounterValue) -> [u8; 12] {
    let fragment_count = (packet_len / mtu) + (((packet_len % mtu) != 0) as usize);
    debug_assert!(mtu >= MIN_MTU);
    debug_assert!(packet_len >= HEADER_SIZE);
    debug_assert!(fragment_count <= MAX_FRAGMENTS);
    debug_assert!(packet_type <= 0x0f); // packet type is 4 bits
    debug_assert!(recipient_session_id <= 0xffffffffffff); // session ID is 48 bits

    // Header bytes: TTRRRRRRCCCC where T == type/fragment, R == recipient session ID, C == counter
    let mut header = (fragment_count.wrapping_shl(4) | (packet_type as usize)) as u128;
    header |= recipient_session_id.wrapping_shl(16) as u128;
    header |= (counter.to_u32() as u128).wrapping_shl(64);
    header.to_le_bytes()[..HEADER_SIZE].try_into().unwrap()
}

#[inline(always)]
fn send_with_fragmentation<SendFunction: FnMut(&mut [u8])>(mut send: SendFunction, packet: &mut [u8], mtu: usize, header: &mut [u8; HEADER_SIZE]) {
    let packet_len = packet.len();
    let mut fragment_start = 0;
    let mut fragment_end = packet_len.min(mtu);
    loop {
        send(&mut packet[fragment_start..fragment_end]);
        if fragment_end < packet_len {
            fragment_start = fragment_end - HEADER_SIZE;
            fragment_end = (fragment_start + mtu).min(packet_len);
            header[1] += 0x04; // increment fragment number, at bit 2 in byte 1 since type/fragment u16 is little-endian
            packet[fragment_start..(fragment_start + HEADER_SIZE)].copy_from_slice(header);
        } else {
            debug_assert_eq!(fragment_end, packet_len);
            break;
        }
    }
}

fn parse_key_offer_after_header(incoming_packet: &[u8], packet_type: u8) -> Result<(SessionId, &[u8], &[u8], &[u8]), Error> {
    let mut p = &incoming_packet[..];
    let alice_session_id = SessionId::new_from_reader(&mut p)?;
    if alice_session_id.is_none() {
        return Err(Error::InvalidPacket);
    }
    let alice_session_id = alice_session_id.unwrap();
    let alice_s_public_len = varint::read(&mut p)?.0;
    if (p.len() as u64) < alice_s_public_len {
        return Err(Error::InvalidPacket);
    }
    let alice_s_public = &p[..(alice_s_public_len as usize)];
    p = &p[(alice_s_public_len as usize)..];
    let alice_metadata_len = varint::read(&mut p)?.0;
    if (p.len() as u64) < alice_metadata_len {
        return Err(Error::InvalidPacket);
    }
    let alice_metadata = &p[..(alice_metadata_len as usize)];
    p = &p[(alice_metadata_len as usize)..];
    if p.is_empty() {
        return Err(Error::InvalidPacket);
    }
    let alice_e1_public = match p[0] {
        E1_TYPE_KYBER1024 => {
            if packet_type == PACKET_TYPE_KEY_OFFER {
                if p.len() < (pqc_kyber::KYBER_PUBLICKEYBYTES + 1) {
                    return Err(Error::InvalidPacket);
                }
                &p[1..(pqc_kyber::KYBER_PUBLICKEYBYTES + 1)]
            } else {
                if p.len() < (pqc_kyber::KYBER_CIPHERTEXTBYTES + 1) {
                    return Err(Error::InvalidPacket);
                }
                &p[1..(pqc_kyber::KYBER_CIPHERTEXTBYTES + 1)]
            }
        }
        _ => &[],
    };

    Ok((alice_session_id, alice_s_public, alice_metadata, alice_e1_public))
}

enum Role {
    Alice,
    Bob,
}

#[allow(unused)]
struct SessionKey {
    lifetime: KeyLifetime,
    receive_key: Secret<32>,
    send_key: Secret<32>,
    receive_cipher_pool: Mutex<Vec<Box<AesGcm>>>,
    send_cipher_pool: Mutex<Vec<Box<AesGcm>>>,
    role: Role,
    jedi: bool, // true if kyber was used
}

impl SessionKey {
    /// Create a new symmetric shared session key and set its key expiration times, etc.
    fn new(key: Secret<64>, role: Role, current_time: i64, current_counter: CounterValue, jedi: bool) -> Self {
        let a2b: Secret<32> = kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_AES_GCM_ALICE_TO_BOB).first_n_clone();
        let b2a: Secret<32> = kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_AES_GCM_BOB_TO_ALICE).first_n_clone();
        let (receive_key, send_key) = match role {
            Role::Alice => (b2a, a2b),
            Role::Bob => (a2b, b2a),
        };
        Self {
            lifetime: KeyLifetime::new(current_counter, current_time),
            receive_key,
            send_key,
            receive_cipher_pool: Mutex::new(Vec::with_capacity(2)),
            send_cipher_pool: Mutex::new(Vec::with_capacity(2)),
            role,
            jedi,
        }
    }

    #[inline(always)]
    fn get_send_cipher(&self, counter: CounterValue) -> Result<Box<AesGcm>, Error> {
        if !self.lifetime.expired(counter) {
            Ok(self.send_cipher_pool.lock().pop().unwrap_or_else(|| Box::new(AesGcm::new(self.send_key.as_bytes(), true))))
        } else {
            Err(Error::MaxKeyLifetimeExceeded)
        }
    }

    #[inline(always)]
    fn return_send_cipher(&self, c: Box<AesGcm>) {
        self.send_cipher_pool.lock().push(c);
    }

    #[inline(always)]
    fn get_receive_cipher(&self) -> Box<AesGcm> {
        self.receive_cipher_pool.lock().pop().unwrap_or_else(|| Box::new(AesGcm::new(self.receive_key.as_bytes(), false)))
    }

    #[inline(always)]
    fn return_receive_cipher(&self, c: Box<AesGcm>) {
        self.receive_cipher_pool.lock().push(c);
    }
}

/// HMAC-SHA512 key derivation function modeled on: https://csrc.nist.gov/publications/detail/sp/800-108/final (page 12)
fn kbkdf512(key: &[u8], label: u8) -> Secret<64> {
    Secret(hmac_sha512(key, &[0, 0, 0, 0, b'Z', b'T', label, 0, 0, 0, 0, 0x02, 0x00]))
}

#[inline(always)]
fn get_aes_gcm_nonce(packet: &[u8]) -> [u8; 16] {
    let mut tmp = 0u128.to_ne_bytes();
    tmp[..HEADER_SIZE].copy_from_slice(&packet[..HEADER_SIZE]);
    tmp
}

#[cfg(test)]
mod tests {}

/*
#[cfg(test)]
mod tests {
    use std::rc::Rc;

    #[allow(unused_imports)]
    use super::*;

    #[test]
    fn alice_bob() {
        let psk: Secret<64> = Secret::default();
        let mut a_buffer = [0_u8; 1500];
        let mut b_buffer = [0_u8; 1500];
        let alice_static_keypair = P384KeyPair::generate();
        let bob_static_keypair = P384KeyPair::generate();
        let outgoing_obfuscator_to_alice = Obfuscator::new(alice_static_keypair.public_key_bytes());
        let outgoing_obfuscator_to_bob = Obfuscator::new(bob_static_keypair.public_key_bytes());

        let mut from_alice: Vec<Vec<u8>> = Vec::new();
        let mut from_bob: Vec<Vec<u8>> = Vec::new();

        // Session TO Bob, on Alice's side.
        let (alice, packet) = Session::new(
            &mut a_buffer,
            SessionId::new_random(),
            alice_static_keypair.public_key_bytes(),
            &alice_static_keypair,
            bob_static_keypair.public_key_bytes(),
            bob_static_keypair.public_key(),
            &psk,
            0,
            0,
            true,
        )
        .unwrap();
        let alice = Rc::new(alice);
        from_alice.push(packet.to_vec());

        // Session FROM Alice, on Bob's side.
        let mut bob: Option<Rc<Session<u32>>> = None;

        for _ in 0..256 {
            while !from_alice.is_empty() || !from_bob.is_empty() {
                if let Some(packet) = from_alice.pop() {
                    let r = Session::receive(
                        packet.as_slice(),
                        &mut b_buffer,
                        &bob_static_keypair,
                        &outgoing_obfuscator_to_bob,
                        |p: &[u8; P384_PUBLIC_KEY_SIZE]| P384PublicKey::from_bytes(p),
                        |sid| {
                            println!("[noise] [bob] session ID: {}", u64::from(sid));
                            if let Some(bob) = bob.as_ref() {
                                if sid == bob.id {
                                    Some(bob.clone())
                                } else {
                                    None
                                }
                            } else {
                                None
                            }
                        },
                        |_: &[u8; P384_PUBLIC_KEY_SIZE]| {
                            if bob.is_none() {
                                Some((SessionId::new_random(), psk.clone(), 0))
                            } else {
                                panic!("[noise] [bob] Bob received a second new session request from Alice");
                            }
                        },
                        0,
                        true,
                    );
                    if let Ok(r) = r {
                        match r {
                            ReceiveResult::OkData(data, counter) => {
                                println!("[noise] [bob] DATA len=={} counter=={}", data.len(), counter);
                            }
                            ReceiveResult::OkSendReply(p) => {
                                println!("[noise] [bob] OK (reply: {} bytes)", p.len());
                                from_bob.push(p.to_vec());
                            }
                            ReceiveResult::OkNewSession(ns, p) => {
                                if bob.is_some() {
                                    panic!("[noise] [bob] attempt to create new session on Bob's side when he already has one");
                                }
                                let id: u64 = ns.id.into();
                                let _ = bob.replace(Rc::new(ns));
                                from_bob.push(p.to_vec());
                                println!("[noise] [bob] NEW SESSION {}", id);
                            }
                            ReceiveResult::Ok => {
                                println!("[noise] [bob] OK");
                            }
                            ReceiveResult::Duplicate => {
                                println!("[noise] [bob] duplicate packet");
                            }
                            ReceiveResult::Ignored => {
                                println!("[noise] [bob] ignored packet");
                            }
                        }
                    } else {
                        println!("ERROR (bob): {}", r.err().unwrap().to_string());
                        panic!();
                    }
                }

                if let Some(packet) = from_bob.pop() {
                    let r = Session::receive(
                        packet.as_slice(),
                        &mut b_buffer,
                        &alice_static_keypair,
                        &outgoing_obfuscator_to_alice,
                        |p: &[u8; P384_PUBLIC_KEY_SIZE]| P384PublicKey::from_bytes(p),
                        |sid| {
                            println!("[noise] [alice] session ID: {}", u64::from(sid));
                            if sid == alice.id {
                                Some(alice.clone())
                            } else {
                                panic!("[noise] [alice] received from Bob addressed to unknown session ID, not Alice");
                            }
                        },
                        |_: &[u8; P384_PUBLIC_KEY_SIZE]| {
                            panic!("[noise] [alice] Alice received an unexpected new session request from Bob");
                        },
                        0,
                        true,
                    );
                    if let Ok(r) = r {
                        match r {
                            ReceiveResult::OkData(data, counter) => {
                                println!("[noise] [alice] DATA len=={} counter=={}", data.len(), counter);
                            }
                            ReceiveResult::OkSendReply(p) => {
                                println!("[noise] [alice] OK (reply: {} bytes)", p.len());
                                from_alice.push(p.to_vec());
                            }
                            ReceiveResult::OkNewSession(_, _) => {
                                panic!("[noise] [alice] attempt to create new session on Alice's side; Bob should not initiate");
                            }
                            ReceiveResult::Ok => {
                                println!("[noise] [alice] OK");
                            }
                            ReceiveResult::Duplicate => {
                                println!("[noise] [alice] duplicate packet");
                            }
                            ReceiveResult::Ignored => {
                                println!("[noise] [alice] ignored packet");
                            }
                        }
                    } else {
                        println!("ERROR (alice): {}", r.err().unwrap().to_string());
                        panic!();
                    }
                }
            }

            if (random::next_u32_secure() & 1) == 0 {
                from_alice.push(alice.send(&mut a_buffer, &[0_u8; 16]).unwrap().to_vec());
            } else if bob.is_some() {
                from_bob.push(bob.as_ref().unwrap().send(&mut b_buffer, &[0_u8; 16]).unwrap().to_vec());
            }
        }
    }
}
*/
