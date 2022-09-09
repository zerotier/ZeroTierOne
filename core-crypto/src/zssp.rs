// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

// ZSSP: ZeroTier Secure Session Protocol
// FIPS compliant Noise_IK with Jedi powers and built-in attack-resistant large payload (fragmentation) support.

use std::collections::LinkedList;
use std::io::{Read, Write};
use std::num::NonZeroU64;
use std::ops::Deref;
use std::sync::atomic::{AtomicU64, Ordering};

use crate::aes::{Aes, AesGcm};
use crate::hash::{hmac_sha512, HMACSHA384, SHA384};
use crate::p384::{P384KeyPair, P384PublicKey, P384_PUBLIC_KEY_SIZE};
use crate::random;
use crate::secret::Secret;

use zerotier_utils::gatherarray::GatherArray;
use zerotier_utils::memory;
use zerotier_utils::ringbuffermap::RingBufferMap;
use zerotier_utils::varint;

use parking_lot::{Mutex, RwLock, RwLockUpgradableReadGuard};

/// Minimum size of a valid packet.
pub const MIN_PACKET_SIZE: usize = HEADER_SIZE + AES_GCM_TAG_SIZE;

/// Minimum wire MTU for ZSSP to function normally.
pub const MIN_MTU: usize = 1280;

/// Minimum recommended interval between calls to service() on each session, in milliseconds.
pub const SERVICE_INTERVAL: u64 = 10000;

/// Setting this to true enables kyber1024 post-quantum forward secrecy.
///
/// Kyber1024 is used for data forward secrecy but not authentication. Authentication would
/// require Kyber1024 in identities, which would make them huge, and isn't needed for our
/// threat model which is data warehousing today to decrypt tomorrow. Breaking authentication
/// is only relevant today, not in some mid-future where a QC that can break 384-bit ECC
/// exists.
///
/// This is normally enabled but could be disabled at build time for e.g. very small devices.
/// It might not even be necessary there to disable it since it's not that big and is usually
/// faster than NIST P-384 ECDH.
const JEDI: bool = true;

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

// Packet types can range from 0 to 15 (4 bits) -- 0-3 are defined and 4-15 are reserved for future use
const PACKET_TYPE_DATA: u8 = 0;
const PACKET_TYPE_NOP: u8 = 1;
const PACKET_TYPE_KEY_OFFER: u8 = 2; // "alice"
const PACKET_TYPE_KEY_COUNTER_OFFER: u8 = 3; // "bob"

/// No additional keys included for hybrid exchange, just normal Noise_IK with P-384.
const E1_TYPE_NONE: u8 = 0;

/// Kyber1024 key (alice) or ciphertext (bob) included.
const E1_TYPE_KYBER1024: u8 = 1;

/// Maximum number of fragments for data packets.
const MAX_FRAGMENTS: usize = 48; // protocol max: 63

/// Maximum number of fragments for key exchange packets (can be smaller to save memory, only a few needed)
const KEY_EXCHANGE_MAX_FRAGMENTS: usize = 2; // enough room for p384 + ZT identity + kyber1024 + tag/hmac/etc.

/// Size of packet header
const HEADER_SIZE: usize = 16;

/// Size of AES-GCM MAC tags
const AES_GCM_TAG_SIZE: usize = 16;

/// Size of HMAC-SHA384
const HMAC_SIZE: usize = 48;

/// Size of a session ID, which is a bit like a TCP port number.
const SESSION_ID_SIZE: usize = 6;

/// Maximum number of present and future keys to hold at any given time.
const KEY_HISTORY_SIZE_MAX: usize = 3;

// Key usage labels for sub-key derivation using kbkdf (HMAC).
const KBKDF_KEY_USAGE_LABEL_HMAC: u8 = b'M';
const KBKDF_KEY_USAGE_LABEL_HEADER_CHECK: u8 = b'H';
const KBKDF_KEY_USAGE_LABEL_AES_GCM_ALICE_TO_BOB: u8 = b'A';
const KBKDF_KEY_USAGE_LABEL_AES_GCM_BOB_TO_ALICE: u8 = b'B';
const KBKDF_KEY_USAGE_LABEL_RATCHETING: u8 = b'R';

/// Aribitrary starting value for master key derivation.
///
/// It doesn't matter very much what this is but it's good for it to be unique. It should
/// be changed if this code is changed in any cryptographically meaningful way like changing
/// the primary algorithm from NIST P-384.
const INITIAL_KEY: [u8; 64] = [
    // macOS command line to generate:
    // echo -n 'ZSSP_Noise_IKpsk2_NISTP384_?KYBER1024_AESGCM_SHA512' | shasum -a 512  | cut -d ' ' -f 1 | xxd -r -p | xxd -i
    0x35, 0x6a, 0x75, 0xc0, 0xbf, 0xbe, 0xc3, 0x59, 0x70, 0x94, 0x50, 0x69, 0x4c, 0xa2, 0x08, 0x40, 0xc7, 0xdf, 0x67, 0xa8, 0x68, 0x52, 0x6e, 0xd5, 0xdd, 0x77, 0xec, 0x59, 0x6f, 0x8e, 0xa1, 0x99,
    0xb4, 0x32, 0x85, 0xaf, 0x7f, 0x0d, 0xa9, 0x6c, 0x01, 0xfb, 0x72, 0x46, 0xc0, 0x09, 0x58, 0xb8, 0xe0, 0xa8, 0xcf, 0xb1, 0x58, 0x04, 0x6e, 0x32, 0xba, 0xa8, 0xb8, 0xf9, 0x0a, 0xa4, 0xbf, 0x36,
];

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

    /// Data object is too large to send, even fragmented.
    DataTooLarge,

    /// An unexpected error occurred elsewhere in the code (may indicate a bug).
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
            Self::DataTooLarge => f.write_str("DataTooLarge"),
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

    /// Packet is valid and a data payload was decoded and authenticated.
    ///
    /// The returned reference is to the filled parts of the data buffer supplied to receive.
    OkData(&'a mut [u8]),

    /// Packet is valid and a new session was created.
    ///
    /// The session will have already been gated by the accept_new_session() method in the Host trait.
    OkNewSession(Session<H>),

    /// Packet apperas valid but was ignored e.g. as a duplicate.
    Ignored,
}

/// 48-bit session ID (most significant 24 bits of u64 are unused)
#[derive(Copy, Clone, PartialEq, Eq, Hash, PartialOrd, Ord)]
#[repr(transparent)]
pub struct SessionId(NonZeroU64);

impl SessionId {
    pub const MAX_BIT_MASK: u64 = 0xffffffffffff;

    #[inline(always)]
    pub fn new_from_u64(i: u64) -> Option<SessionId> {
        debug_assert!(i <= Self::MAX_BIT_MASK);
        NonZeroU64::new(i).map(|i| Self(i))
    }

    pub fn new_from_reader<R: Read>(r: &mut R) -> std::io::Result<Option<SessionId>> {
        let mut tmp = [0_u8; 8];
        r.read_exact(&mut tmp[..SESSION_ID_SIZE]).map(|_| NonZeroU64::new(u64::from_le_bytes(tmp)).map(|i| Self(i)))
    }
}

impl From<SessionId> for u64 {
    #[inline(always)]
    fn from(sid: SessionId) -> Self {
        sid.0.get()
    }
}

/// Trait to implement to integrate the session into an application.
pub trait Host: Sized {
    /// Arbitrary object that can be associated with sessions.
    type AssociatedObject;

    /// Arbitrary object that dereferences to the session, such as Arc<Session<Self>>.
    type SessionRef: Deref<Target = Session<Self>>;

