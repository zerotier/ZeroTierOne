// (c) 2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::fmt::Display;
use std::mem::size_of;
use std::sync::atomic::{AtomicU64, Ordering};

use zerotier_core_crypto::aes::{Aes, AesGcm};
use zerotier_core_crypto::hash::{hmac_sha384, hmac_sha512, HMACSHA384, SHA384, SHA512};
use zerotier_core_crypto::kbkdf::zt_kbkdf_hmac_sha384;
use zerotier_core_crypto::p384::{P384KeyPair, P384PublicKey, P384_PUBLIC_KEY_SIZE};
use zerotier_core_crypto::random;
use zerotier_core_crypto::secret::Secret;

use parking_lot::{Mutex, RwLock};

/// Minimum suggested buffer size for work and output buffer supplied to functions.
/// Supplying work buffers smaller than this will likely result in panics.
pub const BUFFER_SIZE_MIN: usize = 1400;

/// Start attempting to rekey after a key has been used to send packets this many times.
pub const REKEY_AFTER_USES: u64 = 1073741824;

/// Start attempting to rekey after a key has been in use for this many milliseconds.
pub const REKEY_AFTER_TIME_MS: i64 = 1000 * 60 * 60;

#[derive(Debug)]
pub enum Error {
    /// The packet was addressed to an unrecognized local session.
    UnknownLocalSessionId,

    /// Packet was not well formed.
    InvalidPacket,

    /// An invalid paramter was supplied.
    InvalidParameter,

    /// Packet failed one or more authentication checks.
    FailedAuthentication,

    /// The supplied authenticator function rejected a new session.
    NewSessionRejected,

    /// An unexpected error occurred, such as an internal error.
    UnexpectedError,

    /// Shared secret has far exceeded rekey thresholds (will only happen if rekeying isn't done or repeatedly fails)
    MaxKeyLifetimeExceeded,
}

impl Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::UnknownLocalSessionId => f.write_str("zssp::Error::UnknownLocalSessionId"),
            Self::InvalidPacket => f.write_str("zssp::Error::InvalidPacket"),
            Self::InvalidParameter => f.write_str("zssp::Error::InvalidParameter"),
            Self::FailedAuthentication => f.write_str("zssp::Error::FailedAuthentication"),
            Self::NewSessionRejected => f.write_str("zssp::Error::NewSessionRejected"),
            Self::UnexpectedError => f.write_str("zssp::Error::UnexpectedError"),
            Self::MaxKeyLifetimeExceeded => f.write_str("zssp::Error::MaxKeyLifetimeExceeded"),
        }
    }
}

impl std::error::Error for Error {}

pub enum ReceiveResult<'a> {
    /// Packet is valid and contained a data payload.
    OkData(&'a [u8]),

    /// Packet is valid, no action needs to be taken.
    Ok,

    /// Packet is valid and the provided reply should be sent back.
    OkSendReply(&'a [u8]),

    /// Packet appears valid but was ignored as a duplicate.
    Duplicate,

    /// Packet apperas valid but was ignored for another reason.
    Ignored,
}

pub struct Obfuscator(Aes);

impl Obfuscator {
    pub fn new(static_public: &[u8]) -> Self {
        Self(Aes::new(&SHA384::hash(static_public)[..16]))
    }
}

pub struct Session<O> {
    pub local_session_id: u64,
    pub creation_time: i64,
    remote_session_id: AtomicU64,
    outgoing_packet_counter: Counter,
    static_secret: Secret<64>,
    local_to_remote_obfuscator: Obfuscator,
    offer: Mutex<Option<Box<EphemeralOffer>>>,
    shared_secret: RwLock<SymmetricKeyQueue>, // previous, current, next
    pub associated_object: O,
}

impl<O> Session<O> {
    /// Create a new session and return this plus an outgoing packet to call the other side.
    ///
    /// If 'jedi' is true, the newly approved NIST post-quantum Kyber algorithm is used.
    ///
    /// The supplied 'local_session_id' must not be zero or this will return an error. An error may also be
    /// returned if a supplied key is invalid or an internal error occurs.
    pub fn new<'a, const BS: usize, const STATIC_PUBLIC_SIZE: usize>(
        buffer: &'a mut [u8; BS],
        associated_object: O,
        local_session_id: u64,
        local_static_public: &[u8; STATIC_PUBLIC_SIZE],
        static_secret: &Secret<64>,
        remote_static_public: &[u8; STATIC_PUBLIC_SIZE],
        remote_static_p384_public: &P384PublicKey,
        current_time: i64,
        jedi: bool,
    ) -> Result<(Self, &'a [u8]), Error> {
        debug_assert!(BS >= BUFFER_SIZE_MIN);
        if local_session_id != 0 {
            let counter = Counter::new();
            let local_to_remote_obfuscator = Obfuscator::new(remote_static_public);
            if let Some((offer, offer_packet_size)) = EphemeralOffer::create(buffer, counter.next(), local_session_id, 0, local_static_public, remote_static_p384_public, static_secret, &local_to_remote_obfuscator, jedi) {
                return Ok((
                    Self {
                        local_session_id,
                        creation_time: current_time,
                        remote_session_id: AtomicU64::new(0),
                        outgoing_packet_counter: counter,
                        static_secret: static_secret.clone(),
                        local_to_remote_obfuscator,
                        offer: Mutex::new(Some(offer)),
                        shared_secret: RwLock::new(SymmetricKeyQueue { current: None, previous: None, next: None }),
                        associated_object,
                    },
                    &buffer[..offer_packet_size],
                ));
            }
        }
        return Err(Error::InvalidParameter);
    }
}

