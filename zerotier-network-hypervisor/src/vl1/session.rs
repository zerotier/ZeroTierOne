// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::fmt::{Debug, Display};
use std::num::NonZeroU64;
use std::sync::Arc;

use parking_lot::{RwLock, RwLockUpgradableReadGuard};

use zerotier_core_crypto::aes::*;
use zerotier_core_crypto::hash::*;
use zerotier_core_crypto::kbkdf::*;
use zerotier_core_crypto::p384::*;
use zerotier_core_crypto::pqc_kyber;
use zerotier_core_crypto::random;
use zerotier_core_crypto::secret::Secret;
use zerotier_core_crypto::x25519::*;

use crate::util::buffer::Buffer;
use crate::util::marshalable::Marshalable;
use crate::util::pool::*;
use crate::vl1::identity::Identity;

pub const SESSION_SETUP_PACKET_SIZE_MAX: usize = crate::vl1::protocol::UDP_DEFAULT_MTU;
pub const AUTH_DATA_SIZE_MAX: usize = 1024;

pub const KEX_NISTP384: u8 = 0x01;
pub const KEX_KYBER768: u8 = 0x02;
pub const KEX_C25519: u8 = 0x04;

pub const REKEY_AFTER_PACKET_COUNT: u32 = 1073741824;
pub const REKEY_AFTER_TIME_MS: i64 = 3600000; // one hour

const HEADER_SIZE: usize = 10;

const TYPE_MASK: u8 = 0x03;

const TYPE_DATA: u8 = 0;
const TYPE_INIT: u8 = 1;
const TYPE_REKEY: u8 = 2;
const TYPE_ACK: u8 = 3;

const EPHEMERAL_TYPE_EOF: u8 = 0;
const EPHEMERAL_TYPE_X25519: u8 = 1;
const EPHEMERAL_TYPE_NISTP384: u8 = 2;
const EPHEMERAL_TYPE_KYBER_PUBLIC: u8 = 3;
const EPHEMERAL_TYPE_KYBER_CIPHERTEXT: u8 = 4;

const KBKDF_KEY_USAGE_INIT_PAYLOAD_ENCRYPT: u8 = b'i';
const KBKDF_KEY_USAGE_INIT_PAYLOAD_HMAC: u8 = b'h';

const KEY_HISTORY_SIZE: usize = 4;

#[derive(Debug)]
enum Error {
    IoError(std::io::Error),
    Invalid,
}

impl From<std::io::Error> for Error {
    #[inline(always)]
    fn from(e: std::io::Error) -> Self {
        Self::IoError(e)
    }
}

impl Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::IoError(e) => Display::fmt(e, f),
            Self::Invalid => f.write_str("Invalid"),
        }
    }
}

impl std::error::Error for Error {}

pub struct Session<O> {
    pub local_session_id: NonZeroU64,
    m: RwLock<SessionMut>,
    pub associated_object: O,
    pub enabled_kex_algorithms: u8,
}

struct Key {
    secret: Secret<64>,
    gcm_pool: Pool<AesGcm, AesGcmPoolFactory>,
    ratchet_count: u64,
}

impl Key {
    fn new(secret: Secret<64>, ratchet_count: u64) -> Self {
        Self {
            secret,
            gcm_pool: Pool::new(2, AesGcmPoolFactory(secret.first_n(), false)),
            ratchet_count,
        }
    }
}

struct EphemeralOffer {
    p384: Option<P384PublicKey>,
    x25519: Option<[u8; C25519_PUBLIC_KEY_SIZE]>,
    kyber_public: Option<[u8; pqc_kyber::KYBER_PUBLICKEYBYTES]>,
    kyber_ciphertext: Option<[u8; pqc_kyber::KYBER_CIPHERTEXTBYTES]>,
    expected_secret_hash: [u8; 48],
}

impl EphemeralOffer {
    fn read<const L: usize>(b: &Buffer<L>, cursor: &mut usize) -> Result<Self, Error> {
        let mut offer = Self {
            p384: None,
            x25519: None,
            kyber_public: None,
            kyber_ciphertext: None,
            expected_secret_hash: [0_u8; 48],
        };
        loop {
            match b.read_u8(cursor)? {
                EPHEMERAL_TYPE_EOF => break,
                EPHEMERAL_TYPE_NISTP384 => {
                    if offer.p384.is_some() {
                        return Err(Error::Invalid);
                    } else if let Some(p384) = P384PublicKey::from_bytes(b.read_bytes_fixed::<P384_PUBLIC_KEY_SIZE>(cursor)?) {
                        offer.p384.replace(p384);
                    } else {
                        return Err(Error::Invalid);
                    }
                }
                EPHEMERAL_TYPE_X25519 => {
                    if offer.x25519.is_some() {
                        return Err(Error::Invalid);
                    } else {
                        offer.x25519.replace(b.read_bytes_fixed(cursor)?.clone());
                    }
                }
                EPHEMERAL_TYPE_KYBER_PUBLIC => {
                    if offer.kyber_public.is_some() {
                        return Err(Error::Invalid);
                    } else {
                        offer.kyber_public.replace(b.read_bytes_fixed(cursor)?.clone());
                    }
                }
                EPHEMERAL_TYPE_KYBER_CIPHERTEXT => {
                    if offer.kyber_ciphertext.is_some() {
                        return Err(Error::Invalid);
                    } else {
                        offer.kyber_ciphertext.replace(b.read_bytes_fixed(cursor)?.clone());
                    }
                }
                _ => {
                    let key_size = b.read_varint(cursor)?;
                    *cursor += key_size as usize;
                }
            }
        }
        Ok(offer)
    }
}

struct SessionMut {
    receive_key: Key,
    remote_kyber_public: Option<pqc_kyber::PublicKey>,
    remote_session_id: Option<NonZeroU64>,
}

pub fn initiate<O>(
    local_identity: &Identity,
    remote_identity: &Identity,
    static_key: &Secret<64>,
    local_session_id: u64,
    current_time: i64,
    enable_kex_algorithms: u8,
    associated_object: O,
) -> Session<O> {
    todo!()
}

pub fn receive<
    GetSessionByLocalId: FnOnce(NonZeroU64) -> Option<Arc<Session<O>>>,
    OnData: FnOnce(&Arc<Session<O>>, u32, &mut Buffer<L>),
    OnDatagram: FnOnce(Identity, &mut Buffer<L>),
    SendReply: FnOnce(&Arc<Session<O>>, &mut Buffer<L>),
    AllowNewSession: FnOnce(&Identity, Option<&[u8]>) -> Option<(NonZeroU64, O)>,
    OnNewSession: FnOnce(Identity, Arc<Session<O>>),
    O,
    const L: usize,