    /// A buffer containing data read from the network that can be cached.
    ///
    /// This can be e.g. a pooled buffer that automatically returns itself to the pool when dropped.
    type IncomingPacketBuffer: AsRef<[u8]>;

    /// Get a reference to this host's static public key blob.
    ///
    /// This must contain a NIST P-384 public key but can contain other information.
    fn get_local_s_public(&self) -> &[u8];

    /// Get SHA384(this host's static public key blob), included here so we don't have to calculate it each time.
    fn get_local_s_public_hash(&self) -> &[u8; 48];

    /// Get a reference to this hosts' static public key's NIST P-384 secret key pair
    fn get_local_s_keypair_p384(&self) -> &P384KeyPair;

    /// Extract the NIST P-384 ECC public key component from a static public key blob or return None on failure.
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

/// ZSSP bi-directional packet transport channel.
pub struct Session<H: Host> {
    pub id: SessionId,
    pub associated_object: H::AssociatedObject,

    send_counter: Counter,
    psk: Secret<64>,                                  // Arbitrary PSK provided by external code
    ss: Secret<48>,                                   // NIST P-384 raw ECDH key agreement with peer
    header_check_cipher: Aes,                         // Cipher used for fast 32-bit header MAC
    state: RwLock<SessionMutableState>,               // Mutable parts of state (other than defrag buffers)
    remote_s_public_hash: [u8; 48],                   // SHA384(remote static public key blob)
    remote_s_public_p384: [u8; P384_PUBLIC_KEY_SIZE], // Remote NIST P-384 static public key
    defrag: Mutex<RingBufferMap<u32, GatherArray<H::IncomingPacketBuffer, MAX_FRAGMENTS>, 16, 4>>,
}

struct SessionMutableState {
    remote_session_id: Option<SessionId>,
    keys: LinkedList<SessionKey>,
    offer: Option<Box<EphemeralOffer>>,
}

/// State information to associate with receiving contexts such as sockets or remote paths/endpoints.
///
/// This holds the data structures used to defragment incoming packets that are not associated with an
/// existing session, which would be new attempts to create sessions.
pub struct ReceiveContext<H: Host> {
    initial_offer_defrag: Mutex<RingBufferMap<u32, GatherArray<H::IncomingPacketBuffer, KEY_EXCHANGE_MAX_FRAGMENTS>, 1024, 128>>,
    incoming_init_header_check_cipher: Aes,
}

impl<H: Host> Session<H> {
    /// Create a new session and send the first key offer message.
    ///
    /// * `local_session_id` - ID for this side of the session, must be locally unique
    /// * `remote_s_public` - Remote side's public key/identity
    /// * `offer_metadata` - Arbitrary meta-data to send with key offer (empty if none)
    /// * `psk` - Arbitrary pre-shared key to include as initial key material (use all zero secret if none)
    /// * `associated_object` - Arbitrary object to put into session
    /// * `mtu` - Physical wire MTU
    /// * `current_time` - Current monotonic time in milliseconds
    pub fn new<SendFunction: FnMut(&mut [u8])>(
        host: &H,
        mut send: SendFunction,
        local_session_id: SessionId,
        remote_s_public: &[u8],
        offer_metadata: &[u8],
        psk: &Secret<64>,
        associated_object: H::AssociatedObject,
        mtu: usize,
        current_time: i64,
    ) -> Result<Self, Error> {
        if let Some(remote_s_public_p384) = H::extract_p384_static(remote_s_public) {
            if let Some(ss) = host.get_local_s_keypair_p384().agree(&remote_s_public_p384) {
                let send_counter = Counter::new();
                let header_check_cipher = Aes::new(kbkdf512(ss.as_bytes(), KBKDF_KEY_USAGE_LABEL_HEADER_CHECK).first_n::<16>());
                let remote_s_public_hash = SHA384::hash(remote_s_public);
                let outgoing_init_header_check_cipher = Aes::new(kbkdf512(&remote_s_public_hash, KBKDF_KEY_USAGE_LABEL_HEADER_CHECK).first_n::<16>());
                if let Ok(offer) = create_initial_offer(
                    &mut send,
                    send_counter.next(),
                    local_session_id,
                    None,
                    host.get_local_s_public(),
                    offer_metadata,
                    &remote_s_public_p384,
                    &remote_s_public_hash,
                    &ss,
                    None,
                    &outgoing_init_header_check_cipher,
                    mtu,
                    current_time,
                ) {
                    return Ok(Self {
                        id: local_session_id,
                        associated_object,
                        send_counter,
                        psk: psk.clone(),
                        ss,
                        header_check_cipher,
                        state: RwLock::new(SessionMutableState {
                            remote_session_id: None,
                            keys: LinkedList::new(),
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

    /// Send data over the session.
    ///
    /// * `mtu_buffer` - A writable work buffer whose size must be equal to the wire MTU
    /// * `data` - Data to send
    pub fn send<SendFunction: FnMut(&mut [u8])>(&self, mut send: SendFunction, mtu_buffer: &mut [u8], mut data: &[u8]) -> Result<(), Error> {
        debug_assert!(mtu_buffer.len() >= MIN_MTU);
        let state = self.state.read();
        if let Some(remote_session_id) = state.remote_session_id {
            if let Some(key) = state.keys.front() {
                let mut packet_len = data.len() + HEADER_SIZE + AES_GCM_TAG_SIZE;
                let counter = self.send_counter.next();

                create_packet_header(mtu_buffer, packet_len, mtu_buffer.len(), PACKET_TYPE_DATA, remote_session_id.into(), counter)?;

                let mut c = key.get_send_cipher(counter)?;
                c.init(memory::as_byte_array::<Pseudoheader, 12>(&Pseudoheader::make(
                    remote_session_id.into(),
                    PACKET_TYPE_DATA,
                    counter.to_u32(),
                )));

                if packet_len > mtu_buffer.len() {
                    let mut header: [u8; 16] = mtu_buffer[..HEADER_SIZE].try_into().unwrap();
                    let fragment_data_mtu = mtu_buffer.len() - HEADER_SIZE;
                    let last_fragment_data_mtu = mtu_buffer.len() - (HEADER_SIZE + AES_GCM_TAG_SIZE);
                    loop {
                        let fragment_data_size = fragment_data_mtu.min(data.len());
                        let fragment_size = fragment_data_size + HEADER_SIZE;
                        c.crypt(&data[..fragment_data_size], &mut mtu_buffer[HEADER_SIZE..fragment_size]);
                        data = &data[fragment_data_size..];
                        armor_header(mtu_buffer, &self.header_check_cipher);
                        send(&mut mtu_buffer[..fragment_size]);

                        debug_assert!(header[7].wrapping_shr(2) < 63);
                        header[7] += 0x04; // increment fragment number
                        mtu_buffer[..HEADER_SIZE].copy_from_slice(&header);

                        if data.len() <= last_fragment_data_mtu {
                            break;
                        }
                    }
                    packet_len = data.len() + HEADER_SIZE + AES_GCM_TAG_SIZE;
                }

                let gcm_tag_idx = data.len() + HEADER_SIZE;
                c.crypt(data, &mut mtu_buffer[HEADER_SIZE..gcm_tag_idx]);
                mtu_buffer[gcm_tag_idx..packet_len].copy_from_slice(&c.finish_encrypt());

                armor_header(mtu_buffer, &self.header_check_cipher);
                send(&mut mtu_buffer[..packet_len]);

                key.return_send_cipher(c);

                return Ok(());
            }
        }
        return Err(Error::SessionNotEstablished);
    }

    /// Check whether this session is established.
    pub fn established(&self) -> bool {
        let state = self.state.read();
        state.remote_session_id.is_some() && !state.keys.is_empty()
    }

    /// Get information about this session's security state.
    ///
    /// This returns a tuple of: the time at which the current key was established, the length of its ratchet chain,
    /// and whether Kyber1024 was used. None is returned if the session isn't established.
    pub fn security_info(&self) -> Option<(i64, u64, bool)> {
        let state = self.state.read();
        if let Some(key) = state.keys.front() {
            Some((key.establish_time, key.ratchet_count, key.jedi))
        } else {
            None
        }
    }

    /// This function needs to be called on each session at least every SERVICE_INTERVAL milliseconds.
    ///
    /// * `offer_metadata' - Any meta-data to include with initial key offers sent.
    /// * `mtu` - Physical MTU for sent packets
    /// * `current_time` - Current monotonic time in milliseconds
    pub fn service<SendFunction: FnMut(&mut [u8])>(&self, host: &H, mut send: SendFunction, offer_metadata: &[u8], mtu: usize, current_time: i64) {
        let state = self.state.upgradable_read();
        if state.keys.front().map_or(true, |key| key.lifetime.should_rekey(self.send_counter.current(), current_time))
            && state.offer.as_ref().map_or(true, |o| (current_time - o.creation_time) > OFFER_RATE_LIMIT_MS)
        {
            if let Some(remote_s_public_p384) = P384PublicKey::from_bytes(&self.remote_s_public_p384) {
                let mut tmp_header_check_cipher = None;
                if let Ok(offer) = create_initial_offer(
                    &mut send,
                    self.send_counter.next(),
                    self.id,
                    state.remote_session_id,
                    host.get_local_s_public(),
                    offer_metadata,
                    &remote_s_public_p384,
                    &self.remote_s_public_hash,
                    &self.ss,
                    state.keys.front(),
                    if state.remote_session_id.is_some() {
                        &self.header_check_cipher
                    } else {
                        let _ = tmp_header_check_cipher.insert(Aes::new(kbkdf512(&self.remote_s_public_hash, KBKDF_KEY_USAGE_LABEL_HEADER_CHECK).first_n::<16>()));
                        tmp_header_check_cipher.as_ref().unwrap()
                    },
                    mtu,
                    current_time,
                ) {
                    let _ = RwLockUpgradableReadGuard::upgrade(state).offer.replace(offer);
                }
            }
        }
    }
}

impl<H: Host> ReceiveContext<H> {
    pub fn new(host: &H) -> Self {
        Self {
            initial_offer_defrag: Mutex::new(RingBufferMap::new(random::xorshift64_random() as u32)),
            incoming_init_header_check_cipher: Aes::new(kbkdf512(host.get_local_s_public_hash(), KBKDF_KEY_USAGE_LABEL_HEADER_CHECK).first_n::<16>()),
        }
    }

    /// Receive, authenticate, decrypt, and process a physical wire packet.
    ///
    /// `data_buf` - Data buffer that must be as large as the largest supported data object to be transferred (or you'll get errors)
    /// `incoming_packet_buf` - Buffer containing incoming wire packet, ownership taken by receive().
    /// `mtu` - Physical wire MTU
    /// `current_time` - Current monotonic time in milliseconds
    pub fn receive<'a, SendFunction: FnMut(&mut [u8])>(
        &self,
        host: &H,
        mut send: SendFunction,
        data_buf: &'a mut [u8],
        incoming_packet_buf: H::IncomingPacketBuffer,
        mtu: usize,
        current_time: i64,
    ) -> Result<ReceiveResult<'a, H>, Error> {
        let incoming_packet = incoming_packet_buf.as_ref();
        if incoming_packet.len() < MIN_PACKET_SIZE {
            unlikely_branch();
            return Err(Error::InvalidPacket);
        }

        let local_session_id = SessionId::new_from_u64(memory::u64_from_le_bytes(incoming_packet) & SessionId::MAX_BIT_MASK);

        if let Some(local_session_id) = local_session_id {
            if let Some(session) = host.session_lookup(local_session_id) {
                if let Some((packet_type, fragment_count, fragment_no, counter)) = dearmor_header(incoming_packet, &session.header_check_cipher) {
                    if fragment_count > 1 {
                        if fragment_count <= (MAX_FRAGMENTS as u8) && fragment_no < fragment_count {
                            let mut defrag = session.defrag.lock();
                            let fragment_gather_array = defrag.get_or_create_mut(&counter, || GatherArray::new(fragment_count));
                            if let Some(assembled_packet) = fragment_gather_array.add(fragment_no, incoming_packet_buf) {
                                drop(defrag); // release lock
                                return self.receive_complete(
                                    host,
                                    &mut send,
                                    data_buf,
                                    memory::as_byte_array(&Pseudoheader::make(u64::from(local_session_id), packet_type, counter)),
                                    assembled_packet.as_ref(),
                                    packet_type,
                                    Some(session),
                                    mtu,
                                    current_time,
                                );
                            }
                        } else {
                            unlikely_branch();
                            return Err(Error::InvalidPacket);
                        }
                    } else {
                        return self.receive_complete(
                            host,
                            &mut send,
                            data_buf,
                            memory::as_byte_array(&Pseudoheader::make(u64::from(local_session_id), packet_type, counter)),
                            &[incoming_packet_buf],
                            packet_type,
                            Some(session),
                            mtu,
                            current_time,
                        );
                    }
                } else {
                    unlikely_branch();
                    return Err(Error::FailedAuthentication);
                }
            } else {
                unlikely_branch();
                return Err(Error::UnknownLocalSessionId(local_session_id));
            }
        } else {
            unlikely_branch();
            if let Some((packet_type, fragment_count, fragment_no, counter)) = dearmor_header(incoming_packet, &self.incoming_init_header_check_cipher) {
                let mut defrag = self.initial_offer_defrag.lock();
                let fragment_gather_array = defrag.get_or_create_mut(&counter, || GatherArray::new(fragment_count));
                if let Some(assembled_packet) = fragment_gather_array.add(fragment_no, incoming_packet_buf) {
                    drop(defrag); // release lock
                    return self.receive_complete(
                        host,
                        &mut send,
                        data_buf,
                        memory::as_byte_array(&Pseudoheader::make(0, packet_type, counter)),
                        assembled_packet.as_ref(),
                        packet_type,
                        None,
                        mtu,
                        current_time,
                    );
                }
            } else {
                unlikely_branch();
                return Err(Error::FailedAuthentication);
            }
        };

        return Ok(ReceiveResult::Ok);
    }

    fn receive_complete<'a, SendFunction: FnMut(&mut [u8])>(
        &self,
        host: &H,
        send: &mut SendFunction,
        data_buf: &'a mut [u8],
        pseudoheader: &[u8; 12],
        fragments: &[H::IncomingPacketBuffer],
        packet_type: u8,
        session: Option<H::SessionRef>,
        mtu: usize,
        current_time: i64,
    ) -> Result<ReceiveResult<'a, H>, Error> {
        debug_assert!(fragments.len() >= 1);

        debug_assert_eq!(PACKET_TYPE_DATA, 0);
        debug_assert_eq!(PACKET_TYPE_NOP, 1);
        if packet_type <= PACKET_TYPE_NOP {
            if let Some(session) = session {
                let state = session.state.read();
                let key_count = state.keys.len();
                for (key_index, key) in state.keys.iter().enumerate() {
                    let tail = fragments.last().unwrap().as_ref();
                    if tail.len() < (HEADER_SIZE + AES_GCM_TAG_SIZE) {
                        unlikely_branch();
                        return Err(Error::InvalidPacket);
                    }

                    let mut c = key.get_receive_cipher();
                    c.init(pseudoheader);

                    let mut data_len = 0;

                    for f in fragments[..(fragments.len() - 1)].iter() {
                        let f = f.as_ref();
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

                    let current_frag_data_start = data_len;
                    data_len += tail.len() - (HEADER_SIZE + AES_GCM_TAG_SIZE);
                    if data_len > data_buf.len() {
                        unlikely_branch();
                        key.return_receive_cipher(c);
                        return Err(Error::DataBufferTooSmall);
                    }
                    c.crypt(&tail[HEADER_SIZE..(tail.len() - AES_GCM_TAG_SIZE)], &mut data_buf[current_frag_data_start..data_len]);

                    let ok = c.finish_decrypt(&tail[(tail.len() - AES_GCM_TAG_SIZE)..]);
                    key.return_receive_cipher(c);
                    if ok {
                        // Drop obsolete keys if we had to iterate past the first key to get here.
                        if key_index > 0 {
                            unlikely_branch();
                            drop(state);
                            let mut state = session.state.write();
                            if state.keys.len() == key_count {
                                for _ in 0..key_index {
                                    let _ = state.keys.pop_front();
                                }
                            }
                        }

                        if packet_type == PACKET_TYPE_DATA {
                            return Ok(ReceiveResult::OkData(&mut data_buf[..data_len]));
                        } else {
                            unlikely_branch();
                            return Ok(ReceiveResult::Ok);
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

            let mut incoming_packet_buf = [0_u8; 4096];
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

            if incoming_packet[HEADER_SIZE] != SESSION_PROTOCOL_VERSION {
                return Err(Error::UnknownProtocolVersion);
            }

            match packet_type {
                PACKET_TYPE_KEY_OFFER => {
                    // alice (remote) -> bob (local)

                    if incoming_packet_len < (HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE + AES_GCM_TAG_SIZE + HMAC_SIZE + HMAC_SIZE) {
                        return Err(Error::InvalidPacket);
                    }
                    let payload_end = incoming_packet_len - (AES_GCM_TAG_SIZE + HMAC_SIZE + HMAC_SIZE);
                    let aes_gcm_tag_end = incoming_packet_len - (HMAC_SIZE + HMAC_SIZE);
                    let hmac1_end = incoming_packet_len - HMAC_SIZE;

                    // Check that the sender knows this host's identity before doing anything else.
                    if !hmac_sha384_2(host.get_local_s_public_hash(), pseudoheader, &incoming_packet[HEADER_SIZE..hmac1_end]).eq(&incoming_packet[hmac1_end..]) {
                        return Err(Error::FailedAuthentication);
                    }

                    // Check rate limits.
                    if let Some(session) = session.as_ref() {
                        if let Some(offer) = session.state.read().offer.as_ref() {
                            if (current_time - offer.creation_time) < OFFER_RATE_LIMIT_MS {
                                return Err(Error::RateLimited);
                            }
                        }
                    }

                    // Key agreement: alice (remote) ephemeral NIST P-384 <> local static NIST P-384
                    let (alice_e0_public, e0s) = P384PublicKey::from_bytes(&incoming_packet[(HEADER_SIZE + 1)..(HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE)])
                        .and_then(|pk| host.get_local_s_keypair_p384().agree(&pk).map(move |s| (pk, s)))
                        .ok_or(Error::FailedAuthentication)?;

                    // Initial key derivation from starting point, mixing in alice's ephemeral public and the e0<>s shared secret.
                    let mut key = Secret(hmac_sha512(&hmac_sha512(&INITIAL_KEY, alice_e0_public.as_bytes()), e0s.as_bytes()));

                    // Decrypt the encrypted part of the packet payload and authenticate the above key exchange via AES-GCM auth.
                    let mut c = AesGcm::new(kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_AES_GCM_ALICE_TO_BOB).first_n::<32>(), false);
                    c.init(pseudoheader);
                    c.crypt_in_place(&mut incoming_packet[(HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE)..payload_end]);
                    if !c.finish_decrypt(&incoming_packet[payload_end..aes_gcm_tag_end]) {
                        return Err(Error::FailedAuthentication);
                    }

                    // Parse payload and get alice's session ID, alice's public blob, metadata, and (if present) Alice's Kyber1024 public.
                    let (offer_id, alice_session_id, alice_s_public, alice_metadata, alice_e1_public, alice_ratchet_key_id) =
                        parse_key_offer_after_header(&incoming_packet[(HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE)..], packet_type)?;

                    // Important! If there's already a session, make sure the caller is the same endpoint as that session! Also
                    // grab the actual ratchet key if the ratchet key ID the other side sent matches a pre-existing key we have.
                    let (ratchet_key, ratchet_count) = if let Some(session) = session.as_ref() {
                        if !session.remote_s_public_hash.eq(&SHA384::hash(&alice_s_public)) {
                            return Err(Error::FailedAuthentication);
                        }
                        if let Some(alice_ratchet_key_id) = alice_ratchet_key_id.as_ref() {
                            let mut ratchet_key = None;
                            let mut ratchet_count = 0;
                            let state = session.state.read();
                            for k in state.keys.iter() {
                                if SHA384::hash(k.ratchet_key.as_bytes())[..16].eq(alice_ratchet_key_id) {
                                    ratchet_key = Some(k.ratchet_key.clone());
                                    ratchet_count = k.ratchet_count;
                                }
                            }
                            (ratchet_key, ratchet_count)
                        } else {
                            (None, 0)
                        }
                    } else {
                        (None, 0)
                    };

                    // Extract alice's static NIST P-384 public key from her public blob.
                    let alice_s_public_p384 = H::extract_p384_static(alice_s_public).ok_or(Error::InvalidPacket)?;

                    // Key agreement: both sides' static P-384 keys.
                    let ss = host.get_local_s_keypair_p384().agree(&alice_s_public_p384).ok_or(Error::FailedAuthentication)?;

                    // Mix result of 'ss' agreement into master key.
                    key = Secret(hmac_sha512(key.as_bytes(), ss.as_bytes()));

                    // Authenticate entire packet with HMAC-SHA384, verifying alice's identity via 'ss' secret.
                    if !hmac_sha384_2(
                        kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_HMAC).first_n::<48>(),
                        pseudoheader,
                        &original_ciphertext[HEADER_SIZE..aes_gcm_tag_end],
                    )
                    .eq(&incoming_packet[aes_gcm_tag_end..hmac1_end])
                    {
                        return Err(Error::FailedAuthentication);
                    }

                    // Alice's offer has been verified and her current key state reconstructed.

                    // Generate our ephemeral NIST P-384 key pair.
                    let bob_e0_keypair = P384KeyPair::generate();

                    // Key agreement: both sides' ephemeral P-384 public keys.
                    let e0e0 = bob_e0_keypair.agree(&alice_e0_public).ok_or(Error::FailedAuthentication)?;

                    // Key agreement: bob (local) static NIST P-384, alice (remote) ephemeral P-384.
                    let se0 = bob_e0_keypair.agree(&alice_s_public_p384).ok_or(Error::FailedAuthentication)?;

                    // Gate (via host) and then create new session object if this is a new session.
                    let new_session = if session.is_some() {
                        None
                    } else {
                        if let Some((new_session_id, psk, associated_object)) = host.accept_new_session(alice_s_public, alice_metadata) {
                            let header_check_cipher = Aes::new(kbkdf512(ss.as_bytes(), KBKDF_KEY_USAGE_LABEL_HEADER_CHECK).first_n::<16>());
                            Some(Session::<H> {
                                id: new_session_id,
                                associated_object,
                                send_counter: Counter::new(),
                                psk,
                                ss,
                                header_check_cipher,
                                state: RwLock::new(SessionMutableState {
                                    remote_session_id: Some(alice_session_id),
                                    keys: LinkedList::new(),
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

                    // Mix in the psk, the key to this point, our ephemeral public, e0e0, and se0, completing Noise_IK on our side.
                    //
                    // FIPS note: the order of HMAC parameters are flipped here from the usual Noise HMAC(key, X). That's because
                    // NIST/FIPS allows HKDF with HMAC(salt, key) and salt is allowed to be anything. This way if the PSK is not
                    // FIPS compliant the compliance of the entire key derivation is not invalidated. Both inputs are secrets of
                    // fixed size so this shouldn't matter cryptographically.
                    key = Secret(hmac_sha512(
                        session.psk.as_bytes(),
                        &hmac_sha512(&hmac_sha512(&hmac_sha512(key.as_bytes(), bob_e0_keypair.public_key_bytes()), e0e0.as_bytes()), se0.as_bytes()),
                    ));

                    // At this point we've completed Noise_IK key derivation with NIST P-384 ECDH, but now for hybrid and ratcheting...

                    // Generate a Kyber encapsulated ciphertext if Kyber is enabled and the other side sent us a public key.
                    let (bob_e1_public, e1e1) = if JEDI && alice_e1_public.len() > 0 {
                        if let Ok((bob_e1_public, e1e1)) = pqc_kyber::encapsulate(alice_e1_public, &mut random::SecureRandom::default()) {
                            (Some(bob_e1_public), Some(Secret(e1e1)))
                        } else {
                            return Err(Error::FailedAuthentication);
                        }
                    } else {
                        (None, None)
                    };

                    // Create reply packet.
                    const REPLY_BUF_LEN: usize = MIN_MTU * KEY_EXCHANGE_MAX_FRAGMENTS;
                    let mut reply_buf = [0_u8; REPLY_BUF_LEN];
                    let reply_counter = session.send_counter.next();
                    let mut reply_len = {
                        let mut rp = &mut reply_buf[HEADER_SIZE..];

                        rp.write_all(&[SESSION_PROTOCOL_VERSION])?;
                        rp.write_all(bob_e0_keypair.public_key_bytes())?;

                        rp.write_all(&offer_id)?;
                        rp.write_all(&session.id.0.get().to_le_bytes()[..SESSION_ID_SIZE])?;
                        varint::write(&mut rp, 0)?; // they don't need our static public; they have it
                        varint::write(&mut rp, 0)?; // no meta-data in counter-offers (could be used in the future)
                        if let Some(bob_e1_public) = bob_e1_public.as_ref() {
                            rp.write_all(&[E1_TYPE_KYBER1024])?;
                            rp.write_all(bob_e1_public)?;
                        } else {
                            rp.write_all(&[E1_TYPE_NONE])?;
                        }
                        if ratchet_key.is_some() {
                            rp.write_all(&[0x01])?;
                            rp.write_all(alice_ratchet_key_id.as_ref().unwrap())?;
                        } else {
                            rp.write_all(&[0x00])?;
                        }

                        REPLY_BUF_LEN - rp.len()
                    };
                    create_packet_header(&mut reply_buf, reply_len, mtu, PACKET_TYPE_KEY_COUNTER_OFFER, alice_session_id.into(), reply_counter)?;
                    let reply_pseudoheader = Pseudoheader::make(alice_session_id.into(), PACKET_TYPE_KEY_COUNTER_OFFER, reply_counter.to_u32());

                    // Encrypt reply packet using final Noise_IK key BEFORE mixing hybrid or ratcheting, since the other side
                    // must decrypt before doing these things.
                    let mut c = AesGcm::new(kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_AES_GCM_BOB_TO_ALICE).first_n::<32>(), true);
                    c.init(memory::as_byte_array::<Pseudoheader, 12>(&reply_pseudoheader));
                    c.crypt_in_place(&mut reply_buf[(HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE)..reply_len]);
                    let c = c.finish_encrypt();
                    reply_buf[reply_len..(reply_len + AES_GCM_TAG_SIZE)].copy_from_slice(&c);
                    reply_len += AES_GCM_TAG_SIZE;

                    // Mix ratchet key from previous session key (if any) and Kyber1024 hybrid shared key (if any).
                    if let Some(ratchet_key) = ratchet_key {
                        key = Secret(hmac_sha512(ratchet_key.as_bytes(), key.as_bytes()));
                    }
                    if let Some(e1e1) = e1e1.as_ref() {
                        key = Secret(hmac_sha512(e1e1.as_bytes(), key.as_bytes()));
                    }

                    // Authenticate packet using HMAC-SHA384 with final key. Note that while the final key now has the Kyber secret
                    // mixed in, this doesn't constitute session authentication with Kyber because there's no static Kyber key
                    // associated with the remote identity. An attacker who can break NIST P-384 (and has the psk) could MITM the
                    // Kyber exchange, but you'd need a not-yet-existing quantum computer for that.
                    let hmac = hmac_sha384_2(
                        kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_HMAC).first_n::<48>(),
                        memory::as_byte_array::<Pseudoheader, 12>(&reply_pseudoheader),
                        &reply_buf[HEADER_SIZE..reply_len],
                    );
                    reply_buf[reply_len..(reply_len + HMAC_SIZE)].copy_from_slice(&hmac);
                    reply_len += HMAC_SIZE;

                    let mut state = session.state.write();
                    let _ = state.remote_session_id.replace(alice_session_id);
                    add_session_key(&mut state.keys, SessionKey::new(key, Role::Bob, current_time, reply_counter, ratchet_count + 1, e1e1.is_some()));
                    drop(state);

                    // Bob now has final key state for this exchange. Yay! Now reply to Alice so she can construct it.

                    send_with_fragmentation(send, &mut reply_buf[..reply_len], mtu, &session.header_check_cipher);

                    if new_session.is_some() {
                        return Ok(ReceiveResult::OkNewSession(new_session.unwrap()));
                    } else {
                        return Ok(ReceiveResult::Ok);
                    }
                }

                PACKET_TYPE_KEY_COUNTER_OFFER => {
                    // bob (remote) -> alice (local)

                    if incoming_packet_len < (HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE + AES_GCM_TAG_SIZE + HMAC_SIZE) {
                        return Err(Error::InvalidPacket);
                    }
                    let payload_end = incoming_packet_len - (AES_GCM_TAG_SIZE + HMAC_SIZE);
                    let aes_gcm_tag_end = incoming_packet_len - HMAC_SIZE;

                    if let Some(session) = session {
                        let state = session.state.upgradable_read();
                        if let Some(offer) = state.offer.as_ref() {
                            let (bob_e0_public, e0e0) = P384PublicKey::from_bytes(&incoming_packet[(HEADER_SIZE + 1)..(HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE)])
                                .and_then(|pk| offer.alice_e0_keypair.agree(&pk).map(move |s| (pk, s)))
                                .ok_or(Error::FailedAuthentication)?;
                            let se0 = host.get_local_s_keypair_p384().agree(&bob_e0_public).ok_or(Error::FailedAuthentication)?;

                            let mut key = Secret(hmac_sha512(
                                session.psk.as_bytes(),
                                &hmac_sha512(&hmac_sha512(&hmac_sha512(offer.key.as_bytes(), bob_e0_public.as_bytes()), e0e0.as_bytes()), se0.as_bytes()),
                            ));

                            let mut c = AesGcm::new(kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_AES_GCM_BOB_TO_ALICE).first_n::<32>(), false);
                            c.init(pseudoheader);
                            c.crypt_in_place(&mut incoming_packet[(HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE)..payload_end]);
                            if !c.finish_decrypt(&incoming_packet[payload_end..aes_gcm_tag_end]) {
                                return Err(Error::FailedAuthentication);
                            }

                            // Alice has now completed Noise_IK with NIST P-384 and verified with GCM auth, but now for hybrid...

                            let (offer_id, bob_session_id, _, _, bob_e1_public, bob_ratchet_key_id) =
                                parse_key_offer_after_header(&incoming_packet[(HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE)..], packet_type)?;

                            if !offer.id.eq(&offer_id) {
                                return Ok(ReceiveResult::Ignored);
                            }

                            let e1e1 = if JEDI && bob_e1_public.len() > 0 && offer.alice_e1_keypair.is_some() {
                                if let Ok(e1e1) = pqc_kyber::decapsulate(bob_e1_public, &offer.alice_e1_keypair.as_ref().unwrap().secret) {
                                    Some(Secret(e1e1))
                                } else {
                                    return Err(Error::FailedAuthentication);
                                }
                            } else {
                                None
                            };

                            let mut ratchet_count = 0;
                            if bob_ratchet_key_id.is_some() && offer.ratchet_key.is_some() {
                                key = Secret(hmac_sha512(offer.ratchet_key.as_ref().unwrap().as_bytes(), key.as_bytes()));
                                ratchet_count = offer.ratchet_count;
                            }
                            if let Some(e1e1) = e1e1.as_ref() {
                                key = Secret(hmac_sha512(e1e1.as_bytes(), key.as_bytes()));
                            }

                            if !hmac_sha384_2(
                                kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_HMAC).first_n::<48>(),
                                pseudoheader,
                                &original_ciphertext[HEADER_SIZE..aes_gcm_tag_end],
                            )
                            .eq(&incoming_packet[aes_gcm_tag_end..incoming_packet.len()])
                            {
                                return Err(Error::FailedAuthentication);
                            }

                            // Alice has now completed and validated the full hybrid exchange.

                            let counter = session.send_counter.next();
                            let key = SessionKey::new(key, Role::Alice, current_time, counter, ratchet_count + 1, e1e1.is_some());

                            let mut reply_buf = [0_u8; HEADER_SIZE + AES_GCM_TAG_SIZE];
                            create_packet_header(&mut reply_buf, HEADER_SIZE + AES_GCM_TAG_SIZE, mtu, PACKET_TYPE_NOP, bob_session_id.into(), counter)?;

                            let mut c = key.get_send_cipher(counter)?;
                            c.init(memory::as_byte_array::<Pseudoheader, 12>(&Pseudoheader::make(bob_session_id.into(), PACKET_TYPE_NOP, counter.to_u32())));
                            reply_buf[HEADER_SIZE..].copy_from_slice(&c.finish_encrypt());
                            key.return_send_cipher(c);

                            armor_header(&mut reply_buf, &session.header_check_cipher);
                            send(&mut reply_buf);

                            let mut state = RwLockUpgradableReadGuard::upgrade(state);
                            let _ = state.remote_session_id.replace(bob_session_id);
                            let _ = state.offer.take();
                            add_session_key(&mut state.keys, key);

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
    #[inline(always)]
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

/// Temporary object to construct a "pseudo-header" for AES-GCM nonce and HMAC calculation.
#[derive(Clone, Copy)]
#[repr(C, packed)]
struct Pseudoheader(u64, u32);

impl Pseudoheader {
    #[inline(always)]
    pub fn make(session_id: u64, packet_type: u8, counter: u32) -> Self {
        Pseudoheader((session_id | (packet_type as u64)).to_le(), counter.to_le())
    }
}

/// Ephemeral offer sent with KEY_OFFER and rememebered so state can be reconstructed on COUNTER_OFFER.
struct EphemeralOffer {
    id: [u8; 16],
    creation_time: i64,
    ratchet_count: u64,
    ratchet_key: Option<Secret<64>>,
    key: Secret<64>,
    alice_e0_keypair: P384KeyPair,
    alice_e1_keypair: Option<pqc_kyber::Keypair>,
}

fn create_initial_offer<SendFunction: FnMut(&mut [u8])>(
    send: &mut SendFunction,
    counter: CounterValue,
    alice_session_id: SessionId,
    bob_session_id: Option<SessionId>,
    alice_s_public: &[u8],
    alice_metadata: &[u8],
    bob_s_public_p384: &P384PublicKey,
    bob_s_public_hash: &[u8],
    ss: &Secret<48>,
    current_key: Option<&SessionKey>,
    header_check_cipher: &Aes,
    mtu: usize,
    current_time: i64,
) -> Result<Box<EphemeralOffer>, Error> {
    let alice_e0_keypair = P384KeyPair::generate();
    let e0s = alice_e0_keypair.agree(bob_s_public_p384);
    if e0s.is_none() {
        return Err(Error::InvalidPacket);
    }

    let alice_e1_keypair = if JEDI {
        Some(pqc_kyber::keypair(&mut random::SecureRandom::get()))
    } else {
        None
    };

    let (ratchet_key, ratchet_count) = if let Some(current_key) = current_key {
        (Some(current_key.ratchet_key.clone()), current_key.ratchet_count)
    } else {
        (None, 0)
    };

    let id: [u8; 16] = random::get_bytes_secure();

    const PACKET_BUF_SIZE: usize = MIN_MTU * KEY_EXCHANGE_MAX_FRAGMENTS;
    let mut packet_buf = [0_u8; PACKET_BUF_SIZE];
    let mut packet_len = {
        let mut p = &mut packet_buf[HEADER_SIZE..];

        p.write_all(&[SESSION_PROTOCOL_VERSION])?;
        p.write_all(alice_e0_keypair.public_key_bytes())?;

        p.write_all(&id)?;
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
        if let Some(ratchet_key) = ratchet_key.as_ref() {
            p.write_all(&[0x01])?;
            p.write_all(&SHA384::hash(ratchet_key.as_bytes())[..16])?;
        } else {
            p.write_all(&[0x00])?;
        }

        PACKET_BUF_SIZE - p.len()
    };

    let bob_session_id: u64 = bob_session_id.map_or(0_u64, |i| i.into());
    create_packet_header(&mut packet_buf, packet_len, mtu, PACKET_TYPE_KEY_OFFER, bob_session_id, counter)?;
    let pseudoheader = Pseudoheader::make(bob_session_id, PACKET_TYPE_KEY_OFFER, counter.to_u32());

    let key = Secret(hmac_sha512(&hmac_sha512(&INITIAL_KEY, alice_e0_keypair.public_key_bytes()), e0s.unwrap().as_bytes()));

    let gcm_tag = {
        let mut c = AesGcm::new(kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_AES_GCM_ALICE_TO_BOB).first_n::<32>(), true);
        c.init(memory::as_byte_array::<Pseudoheader, 12>(&pseudoheader));
        c.crypt_in_place(&mut packet_buf[(HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE)..packet_len]);
        c.finish_encrypt()
    };
    packet_buf[packet_len..(packet_len + AES_GCM_TAG_SIZE)].copy_from_slice(&gcm_tag);
    packet_len += AES_GCM_TAG_SIZE;

    let key = Secret(hmac_sha512(key.as_bytes(), ss.as_bytes()));

    let hmac = hmac_sha384_2(
        kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_HMAC).first_n::<48>(),
        memory::as_byte_array::<Pseudoheader, 12>(&pseudoheader),
        &packet_buf[HEADER_SIZE..packet_len],
    );
    packet_buf[packet_len..(packet_len + HMAC_SIZE)].copy_from_slice(&hmac);
    packet_len += HMAC_SIZE;

    let hmac = hmac_sha384_2(bob_s_public_hash, memory::as_byte_array::<Pseudoheader, 12>(&pseudoheader), &packet_buf[HEADER_SIZE..packet_len]);
    packet_buf[packet_len..(packet_len + HMAC_SIZE)].copy_from_slice(&hmac);
    packet_len += HMAC_SIZE;

    send_with_fragmentation(send, &mut packet_buf[..packet_len], mtu, header_check_cipher);

    Ok(Box::new(EphemeralOffer {
        id,
        creation_time: current_time,
        ratchet_count,
        ratchet_key,
        key,
        alice_e0_keypair,
        alice_e1_keypair,
    }))
}

#[inline(always)]
fn create_packet_header(header: &mut [u8], packet_len: usize, mtu: usize, packet_type: u8, recipient_session_id: u64, counter: CounterValue) -> Result<(), Error> {
    let fragment_count = ((packet_len as f32) / (mtu - HEADER_SIZE) as f32).ceil() as usize;

    debug_assert!(header.len() >= HEADER_SIZE);
    debug_assert!(mtu >= MIN_MTU);
    debug_assert!(packet_len >= MIN_PACKET_SIZE);
    debug_assert!(fragment_count <= MAX_FRAGMENTS);
    debug_assert!(fragment_count > 0);
    debug_assert!(packet_type <= 0x0f); // packet type is 4 bits
    debug_assert!(recipient_session_id <= 0xffffffffffff); // session ID is 48 bits

    if fragment_count <= MAX_FRAGMENTS {
        header[0..8].copy_from_slice(&(recipient_session_id | (packet_type as u64).wrapping_shl(48) | ((fragment_count - 1) as u64).wrapping_shl(52)).to_le_bytes());
        header[8..12].copy_from_slice(&counter.to_u32().to_le_bytes());
        header[12..16].fill(0);
        Ok(())
    } else {
        unlikely_branch();
        Err(Error::DataTooLarge)
    }
}

fn send_with_fragmentation<SendFunction: FnMut(&mut [u8])>(send: &mut SendFunction, packet: &mut [u8], mtu: usize, header_check_cipher: &Aes) {
    let packet_len = packet.len();
    let mut fragment_start = 0;
    let mut fragment_end = packet_len.min(mtu);
    let mut header: [u8; 16] = packet[..HEADER_SIZE].try_into().unwrap();
    loop {
        let fragment = &mut packet[fragment_start..fragment_end];
        armor_header(fragment, header_check_cipher);
        send(fragment);
        if fragment_end < packet_len {
            debug_assert!(header[7].wrapping_shr(2) < 63);
            header[7] += 0x04; // increment fragment number
            fragment_start = fragment_end - HEADER_SIZE;
            fragment_end = (fragment_start + mtu).min(packet_len);
            packet[fragment_start..(fragment_start + HEADER_SIZE)].copy_from_slice(&header);
        } else {
            debug_assert_eq!(fragment_end, packet_len);
            break;
        }
    }
}

/// Encrypt everything in header after session ID using AES-CTR and the second 16 bytes as a nonce.
/// The last four bytes of the header must be zero, so this also embeds a small header MAC.
#[inline(always)]
fn armor_header(packet: &mut [u8], header_check_cipher: &Aes) {
    debug_assert!(packet.len() >= MIN_PACKET_SIZE);
    let mut header_pad = 0u128.to_ne_bytes();
    header_check_cipher.encrypt_block(&packet[16..32], &mut header_pad);
    packet[SESSION_ID_SIZE..HEADER_SIZE].iter_mut().zip(header_pad.iter()).for_each(|(x, y)| *x ^= *y);
}

/// Dearmor the armored part of the header and return it if the 32-bit MAC matches.
fn dearmor_header(packet: &[u8], header_check_cipher: &Aes) -> Option<(u8, u8, u8, u32)> {
    debug_assert!(packet.len() >= MIN_PACKET_SIZE);
    let mut header_pad = 0u128.to_ne_bytes();
    header_check_cipher.encrypt_block(&packet[16..32], &mut header_pad);
    let header_pad = u128::from_ne_bytes(header_pad);

    #[cfg(target_endian = "little")]
    let (header_0_8, header_8_16) = {
        let header = memory::u128_from_ne_bytes(packet) ^ header_pad.wrapping_shl(48);
        (header as u64, header.wrapping_shr(64) as u64)
    };
    #[cfg(target_endian = "big")]
    let (header_0_8, header_8_16) = {
        let header = memory::u128_from_ne_bytes(packet) ^ header_pad.wrapping_shr(48);
        ((header.wrapping_shr(64) as u64).swap_bytes(), (header as u64).swap_bytes())
    };

    if header_8_16.wrapping_shr(32) == 0 {
        let packet_type = (header_0_8.wrapping_shr(48) as u8) & 15;
        let fragment_count = ((header_0_8.wrapping_shr(52) as u8) & 63).wrapping_add(1);
        let fragment_no = (header_0_8.wrapping_shr(58) as u8) & 63;
        Some((packet_type, fragment_count, fragment_no, header_8_16 as u32))
    } else {
        None
    }
}

fn add_session_key(keys: &mut LinkedList<SessionKey>, key: SessionKey) {
    // Sanity check to make sure duplicates can't get in here. Should be impossible.
    for k in keys.iter() {
        if k.receive_key.eq(&key.receive_key) {
            return;
        }
    }

    debug_assert!(KEY_HISTORY_SIZE_MAX >= 2);
    while keys.len() >= KEY_HISTORY_SIZE_MAX {
        let current = keys.pop_front().unwrap();
        let _ = keys.pop_front();
        keys.push_front(current);
    }
    keys.push_back(key);
}

fn parse_key_offer_after_header(incoming_packet: &[u8], packet_type: u8) -> Result<([u8; 16], SessionId, &[u8], &[u8], &[u8], Option<[u8; 16]>), Error> {
    let mut p = &incoming_packet[..];
    let mut offer_id = [0_u8; 16];
    p.read_exact(&mut offer_id)?;
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
    if p.is_empty() {
        return Err(Error::InvalidPacket);
    }
    let alice_ratchet_key_id = if p[0] == 0x01 {
        if p.len() < 16 {
            return Err(Error::InvalidPacket);
        }
        Some(p[1..17].try_into().unwrap())
    } else {
        None
    };

    Ok((offer_id, alice_session_id, alice_s_public, alice_metadata, alice_e1_public, alice_ratchet_key_id))
}

enum Role {
    Alice,
    Bob,
}

/// Specialized class for the careful management of key lifetimes.
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

#[allow(unused)]
struct SessionKey {
    establish_time: i64,
    lifetime: KeyLifetime,
    ratchet_key: Secret<64>,
    receive_key: Secret<32>,
    send_key: Secret<32>,
    receive_cipher_pool: Mutex<Vec<Box<AesGcm>>>,
    send_cipher_pool: Mutex<Vec<Box<AesGcm>>>,
    role: Role,
    ratchet_count: u64,
    jedi: bool, // true if kyber was enabled on both sides
}

impl SessionKey {
    /// Create a new symmetric shared session key and set its key expiration times, etc.
    fn new(key: Secret<64>, role: Role, current_time: i64, current_counter: CounterValue, ratchet_count: u64, jedi: bool) -> Self {
        let a2b: Secret<32> = kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_AES_GCM_ALICE_TO_BOB).first_n_clone();
        let b2a: Secret<32> = kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_AES_GCM_BOB_TO_ALICE).first_n_clone();
        let (receive_key, send_key) = match role {
            Role::Alice => (b2a, a2b),
            Role::Bob => (a2b, b2a),
        };
        Self {
            establish_time: current_time,
            lifetime: KeyLifetime::new(current_counter, current_time),
            ratchet_key: kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_RATCHETING),
            receive_key,
            send_key,
            receive_cipher_pool: Mutex::new(Vec::with_capacity(2)),
            send_cipher_pool: Mutex::new(Vec::with_capacity(2)),
            role,
            ratchet_count,
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

/// Shortcut to HMAC data split into two slices.
fn hmac_sha384_2(key: &[u8], a: &[u8], b: &[u8]) -> [u8; 48] {
    let mut hmac = HMACSHA384::new(key);
    hmac.update(a);
    hmac.update(b);
    hmac.finish()
}

/// HMAC-SHA512 key derivation function modeled on: https://csrc.nist.gov/publications/detail/sp/800-108/final (page 12)
fn kbkdf512(key: &[u8], label: u8) -> Secret<64> {
    Secret(hmac_sha512(key, &[0, 0, 0, 0, b'Z', b'T', label, 0, 0, 0, 0, 0x02, 0x00]))
}

#[cfg(test)]
mod tests {
    use parking_lot::Mutex;
    use std::collections::LinkedList;
    use std::sync::Arc;

    #[allow(unused_imports)]
    use super::*;

    struct TestHost {
        local_s: P384KeyPair,
        local_s_hash: [u8; 48],
        psk: Secret<64>,
        session: Mutex<Option<Arc<Session<Box<TestHost>>>>>,
        session_id_counter: Mutex<u64>,
        pub queue: Mutex<LinkedList<Vec<u8>>>,
        pub this_name: &'static str,
        pub other_name: &'static str,
    }

    impl TestHost {
        fn new(psk: Secret<64>, this_name: &'static str, other_name: &'static str) -> Self {
            let local_s = P384KeyPair::generate();
            let local_s_hash = SHA384::hash(local_s.public_key_bytes());
            Self {
                local_s,
                local_s_hash,
                psk,
                session: Mutex::new(None),
                session_id_counter: Mutex::new(random::next_u64_secure().wrapping_shr(16) | 1),
                queue: Mutex::new(LinkedList::new()),
                this_name,
                other_name,
            }
        }
    }

    impl Host for Box<TestHost> {
        type AssociatedObject = u32;
        type SessionRef = Arc<Session<Box<TestHost>>>;
        type IncomingPacketBuffer = Vec<u8>;

        fn get_local_s_public(&self) -> &[u8] {
            self.local_s.public_key_bytes()
        }

        fn get_local_s_public_hash(&self) -> &[u8; 48] {
            &self.local_s_hash
        }

        fn get_local_s_keypair_p384(&self) -> &P384KeyPair {
            &self.local_s
        }

        fn extract_p384_static(static_public: &[u8]) -> Option<P384PublicKey> {
            P384PublicKey::from_bytes(static_public)
        }

        fn session_lookup(&self, local_session_id: SessionId) -> Option<Self::SessionRef> {
            self.session.lock().as_ref().and_then(|s| {
                if s.id == local_session_id {
                    Some(s.clone())
                } else {
                    None
                }
            })
        }

        fn accept_new_session(&self, _: &[u8], _: &[u8]) -> Option<(SessionId, Secret<64>, Self::AssociatedObject)> {
            loop {
                let mut new_id = self.session_id_counter.lock();
                *new_id += 1;
                return Some((SessionId::new_from_u64(*new_id).unwrap(), self.psk.clone(), 0));
            }
        }
    }

    #[allow(unused_variables)]
    #[test]
    fn establish_session() {
        let mut data_buf = [0_u8; (1280 - 32) * MAX_FRAGMENTS];
        let mut mtu_buffer = [0_u8; 1280];
        let mut psk: Secret<64> = Secret::default();
        random::fill_bytes_secure(&mut psk.0);

        let alice_host = Box::new(TestHost::new(psk.clone(), "alice", "bob"));
        let bob_host = Box::new(TestHost::new(psk.clone(), "bob", "alice"));
        let alice_rc: Box<ReceiveContext<Box<TestHost>>> = Box::new(ReceiveContext::new(&alice_host));
        let bob_rc: Box<ReceiveContext<Box<TestHost>>> = Box::new(ReceiveContext::new(&bob_host));

        //println!("zssp: size of session (bytes): {}", std::mem::size_of::<Session<Box<TestHost>>>());

        let _ = alice_host.session.lock().insert(Arc::new(
            Session::new(
                &alice_host,
                |data| bob_host.queue.lock().push_front(data.to_vec()),
                SessionId::new_from_u64(random::xorshift64_random().wrapping_shr(16)).unwrap(),
                bob_host.local_s.public_key_bytes(),
                &[],
                &psk,
                1,
                mtu_buffer.len(),
                1,
            )
            .unwrap(),
        ));

        let mut ts = 0;
        for _ in 0..3 {
            for host in [&alice_host, &bob_host] {
                let send_to_other = |data: &mut [u8]| {
                    if std::ptr::eq(host, &alice_host) {
                        bob_host.queue.lock().push_front(data.to_vec());
                    } else {
                        alice_host.queue.lock().push_front(data.to_vec());
                    }
                };

                let rc = if std::ptr::eq(host, &alice_host) {
                    &alice_rc
                } else {
                    &bob_rc
                };

                loop {
                    if let Some(qi) = host.queue.lock().pop_back() {
                        let qi_len = qi.len();
                        ts += 1;
                        let r = rc.receive(host, send_to_other, &mut data_buf, qi, mtu_buffer.len(), ts);
                        if r.is_ok() {
                            let r = r.unwrap();
                            match r {
                                ReceiveResult::Ok => {
                                    //println!("zssp: {} => {} ({}): Ok", host.other_name, host.this_name, qi_len);
                                }
                                ReceiveResult::OkData(data) => {
                                    //println!("zssp: {} => {} ({}): OkData length=={}", host.other_name, host.this_name, qi_len, data.len());
                                    assert!(!data.iter().any(|x| *x != 0x12));
                                }
                                ReceiveResult::OkNewSession(new_session) => {
                                    println!("zssp: {} => {} ({}): OkNewSession ({})", host.other_name, host.this_name, qi_len, u64::from(new_session.id));
                                    let mut hs = host.session.lock();
                                    assert!(hs.is_none());
                                    let _ = hs.insert(Arc::new(new_session));
                                }
                                ReceiveResult::Ignored => {
                                    println!("zssp: {} => {} ({}): Ignored", host.other_name, host.this_name, qi_len);
                                }
                            }
                        } else {
                            println!("zssp: {} => {} ({}): error: {}", host.other_name, host.this_name, qi_len, r.err().unwrap().to_string());
                            panic!();
                        }
                    } else {
                        break;
                    }
                }

                data_buf.fill(0x12);
                if let Some(session) = host.session.lock().as_ref().cloned() {
                    if session.established() {
                        for _ in 0..16 {
                            assert!(session
                                .send(send_to_other, &mut mtu_buffer, &data_buf[..((random::xorshift64_random() as usize) % data_buf.len())])
                                .is_ok());
                        }
                    }
                }
            }
        }
    }
}