/// Receive a packet from the network and take the appropriate action.
///
/// Sessions returned by 'session_lookup' are anything that implements Deref for the correct session type.
/// This allows external code to use Arc<>, Box<>, etc. to contain sessions at its discretion.
///
/// The authenticator must return a non-zero new local session ID and a value for associated_object if
/// the new session should be accepted.
///
/// If 'jedi' is true this side will support and reciprocate Kyber key exchanges. Otherwise these will
/// be ignored and our responses will not contain them, causing the session to not be quantum-forward-secure.
///
/// Check ReceiveResult to see if it includes data or a reply packet.
#[inline(always)]
pub fn receive<
    'a,
    StaticStaticAgreeFunction: FnOnce(&[u8; STATIC_PUBLIC_SIZE]) -> Option<Secret<64>>,
    P384PublicKeyExtractFunction: FnOnce(&[u8; STATIC_PUBLIC_SIZE]) -> Option<P384PublicKey>,
    SessionLookupFunction: FnOnce(u64) -> Option<S>,
    NewSessionAuthenticatorFunction: FnOnce(&[u8; STATIC_PUBLIC_SIZE]) -> Option<(u64, O)>,
    S: std::ops::Deref<Target = Session<O>>,
    O,
    const BS: usize,
    const STATIC_PUBLIC_SIZE: usize,