>(
    local_identity: &Identity,
    local_obfuscation_cipher: &Aes,
    current_time: i64,
    enable_kex_algorithms: u8,
    incoming_packet: &Buffer<L>,
    get_session_by_local_id: GetSessionByLocalId,
    on_data: OnData,
    on_datagram: OnDatagram,
    send_reply: SendReply,
    allow_new_session: AllowNewSession,
    on_new_session: OnNewSession,
) -> std::io::Result<bool> {
    let len_without_gcm_tag = incoming_packet.len().wrapping_sub(16);
    let gcm_tag = incoming_packet.bytes_fixed_at::<16>(len_without_gcm_tag)?;

    let (encryption_nonce, counter, mut local_session_id) = incoming_packet.bytes_fixed_at::<HEADER_SIZE>(0).map(|h| {
        let mut deobfuscated_header = unsafe { std::mem::MaybeUninit::<[u8; 16]>::uninit().assume_init() };
        local_obfuscation_cipher.encrypt_block(gcm_tag, &mut deobfuscated_header);
        for i in 0..HEADER_SIZE {
            deobfuscated_header[i] ^= h[i];
        }
        deobfuscated_header[HEADER_SIZE..].fill(0);
        (deobfuscated_header, u32::from_le_bytes(deobfuscated_header[0..4].try_into().unwrap()), NonZeroU64::new(u64::from_le_bytes(deobfuscated_header[4..12].try_into().unwrap())))
    })?;

    let (mut session, session_m) = if let Some(lsid) = local_session_id {
        if let Some(s) = get_session_by_local_id(lsid) {
            let m = s.m.upgradable_read();
            (Some(s), Some(m))
        } else {
            return Ok(false);
        }
    } else if counter == 0 {
        // This can only be an INIT packet, and counter must also be zero.
        (None, None)
    } else {
        return Ok(false);
    };

    let mut init_cipher = None;
    let mut pooled_cipher = None;
    let aes_gcm: &mut AesGcm = if let Some(m) = session_m.as_ref() {
        pooled_cipher = Some(m.receive_key.gcm_pool.get());
        pooled_cipher.as_mut().unwrap()
    } else {
        init_cipher = Some(AesGcm::new(&local_identity.fingerprint[32..], false));
        init_cipher.as_mut().unwrap()
    };

    let mut packet = unsafe { Buffer::<L>::new_without_memzero() };
    aes_gcm.init(&encryption_nonce);
    aes_gcm.crypt(incoming_packet.as_byte_range(HEADER_SIZE, len_without_gcm_tag)?, packet.append_bytes_get_mut(len_without_gcm_tag - HEADER_SIZE)?);
    if !aes_gcm.finish().eq(gcm_tag) {
        return Ok(false);
    }

    let mut cursor = 0;
    let packet_info = packet.read_u8(&mut cursor)?;
    let packet_type = packet_info & TYPE_MASK;
    match packet_type {
        TYPE_DATA => {
            if let Some(session) = session {
            } else {
                return Ok(false);
            }
        }

        TYPE_INIT => {
            if session.is_none() {
                let local_identity_secrets = local_identity.secret.as_ref().unwrap();
                let init_data_start = cursor;

                // Bootstrap secret is agree(ephemeral key, static identity key)
                let mut bootstrap_secret = Secret(SHA512::hash(&local_identity.fingerprint));
                let mut remote_kyber_public = None;
                loop {
                    match packet.read_u8(&mut cursor)? {
                        EPHEMERAL_TYPE_EOF => break,
                        EPHEMERAL_TYPE_NISTP384 => {
                            if let Some(local_p384_secret) = local_identity_secrets.p384.as_ref().map(|p384| &p384.ecdh) {
                                if let Some(s) = P384PublicKey::from_bytes(packet.read_bytes_fixed::<P384_PUBLIC_KEY_SIZE>(&mut cursor)?).and_then(|p| local_p384_secret.agree(&p)) {
                                    bootstrap_secret.0.copy_from_slice(&hmac_sha512(bootstrap_secret.as_bytes(), s.as_bytes()));
                                } else {
                                    return Ok(false);
                                }
                            } else {
                                return Ok(false);
                            }
                        }
                        EPHEMERAL_TYPE_C25519 => {
                            bootstrap_secret.0.copy_from_slice(&hmac_sha512(
                                bootstrap_secret.as_bytes(),
                                local_identity_secrets.c25519.agree(packet.read_bytes_fixed::<C25519_PUBLIC_KEY_SIZE>(&mut cursor)?).as_bytes(),
                            ));
                        }
                        EPHEMERAL_TYPE_KYBER_PUBLIC => {
                            remote_kyber_public = Some(packet.read_bytes_fixed::<{ pqc_kyber::KYBER_PUBLICKEYBYTES }>(&mut cursor)?.clone());
                        }
                        _ => return Ok(false),
                    }
                }

                // Decrypt and authenticate init payload using bootstrap secret.
                let mut init_payload_bytes: Buffer<L> = Buffer::new();
                let init_payload_len = (packet.read_varint(&mut cursor)? as usize).wrapping_sub(16);
                let mut init_payload_crypt = AesGcm::new(&zt_kbkdf_hmac_sha384(&bootstrap_secret.as_bytes()[..48], KBKDF_KEY_USAGE_INIT_PAYLOAD_ENCRYPT).as_bytes()[..32], false);
                init_payload_crypt.init(&encryption_nonce);
                init_payload_crypt.crypt(packet.read_bytes(init_payload_len, &mut cursor)?, init_payload_bytes.append_bytes_get_mut(init_payload_len).unwrap());
                if !init_payload_crypt.finish().eq(packet.read_bytes_fixed::<16>(&mut cursor)?) {
                    return Ok(false);
                }

                // Decode rest of init payload, compute first real key incorporating the static secret
                // to authenticate both sides, then authenticate the INIT payload again with this key.
                let mut init_payload_cursor = 0;
                let remote_session_id = init_payload_bytes.read_bytes_fixed::<6>(&mut init_payload_cursor)?;
                let remote_session_id = (remote_session_id[0] as u64)
                    | (remote_session_id[1] as u64).wrapping_shl(8)
                    | (remote_session_id[2] as u64).wrapping_shl(16)
                    | (remote_session_id[3] as u64).wrapping_shl(24)
                    | (remote_session_id[4] as u64).wrapping_shl(32)
                    | (remote_session_id[5] as u64).wrapping_shl(40);
                if remote_session_id != 0 {
                    let remote_identity = Identity::unmarshal(&init_payload_bytes, &mut init_payload_cursor)?;
                    let auth_data_len = init_payload_bytes.read_varint(&mut init_payload_cursor).unwrap_or(0) as usize;
                    if let Some(static_secret) = local_identity.agree(&remote_identity) {
                        let new_session_key = Secret(hmac_sha512(static_secret.as_bytes(), bootstrap_secret.as_bytes()));

                        if !hmac_sha384(zt_kbkdf_hmac_sha384(&new_session_key.as_bytes()[..48], KBKDF_KEY_USAGE_INIT_PAYLOAD_HMAC).as_bytes(), packet.as_byte_range(init_data_start, cursor)?)
                            .eq(packet.read_bytes_fixed::<48>(&mut cursor)?)
                        {
                            return Ok(false);
                        }

                        if let Some((id, obj)) =
                            allow_new_session(&remote_identity, if auth_data_len > 0 { Some(init_payload_bytes.read_bytes(auth_data_len, &mut init_payload_cursor)?) } else { None })
                        {
                            session = Some(Arc::new(Session {
                                local_session_id: id,
                                m: RwLock::new(SessionMut {
                                    receive_key: Key::new(new_session_key, 0),
                                    remote_kyber_public,
                                    remote_session_id: NonZeroU64::new(remote_session_id),
                                }),
                                associated_object: obj,
                                enabled_kex_algorithms: enable_kex_algorithms,
                            }));
                            on_new_session(remote_identity, session.as_ref().unwrap().clone());
                            // TODO: send REKEY
                            return Ok(true);
                        } else {
                            return Ok(false);
                        }
                    } else {
                        return Ok(false);
                    }
                } else {
                    return Ok(false);
                }
            } else {
                return Ok(false);
            }
        }

        TYPE_REKEY => {
            if let Some(session) = session {
                loop {
                    match packet.read_u8(&mut cursor)? {
                        EPHEMERAL_TYPE_EOF => break,
                        EPHEMERAL_TYPE_NISTP384 => {}
                        EPHEMERAL_TYPE_C25519 => {}
                        EPHEMERAL_TYPE_KYBER_PUBLIC => {}
                        EPHEMERAL_TYPE_KYBER_CIPHERTEXT => {}
                        _ => {}
                    }
                }
            } else {
                return Ok(false);
            }
        }

        TYPE_ACK => {
            if let Some(session) = session {
            } else {
                return Ok(false);
            }
        }
    }

    /*
    if let Some(mut next_ratchet_key) = next_ratchet_key {
        let next_ratchet_counter = if let Some(ck) = current_key.and_then(|cc| cc.as_ref()) {
            next_ratchet_key = Secret(hmac_sha512(ck.1.as_bytes(), next_ratchet_key.as_bytes()));
            ck.2 + 1
        } else {
            0
        };
        let mut session_m = RwLockUpgradableReadGuard::upgrade(session_m);
        let mut old_receive_key = session_m.receive_key_history[0].replace((Pool::new(2, AesGcmPoolFactory(next_ratchet_key.first_n(), false)), next_ratchet_key, next_ratchet_counter));
        for i in 1..KEY_HISTORY_SIZE {
            std::mem::swap(&mut session_m.receive_key_history[i], &mut old_receive_key);
        }
        session_m.last_successful_receive_key = 0;
    } else if session_m.last_successful_receive_key != trying_cipher {
        RwLockUpgradableReadGuard::upgrade(session_m).last_successful_receive_key = trying_cipher;
    }
    */

    return Ok(true);
}