>(
    incoming_packet: &[u8],
    buffer: &'a mut [u8; BS],
    local_static_p384_secret: &P384KeyPair,
    remote_to_local_obfuscator: &Obfuscator,
    static_static_agree: StaticStaticAgreeFunction,
    extract_static_p384_public: P384PublicKeyExtractFunction,
    session_lookup: SessionLookupFunction,
    new_session_auth: NewSessionAuthenticatorFunction,
    current_time: i64,
    jedi: bool,
) -> Result<ReceiveResult<'a>, Error> {
    debug_assert!(BS >= BUFFER_SIZE_MIN);
    debug_assert!(BS >= size_of::<packed::KeyOffer<STATIC_PUBLIC_SIZE>>());

    if incoming_packet.len() > BS {
        unlikely_branch();
        return Err(Error::InvalidParameter);
    }
    if incoming_packet.len() < 16 {
        unlikely_branch();
        return Err(Error::InvalidPacket);
    }

    remote_to_local_obfuscator.0.decrypt_block(&incoming_packet[0..16], &mut buffer[0..16]);
    let local_session_id = u64::from_le_bytes(buffer[3..11].try_into().unwrap()).wrapping_shr(16);
    let packet_type = buffer[0];

    if packet_type == PACKET_TYPE_DATA {
        // This is checked immediately so it becomes the fast path and all other branches can be treated as unlikely by the compiler.
        if let Some(session) = session_lookup(local_session_id) {
            todo!()
        } else {
            unlikely_branch();
            return Err(Error::UnknownLocalSessionId);
        }
    } else {
        unlikely_branch();

        let session = if local_session_id == 0 {
            None
        } else {
            let s = session_lookup(local_session_id);
            if s.is_none() {
                return Err(Error::UnknownLocalSessionId);
            }
            s
        };

        match packet_type {
            PACKET_TYPE_KEY_OFFER => {
                if incoming_packet.len() < size_of::<packed::KeyOffer<STATIC_PUBLIC_SIZE>>() {
                    return Err(Error::InvalidPacket);
                }
                let (remote_e0, local_static_remote_e0) = decrypt_and_auth_key_exchange_packet::<BS, STATIC_PUBLIC_SIZE>(incoming_packet, buffer, remote_to_local_obfuscator, local_static_p384_secret)?;

                let p = packed::as_packed_struct::<packed::KeyOffer<STATIC_PUBLIC_SIZE>>(buffer).unwrap();
                let remote_static_public = p.static_public; // eliminate possibly spurious alignment error
                let remote_session_id = u48_from_le_bytes(&p.from_session_id);

                if let Some(session) = session {
                    let ss_hmac_key = zt_kbkdf_hmac_sha384(&session.static_secret.0[..48], KBKDF_KEY_USAGE_LABEL_HMAC);
                    if !hmac_sha384(ss_hmac_key.as_bytes(), &buffer[..size_of::<packed::KeyOffer<STATIC_PUBLIC_SIZE>>() - 48]).eq(&buffer[size_of::<packed::KeyOffer<STATIC_PUBLIC_SIZE>>() - 48..size_of::<packed::KeyOffer<STATIC_PUBLIC_SIZE>>()]) {
                        return Err(Error::FailedAuthentication);
                    }

                    // Learn remote session ID. If already set this does nothing. It could theoretically change without consequence.
                    session.remote_session_id.store(remote_session_id, Ordering::Relaxed);

                    // Drop any old offers we've made since we are now making a counter offer to their offer.
                    let _ = session.offer.lock().take();

                    let local_e0_secret = P384KeyPair::generate();
                    let (local_e1_secret, local_e1_remote_e1) = if jedi && p.e1.iter().any(|b| *b != 0) {
                        if let Ok((local_e1_secret, local_e1_remote_e1)) = pqc_kyber::encapsulate(&p.e1, &mut random::SecureRandom::default()) {
                            (Some(local_e1_secret), Some(local_e1_remote_e1))
                        } else {
                            (None, None)
                        }
                    } else {
                        (None, None)
                    };

                    // TODO: locally agree, make local shared secret, etc.

                    let mut reply: packed::KeyCounterOffer<STATIC_PUBLIC_SIZE> = packed::zeroed();
                    reply.t = PACKET_TYPE_KEY_COUNTER_OFFER;
                    //rp.counter = session.outgoing_packet_counter.fetch_add(1, Ordering::SeqCst).to_le_bytes();
                    reply.to_session_id.copy_from_slice(&remote_session_id.to_le_bytes()[..6]);
                    reply.e0 = local_e0_secret.public_key_bytes().clone();
                    reply.from_session_id.copy_from_slice(&session.local_session_id.to_le_bytes()[..6]);

                    todo!()
                } else {
                    // This is an initial key exchange attempt, so do full packet validation and then check to see if we should accept.

                    let static_secret = if let Some(static_secret) = static_static_agree(&remote_static_public) {
                        static_secret
                    } else {
                        return Err(Error::InvalidPacket);
                    };
                    let ss_hmac_key = zt_kbkdf_hmac_sha384(&static_secret.0[..48], KBKDF_KEY_USAGE_LABEL_HMAC);
                    if !hmac_sha384(ss_hmac_key.as_bytes(), &buffer[..size_of::<packed::KeyOffer<STATIC_PUBLIC_SIZE>>() - 48]).eq(&buffer[size_of::<packed::KeyOffer<STATIC_PUBLIC_SIZE>>() - 48..size_of::<packed::KeyOffer<STATIC_PUBLIC_SIZE>>()]) {
                        return Err(Error::FailedAuthentication);
                    }

                    if let Some((local_session_id, obj)) = new_session_auth(&remote_static_public) {
                        todo!()
                    } else {
                        return Err(Error::NewSessionRejected);
                    }
                }
            }
            PACKET_TYPE_KEY_COUNTER_OFFER => {
                if incoming_packet.len() < size_of::<packed::KeyCounterOffer<STATIC_PUBLIC_SIZE>>() {
                    return Err(Error::InvalidPacket);
                }
                let (remote_e0, local_static_remote_e0) = decrypt_and_auth_key_exchange_packet::<BS, STATIC_PUBLIC_SIZE>(incoming_packet, buffer, remote_to_local_obfuscator, local_static_p384_secret)?;

                if let Some(session) = session {
                    let p = packed::as_packed_struct::<packed::KeyCounterOffer<STATIC_PUBLIC_SIZE>>(buffer).unwrap();
                    let remote_session_id = u48_from_le_bytes(&p.from_session_id);

                    if let Some(last_offer) = session.offer.lock().take() {
                        if last_offer.id.eq(&p.in_re_offer_id) {
                            if let Some(ee0) = last_offer.alice_e0.agree(&remote_e0) {
                                let local_e1_remote_e1 = last_offer.alice_e1.and_then(|e1| {
                                    if p.e1.iter().any(|b| *b != 0) {
                                        if let Ok(ee1) = pqc_kyber::decapsulate(&p.e1[..pqc_kyber::KYBER_CIPHERTEXTBYTES], &e1.secret) {
                                            Some(ee1)
                                        } else {
                                            None
                                        }
                                    } else {
                                        None
                                    }
                                });

                                let counter = session.outgoing_packet_counter.next();

                                let new_shared_secret = SymmetricKey::new(&last_offer.alice_e0_bob_static, &local_static_remote_e0, &session.static_secret, ee0, local_e1_remote_e1, current_time, counter, false);
                                let new_secret_key = new_shared_secret.key.clone();

                                let mut session_shared_secret = session.shared_secret.write();
                                session_shared_secret.previous = session_shared_secret.current.replace(new_shared_secret);
                                let _ = session_shared_secret.next.take();
                                drop(session_shared_secret); // release lock

                                let mut reply: packed::KeyAck = packed::zeroed();
                                reply.t = PACKET_TYPE_KEY_COUNTER_OFFER_ACK;
                                reply.counter = counter.to_bytes();
                                reply.to_session_id.copy_from_slice(&remote_session_id.to_le_bytes()[..6]);
                                let mut hmac = HMACSHA384::new(zt_kbkdf_hmac_sha384(&new_secret_key.as_bytes()[..48], KBKDF_KEY_USAGE_LABEL_HMAC).as_bytes());
                                hmac.update(&packed::as_bytes(&reply)[..COMMON_HEADER_SIZE]);
                                hmac.update(new_secret_key.as_bytes());
                                reply.hmac_shared_secret = hmac.finish();
                                reply.hmac_static_key = hmac_sha384(zt_kbkdf_hmac_sha384(&session.static_secret.0[..48], KBKDF_KEY_USAGE_LABEL_HMAC).as_bytes(), &packed::as_bytes(&reply)[..size_of::<packed::KeyAck>() - 48]);

                                buffer[..size_of::<packed::KeyAck>()].copy_from_slice(&packed::as_bytes(&reply)[..size_of::<packed::KeyAck>()]);
                                session.local_to_remote_obfuscator.0.encrypt_block_in_place(&mut buffer[0..16]);

                                return Ok(ReceiveResult::OkSendReply(&buffer[..size_of::<packed::KeyAck>()]));
                            } else {
                                return Err(Error::InvalidPacket);
                            }
                        }
                    }
                }

                return Ok(ReceiveResult::Ignored);
            }
            PACKET_TYPE_KEY_COUNTER_OFFER_ACK => {
                todo!()
            }
            _ => return Err(Error::InvalidPacket),
        }
    }
}

fn decrypt_and_auth_key_exchange_packet<const BS: usize, const STATIC_PUBLIC_SIZE: usize>(incoming_packet: &[u8], buffer: &mut [u8; BS], incoming_obfuscator: &Obfuscator, local_static_p384_secret: &P384KeyPair) -> Result<(P384PublicKey, Secret<64>), Error> {
    incoming_obfuscator.0.decrypt_block(&incoming_packet[16..32], &mut buffer[16..32]);
    incoming_obfuscator.0.decrypt_block(&incoming_packet[32..48], &mut buffer[32..48]);
    incoming_obfuscator.0.decrypt_block(&incoming_packet[48..64], &mut buffer[48..64]);
    buffer[64..incoming_packet.len()].copy_from_slice(&incoming_packet[64..]);

    // Decrypt and authenticate with agree(remote e0, local static)
    let (remote_e0, local_static_remote_e0) = if let Some(incoming_e0) = P384PublicKey::from_bytes(&incoming_packet[packed::KEY_EXCHANGE_PACKET_E0_INDEX..packed::KEY_EXCHANGE_PACKET_E0_INDEX + P384_PUBLIC_KEY_SIZE]) {
        if let Some(se) = local_static_p384_secret.agree(&incoming_e0) {
            (incoming_e0, Secret(SHA512::hash(se.as_bytes())))
        } else {
            return Err(Error::InvalidPacket);
        }
    } else {
        return Err(Error::InvalidPacket);
    };

    let mut c = AesGcm::new(&local_static_remote_e0.0[..32], true);
    let mut nonce: [u8; 16] = 0_u128.to_ne_bytes();
    for i in 0..COMMON_HEADER_SIZE {
        nonce[i] = buffer[i];
    }
    c.init(&nonce);
    c.crypt_in_place(&mut buffer[packed::KEY_EXCHANGE_PACKET_ENCRYPT_ENVELOPE_START..incoming_packet.len() - (16 + 48)]);
    if !c.finish().eq(&buffer[incoming_packet.len() - (16 + 48)..incoming_packet.len() - 48]) {
        return Err(Error::FailedAuthentication);
    }

    return Ok((remote_e0, local_static_remote_e0));
}