pub(crate) struct AesGcmPoolFactory(Secret<32>, bool);

impl PoolFactory<AesGcm> for AesGcmPoolFactory {
    #[inline(always)]
    fn create(&self) -> AesGcm {
        AesGcm::new(&self.0 .0, self.1)
    }

    #[inline(always)]
    fn reset(&self, _: &mut AesGcm) {}
}

/*

/// Internal method to read and extract data from a series of field ID, value, ...
/// Returns false if a parse or other error occurs.
fn read_fields<const L: usize>(
    cursor: &mut usize,
    packet: &Buffer<L>,
    data: &mut Option<(usize, usize)>,
    auth_data: &mut Option<(usize, usize)>,
    identity: &mut Option<Identity>,
    session_id: &mut Option<u64>,
    ratchet_counter: &mut Option<u64>,
    ephemeral_c25519: &mut Option<[u8; C25519_PUBLIC_KEY_SIZE]>,
    ephemeral_p384: &mut Option<P384PublicKey>,
    ephemeral_kyber_public: &mut Option<[u8; pqc_kyber::KYBER_PUBLICKEYBYTES]>,
    ephemeral_kyber_encapsulated_secret: &mut Option<[u8; pqc_kyber::KYBER_CIPHERTEXTBYTES]>,
) -> bool {
    while *cursor < packet.len() {
        let field_type = packet.read_u8(cursor).unwrap(); // bounds already checked by while condition
        match field_type {
            FIELD_DATA => {
                if data.is_some() {
                    return false;
                }
                if let Ok(dlen) = packet.read_varint(cursor) {
                    let start = *cursor;
                    let end = *cursor + (dlen as usize);
                    if end > packet.len() {
                        return false;
                    }
                    data.insert((start, end));
                } else {
                    return false;
                }
            }
            FIELD_AUTH_DATA => {
                if auth_data.is_some() {
                    return false;
                }
                if let Ok(dlen) = packet.read_varint(cursor) {
                    let start = *cursor;
                    let end = *cursor + (dlen as usize);
                    if end > packet.len() {
                        return false;
                    }
                    auth_data.insert((start, end));
                } else {
                    return false;
                }
            }
            FIELD_STATIC_IDENTITY => {
                if identity.is_some() {
                    return false;
                }
                if let Ok(id) = Identity::unmarshal(packet, cursor) {
                    identity.insert(id);
                } else {
                    return false;
                }
            }
            FIELD_SESSION_ID => {
                if session_id.is_some() {
                    return false;
                }
                if let Ok(sid) = packet.read_bytes_fixed::<6>(cursor) {
                    session_id.insert(
                        (sid[0] as u64)
                            | (sid[1] as u64).wrapping_shl(8)
                            | (sid[2] as u64).wrapping_shl(16)
                            | (sid[3] as u64).wrapping_shl(24)
                            | (sid[4] as u64).wrapping_shl(32)
                            | (sid[5] as u64).wrapping_shl(40),
                    );
                } else {
                    return false;
                }
            }
            FIELD_EPHEMERAL_C25519 => {
                if ephemeral_c25519.is_some() {
                    return false;
                }
                if let Ok(c25519) = packet.read_bytes_fixed::<C25519_PUBLIC_KEY_SIZE>(cursor) {
                    ephemeral_c25519.insert(c25519.clone());
                } else {
                    return false;
                }
            }
            FIELD_EPHEMERAL_NISTP384 => {
                if ephemeral_p384.is_some() {
                    return false;
                }
                if let Ok(remote_p384) = packet.read_bytes_fixed::<P384_PUBLIC_KEY_SIZE>(cursor) {
                    if let Some(remote_p384) = P384PublicKey::from_bytes(remote_p384) {
                        ephemeral_p384.insert(remote_p384);
                    } else {
                        return false;
                    }
                } else {
                    return false;
                }
            }
            FIELD_EPHEMERAL_KYBER_PUBLIC => {
                if ephemeral_kyber_public.is_some() {
                    return false;
                }
                if let Ok(remote_kyber) = packet.read_bytes_fixed::<{ pqc_kyber::KYBER_PUBLICKEYBYTES }>(cursor) {
                    ephemeral_kyber_public.insert(remote_kyber.clone());
                } else {
                    return false;
                }
            }
            FIELD_EPHEMERAL_KYBER_ENCAPSULATED_SECRET => {
                if ephemeral_kyber_encapsulated_secret.is_some() {
                    return false;
                }
                if let Ok(remote_kyber_secret) = packet.read_bytes_fixed::<{ pqc_kyber::KYBER_CIPHERTEXTBYTES }>(cursor) {
                    ephemeral_kyber_encapsulated_secret.insert(remote_kyber_secret.clone());
                } else {
                    return false;
                }
            }
            FIELD_TIMESTAMP => {
                if timestamp.is_some() {
                    return false;
                }
                if let Ok(ts) = packet.read_u64_le(cursor) {
                    timestamp.insert(ts);
                } else {
                    return false;
                }
            }
            FIELD_TIMESTAMP_ECHO => {
                if timestamp_echo.is_some() {
                    return false;
                }
                if let Ok(ts) = packet.read_u64_le(cursor) {
                    timestamp_echo.insert(ts);
                } else {
                    return false;
                }
            }
            FIELD_RATCHET_COUNTER => {
                if ratchet_counter.is_some() {
                    return false;
                }
                if let Ok(c) = packet.read_varint(cursor) {
                    ratchet_counter.insert(c);
                } else {
                    return false;
                }
            }
            _ => {
                // Any fields we add in the future will need to be prefixed by their size.
                if let Ok(additional_field_len) = packet.read_varint(cursor) {
                    *cursor += additional_field_len as usize;
                } else {
                    return false;
                }
            }
        }
    }
    return true;
}

/// Trait to be implemented by whatever "owns" sessions.
pub(crate) trait SessionContext<O> {
    /// Get a session with a given local session ID
    fn lookup_session(&self, local_session_id: u64) -> Option<Arc<Session<O>>>;

    /// Check if a new session is allowed, and if so return local session ID and value for 'obj' field.
    fn authorize_new_session(&self, remote_identity: &Identity, auth_data: Option<Buffer<AUTH_DATA_SIZE_MAX>>) -> Option<(u64, O)>;

    /// Locally validate identity
    ///
    /// This is provided as an external function to be called because local validation in
    /// identities is slightly CPU-intensive. This information might therefore be cached.
    /// This also allows any other checks to be performed
    fn validate_identity(&self, id: &Identity) -> bool;
}

/// Role: Alice sends INIT and Bob responds with INIT_ACK, otherwise basically identical.
#[repr(u8)]
pub enum Role {
    Alice = 0_u8,
    Bob = 1_u8,
}

/// ZeroTier Secure Session Protocol (ZSSP) session
///
/// The O template argument is used to specify a type to be attached to the session such
/// as a ZeroTier peer.
#[allow(unused)]
pub(crate) struct Session<O> {
    /// Arbitrary object that may be attached by external code to this session.
    pub obj: O,

    /// SHA512(agree(unencrypted ephemeral sent with INIT, matching key in recipient identity))
    setup_key: Secret<64>,

    /// Static key resulting from sender and recipient identity key agreement
    static_key: Secret<64>,

    /// Key for the HMAC-SHA384 part of control message auth
    hmac_key: Secret<48>,

    /// AES-128 initialized with the first 16 bytes of the recipients' identity fingerprint.
    outgoing_obfuscation_cipher: Aes,

    /// Mutable state
    state: RwLock<MutState>,

    /// Time session was created (not most recent re-key)
    pub creation_time: i64,

    /// Local session ID
    local_session_id: u64,

    /// Outgoing packet counter, incremented for each send of any kind.
    outbound_counter: AtomicU32,

    /// Most recently witnessed inbound counter value.
    last_inbound_counter: AtomicU32,

    /// Deduplication hash table for incoming packet IDs (each ID mod 256).
    inbound_counter_dedup: [AtomicU32; 256],

    /// Role: Alice (0) or Bob (1)
    role: Role,

    /// Bit mask of enabled asymmetric key exchange algorithms
    enabled_kex_algorithms: u8,

    /// Which of the KEX algorithms was offered for setup?
    setup_kex_algorithm: u8,
}

struct MutState {
    keys: [Option<Box<SessionKey>>; SESSION_KEY_INDEX_COUNT],
    current_key: usize,
    offer: Option<Box<EphemeralOffer>>,
    remote_session_id: u64,
}

pub(crate) struct AesGcmPoolFactory(Secret<32>, bool);

impl PoolFactory<AesGcm> for AesGcmPoolFactory {
    #[inline(always)]
    fn create(&self) -> AesGcm {
        AesGcm::new(&self.0 .0, self.1)
    }

    #[inline(always)]
    fn reset(&self, _: &mut AesGcm) {}
}

struct EphemeralOffer {
    ratchet_counter: u64,
    timestamp: i64,
    c25519: Option<C25519KeyPair>,
    kyber768_alice_keypair: Option<pqc_kyber::Keypair>,
    kyber768_bob_ciphertext: Option<[u8; pqc_kyber::KYBER_CIPHERTEXTBYTES]>,
    p384: Option<P384KeyPair>,
}

struct SessionKey {
    aes_ctr_key: Secret<32>,
    aes_gcm_encrypt_pool: Pool<AesGcm, AesGcmPoolFactory>,
    aes_gcm_decrypt_pool: Pool<AesGcm, AesGcmPoolFactory>,
    timestamp: i64,
    ratchet_counter: u64,
    initial_packet_counter: u32, // session is dead if counter hits this
    acknowledged: bool,
    session_kex_algorithms: u8,
}

impl SessionKey {
    fn new(key: Secret<64>, timestamp: i64, ratchet_counter: u64, initial_packet_counter: u32, session_kex_algorithms: u8) -> Self {
        let gcm_key = zt_kbkdf_hmac_sha384(&key.as_bytes()[..48], KBKDF_KEY_USAGE_LABEL_AES_GCM);
        Self {
            aes_ctr_key: zt_kbkdf_hmac_sha384(&key.as_bytes()[..48], KBKDF_KEY_USAGE_LABEL_AES_CTR).first_n(),
            aes_gcm_encrypt_pool: Pool::new(2, AesGcmPoolFactory(gcm_key.first_n(), true)),
            aes_gcm_decrypt_pool: Pool::new(2, AesGcmPoolFactory(gcm_key.first_n(), false)),
            timestamp,
            ratchet_counter,
            initial_packet_counter,
            acknowledged: false,
            session_kex_algorithms,
        }
    }
}

pub enum ReceiveError {
    Invalid,
    UnknownLocalSessionId,
    NoSessionKeyAtIndex,
    Duplicate,
    CounterTooOutOfSync,
    UnsupportedCipher,
    CipherNotAllowedInContext,
    MissingRequiredFields,
    FailedAuthentication,
    NewSessionRejected,
}

pub enum ReceiveResult<O> {
    /// Packet OK, but no additional action needs to be taken.
    Ok,

    /// Packet OK, 'output' contains a reply that should be sent back.
    OkReply,

    /// Packet OK, new session created via remote INIT, 'output' contains reply.
    ///
    /// The authorize_new_session() session context method will have been called during processing.
    OkNewSession(Identity, Arc<Session<O>>),

    /// Packet OK, data was received and is in 'output'.
    OkData(Arc<Session<O>>),

    /// Sessionless datagram received, data is in 'output' within the provided range (start, end).
    OkDatagram(Identity, (usize, usize)),

    /// An error occurred and the packet should be discarded.
    Error(ReceiveError),
}

pub fn initiate<O>(
    local_identity: &Identity,
    remote_identity: &Identity,
    static_key: &Secret<64>,
    local_session_id: u64,
    current_time: i64,
    enable_kex_algorithms: u8,
    obj: O,
) -> Option<(Arc<Session<O>>, Buffer<SESSION_SETUP_PACKET_SIZE_MAX>)> {
    let mut packet: Buffer<SESSION_SETUP_PACKET_SIZE_MAX> = Buffer::new();
    packet.append_bytes(&INIT_HEADER_AND_CTR_NONCE[..HEADER_SIZE]).expect(UNEXPECTED_OVERFLOW_PANIC_MSG);

    let (mut offer_c25519, mut offer_p384, mut offer_kyber, mut setup_kex_algorithm, mut setup_key, mut start_encrypted_payload_at) = (None, None, None, 0, None, 0);

    // FIPS note: for setup P-384 must be first if available and enabled so the setup key is compliant.
    if (enable_kex_algorithms & KEX_NISTP384) != 0 && remote_identity.p384.is_some() {
        if let Some(local_secret) = local_identity.secret.as_ref() {
            if let Some(local_p384_secret) = local_secret.p384.as_ref() {
                let kp = P384KeyPair::generate();
                setup_key = setup_key.or_else(|| {
                    kp.agree(&remote_identity.p384.as_ref().unwrap().ecdh).map(|sk| {
                        setup_kex_algorithm = KEX_NISTP384;
                        Secret(SHA512::hash(sk.as_bytes()))
                    })
                });
                packet.append_u8(FIELD_EPHEMERAL_NISTP384).expect(UNEXPECTED_OVERFLOW_PANIC_MSG);
                packet.append_bytes_fixed(kp.public_key_bytes()).expect(UNEXPECTED_OVERFLOW_PANIC_MSG);
                if start_encrypted_payload_at == 0 {
                    start_encrypted_payload_at = packet.len();
                }
                offer_p384 = Some(kp);
            }
        }
    }

    if (enable_kex_algorithms & KEX_C25519) != 0 {
        let kp = C25519KeyPair::generate();
        setup_key = setup_key.or_else(|| {
            setup_kex_algorithm = KEX_C25519;
            Some(Secret(SHA512::hash(kp.agree(&remote_identity.c25519).as_bytes())))
        });
        packet.append_u8(FIELD_EPHEMERAL_C25519).expect(UNEXPECTED_OVERFLOW_PANIC_MSG);
        packet.append_bytes_fixed(&kp.public_bytes()).expect(UNEXPECTED_OVERFLOW_PANIC_MSG);
        if start_encrypted_payload_at == 0 {
            start_encrypted_payload_at = packet.len();
        }
        offer_c25519 = Some(kp);
    }

    if (enable_kex_algorithms & KEX_KYBER768) != 0 {
        let ephemeral_kyber = pqc_kyber::Keypair::generate(&mut random::SecureRandom::default());
        packet.append_u8(FIELD_EPHEMERAL_KYBER_PUBLIC).expect(UNEXPECTED_OVERFLOW_PANIC_MSG);
        packet.append_bytes_fixed(&ephemeral_kyber.public).expect(UNEXPECTED_OVERFLOW_PANIC_MSG);
        offer_kyber = Some(ephemeral_kyber);
    }

    let setup_key = setup_key?; // will return None if no mutually usable algorithms were enabled

    packet.append_u8(FIELD_STATIC_IDENTITY).expect(UNEXPECTED_OVERFLOW_PANIC_MSG);
    local_identity.marshal(&mut packet).expect(UNEXPECTED_OVERFLOW_PANIC_MSG);

    packet.append_u8(FIELD_SESSION_ID).expect(UNEXPECTED_OVERFLOW_PANIC_MSG);
    packet.append_bytes(&local_session_id.to_le_bytes()[..HEADER_SESSION_ID_LENGTH_BYTES]).expect(UNEXPECTED_OVERFLOW_PANIC_MSG);

    let outgoing_obfuscation_cipher = Aes::new(&remote_identity.fingerprint[..32]);
    let hmac_key: Secret<48> = zt_kbkdf_hmac_sha384(&static_key.as_bytes()[..48], KBKDF_KEY_USAGE_LABEL_HMAC).first_n();
    armor_control_packet(
        &mut packet,
        &INIT_HEADER_AND_CTR_NONCE,
        start_encrypted_payload_at,
        &outgoing_obfuscation_cipher,
        &zt_kbkdf_hmac_sha384(&setup_key.as_bytes()[..48], KBKDF_KEY_USAGE_LABEL_AES_CTR).first_n(),
        &hmac_key,
    );

    Some((
        Arc::new(Session::<O> {
            obj,
            setup_key,
            static_key: static_key.clone(),
            hmac_key,
            outgoing_obfuscation_cipher,
            state: RwLock::new(MutState {
                keys: [None, None, None, None, None, None, None, None],
                current_key: 0,
                offer: Some(Box::new(EphemeralOffer {
                    ratchet_counter: 0,
                    timestamp: current_time,
                    c25519: offer_c25519,
                    kyber768_alice_keypair: offer_kyber,
                    kyber768_bob_ciphertext: None,
                    p384: offer_p384,
                })),
                remote_session_id: 0, // not known yet
            }),
            creation_time: current_time,
            local_session_id,
            outbound_counter: AtomicU32::new(1), // INIT was counter==0
            last_inbound_counter: AtomicU32::new(0),
            inbound_counter_dedup: array_from_fn(|| AtomicU32::new(0xffffffff)),
            role: Role::Alice,
            enabled_kex_algorithms: enable_kex_algorithms,
            setup_kex_algorithm,
        }),
        packet,
    ))
}

pub fn receive<O, const L: usize, SC: SessionContext<O>>(
    sc: &SC,
    local_identity: &Identity,
    local_obfuscation_cipher: &Aes,
    current_time: i64,
    enable_kex_algorithms: u8,
    packet: &Buffer<L>,
    output: &mut Buffer<L>,
) -> ReceiveResult<O> {
    // Mini AES-CTR to deobfuscate the packet. Usually we only have to deobfuscate 11 bytes.
    let obfuscation_counter = if let Ok(obfuscation_nonce) = packet.bytes_fixed_at::<12>(packet.len() - 12) {
        let mut tmp = [0_u8; 16];
        tmp[..12].copy_from_slice(obfuscation_nonce);
        tmp
    } else {
        unlikely_branch();
        return ReceiveResult::Error(ReceiveError::Invalid);
    };
    let mut obfuscation_keystream = [0_u8; 16];
    local_obfuscation_cipher.encrypt_block(&obfuscation_counter, &mut obfuscation_keystream);

    // Deobfuscate and extract values from common header.
    let (counter, local_session_id, key_index, packet_type, nonce) = if let Ok(h) = packet.bytes_fixed_mut_at::<HEADER_SIZE>(0) {
        let ct = h[0] ^ obfuscation_keystream[0];
        let mut h = [
            ct,
            h[1] ^ obfuscation_keystream[1],
            h[2] ^ obfuscation_keystream[2],
            h[3] ^ obfuscation_keystream[3],
            h[4] ^ obfuscation_keystream[4],
            h[5] ^ obfuscation_keystream[5],
            h[6] ^ obfuscation_keystream[6],
            h[7] ^ obfuscation_keystream[7],
            h[8] ^ obfuscation_keystream[8],
            h[9] ^ obfuscation_keystream[9],
            h[10] ^ obfuscation_keystream[10],
            0, // last byte of CTR/GCM nonce will be set to 'role'
        ];
        (
            u32::from_le_bytes(*byte_array_range::<12, 1, 4>(&h)),
            u64::from_le_bytes(*byte_array_range::<12, 3, 8>(&h)).wrapping_shr(16), // read u64 then shift to get u48
            ct.wrapping_shr(HEADER_KT_KEY_ID_SHIFT),
            ct & HEADER_KT_PACKET_TYPE_MASK,
            h,
        )
    } else {
        unlikely_branch();
        return ReceiveResult::Error(ReceiveError::Invalid);
    };

    if packet_type == PACKET_TYPE_DATA {
        if let Some(session) = sc.lookup_session(local_session_id) {
            let state = session.state.read();
            if let Some(session_key) = state.keys[key_index as usize].as_ref() {
                if let Some(e) = session.check_incoming_packet_counter(counter) {
                    unlikely_branch();
                    return ReceiveResult::Error(e);
                }
                todo!()
            } else {
                unlikely_branch();
                return ReceiveResult::Error(ReceiveError::FailedAuthentication);
            }
        } else {
            unlikely_branch();
            return ReceiveResult::Error(ReceiveError::UnknownLocalSessionId);
        }
    } else {
        unlikely_branch();

        // These optional values are filled in by the private read_fields() function. It has to be called
        // differently depending on whether this is INIT/DATAGRAM or something else. For those two the
        // payload must be decrypted first, then parsed, then authenticated because we don't know the
        // identity of the other side to get the static key to authenticate yet. For other types we do
        // have the static key and can authenticate first.
        let mut cursor = HEADER_SIZE;
        let (
            mut remote_data,
            mut remote_auth_data,
            mut remote_ephemeral_c25519,
            mut remote_ephemeral_p384,
            mut remote_identity,
            mut remote_session_id,
            mut remote_ephemeral_kyber_public,
            mut remote_ephemeral_kyber_encapsulated_secret,
            mut remote_timestamp,
            mut remote_timestamp_echo,
            mut remote_ratchet_counter,
        ) = (None, None, None, None, None, None, None, None, None, None, None);

        if packet_type == PACKET_TYPE_DATAGRAM || packet_type == PACKET_TYPE_INIT {
            // DATAGRAM and INIT are sent to initiate a session or pass a bit of sessionless data. They
            // carry an unencrypted epheemral key before their encrypted payload. This key agrees with
            // the recipient's static identity and creates a "setup key" used for INIT/INIT_ACK or only
            // once for DATAGRAM.

            if key_index != 0 || local_session_id != 0 {
                return ReceiveResult::Error(ReceiveError::Invalid);
            }

            // Save deobfuscated outer ephemeral data in a buffer for performing auth later, as we'll need to
            // auth the whole packet as it was before the sender obfuscated it.
            let mut outer_data_for_auth = [0_u8; 64];
            let mut outer_data_for_auth_ptr = 0;

            // Create a little deobufscator closure to deobfuscate outer ephemeral keys, also adds to outer
            // data for auth buffer above. Start at HEADER_SIZE since we used those bytes at the top.
            debug_assert!(HEADER_SIZE < 16); // sanity check
            let mut obfuscation_keystream_ptr = HEADER_SIZE;
            let deobfuscate = |b: &mut [u8]| {
                for i in 0..b.len() {
                    if obfuscation_keystream_ptr == 16 {
                        obfuscation_counter[15] += 1;
                        local_obfuscation_cipher.encrypt_block(&obfuscation_counter, &mut obfuscation_keystream);
                        obfuscation_keystream_ptr = 0;
                    }
                    let deob = b[i] ^ obfuscation_keystream[obfuscation_keystream_ptr];
                    obfuscation_keystream_ptr += 1;
                    b[i] = deob;
                    outer_data_for_auth[outer_data_for_auth_ptr] = deob;
                    outer_data_for_auth_ptr += 1;
                }
            };

            // Parse and extract the outer ephemeral key and use key agreement between it and the local identity
            // to create the setup key, which is used for INIT and INITACK.
            let (setup_key, setup_kex_algorithm) = if let Ok(mut outer_ephemeral_type) = packet.read_u8(&mut cursor) {
                // Deobfuscate the outer ephemeral key type byte (and save it for future auth inclusion).
                outer_ephemeral_type ^= obfuscation_keystream[obfuscation_keystream_ptr];
                obfuscation_keystream_ptr += 1;
                outer_data_for_auth[outer_data_for_auth_ptr] = outer_ephemeral_type;
                outer_data_for_auth_ptr += 1;

                if outer_ephemeral_type == FIELD_EPHEMERAL_NISTP384 {
                    // NIST P-384, the current default.
                    if let Some(local_static_p384) = local_identity.secret.as_ref().and_then(|lss| lss.p384.as_ref()) {
                        if let Ok(remote_p384) = packet.read_bytes_fixed::<P384_PUBLIC_KEY_SIZE>(&mut cursor) {
                            let mut remote_p384 = remote_p384.clone();
                            deobfuscate(&mut remote_p384);
                            if let Some(sk) = P384PublicKey::from_bytes(&remote_p384).and_then(|rp384| {
                                remote_ephemeral_p384.insert(rp384);
                                local_static_p384.ecdh.agree(&rp384)
                            }) {
                                (Secret(SHA512::hash(sk.as_bytes())), KEX_NISTP384)
                            } else {
                                return ReceiveResult::Error(ReceiveError::FailedAuthentication);
                            }
                        } else {
                            return ReceiveResult::Error(ReceiveError::Invalid);
                        }
                    } else {
                        return ReceiveResult::Error(ReceiveError::UnsupportedCipher);
                    }
                } else if outer_ephemeral_type == FIELD_EPHEMERAL_C25519 {
                    if let Some(local_static_secret) = local_identity.secret.as_ref() {
                        if let Ok(remote_c25519) = packet.read_bytes_fixed::<C25519_PUBLIC_KEY_SIZE>(&mut cursor) {
                            let mut remote_c25519 = remote_c25519.clone();
                            deobfuscate(&mut remote_c25519);
                            let sk = Secret(SHA512::hash(local_static_secret.c25519.agree(&remote_c25519).as_bytes()));
                            remote_ephemeral_c25519.insert(remote_c25519);
                            (sk, KEX_C25519)
                        } else {
                            return ReceiveResult::Error(ReceiveError::Invalid);
                        }
                    } else {
                        return ReceiveResult::Error(ReceiveError::UnsupportedCipher);
                    }
                } else {
                    return ReceiveResult::Error(ReceiveError::UnsupportedCipher);
                }
            } else {
                return ReceiveResult::Error(ReceiveError::Invalid);
            };

            if (packet.len() - cursor) < SHA384_HASH_SIZE {
                return ReceiveResult::Error(ReceiveError::Invalid);
            }
            let (ciphertext, packet_hmac) = {
                let ct_len = packet.len() - SHA384_HASH_SIZE;
                let pb = packet.as_bytes();
                (&pb[cursor..ct_len], &pb[ct_len..])
            };

            let mut ctr = AesCtr::new(&zt_kbkdf_hmac_sha384(&setup_key.as_bytes()[..48], KBKDF_KEY_USAGE_LABEL_AES_CTR).as_bytes()[..32]);
            nonce[11] = Role::Alice as u8; // INIT and DATAGRAM always sent by "alice"
            ctr.init(&nonce[..12]);
            debug_assert!(output.is_empty());
            ctr.crypt(ciphertext, output.append_bytes_get_mut(ciphertext.len()).unwrap());

            cursor = 0; // start reading from 'output' now
            if !read_fields(
                &mut cursor,
                output,
                &mut remote_data,
                &mut remote_auth_data,
                &mut remote_ephemeral_c25519,
                &mut remote_ephemeral_p384,
                &mut remote_identity,
                &mut remote_session_id,
                &mut remote_ephemeral_kyber_public,
                &mut remote_ephemeral_kyber_encapsulated_secret,
                &mut remote_timestamp,
                &mut remote_timestamp_echo,
                &mut remote_ratchet_counter,
            ) {
                return ReceiveResult::Error(ReceiveError::Invalid);
            }

            let remote_identity = if let Some(id) = remote_identity {
                id
            } else {
                return ReceiveResult::Error(ReceiveError::MissingRequiredFields);
            };
            if !sc.validate_identity(&remote_identity) {
                return ReceiveResult::Error(ReceiveError::FailedAuthentication);
            }
            let static_key = if let Some(k) = local_identity.agree(&remote_identity) {
                k
            } else {
                return ReceiveResult::Error(ReceiveError::FailedAuthentication);
            };

            let mut hmac = HMACSHA384::new(zt_kbkdf_hmac_sha384(&static_key.as_bytes()[..48], KBKDF_KEY_USAGE_LABEL_HMAC).as_bytes());
            hmac.update(&nonce[..HEADER_SIZE]); // first 11 bytes of nonce are deobfuscated header
            hmac.update(&outer_data_for_auth[..outer_data_for_auth_ptr]); // deobfuscated outer unencrypted data
            hmac.update(ciphertext); // encrypted ciphertext as it would have been HMAC'd by sender
            if !hmac.finish().eq(packet_hmac) {
                return ReceiveResult::Error(ReceiveError::FailedAuthentication);
            }

            if packet_type == PACKET_TYPE_INIT {
                let (offer, session_key, session_kex_algorithms) =
                    ephemeral_counter_offer(0, current_time, &static_key, &remote_ephemeral_c25519, &remote_ephemeral_kyber_public, &remote_ephemeral_p384, enable_kex_algorithms);
                if session_kex_algorithms != 0 {
                    let remote_session_id = if let Some(sid) = remote_session_id {
                        sid
                    } else {
                        return ReceiveResult::Error(ReceiveError::MissingRequiredFields);
                    };
                    let (local_session_id, obj) = if let Some(auth) = sc.authorize_new_session(
                        &remote_identity,
                        remote_auth_data
                            .and_then(|(start, end)| output.as_byte_range(start, end).map_or(None, |br| Buffer::<AUTH_DATA_SIZE_MAX>::from_bytes(br).map_or(None, |br| Some(br)))),
                    ) {
                        auth
                    } else {
                        return ReceiveResult::Error(ReceiveError::NewSessionRejected);
                    };

                    let outgoing_obfuscation_cipher = Aes::new(&remote_identity.fingerprint[..32]);
                    let hmac_key: Secret<48> = zt_kbkdf_hmac_sha384(&static_key.as_bytes()[..48], KBKDF_KEY_USAGE_LABEL_HMAC).first_n();

                    output.clear();
                    create_ack_packet(
                        &mut output,
                        Some(local_session_id),
                        remote_session_id,
                        0,
                        0,
                        &offer,
                        &session_key,
                        &zt_kbkdf_hmac_sha384(&setup_key.as_bytes()[..48], KBKDF_KEY_USAGE_LABEL_AES_CTR).first_n(),
                        &hmac_key,
                        &outgoing_obfuscation_cipher,
                        PACKET_TYPE_INIT_ACK,
                        Role::Bob,
                    );

                    return ReceiveResult::OkNewSession(
                        remote_identity,
                        Arc::new(Session::<O> {
                            obj,
                            setup_key,
                            static_key: static_key.clone(),
                            hmac_key,
                            outgoing_obfuscation_cipher,
                            state: RwLock::new(MutState {
                                keys: [Some(Box::new(SessionKey::new(session_key, current_time, 0, 0, session_kex_algorithms))), None, None, None, None, None, None, None],
                                current_key: 0,
                                offer: Some(offer),
                                remote_session_id,
                            }),
                            creation_time: current_time,
                            local_session_id,
                            outbound_counter: AtomicU32::new(1), // ACK was counter==0
                            last_inbound_counter: AtomicU32::new(counter),
                            inbound_counter_dedup: array_from_fn(|| AtomicU32::new(0xffffffff)),
                            role: Role::Bob,
                            enabled_kex_algorithms: enable_kex_algorithms,
                            setup_kex_algorithm,
                        }),
                    );
                } else {
                    return ReceiveResult::Error(ReceiveError::MissingRequiredFields);
                }
            } else if let Some(d) = remote_data {
                return ReceiveResult::OkDatagram(remote_identity, d);
            } else {
                return ReceiveResult::Error(ReceiveError::MissingRequiredFields);
            }
        } else if let Some(session) = sc.lookup_session(local_session_id) {
            // With everything but INIT and DATAGRAM we can look up the session and auth first.

            // It shouldn't be possible to make it here with these values. (In non-debug they'd be ignored.)
            debug_assert_ne!(packet_type, PACKET_TYPE_DATA);
            debug_assert_ne!(packet_type, PACKET_TYPE_DATAGRAM);
            debug_assert_ne!(packet_type, PACKET_TYPE_INIT);

            if (packet.len() - HEADER_SIZE) < SHA384_HASH_SIZE {
                return ReceiveResult::Error(ReceiveError::Invalid);
            }
            let (ciphertext, packet_hmac) = {
                let ct_len = packet.len() - SHA384_HASH_SIZE;
                let pb = packet.as_bytes();
                (&pb[HEADER_SIZE..ct_len], &pb[ct_len..])
            };

            let mut hmac = HMACSHA384::new(zt_kbkdf_hmac_sha384(&session.static_key.as_bytes()[..48], KBKDF_KEY_USAGE_LABEL_HMAC).as_bytes());
            hmac.update(&nonce[..HEADER_SIZE]);
            hmac.update(ciphertext);
            if !hmac.finish().eq(packet_hmac) {
                return ReceiveResult::Error(ReceiveError::FailedAuthentication);
            }

            if let Some(e) = session.check_incoming_packet_counter(counter) {
                return ReceiveResult::Error(e);
            }

            let session_m = session.state.upgradable_read();

            let mut ctr = if packet_type == PACKET_TYPE_INIT_ACK {
                AesCtr::new(&zt_kbkdf_hmac_sha384(&session.setup_key.as_bytes()[..48], KBKDF_KEY_USAGE_LABEL_AES_CTR).as_bytes()[..32])
            } else {
                if let Some(k) = session_m.keys[key_index as usize].as_ref() {
                    AesCtr::new(k.aes_ctr_key.as_bytes())
                } else {
                    return ReceiveResult::Error(ReceiveError::NoSessionKeyAtIndex);
                }
            };
            ctr.init(&nonce);
            debug_assert!(output.is_empty());
            ctr.crypt(ciphertext, output.append_bytes_get_mut(ciphertext.len()).unwrap());

            let mut cursor = HEADER_SIZE;
            if !read_fields(
                &mut cursor,
                output,
                &mut remote_data,
                &mut remote_auth_data,
                &mut remote_ephemeral_c25519,
                &mut remote_ephemeral_p384,
                &mut remote_identity,
                &mut remote_session_id,
                &mut remote_ephemeral_kyber_public,
                &mut remote_ephemeral_kyber_encapsulated_secret,
                &mut remote_timestamp,
                &mut remote_timestamp_echo,
                &mut remote_ratchet_counter,
            ) {
                return ReceiveResult::Error(ReceiveError::Invalid);
            }

            match packet_type {
                PACKET_TYPE_INIT_ACK | PACKET_TYPE_REKEY_ACK => {
                    // ACKs carrying ephemeral key counter-offers; the only difference between INIT_ACK
                    // REKEY_ACK is that INIT_ACK is encrypted with the setup key not a session key.
                }
                PACKET_TYPE_REKEY => {}
                _ => return ReceiveResult::Ok, // passed auth, but ignore unknown packet types
            }
            todo!()
        } else {
            return ReceiveResult::Error(ReceiveError::UnknownLocalSessionId);
        }
    }
}

impl<O> Session<O> {
    #[inline(always)]
    fn check_incoming_packet_counter(&self, counter: u32) -> Option<ReceiveError> {
        let last_counter = self.last_inbound_counter.load(Ordering::Relaxed);
        if counter.wrapping_sub(last_counter).min(last_counter.wrapping_sub(counter)) > MAX_COUNTER_DIFFERENCE {
            unlikely_branch();
            return Some(ReceiveError::CounterTooOutOfSync);
        }
        if self.inbound_counter_dedup[(counter & 0xff) as usize].swap(counter, Ordering::Relaxed) == counter {
            unlikely_branch();
            return Some(ReceiveError::Duplicate);
        }
        self.last_inbound_counter.store(counter, Ordering::Relaxed);
        None
    }
}

/// Create both 11-byte header and 12-byte nonce (header is just first 11 bytes)
#[inline(always)]
fn make_header_and_nonce(ratchet_counter: u64, packet_type: u8, counter: u32, session_id: u64, role: u8) -> [u8; HEADER_SIZE + 1] {
    let counter = counter.to_le_bytes();
    let session_id = session_id.to_le_bytes();
    [
        (ratchet_counter as u8).wrapping_shl(HEADER_KT_KEY_ID_SHIFT) | packet_type,
        counter[0],
        counter[1],
        counter[2],
        counter[3],
        session_id[0],
        session_id[1],
        session_id[2],
        session_id[3],
        session_id[4],
        session_id[5],
        role,
    ]
}

/// Creates an INIT_ACK or REKEY_ACK packet to send back an ephemeral "counter-offer."
fn create_ack_packet<const L: usize>(
    output: &mut Buffer<L>,
    local_session_id: Option<u64>,
    remote_session_id: u64,
    ratchet_counter: u64,
    packet_counter: u32,
    offer: &EphemeralOffer,
    new_session_key: &Secret<64>,
    ctr_key: &Secret<32>,
    hmac_key: &Secret<48>,
    outgoing_obfuscation_cipher: &Aes,
    packet_type: u8,
    role: Role,
) {
    let header_and_nonce = make_header_and_nonce(ratchet_counter, packet_type, packet_counter, remote_session_id, role as u8);
    debug_assert!(output.is_empty());
    output.append_bytes(&header_and_nonce[..HEADER_SIZE]).expect(UNEXPECTED_OVERFLOW_PANIC_MSG);

    if let Some(local_session_id) = local_session_id {
        // Only included in INIT_ACK
        output.append_u8(FIELD_SESSION_ID).expect(UNEXPECTED_OVERFLOW_PANIC_MSG);
        output.append_bytes(&local_session_id.to_le_bytes()[..HEADER_SESSION_ID_LENGTH_BYTES]).expect(UNEXPECTED_OVERFLOW_PANIC_MSG);
    }

    output.append_u8(FIELD_RATCHET_COUNTER).expect(UNEXPECTED_OVERFLOW_PANIC_MSG);
    output.append_varint(ratchet_counter).expect(UNEXPECTED_OVERFLOW_PANIC_MSG);

    if let Some(c25519_offer) = offer.c25519.as_ref() {
        output.append_u8(FIELD_EPHEMERAL_C25519).expect(UNEXPECTED_OVERFLOW_PANIC_MSG);
        output.append_bytes_fixed(&c25519_offer.public_bytes()).expect(UNEXPECTED_OVERFLOW_PANIC_MSG);
    }
    if let Some(kyber768_bob_ciphertext) = offer.kyber768_bob_ciphertext.as_ref() {
        output.append_u8(FIELD_EPHEMERAL_KYBER_CIPHERTEXT).expect(UNEXPECTED_OVERFLOW_PANIC_MSG);
        output.append_bytes_fixed(&kyber768_bob_ciphertext).expect(UNEXPECTED_OVERFLOW_PANIC_MSG);
    }
    if let Some(p384_offer) = offer.p384.as_ref() {
        output.append_u8(FIELD_EPHEMERAL_NISTP384).expect(UNEXPECTED_OVERFLOW_PANIC_MSG);
        output.append_bytes_fixed(p384_offer.public_key_bytes()).expect(UNEXPECTED_OVERFLOW_PANIC_MSG);
    }
    output.append_u8(FIELD_SESSION_KEY_HASH).expect(UNEXPECTED_OVERFLOW_PANIC_MSG);
    output.append_bytes_fixed(&SHA384::hash(new_session_key.as_bytes())).expect(UNEXPECTED_OVERFLOW_PANIC_MSG);

    armor_control_packet(&mut output, &header_and_nonce, HEADER_SIZE, &outgoing_obfuscation_cipher, ctr_key, hmac_key);
}

/// Armor a packet with AES-256-CTR+HMAC-SHA384
fn armor_control_packet<const L: usize>(
    packet: &mut Buffer<L>,
    ctr_nonce: &[u8],
    start_encrypted_payload_at: usize,
    outgoing_obfuscation_cipher: &Aes,
    ctr_key: &Secret<32>,
    hmac_key: &Secret<48>,
) {
    let mut ctr = AesCtr::new(ctr_key.as_bytes());
    ctr.init(ctr_nonce);
    ctr.crypt_in_place(&mut packet.as_bytes_mut()[start_encrypted_payload_at..]);

    let mut hmac = HMACSHA384::new(hmac_key.as_bytes());
    hmac.update(packet.as_bytes());
    let hmac = hmac.finish();
    packet.append_bytes_fixed(&hmac).expect(UNEXPECTED_OVERFLOW_PANIC_MSG);

    let mut obfuscation_keystream = [0_u8; 64];
    let mut obfuscation_counter = [0_u8; 16];
    obfuscation_counter[..12].copy_from_slice(&hmac[36..48]);
    for i in 0..4 {
        obfuscation_counter[15] = i as u8;
        outgoing_obfuscation_cipher.encrypt_block(&obfuscation_counter, &mut obfuscation_keystream[(16 * i)..(16 * (i + 1))]);
    }
    let packet_bytes = packet.as_bytes_mut();
    for i in 0..start_encrypted_payload_at {
        packet_bytes[i] ^= obfuscation_keystream[i];
    }
}

/// Generate a counter-offer to ephemeral keys from an INIT or REKEY, and agree to get session key.
/// Return: offer, session secret, algorithm bit map (check that the latter is not zero!)
fn ephemeral_counter_offer(
    ratchet_counter: u64,
    current_time: i64,
    static_key: &Secret<64>,
    remote_ephemeral_c25519: &Option<[u8; C25519_PUBLIC_KEY_SIZE]>,
    remote_ephemeral_kyber_public: &Option<[u8; pqc_kyber::KYBER_PUBLICKEYBYTES]>,
    remote_ephemeral_p384: &Option<P384PublicKey>,
    enabled_kex_algorithms: u8,
) -> (Box<EphemeralOffer>, Secret<64>, u8) {
    let mut session_key = Secret(hmac_sha512(&ratchet_counter.to_le_bytes(), static_key.as_bytes()));
    let mut session_kex_algorithms = 0;

    let c25519 = if (enabled_kex_algorithms & KEX_C25519) != 0 {
        remote_ephemeral_c25519.as_ref().map(|pk| {
            session_kex_algorithms |= KEX_C25519;
            let e = C25519KeyPair::generate();
            session_key = Secret(hmac_sha512(session_key.as_bytes(), pk));
            e
        })
    } else {
        None
    };

    let kyber_ciphertext = if (enabled_kex_algorithms & KEX_KYBER768) != 0 {
        remote_ephemeral_kyber_public.as_ref().and_then(|pk| {
            if let Ok((ct, ss)) = pqc_kyber::encapsulate(pk, &mut random::SecureRandom::get()) {
                session_kex_algorithms |= KEX_KYBER768;
                session_key = Secret(hmac_sha512(session_key.as_bytes(), &ss));
                Some(ct)
            } else {
                None
            }
        })
    } else {
        None
    };

    // FIPS note: for hybrid key agreement P-384 must be last so the session key is HMAC(salt, compliant key) as far as FIPS is concerned.
    let p384 = if (enabled_kex_algorithms & KEX_NISTP384) != 0 {
        remote_ephemeral_p384.as_ref().and_then(|pk| {
            let e = P384KeyPair::generate();
            let k = e.agree(pk);
            k.map(|k| {
                session_kex_algorithms |= KEX_NISTP384;
                session_key = Secret(hmac_sha512(session_key.as_bytes(), k.as_bytes()));
                e
            })
        })
    } else {
        None
    };

    (
        Box::new(EphemeralOffer {
            ratchet_counter,
            timestamp: current_time,
            c25519,
            kyber768_alice_keypair: None,
            kyber768_bob_ciphertext: kyber_ciphertext,
            p384,
        }),
        session_key,
        session_kex_algorithms,
    )
}

/// Internal method to read and extract data from a series of field ID, value, ...
/// Returns false if a parse or other error occurs.
fn read_fields<const L: usize>(
    cursor: &mut usize,
    packet: &Buffer<L>,
    data: &mut Option<(usize, usize)>,
    auth_data: &mut Option<(usize, usize)>,
    ephemeral_c25519: &mut Option<[u8; C25519_PUBLIC_KEY_SIZE]>,
    ephemeral_p384: &mut Option<P384PublicKey>,
    identity: &mut Option<Identity>,
    session_id: &mut Option<u64>,
    ephemeral_kyber_public: &mut Option<[u8; pqc_kyber::KYBER_PUBLICKEYBYTES]>,
    ephemeral_kyber_encapsulated_secret: &mut Option<[u8; pqc_kyber::KYBER_CIPHERTEXTBYTES]>,
    timestamp: &mut Option<u64>,
    timestamp_echo: &mut Option<u64>,
    ratchet_counter: &mut Option<u64>,
) -> bool {
    while *cursor < packet.len() {
        let field_type = packet.read_u8(cursor).unwrap(); // bounds already checked by while condition
        match field_type {
            FIELD_DATA => {
                if data.is_some() {
                    return false;
                }
                if let Ok(dlen) = packet.read_varint(cursor) {
                    let start = *cursor;
                    let end = *cursor + (dlen as usize);
                    if end > packet.len() {
                        return false;
                    }
                    data.insert((start, end));
                } else {
                    return false;
                }
            }
            FIELD_AUTH_DATA => {
                if auth_data.is_some() {
                    return false;
                }
                if let Ok(dlen) = packet.read_varint(cursor) {
                    let start = *cursor;
                    let end = *cursor + (dlen as usize);
                    if end > packet.len() {
                        return false;
                    }
                    auth_data.insert((start, end));
                } else {
                    return false;
                }
            }
            FIELD_STATIC_IDENTITY => {
                if identity.is_some() {
                    return false;
                }
                if let Ok(id) = Identity::unmarshal(packet, cursor) {
                    identity.insert(id);
                } else {
                    return false;
                }
            }
            FIELD_SESSION_ID => {
                if session_id.is_some() {
                    return false;
                }
                if let Ok(sid) = packet.read_bytes_fixed::<6>(cursor) {
                    session_id.insert(
                        (sid[0] as u64)
                            | (sid[1] as u64).wrapping_shl(8)
                            | (sid[2] as u64).wrapping_shl(16)
                            | (sid[3] as u64).wrapping_shl(24)
                            | (sid[4] as u64).wrapping_shl(32)
                            | (sid[5] as u64).wrapping_shl(40),
                    );
                } else {
                    return false;
                }
            }
            FIELD_EPHEMERAL_C25519 => {
                if ephemeral_c25519.is_some() {
                    return false;
                }
                if let Ok(c25519) = packet.read_bytes_fixed::<C25519_PUBLIC_KEY_SIZE>(cursor) {
                    ephemeral_c25519.insert(c25519.clone());
                } else {
                    return false;
                }
            }
            FIELD_EPHEMERAL_NISTP384 => {
                if ephemeral_p384.is_some() {
                    return false;
                }
                if let Ok(remote_p384) = packet.read_bytes_fixed::<P384_PUBLIC_KEY_SIZE>(cursor) {
                    if let Some(remote_p384) = P384PublicKey::from_bytes(remote_p384) {
                        ephemeral_p384.insert(remote_p384);
                    } else {
                        return false;
                    }
                } else {
                    return false;
                }
            }
            FIELD_EPHEMERAL_KYBER_PUBLIC => {
                if ephemeral_kyber_public.is_some() {
                    return false;
                }
                if let Ok(remote_kyber) = packet.read_bytes_fixed::<{ pqc_kyber::KYBER_PUBLICKEYBYTES }>(cursor) {
                    ephemeral_kyber_public.insert(remote_kyber.clone());
                } else {
                    return false;
                }
            }
            FIELD_EPHEMERAL_KYBER_ENCAPSULATED_SECRET => {
                if ephemeral_kyber_encapsulated_secret.is_some() {
                    return false;
                }
                if let Ok(remote_kyber_secret) = packet.read_bytes_fixed::<{ pqc_kyber::KYBER_CIPHERTEXTBYTES }>(cursor) {
                    ephemeral_kyber_encapsulated_secret.insert(remote_kyber_secret.clone());
                } else {
                    return false;
                }
            }
            FIELD_TIMESTAMP => {
                if timestamp.is_some() {
                    return false;
                }
                if let Ok(ts) = packet.read_u64_le(cursor) {
                    timestamp.insert(ts);
                } else {
                    return false;
                }
            }
            FIELD_TIMESTAMP_ECHO => {
                if timestamp_echo.is_some() {
                    return false;
                }
                if let Ok(ts) = packet.read_u64_le(cursor) {
                    timestamp_echo.insert(ts);
                } else {
                    return false;
                }
            }
            FIELD_RATCHET_COUNTER => {
                if ratchet_counter.is_some() {
                    return false;
                }
                if let Ok(c) = packet.read_varint(cursor) {
                    ratchet_counter.insert(c);
                } else {
                    return false;
                }
            }
            _ => {
                // Any fields we add in the future will need to be prefixed by their size.
                if let Ok(additional_field_len) = packet.read_varint(cursor) {
                    *cursor += additional_field_len as usize;
                } else {
                    return false;
                }
            }
        }
    }
    return true;
}

#[cfg(test)]
mod tests {
    //use super::*;
}

*/