struct EphemeralOffer {
    alice_e0_bob_static: Secret<64>,
    id: [u8; 48],
    alice_e0: P384KeyPair,
    alice_e1: Option<pqc_kyber::Keypair>,
}

impl EphemeralOffer {
    fn create<const BS: usize, const STATIC_PUBLIC_SIZE: usize>(
        buffer: &mut [u8; BS],
        counter: CounterValue,
        local_session_id: u64,
        remote_session_id: u64,
        local_static_public: &[u8; STATIC_PUBLIC_SIZE],
        remote_static_p384: &P384PublicKey,
        static_secret: &Secret<64>,
        outgoing_obfuscator: &Obfuscator,
        jedi: bool,
    ) -> Option<(Box<EphemeralOffer>, usize)> {
        debug_assert!(BS >= BUFFER_SIZE_MIN);

        let alice_e0 = P384KeyPair::generate();
        let alice_e0_bob_static = Secret(SHA512::hash(alice_e0.agree(remote_static_p384)?.as_bytes())); // es == agree(my ephemeral (e0), their static)
        let alice_e1 = if jedi { Some(pqc_kyber::keypair(&mut random::SecureRandom::get())) } else { None };

        let mut p: packed::KeyOffer<STATIC_PUBLIC_SIZE> = packed::zeroed();
        p.t = PACKET_TYPE_KEY_OFFER;
        p.counter = counter.to_bytes();
        p.to_session_id.copy_from_slice(&remote_session_id.to_le_bytes()[..6]);
        p.e0 = alice_e0.public_key_bytes().clone();
        p.from_session_id.copy_from_slice(&local_session_id.to_le_bytes()[..6]);
        if let Some(e1) = alice_e1.as_ref() {
            p.e1 = e1.public;
        }
        p.static_public = local_static_public.clone();

        let mut offer_fingerprint_hash = SHA384::new();
        offer_fingerprint_hash.update(&p.e0);
        offer_fingerprint_hash.update(&p.e1);
        let offer_fingerprint_hash = offer_fingerprint_hash.finish()[..16].try_into().unwrap();

        let mut c = AesGcm::new(&alice_e0_bob_static.0[..32], true);
        c.init(&packed::get_aes_gcm_nonce(&p));
        buffer[..packed::KEY_EXCHANGE_PACKET_ENCRYPT_ENVELOPE_START].copy_from_slice(&packed::as_bytes(&p)[..packed::KEY_EXCHANGE_PACKET_ENCRYPT_ENVELOPE_START]);
        c.crypt(&packed::as_bytes(&p)[packed::KEY_EXCHANGE_PACKET_ENCRYPT_ENVELOPE_START..size_of::<packed::KeyOffer<STATIC_PUBLIC_SIZE>>() - (16 + 48)], &mut buffer[packed::KEY_EXCHANGE_PACKET_ENCRYPT_ENVELOPE_START..size_of::<packed::KeyOffer<STATIC_PUBLIC_SIZE>>() - (16 + 48)]);
        buffer[size_of::<packed::KeyOffer<STATIC_PUBLIC_SIZE>>() - (16 + 48)..size_of::<packed::KeyOffer<STATIC_PUBLIC_SIZE>>() - 48].copy_from_slice(&c.finish());

        let hmac = hmac_sha384(zt_kbkdf_hmac_sha384(&static_secret.0[..48], KBKDF_KEY_USAGE_LABEL_HMAC).as_bytes(), &buffer[..size_of::<packed::KeyOffer<STATIC_PUBLIC_SIZE>>() - 48]);
        buffer[size_of::<packed::KeyOffer<STATIC_PUBLIC_SIZE>>() - 48..size_of::<packed::KeyOffer<STATIC_PUBLIC_SIZE>>()].copy_from_slice(&hmac);

        outgoing_obfuscator.0.encrypt_block_in_place(&mut buffer[0..16]);
        outgoing_obfuscator.0.encrypt_block_in_place(&mut buffer[16..32]);
        outgoing_obfuscator.0.encrypt_block_in_place(&mut buffer[32..48]);
        outgoing_obfuscator.0.encrypt_block_in_place(&mut buffer[48..64]);

        Some((
            Box::new(EphemeralOffer {
                alice_e0_bob_static,
                id: offer_fingerprint_hash,
                alice_e0,
                alice_e1,
            }),
            size_of::<packed::KeyOffer<STATIC_PUBLIC_SIZE>>(),
        ))
    }
}

#[derive(Copy, Clone)]
struct CounterValue(u64);

impl CounterValue {
    #[inline(always)]
    pub fn to_bytes(&self) -> [u8; 4] {
        (self.0 as u32).to_le_bytes()
    }
}

struct KeyLifetime {
    rekey_at_or_after: u64,
    hard_expire_at: u64,
}

impl KeyLifetime {
    fn new(current_counter: CounterValue) -> Self {
        Self {
            rekey_at_or_after: current_counter.0 + REKEY_AFTER_USES,
            hard_expire_at: current_counter.0 + 0xfffffffe,
        }
    }

    fn expired(&self, current_counter: CounterValue) -> bool {
        current_counter.0 >= self.hard_expire_at
    }

    fn needs_rekey(&self, current_counter: CounterValue) -> bool {
        current_counter.0 >= self.rekey_at_or_after
    }
}

struct Counter(AtomicU64);

impl Counter {
    fn new() -> Self {
        // Counter starts at 1 because it's always created after an initial packet is sent.
        Self(AtomicU64::new(0))
    }

    fn next(&self) -> CounterValue {
        CounterValue(self.0.fetch_add(1, Ordering::SeqCst))
    }
}

struct SymmetricKeyQueue {
    current: Option<SymmetricKey>,
    previous: Option<SymmetricKey>,
    next: Option<SymmetricKey>,
}

struct SymmetricKeyCipherInstance {
    pool: Vec<Box<AesGcm>>,
    lifetime: KeyLifetime,
}

impl SymmetricKeyCipherInstance {
    fn new(counter_at_creation: CounterValue) -> Self {
        Self {
            pool: Vec::with_capacity(2),
            lifetime: KeyLifetime::new(counter_at_creation),
        }
    }

    fn get(&mut self, key: &[u8], encrypt: bool, current_counter: CounterValue) -> Option<Box<AesGcm>> {
        if !self.lifetime.expired(current_counter) {
            self.pool.pop().or_else(|| Some(Box::new(AesGcm::new(key, encrypt))))
        } else {
            unlikely_branch();
            None
        }
    }
}

struct SymmetricKey {
    creation_time: i64,
    key: Secret<64>,
    receive_key_index: usize,
    send_key_index: usize,
    receive_cipher: Mutex<SymmetricKeyCipherInstance>,
    send_cipher: Mutex<SymmetricKeyCipherInstance>,
    jedi: bool, // true if kyber was used
}

impl SymmetricKey {
    fn new(alice_e0_bob_static: &Secret<64>, se: &Secret<64>, alice_static_bob_static: &Secret<64>, alice_e0_bob_e0: Secret<48>, alice_e1_bob_e1: Option<[u8; pqc_kyber::KYBER_SSBYTES]>, current_time: i64, current_counter: CounterValue, this_side_is_alice: bool) -> Self {
        // The 'alice' flag is true on the side that sends the counter offer, false on the side that receives it.
        // This determines which half of the 512-bit secret is used to key AES for sending or receiving.
        let (receive_key_index, send_key_index) = if this_side_is_alice { (0, 32) } else { (32, 0) };

        let jedi = alice_e1_bob_e1.is_some();
        Self {
            creation_time: current_time,
            // It would be fine to just hash SHA384(es | se | ee0 | ee1 | ss) but NIST/FIPS specifies that the preferred
            // KDF method is HMAC(salt, key). The key must be from a FIPS-compliant exchange but the salt may be anything.
            // So we hash all but ee0 (NIST P-384 secret) into a salt and then HMAC to get the final key.
            key: Secret(hmac_sha512(
                &{
                    let mut sha = SHA512::new();
                    sha.update(alice_e0_bob_static.as_bytes());
                    sha.update(se.as_bytes());
                    if let Some(ee1) = alice_e1_bob_e1 {
                        sha.update(&ee1);
                    } else {
                        sha.update(&[0_u8; pqc_kyber::KYBER_SSBYTES]); // input is always the same length
                    }
                    sha.update(alice_static_bob_static.as_bytes());
                    sha.finish()
                },
                alice_e0_bob_e0.as_bytes(),
            )),
            receive_key_index,
            send_key_index,
            receive_cipher: Mutex::new(SymmetricKeyCipherInstance::new(current_counter)),
            send_cipher: Mutex::new(SymmetricKeyCipherInstance::new(current_counter)),
            jedi,
        }
    }

    fn get_receive_cipher(&self, current_counter: CounterValue) -> Option<Box<AesGcm>> {
        self.receive_cipher.lock().get(&self.key.0[self.receive_key_index..self.receive_key_index + 32], false, current_counter)
    }

    fn return_receive_cipher(&self, gcm: Box<AesGcm>) {
        self.receive_cipher.lock().pool.push(gcm);
    }

    fn get_send_cipher(&self, current_counter: CounterValue) -> Option<Box<AesGcm>> {
        self.send_cipher.lock().get(&self.key.0[self.send_key_index..self.send_key_index + 32], true, current_counter)
    }

    fn return_send_cipher(&self, gcm: Box<AesGcm>) {
        self.send_cipher.lock().pool.push(gcm);
    }
}

const PACKET_TYPE_DATA: u8 = 0;
const PACKET_TYPE_KEY_OFFER: u8 = 1;
const PACKET_TYPE_KEY_COUNTER_OFFER: u8 = 2;
const PACKET_TYPE_KEY_COUNTER_OFFER_ACK: u8 = 3;

const COMMON_HEADER_SIZE: usize = 11;

const KBKDF_KEY_USAGE_LABEL_HMAC: u8 = b'h';

// Code for dealing with flat packed structures. A little "unsafe" exists here but nowhere else.
mod packed {
    use super::*;

    pub(super) trait PackedStruct {}

    pub const KEY_EXCHANGE_PACKET_E0_INDEX: usize = COMMON_HEADER_SIZE;
    pub const KEY_EXCHANGE_PACKET_ENCRYPT_ENVELOPE_START: usize = KEY_EXCHANGE_PACKET_E0_INDEX + P384_PUBLIC_KEY_SIZE;

    #[repr(C, packed)]
    pub(super) struct KeyOffer<const STATIC_PUBLIC_SIZE: usize> {
        pub t: u8,
        pub counter: [u8; 4],
        pub to_session_id: [u8; 6], // zero if unknown
        pub e0: [u8; P384_PUBLIC_KEY_SIZE],
        // -- AES-GCM(ECDH(sender_e0, recipient_static)) --
        pub from_session_id: [u8; 6],
        pub e1: [u8; pqc_kyber::KYBER_PUBLICKEYBYTES],
        pub static_public: [u8; STATIC_PUBLIC_SIZE],
        // --
        pub gcm_tag: [u8; 16],
        pub hmac_static_key: [u8; 48], // HMAC-SHA384(static secret (derived HMAC key), packet)
    }

    impl<const STATIC_PUBLIC_SIZE: usize> PackedStruct for KeyOffer<STATIC_PUBLIC_SIZE> {}

    #[repr(C, packed)]
    pub(super) struct KeyCounterOffer<const STATIC_PUBLIC_SIZE: usize> {
        pub t: u8,
        pub counter: [u8; 4],
        pub to_session_id: [u8; 6], // zero if unknown
        pub e0: [u8; P384_PUBLIC_KEY_SIZE],
        // -- AES-GCM(ECDH(sender_e0, recipient_static)) --
        pub from_session_id: [u8; 6],
        pub e1: [u8; pqc_kyber::KYBER_CIPHERTEXTBYTES],
        pub in_re_offer_id: [u8; 48],
        // --
        pub gcm_tag: [u8; 16],
        pub hmac_static_key: [u8; 48], // HMAC-SHA384(static secret (derived HMAC key), packet)
    }

    impl<const STATIC_PUBLIC_SIZE: usize> PackedStruct for KeyCounterOffer<STATIC_PUBLIC_SIZE> {}

    #[repr(C, packed)]
    pub(super) struct KeyAck {
        pub t: u8,
        pub counter: [u8; 4],
        pub to_session_id: [u8; 6],
        pub hmac_shared_secret: [u8; 48], // HMAC-SHA384(shared secret (derived HMAC key), packet header | shared secret)
        pub hmac_static_key: [u8; 48],    // HMAC-SHA384(static secret (derived HMAC key), packet)
    }

    impl PackedStruct for KeyAck {}

    #[inline(always)]
    pub(super) fn as_bytes<T: PackedStruct>(t: &T) -> &[u8] {
        unsafe { &*std::ptr::slice_from_raw_parts((t as *const T).cast::<u8>(), size_of::<T>()) }
    }

    #[inline(always)]
    pub(super) fn as_packed_struct<T: PackedStruct>(t: &[u8]) -> Option<&T> {
        if t.len() >= size_of::<T>() {
            Some(unsafe { &*t.as_ptr().cast::<T>() })
        } else {
            None
        }
    }

    #[inline(always)]
    pub(super) fn get_aes_gcm_nonce<T: PackedStruct>(t: &T) -> [u8; 16] {
        debug_assert!(size_of::<T>() >= 16);
        unsafe {
            let mut tmp: [u8; 16] = std::mem::MaybeUninit::uninit().assume_init();
            std::ptr::copy_nonoverlapping((t as *const T).cast(), tmp.as_mut_ptr(), COMMON_HEADER_SIZE);
            std::ptr::write_bytes(tmp.as_mut_ptr().add(COMMON_HEADER_SIZE), 0, 16 - COMMON_HEADER_SIZE);
            tmp
        }
    }

    #[inline(always)]
    pub(super) fn zeroed<T: PackedStruct>() -> T {
        unsafe { std::mem::zeroed() }
    }
}

#[inline(always)]
fn u48_from_le_bytes<const S: usize>(b: &[u8; S]) -> u64 {
    (b[0] as u64) | (b[1] as u64).wrapping_shl(8) | (b[2] as u64).wrapping_shl(16) | (b[3] as u64).wrapping_shl(24) | (b[4] as u64).wrapping_shl(32) | (b[5] as u64).wrapping_shl(40)
}

#[cold]
#[inline(never)]
extern "C" fn unlikely_branch() {}
